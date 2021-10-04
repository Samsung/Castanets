/* eslint no-unused-vars: "off" */

'use strict';

import { WorkerManager } from '../core/worker-manager';
import serialize from './serialize-javascript';
import OffloadedFuncDetailsMap from './offloaded-func-details-map';
import Profiler from './profiler';
import decisionMaker from './decision-maker';
import { getOffloadId } from '~/util';
import Logger from '~/util/logger';

const logger = new Logger('profiler-index.js');

let workerManager;
let workers;
let coffConfigs;

export function computeResultObtained(data, workerId) {
  const { offloadId, result, workerTime } = data.data;
  if (workers) {
    workers.get(workerId).compute_tasks--;
  }
  OffloadedFuncDetailsMap.getResolver(offloadId)(result);
  OffloadedFuncDetailsMap.setEndTime(offloadId, performance.now());
  Profiler.updateFunctionData(offloadId, {
    isOffloaded: true,
    workerId: workerId,
    workerExecutionTime: Number(workerTime),
    latency: Number(
      OffloadedFuncDetailsMap.getExecutionTime(offloadId) - workerTime
    )
  });
  Profiler.updateWorkerData(offloadId, workerId, workerTime);
}

function setConfigAndOffload(config) {
  logger.log('setConfigAndOffload()');
  coffConfigs = config;
  offloadMain();
}

export function reDecideForWorker() {
  logger.log('reDecideForWorker() called');
  decisionMaker.decide(setConfigAndOffload);
}

