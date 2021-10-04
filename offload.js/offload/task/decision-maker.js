import Profiler from './profiler';
import '../offload';
import { WorkerManager } from '../core/worker-manager';
import { getOffloadId } from '~/util';
import Logger from '~/util/logger';

const logger = new Logger('decision-maker.js');

let workerManager;
let workers;

class DecisionMaker {
  constructor() {
    this._decisionData = {
      db: null,
      coffConfigs: {},
      computeWorkers: [],
      resultCoffConfigs: {
        functions: []
      }
    };
    if (!DecisionMaker.instance) {
      DecisionMaker.instance = this;
    }

    return DecisionMaker.instance;
  }

  /**
   * API to load config.json file.
   */
  loadConfigs() {
    logger.log(' loadConfigs() called');
    try {
      const xobj = new XMLHttpRequest('application/json');
      // Use synchronous request in order to guarantee that
      // loading configs is completed before onload event.
      xobj.open('GET', 'coff_config.json', false);
      xobj.send(null);
      if (xobj.status === 200) {
        this._decisionData.coffConfigs = JSON.parse(xobj.responseText);
      } else {
        logger.log('Unable to load config file: ' + xobj.status);
      }
    } catch (e) {
      logger.log(e);
    }
  }

  /**
   * API to fetch all the workers with Compute support.
   * @return {Array} list of worker Ids
   */
  getOffloadWorkers() {
    logger.log('getOffloadWorkers() called');
    const workerIDs = ['client'];
    workerManager = WorkerManager.getInstance();
    workers = workerManager.getWorkerInfos();
    for (const [key, value] of workers.entries()) {
      for (const feature of value.features) {
        if (feature === 'COMPUTE') {
          workerIDs.push(key);
        }
      }
    }
    this._decisionData.computeWorkers = workerIDs;
    return workerIDs;
  }

  /**
   * Function to push data to resultConfig Array.
   * @param {*} func
   */
  pushToResultantConfig(func) {
    if (func && Object.keys(func).length > 0) {
      this._decisionData.resultCoffConfigs.functions.push(func);
    }
  }

  /**
   * Entry point to decision maker class.
   * @param {Function} callback
   */
  async decide(callback) {
    logger.log(' decide() called ');
    this.decideUtil(callback);
  }

  /**
   * Helper function for decide API.
   * @param {Function} callback
   */
  decideUtil(callback) {
    logger.log(' decideUtil() called');
    this.loadConfigs();
    this.getOffloadWorkers();
    this.profileData();
    this.getResults(callback);
  }

  /**
   * API to profile all function with present workers.
   */
  profileData() {
    logger.log(' profileData() called');
    Profiler.setWorkers(workers);
    const { coffConfigs, computeWorkers } = this._decisionData;
    if (
      coffConfigs &&
      coffConfigs.functions &&
      coffConfigs.functions.length > 0
    ) {
      for (const func of coffConfigs.functions) {
        for (const worker of computeWorkers) {
          Profiler.beginProfiling(func, worker);
        }
      }
    }
  }

  /**
   * API to get results from profiled data.
   * @param {*} callback
   */
  getResults(callback) {
    logger.log('getResults() called');
    const { coffConfigs } = this._decisionData;
    if (
      coffConfigs &&
      coffConfigs.functions &&
      coffConfigs.functions.length > 0
    ) {
      for (const func of coffConfigs.functions) {
        const suitableWorkers = Profiler.getBestWorkerForFunction(
          getOffloadId(func.id)
        );
        if (suitableWorkers && suitableWorkers.length) {
          this.pushToResultantConfig(func);
        }
      }
    }

    callback(this._decisionData.resultCoffConfigs);
  }
}

const instance = new DecisionMaker();
Object.freeze(instance);

export default instance;
