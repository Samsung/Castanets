/* eslint no-unused-vars: "off" */

'use strict';

import { WorkerManager } from '../core/worker-manager';
import OffloadIdAndPromiseMap from './OffloadIDAndPromiseMap';
import serialize from './serialize-javascript';
import Logger from '~/util/logger';

const logger = new Logger('task/index.js');

let workerManager;
let workers;
let coffConfigs;
let globalVar = new Map();

function updateGlobalVar() {
  let string = '';
  for (const [key, value] of Object.entries(globalVar)) {
    string = key.toString() + ' = ' + value.toString();
    try {
      eval(string);
    } catch (e) {
      logger.error(e); // To handle keys from sampleContext
    }
  }
}

export function computeResultObtained(data, workerId) {
  const { offloadId, result } = data.data;
  workers.get(workerId).compute_tasks--;
  const resolver = OffloadIdAndPromiseMap.getResolver(offloadId);
  resolver(result);
  globalVar = data.data.globalVar;
  updateGlobalVar();
}

export function loadConfigurationFrom(path) {
  try {
    const xobj = new XMLHttpRequest('application/json');
    // Use synchronous request in order to guarantee that
    // loading configs is completed before onload event.
    xobj.open('GET', path, false);
    xobj.send(null);
    if (xobj.status === 200) {
      coffConfigs = JSON.parse(xobj.responseText);
      startOffloading();
    } else {
      logger.error('Unable to load config file: ' + xobj.status);
      throw new Error('Unable to load config file');
    }
  } catch (e) {
    logger.error(e);
    throw new Error('Unable to load config file');
  }
}

const startOffloading = function () {
  const offloadInfoMap = new Map();
  const ELAPSED_TIME_THRESHOLD_MS = -1; // Changed it to -1 so as to make offloadable every time.

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
      'Config file is not in expected format. Offloading will not work.'
    );
  }

  function makeOffloadable(fn) {
    // Find an object that is owned the function.
    const functionName = fn.id;
    const globalVariables = Array.from(new Set(fn.globalVars));
    const names = functionName.split('.');
    let object = window;
    for (let i = 0; i < names.length - 1; i++) {
      object = object[names[i]];
    }
    for (const globalVariable of globalVariables) {
      globalVar[globalVariable] =
        object[globalVariable] || eval(globalVariable);
    }

    if (typeof object[names[names.length - 1]] === 'function') {
      const realName = names[names.length - 1];
      const offloadId =
        realName + '_' + Math.random().toString(36).substr(2, 9);
      // Create information for offloading.
      offloadInfoMap[offloadId] = {
        funcObject: object[realName],
        elapsedTime: 0
      };

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
        '"].elapsedTime' +
        ' > ELAPSED_TIME_THRESHOLD_MS) {\n' +
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
        '    const t1 = performance.now();\n' +
        '    console.log("elapsed time for ' +
        realName +
        ': " + (t1 - t0));\n' +
        '    offloadInfoMap["' +
        offloadId +
        '"].elapsedTime = t1 - t0;\n' +
        '  }\n' +
        '  return ret;\n' +
        '};';

      // If current function is async or having any async dependency,
      // then call offloadToAsync and remove time calculations.
      if (fn.isAsync || (fn.asyncDeps && fn.asyncDeps.length > 0)) {
        newoffloadableFuncDef =
          'object[realName] = async function ' +
          realName +
          params +
          ' {\n' +
          '  let ret;\n' +
          '  const args = Array.prototype.slice.call(arguments);\n' +
          '  if (offloadInfoMap["' +
          offloadId +
          '"].elapsedTime' +
          ' > ELAPSED_TIME_THRESHOLD_MS) {\n' +
          '    ret = offloadToAsync("' +
          offloadId +
          '", args);\n' +
          '  } else {\n' +
          '    const t0 = performance.now();\n' +
          '    ret = offloadInfoMap["' +
          offloadId +
          '"].funcObject.\n' +
          '        apply(null, args);\n' +
          '    const t1 = performance.now();\n' +
          '    console.log("elapsed time for ' +
          realName +
          ': " + (t1 - t0));\n' +
          '    offloadInfoMap["' +
          offloadId +
          '"].elapsedTime = t1 - t0;\n' +
          '  }\n' +
          '  return ret;\n' +
          '};';
      }
      eval(newoffloadableFuncDef);
      logger.debug('From now on, ' + functionName + ' is offloadable.');
    } else {
      logger.debug(functionName + 'is not function object.');
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

      const bodyData = {};
      bodyData.execStatement =
        offloadInfoMap[offloadId].funcObject.toString() +
        '\n' +
        offloadInfoMap[offloadId].funcObject.name +
        '.call(null' +
        (params.length > 0 ? ', ' + serialize(params) : '') +
        ');';
      bodyData.timeout = 3000;
      bodyData.globalVar = globalVar;
      bodyData.offloadId = offloadId;
      logger.debug('RESOLVER: ', resolve.toString());
      const offloadWorker = findOffloadWorker();
      if (offloadWorker !== undefined) {
        workers.get(offloadWorker).compute_tasks++;
        OffloadIdAndPromiseMap.setResolver(offloadId, resolve);
        const worker = workerManager.getOrCreateWorker(offloadWorker);
        worker.startJob({
          arguments: JSON.stringify(bodyData),
          feature: 'COMPUTE',
          resolver: resolve
        });
      } else {
        logger.debug('No compute worker registered');
        resolve(undefined);
      }
    });
    // return a promise to client application.
    OffloadIdAndPromiseMap.setPromise(offloadId, promise);
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
      xobj.open('POST', '/api/js-offloading', false);

      const bodyData = {};
      bodyData.execStatement =
        offloadInfoMap[offloadId].funcObject.toString() +
        '\n' +
        offloadInfoMap[offloadId].funcObject.name +
        '.call(null' +
        (params.length > 0 ? ', ' + serialize(params) : '') +
        ');';
      bodyData.timeout = 3000;
      bodyData.globalVar = globalVar;
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
        globalVar = response.globalVar;
        updateGlobalVar();
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
};
