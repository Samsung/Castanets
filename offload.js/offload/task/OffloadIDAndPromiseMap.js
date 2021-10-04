import Logger from '~/util/logger';

const logger = new Logger('OffloadIDAndPromiseMap.js');

class OffloadIdAndPromiseMap {
  constructor() {
    if (!OffloadIdAndPromiseMap.instance) {
      this._data = {
        offloadId: {
          deps: [],
          promise: null,
          resolver: null
        }
      };
      OffloadIdAndPromiseMap.instance = this;
    }

    return OffloadIdAndPromiseMap.instance;
  }

  // ##################### List of Setters ###########################
  set(key, value) {
    logger.debug('Setting Value: ' + JSON.stringify(value) + ' to key: ' + key);
    this._data[key] = value;
  }

  setPromise(key, promise) {
    logger.debug(
      'Setting promise: ' + JSON.stringify(promise) + ' to key: ' + key
    );
    const funcObject = this._data[key];
    if (funcObject) {
      funcObject.promise = promise;
    } else {
      this._data[key] = {
        promise: promise,
        deps: [],
        resolver: null
      };
    }
  }

  setDependencies(key, deps) {
    logger.debug(
      'Setting Dependencies: ' + JSON.stringify(deps) + ' to key: ' + key
    );
    const funcObject = this._data[key];
    if (funcObject) {
      funcObject.deps = deps;
    } else {
      this._data[key] = {
        promise: null,
        deps: deps,
        resolver: null
      };
    }
  }

  setResolver(key, resolver) {
    logger.debug(
      'Setting Resolver: ' + JSON.stringify(resolver) + ' to key: ' + key
    );
    const value = this.get(key);
    if (value) {
      value.resolver = resolver;
    } else {
      this._data[key] = {
        promise: null,
        deps: [],
        resolver: resolver
      };
    }
  }

  // ##################### List of Getters ###########################
  get(key) {
    logger.debug('Getting data for key: ' + key);
    if (this._data.hasOwnProperty(key)) {
      return this._data[key];
    }
    return undefined;
  }

  viewData() {
    logger.debug('Getting data : ');
    logger.table(this._data);
  }

  getPromise(key) {
    logger.debug('Getting promise for key: ' + key);
    const funcObject = this._data[key];
    if (funcObject) {
      return funcObject.promise;
    }
    return undefined;
  }

  getDependencies(key) {
    logger.debug('Getting dependencies for key: ' + key);
    const funcObject = this._data[key];
    if (funcObject) {
      return funcObject.deps;
    }
    return [];
  }

  getResolver(key) {
    logger.debug('Getting Resolver for key: ' + key);
    const funcObject = this._data[key];
    if (funcObject) {
      return funcObject.resolver;
    }
    return null;
  }
}

const instance = new OffloadIdAndPromiseMap();
Object.freeze(instance);

export default instance;
