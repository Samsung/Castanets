import DBManager from './db-manager';
import serialize from './serialize-javascript';
import OffloadedFuncDetailsMap from './offloaded-func-details-map';
import { WorkerManager } from '../core/worker-manager';
import { getOffloadId } from '~/util';
import Logger from '~/util/logger';

const logger = new Logger('profiler.js');

let workers;
let workerManager;

class Profiler {
  constructor() {
    if (!Profiler.instance) {
      Profiler.instance = this;
    }

    return Profiler.instance;
  }

  /**
   * API to update function data in DB.
   * @param {String} functionId
   * @param {Object} data
   */
  updateFunctionData(functionId, data) {
    DBManager.set(functionId, data);
  }

  /**
   * API to update worker data in DB.
   * @param {String} functionId
   * @param {String} workerId
   * @param {Number} time
   */
  updateWorkerData(functionId, workerId, time) {
    DBManager.setWorker(functionId, workerId, time);
  }

  /**
   * API to begin profiling of the functions for respective workers.
   * @param {Object} funcObject - Object of data of function
   * @param {String} workerId
   */
  async beginProfiling(funcObject, workerId) {
    logger.log(' beginProfiling() called');
    this.setWorkers(null);
    const { id, testingParams, paramsCount } = funcObject;
    const names = id.split('.');
    let object = window;
    for (let i = 0; i < names.length - 1; i++) {
      object = object[names[i]];
    }

    if (typeof object[names[names.length - 1]] === 'function') {
      const realName = names[names.length - 1];
      const offloadId = getOffloadId(realName);
      this.updateFunctionData(offloadId, { isOffloaded: false });

      const funcDef =
        typeof object[offloadId] === 'function'
          ? object[offloadId]
          : object[realName];

      if (
        paramsCount <= 0 ||
        (testingParams && testingParams.length === paramsCount)
      ) {
        if (workerId === 'client') {
          this.localExecution(offloadId, funcDef, testingParams);
        } else {
          this.executeOnWorker(offloadId, funcDef, workerId, funcObject);
        }
      }
    }
  }

  /**
   * API to execute given function locally.
   * @param {String} offloadId
   * @param {Object} func - Function Definition
   * @param {Array} args - Function params
   */
  localExecution(offloadId, func, args = null) {
    const t0 = performance.now();
    func(...args);
    const timeTook = performance.now() - t0;
    this.updateWorkerData(offloadId, 'client', timeTook);
  }

  /**
   * API use to set workers to global variable.
   * @param {Array} computeWorkers - List of workers
   */
  setWorkers(computeWorkers) {
    logger.log('setWorkers() ', workers);
    workerManager = WorkerManager.getInstance();
    workers = computeWorkers || workerManager.getWorkerInfos();
  }

  /**
   * API to execute function on worker.
   * @param {String} offloadId - Offload ID of function
   * @param {Function} funcDef - Function definition
   * @param {String} workerId - Worker on which it is going to test
   * @param {Object} funcObject - Function Details
   */
  executeOnWorker(offloadId, funcDef, workerId, funcObject) {
    const { id, testingParams, isAsync, asyncDeps } = funcObject;
    if (isAsync || (asyncDeps && asyncDeps.length > 0)) {
      this.offloadToAsync(offloadId, testingParams, workerId, id, funcDef);
    } else {
      this.offloadTo(offloadId, testingParams, workerId, id, funcDef);
    }
  }

