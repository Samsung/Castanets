import Logger from '~/util/logger';

const logger = new Logger('offloaded-func-details-map.js');

class OffloadedFuncDetailsMap {
  constructor() {
    if (!OffloadedFuncDetailsMap.instance) {
      this._data = {
        offloadId: {
          deps: [],
          promise: null,
          resolver: null,
          startTime: performance.now(),
          endTime: performance.now(),
          execTime: performance.now()
        }
      };
      OffloadedFuncDetailsMap.instance = this;
    }

    return OffloadedFuncDetailsMap.instance;
  }

  // ##################### List of Setters ###########################
  set(key, value) {
    logger.log('Setting Value: ' + JSON.stringify(value) + ' to key: ' + key);
    this._data[key] = value;
  }

  setPromise(key, promise) {
    logger.log(
      'Setting promise: ' + JSON.stringify(promise) + ' to key: ' + key
    );
    const funcObject = this._data[key];
    if (funcObject) {
      funcObject.promise = promise;
    } else {
      this._data[key] = {
        promise: promise,
        deps: [],
        resolver: null,
        startTime: 0,
        endTime: 0,
        execTime: 0
      };
    }
  }

  setDependencies(key, deps) {
    logger.log(
      'Setting Dependencies: ' + JSON.stringify(deps) + ' to key: ' + key
    );
    const funcObject = this._data[key];
    if (funcObject) {
      funcObject.deps = deps;
    } else {
      this._data[key] = {
        promise: null,
        deps: deps,
        resolver: null,
        startTime: 0,
        endTime: 0,
        execTime: 0
      };
    }
  }

  setResolver(key, resolver) {
    logger.log(
      'Setting Resolver: ' + JSON.stringify(resolver) + ' to key: ' + key
    );
    const dataObj = this.get(key);
    if (dataObj) {
      dataObj.resolver = resolver;
    } else {
      this._data[key] = {
        promise: null,
        deps: [],
        resolver: resolver,
        startTime: 0,
        endTime: 0,
        execTime: 0
      };
    }
  }

  setStartTime(key, time) {
    logger.log(
      'Setting Start Time: ' + JSON.stringify(time) + ' to key: ' + key
    );
    const dataObj = this.get(key);
    if (dataObj) {
      dataObj.startTime = time;
      dataObj.execTime = dataObj.endTime - dataObj.startTime;
    } else {
      this._data[key] = {
        promise: null,
        deps: [],
        resolver: null,
        startTime: time,
        endTime: 0,
        execTime: 0
      };
    }
  }

  setEndTime(key, time) {
    logger.log('Setting End Time: ' + JSON.stringify(time) + ' to key: ' + key);
    const dataObj = this.get(key);
    if (dataObj) {
      dataObj.endTime = time;
      dataObj.execTime = dataObj.endTime - dataObj.startTime;
    } else {
      this._data[key] = {
        promise: null,
        deps: [],
        resolver: null,
        startTime: 0,
        endTime: time,
        execTime: 0
      };
    }
  }

  // ##################### List of Getters ###########################
  get(key) {
    logger.log('Getting data for key: ' + key);
    if (this._data.hasOwnProperty(key)) {
      return this._data[key];
    }
    return undefined;
  }

  viewData() {
    logger.log('Getting data : ');
    logger.table(this._data);
  }

  getPromise(key) {
    return this.getValue(key, 'promise');
  }

  getDependencies(key) {
    return this.getValue(key, 'deps', []);
  }

  getResolver(key) {
    return this.getValue(key, 'resolver', () => {});
  }

  getStartTime(key) {
    return this.getValue(key, 'startTime', 0);
  }

  getEndTime(key) {
    return this.getValue(key, 'endTime', 0);
  }

  getExecutionTime(key) {
    return this.getValue(key, 'execTime', 0);
  }

  getValue(key, value, defaultValue = undefined) {
    const funcObject = this._data[key];
    if (funcObject && funcObject.hasOwnProperty(value)) {
      return funcObject[value];
    }
    return defaultValue;
  }
}

const instance = new OffloadedFuncDetailsMap();
Object.freeze(instance);

export default instance;
