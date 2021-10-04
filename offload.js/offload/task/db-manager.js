import Loki from 'lokijs';
import Logger from '~/util/logger';

const logger = new Logger('db-manager.js');

const LokiIndexedAdapter = require('lokijs/src/loki-indexed-adapter');
const idbAdapter = new LokiIndexedAdapter('app');

class DBManager {
  constructor() {
    if (!DBManager.instance) {
      this.data = {
        _db: new Loki('profilerData.db', {
          autoload: true,
          autoloadCallback: () => this.databaseInitialize(),
          autosave: true,
          autosaveInterval: 4000,
          adapter: idbAdapter,
          options: {
            persistenceMethod: 'adapter'
          }
        }),
        collections: {
          functDetails: null,
          workerDetails: null
        }
      };
      DBManager.instance = this;
    }

    return DBManager.instance;
  }

  /**
   * Load the tables of the database.
   */
  databaseInitialize() {
    let funcDetailsCollection = this.data._db.getCollection('funcDetails');
    let workerDetailsCollection = this.data._db.getCollection('workerDetails');
    if (funcDetailsCollection === null) {
      funcDetailsCollection = this.data._db.addCollection('funcDetails', {
        unique: ['functionId']
      });
    }
    if (workerDetailsCollection === null) {
      workerDetailsCollection = this.data._db.addCollection('workerDetails', {
        unique: ['customKey']
      });
    }

    this.data.collections.functDetails = funcDetailsCollection;
    this.data.collections.workerDetails = workerDetailsCollection;

    this.verifyBasicCheck();
  }

  /**
   * Verify the records in both the tables.
   */
  verifyBasicCheck() {
    const { functDetails, workerDetails } = this.data.collections;
    logger.log(
      'Number of entries in functionDetails table : ' + functDetails.count()
    );
    logger.log(
      'Number of entries in workerDetails table : ' + workerDetails.count()
    );
  }

  /**
   * API to set data to functionDetails table.
   * @param {String} key
   * @param {Object} values
   */
  set(key, values) {
    const { functDetails } = this.data.collections;
    if (this.get(key)) {
      this.update(key, values);
    } else {
      const {
        isOffloaded,
        workerId,
        clientExecutionTime,
        workerExecutionTime,
        latency
      } = values;

      functDetails.insert({
        functionId: key,
        timestamp: performance.now(),
        functionName: key.substr(0, key.lastIndexOf('_')),
        isOffloaded: isOffloaded || false,
        workerId: workerId || '',
        clientExecutionTime: clientExecutionTime || null,
        workerExecutionTime,
        latency
      });
      this.data._db.saveDatabase();
      logger.log(
        'Entry added in function DB. Total Count : ' + functDetails.count()
      );
    }
  }

  /**
   * API to update data to functionDetails table.
   * @param {String} key
   * @param {Object} values
   */
  update(key, values) {
    const { functDetails } = this.data.collections;
    const {
      isOffloaded,
      workerId,
      clientExecutionTime,
      workerExecutionTime,
      latency
    } = values;

    const prevValues = this.get(key);

    functDetails.findAndUpdate({ $loki: prevValues.$loki }, item => {
      item.workerId = workerId || prevValues.workerId;
      item.timestamp = performance.now();
      item.isOffloaded = isOffloaded || prevValues.isOffloaded;
      item.clientExecutionTime =
        clientExecutionTime || prevValues.clientExecutionTime;
      item.workerExecutionTime =
        workerExecutionTime || prevValues.workerExecutionTime;
      item.latency = latency || prevValues.latency;
      item.functionName = key.substr(0, key.lastIndexOf('_'));
    });
    this.data._db.saveDatabase();
    logger.log(
      'Entry updated in function DB for functionId : ' +
        key +
        ', total records: ' +
        functDetails.count()
    );
  }

  /**
   * API to fetch the data for given function Id from functionDetails table.
   * @param {String} functionId
   * @return {Object} function details
   */
  get(functionId) {
    const { functDetails } = this.data.collections;
    logger.log('Getting function data from DB for key: ' + functionId);
    return functDetails.findOne({ functionId: functionId });
  }

  /**
   * API to set worker details in workerDetails table.
   * @param {String} functionId
   * @param {String} workerId
   * @param {Number} time
   */
  setWorker(functionId, workerId, time) {
    const { workerDetails } = this.data.collections;
    const key = functionId + '_' + workerId;
    const clientData = this.getWorkerData(functionId, 'client');
    const clientTime = clientData ? clientData.time : 1;
    if (this.getWorkerData(functionId, workerId)) {
      this.updateWorker(functionId, workerId, time, clientTime);
    } else {
      workerDetails.insert({
        functionId,
        timestamp: performance.now(),
        workerId,
        time,
        offloadFactor: workerId === 'client' ? 1 : Number(time / clientTime),
        customKey: key
      });
      this.data._db.saveDatabase();
      logger.log(
        'Entry added in database. Total Count : ' + workerDetails.count()
      );
    }
  }

  /**
   * API to update worker details in table
   * @param {String} functionId
   * @param {String} workerId
   * @param {Number} time
   * @param {Number} clientTime
   */
  updateWorker(functionId, workerId, time, clientTime) {
    const { workerDetails } = this.data.collections;
    const prevValues = this.getWorkerData(functionId, workerId);

    workerDetails.findAndUpdate({ $loki: prevValues.$loki }, item => {
      item.timestamp = performance.now();
      item.time = time || prevValues.time;
      item.offloadFactor =
        Number(time / clientTime) || prevValues.offloadFactor;
    });
    this.data._db.saveDatabase();
    logger.log(
      'Entry updated in database for functionId : ' +
        functionId +
        ', total records: ' +
        workerDetails.count()
    );
  }

  /**
   * API to get data for the combo of given functionId and workerId
   * @param {String} functionId
   * @param {String} workerId
   * @return {Object} worker data
   */
  getWorkerData(functionId, workerId) {
    const { workerDetails } = this.data.collections;
    logger.log('Getting worker data for key: ' + functionId + '_' + workerId);
    return workerDetails.findOne({ customKey: functionId + '_' + workerId });
  }

  /**
   * API to get data for all workers for given functionId/
   * @param {String} functionId
   * @return {Array} workers data
   */
  getAllWorkersDataForFunctionId(functionId) {
    const { workerDetails } = this.data.collections;
    logger.log('Getting all workers data for key: ' + functionId);
    return workerDetails.find({ functionId: functionId });
  }
}

const instance = new DBManager();
Object.freeze(instance);

export default instance;