function offloadMain() {
  const offloadInfoMap = new Map();

  window.onload = init();

  function init() {
    if (
      coffConfigs &&
      coffConfigs.functions &&
      coffConfigs.functions.length > 0
    ) {
      for (const func of coffConfigs.functions) {
        makeOffloadable(func);

        // If parent func is async, then re-write parent function.
        if (func.asyncDeps && func.asyncDeps.length > 0) {
          reWriteNonOffloadableAsyncFunction(func);
        }
      }
    } else {
      logger.debug(
        'Config file has not been loaded. Offloading will not work.'
      );
    }
  }

  function updateLocalTime(offloadId, time, offloadInfoMap) {
    offloadInfoMap[offloadId].localExecTime = Number(time);
    offloadInfoMap[offloadId].elapsedTime = Number(time);
    Profiler.updateFunctionData(offloadId, {
      clientExecutionTime: Number(time)
    });
  }

  function markFunctionNonOffloaded(offloadId) {
    Profiler.updateFunctionData(offloadId, {
      isOffloaded: false
    });
  }

  function makeOffloadable(funcObject) {
    const { id, isAsync, asyncDeps } = funcObject;
    const names = id.split('.');
    let object = window;
    for (let i = 0; i < names.length - 1; i++) {
      object = object[names[i]];
    }

    if (typeof object[names[names.length - 1]] === 'function') {
      const realName = names[names.length - 1];
      const offloadId = getOffloadId(realName);
      if (Profiler.isFunctionOffloaded(offloadId)) {
        offloadInfoMap[offloadId] = {
          funcObject: window[offloadId],
          elapsedTime: 0,
          localExecTime: -1
        };
      } else {
        offloadInfoMap[offloadId] = {
          funcObject: object[realName],
          elapsedTime: 0,
          localExecTime: -1
        };
        window[offloadId] = object[realName];
      }

      // Rewrite original function to make offloadable.
      const funcString = object[realName].toString();
      const params = funcString.slice(
        funcString.indexOf('('),
        funcString.indexOf(')') + 1
      );

      let newoffloadableFuncDef =
        'object[realName] = function ' +
        realName +
        params +
        ' {\n' +
        '  let ret;\n' +
        '  const args = Array.prototype.slice.call(arguments);\n' +
        '  if (offloadInfoMap["' +
        offloadId +
        '"].localExecTime' +
        ' > -1) {\n' +
        '    const t0 = performance.now();\n' +
        '    ret = offloadTo("' +
        offloadId +
        '", args);\n' +
        '    const t1 = performance.now();\n' +
        '    console.log("elapsed time for ' +
        realName +
        ': " + (t1 - t0));\n' +
        '  } else {\n' +
        '    const t0 = performance.now();\n' +
        '    ret = offloadInfoMap["' +
        offloadId +
        '"].funcObject.\n' +
        '        apply(null, args);\n' +
        '    updateLocalTime("' +
        offloadId +
        '", performance.now() - t0, offloadInfoMap);\n' +
        '  }\n' +
        '  return ret;\n' +
        '};';

      // If current function is async or having any async dependency,
      // then call offloadToAsync and remove time calculations.
      if (isAsync || (asyncDeps && asyncDeps.length > 0)) {
        newoffloadableFuncDef =
          'object[realName] = async function ' +
          realName +
          params +
          ' {\n' +
          '  let ret;\n' +
          '  const args = Array.prototype.slice.call(arguments);\n' +
          '  if (offloadInfoMap["' +
          offloadId +
          '"].localExecTime' +
          ' > -1) {\n' +
          '    ret = offloadToAsync("' +
          offloadId +
          '", args);\n' +
          'markFunctionNonOffloaded("' +
          offloadId +
          '");\n' +
          '  } else {\n' +
          '    const t0 = performance.now();\n' +
          '    ret = await offloadInfoMap["' +
          offloadId +
          '"].funcObject.\n' +
          '        apply(null, args);\n' +
          '    updateLocalTime("' +
          offloadId +
          '", performance.now() - t0, offloadInfoMap);\n' +
          '  }\n' +
          '  return ret;\n' +
          '};';
      }
      eval(newoffloadableFuncDef);
      Profiler.updateFunctionData(offloadId, {
        isOffloaded: true
      });
      logger.log('From now on, ' + id + ' is offloadable.');
    } else {
      logger.log(id + 'is not function object.');
    }
  }

  function findOffloadWorker() {
    workerManager = WorkerManager.getInstance();
    workers = workerManager.getWorkerInfos();
    // Choose compute worker with lesser number of pending compute tasks.
    let offloadWorkerId = undefined;
    let mintasks = 999999;
    for (const [key, value] of workers.entries()) {
      for (const feature of value.features) {
        if (feature === 'COMPUTE') {
          if (value.compute_tasks < mintasks) {
            offloadWorkerId = key;
            mintasks = value.compute_tasks;
          }
        }
      }
    }
    return offloadWorkerId;
  }

  /**
   * Rewrite non-offloadable async function
   * @param {Function} func
   */
  function reWriteNonOffloadableAsyncFunction(func) {
    const offloadableFuncName = func.id;
    const offloadableFuncDeps = func.asyncDeps;
    // For all the non offloadable dependencies, re-write its body
    // by replacing offloadableFuncName with await offloadableFuncName.
    offloadableFuncDeps.forEach(dependency => {
      const names = dependency.split('.');
      let object = window;
      for (let i = 0; i < names.length - 1; i++) {
        object = object[names[i]];
      }

      if (typeof object[names[names.length - 1]] === 'function') {
        const realName = names[names.length - 1];

        // Rewrite original function to replace offloadableFuncName with await offloadableFuncName.
        const funcString = serialize(object[realName]).toString();
        const params = funcString.slice(
          funcString.indexOf('('),
          funcString.indexOf(')') + 1
        );
        const fnBody = funcString
          .slice(funcString.indexOf('{'), funcString.lastIndexOf('}') + 1)
          .replace(
            offloadableFuncName + '(',
            'await ' + offloadableFuncName + '('
          );
        const newFuncDefAsync =
          'object[realName] = async function ' + realName + params + fnBody;
        eval(newFuncDefAsync);
        logger.debug(dependency + ' is re-written successfully.');
      } else {
        logger.debug(dependency + ' is not a function object.');
      }
    });
  }

  function offloadToAsync(offloadId, params) {
    const promise = new Promise((resolve, reject) => {
      logger.debug(
        'Function ' +
          offloadInfoMap[offloadId].funcObject.name +
          ' will be offloaded asynchronously with ' +
          params.toString()
      );
      OffloadedFuncDetailsMap.setStartTime(offloadId, performance.now());

      const bodyData = {};
      bodyData.execStatement =
        offloadInfoMap[offloadId].funcObject.toString() +
        '\n' +
        offloadInfoMap[offloadId].funcObject.name +
        '.call(null' +
        (params.length > 0 ? ', ' + serialize(params) : '') +
        ');';
      bodyData.timeout = 3000;
      bodyData.offloadId = offloadId;

      const offloadWorker = findOffloadWorker();
      if (offloadWorker !== undefined) {
        workers.get(offloadWorker).compute_tasks++;
        OffloadedFuncDetailsMap.setResolver(offloadId, resolve);
        const worker = workerManager.getOrCreateWorker(offloadWorker);
        worker.startJob({
          arguments: JSON.stringify(bodyData),
          feature: 'COMPUTE'
        });
        logger.log('message sent to: ' + offloadWorker, bodyData);
      } else {
        logger.debug('No compute worker registered');
        resolve(undefined);
      }
    });
    // return a promise to client application.
    OffloadedFuncDetailsMap.setPromise(offloadId, promise);
    return promise;
  }

  function offloadTo(offloadId, params) {
    logger.debug(
      'Function ' +
        offloadInfoMap[offloadId].funcObject.name +
        ' will be offloaded with ' +
        params.toString()
    );
    try {
      const xobj = new XMLHttpRequest('application/json');
      // Use synchronous request because the function should be returned
      // synchronously.
      xobj.open('POST', 'http://127.0.0.1:9559/api/js-offloading', false);

      const bodyData = {};
      bodyData.execStatement =
        offloadInfoMap[offloadId].funcObject.toString() +
        '\n' +
        offloadInfoMap[offloadId].funcObject.name +
        '.call(null' +
        (params.length > 0 ? ', ' + serialize(params) : '') +
        ');';
      bodyData.timeout = 3000;
      const offloadWorker = findOffloadWorker();
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
      } else {
        logger.debug(
          'Unable to offload ' +
            offloadInfoMap[offloadId].funcObject.name +
            ': ' +
            xobj.status
        );
      }
      return ret;
    } catch (e) {
      logger.error(e);
      return 0;
    }
  }
}