  /**
   * Offload a function in async manner.
   * @param {String} offloadId - OffloadId of the function
   * @param {Array} params - Arguments of function to be called
   * @param {String} workerId - Worker on which function going to be execute
   * @param {String} name - Name of Function
   * @param {Function} funcDef
   * @return {Promise} offload promise
   */
  offloadToAsync(offloadId, params, workerId, name, funcDef) {
    const promise = new Promise((resolve, reject) => {
      logger.log(
        'Function ' +
          name +
          ' will be deciding to be offloaded asynchronously with ' +
          params.toString()
      );
      OffloadedFuncDetailsMap.setStartTime(offloadId, performance.now());

      const bodyData = {};
      bodyData.execStatement =
        funcDef.toString() +
        '\n' +
        name +
        '.call(null' +
        (params.length > 0 ? ', ' + serialize(params) : '') +
        ');';
      bodyData.timeout = 3000;
      bodyData.offloadId = offloadId;

      const offloadWorker = workerId;
      if (offloadWorker !== undefined) {
        workers.get(offloadWorker).compute_tasks++;
        OffloadedFuncDetailsMap.setResolver(offloadId, resolve);
        workerManager.sendMessage(offloadWorker, {
          data: JSON.stringify(bodyData),
          type: 'COMPUTE'
        });
        logger.log('message sent to: ' + offloadWorker, bodyData);
      } else {
        logger.log('No compute worker registered');
        resolve(undefined);
      }
    });
    // return a promise to client application.
    OffloadedFuncDetailsMap.setPromise(offloadId, promise);
    return promise;
  }

  /**
   * Offload a function in sync manner.
   * @param {String} offloadId - OffloadId of the function
   * @param {Array} params - Arguments of function to be called
   * @param {String} workerId - Worker on which function going to be execute
   * @param {String} name - Name of Function
   * @param {Function} funcDef
   * @return {Promise} offload promise
   */
  offloadTo(offloadId, params, workerId, name, funcDef) {
    const t0 = performance.now();
    logger.log(
      'Function ' + name + ' will be offloaded with ' + params.toString()
    );
    logger.log('offloadTo()', funcDef);
    try {
      const xobj = new XMLHttpRequest('application/json');
      // Use synchronous request because the function should be returned
      // synchronously.
      xobj.open('POST', 'http://127.0.0.1:9559/api/js-offloading', false);

      const bodyData = {};
      bodyData.execStatement =
        funcDef.toString() +
        '\n' +
        name +
        '.call(null' +
        (params.length > 0 ? ', ' + serialize(params) : '') +
        ');';
      bodyData.timeout = 3000;
      const offloadWorker = workerId;
      if (offloadWorker !== undefined) {
        workers.get(offloadWorker).compute_tasks++;
        bodyData.workerSocket = workers.get(offloadWorker).socketId;
      }

      xobj.send(JSON.stringify(bodyData));

      let ret = undefined;
      if (xobj.status === 200) {
        const response = JSON.parse(xobj.responseText);
        ret = response.result;
        workers.get(offloadWorker).compute_tasks--;
        const timeTook = performance.now() - t0;
        this.updateWorkerData(offloadId, workerId, timeTook);
      } else {
        logger.log('Unable to offload ' + name + ': ' + xobj.status);
        this.updateWorkerData(offloadId, workerId, 0);
      }
      return ret;
    } catch (e) {
      logger.log(e);
      return 0;
    }
  }

  /**
   * Get function deta from DB.
   * @param {String} functionId
   * @return {Object} function data
   */
  getFunctionData(functionId) {
    return DBManager.get(functionId);
  }

  /**
   * Get best worker for the given function id.
   * @param {String} functionId
   * @return {Array} Workers
   */
  getBestWorkerForFunction(functionId) {
    const workersInfo = DBManager.getAllWorkersDataForFunctionId(functionId);
    const bestWorkersList = [];
    for (const computeWorkerIdx in workersInfo) {
      if (workersInfo[computeWorkerIdx]) {
        const computeWorker = workersInfo[computeWorkerIdx];
        bestWorkersList.push(computeWorker.functionId);
      }
    }
    return bestWorkersList;
  }

  /**
   * API to check given function is already offloaded.
   * @param {String} functionId
   * @return {Boolean} Check if function already offloaded
   */
  isFunctionOffloaded(functionId) {
    return typeof window[functionId] === 'function';
  }
}

const instance = new Profiler();
Object.freeze(instance);

export default instance;
