import vm from 'vm';
import { Worker } from './worker';
import { isPromise } from '~/util';
import Logger from '~/util/logger';

const logger = new Logger('compute.js');

export class ComputeWorker extends Worker {
  constructor() {
    super();
  }

  checkCapability() {
    // The worker is always available for js compute.
    this.features.add('COMPUTE');
    return this.features;
  }

  handleMessage(message) {
    this.doComputingJob(message.arguments);
  }

  doComputingJob(data, callback) {
    const body =
      typeof callback === 'function'
        ? JSON.parse(data.message.data)
        : JSON.parse(data);
    // TODO(djmix.kim) : return sample
    const startTime = performance.now();
    const jsonRet = {
      function: '',
      timeout: 0,
      result: 0,
      errorCode: 0,
      offloadId: body.offloadId
    };

    // TODO(djmix.kim) : Add context with JSON
    const sampleContext = {
      sampleKey1: 1,
      sampleKey2: 2,
      sampleKey3: 0
    };

    // If keys are same then, the value in sampleContext will be considered
    // So keys in sampleContext should be very descriptive
    const context = Object.assign(body.globalVar, sampleContext);
    let ctx;

    let execRet = '-1';
    try {
      const script = new vm.Script(body.execStatement);
      ctx = vm.createContext(context);

      execRet = script.runInContext(
        ctx,
        { timeout: body.timeout },
        { displayErrors: true }
      );
    } catch (e) {
      logger.error(e);
      if (e.code === 'ERR_SCRIPT_EXECUTION_TIMEOUT') {
        logger.error(' 408 error : request timeout ', body.timeout);
        jsonRet.errorCode = e.code;
      }

      // TODO(djmix.kim) : return detail error code to runtime
      logger.error(' 400 error');
      jsonRet.errorCode = 'UNKNOWN ERROR';
      return;
    }

    jsonRet.execStatement = body.execStatement;
    jsonRet.globalVar = ctx;
    // If result is a promise, then resolve it and emit message.
    if (isPromise(execRet)) {
      execRet.then(res => {
        jsonRet.result = res;
        jsonRet.workerTime = performance.now() - startTime;
        if (typeof callback === 'function') {
          logger.debug(' Sending compute result via callback');
          callback(jsonRet);
        } else {
          logger.debug(' Sending compute result back in data channel');
          this.dataChannel.send(
            JSON.stringify({
              type: 'data',
              feature: 'COMPUTE',
              data: jsonRet
            })
          );
        }
      });
    } else {
      // If result is not a promise, then store it and emit message.
      jsonRet.result = execRet;
      jsonRet.workerTime = performance.now() - startTime;
      if (typeof callback === 'function') {
        logger.debug('Sending compute result via callback');
        callback(jsonRet);
      } else {
        logger.debug('Sending compute result back in data channel');
        this.dataChannel.send(
          JSON.stringify({
            type: 'data',
            feature: 'COMPUTE',
            data: jsonRet
          })
        );
      }
    }
  }
}
