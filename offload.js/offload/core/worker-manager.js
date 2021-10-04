import io from 'socket.io-client';
import { Worker } from './worker';
import {
  getClientId,
  getServerUrlFromLocation,
  getServerUrlFromLocalhost,
  isDebugMode
} from '~/util';
import Logger from '~/util/logger';

const logger = new Logger('worker-manager.js');

const debugMode = isDebugMode();
const isTizen = typeof tizen === 'object';

let workerManager;

export class WorkerManager {
  static getInstance() {
    if (!workerManager) {
      workerManager = new WorkerManager();
      // Try to connect the server by default.
      // If you turn on the debugMode using '{url}?debug=true',
      // connection is not automatically established.
      if (!debugMode) {
        workerManager.connect();
      }
    }
    return workerManager;
  }

  constructor() {
    this.workerInfos_ = new Map();
    this.activeWorkers_ = new Map();
    this.capabilities_ = new Map();
    this.socket_ = null;
    this.id_ = getClientId();
    this.qrCode_ = null;
    this.callbacks_ = { qrcode: [], capability: [] };
    this.signalingStatus = false;

    return this;
  }

  connect(address) {
    let serverUrl = address || this.getServerUrl_();

    if (serverUrl === null) {
      logger.error('No valid server URL found.');
      return null;
    }

    if (!serverUrl.endsWith('/offload-js')) {
      serverUrl += '/offload-js';
    }

    if (this.socket_ !== null) {
      this.socket_.close();
    }

    logger.time('socket connected');
    this.socket_ = io(serverUrl, { transports: ['websocket', 'polling'] });
    this.socket_.on('greeting', this.initFromGreeting_.bind(this));
    this.socket_.on('worker', this.handleWorkerEvent_.bind(this));
    this.socket_.on('capabilities', this.updateCapabilities_.bind(this));
    this.socket_.on('message', this.handleMessage_.bind(this));
    this.socket_.on('connect', this.socketConnected_.bind(this));
    this.socket_.on('disconnect', this.socketDisconnected_.bind(this));

    return serverUrl;
  }

  getServerUrl_() {
    return isTizen ? getServerUrlFromLocalhost() : getServerUrlFromLocation();
  }

  initFromGreeting_(data) {
    logger.debug('greeting: ' + JSON.stringify(data));
    this.signalingStatus = true;
    this.qrCode_ = data.qrCode;
    this.workerInfos_ = new Map(data.workers);
    while (this.callbacks_['qrcode'].length) {
      this.callbacks_['qrcode'].pop().call(this, this.qrCode_);
    }
  }

  updateCapabilities_(data) {
    this.capabilities_ = new Map(data);
    while (this.callbacks_['capability'].length) {
      this.callbacks_['capability'].pop().call(this, this.capabilities_);
    }
  }

  handleWorkerEvent_(data) {
    if (data.event === 'join') {
      logger.debug(
        `join: '${data.workerId}' - '${data.name}', '${data.features}', '${data.mediaDeviceInfos}'`
      );
      this.workerInfos_.set(data.workerId, {
        socketId: data.socketId,
        name: data.name,
        features: data.features,
        mediaDeviceInfos: data.mediaDeviceInfos,
        compute_tasks: 0
      });

      if (this.capabilities_.has(data.workerId)) {
        const cap = this.capabilities_.get(data.workerId);
        if (cap.options) {
          logger.debug(`resolve successCallback`);
          cap.options.successCallback();
          cap.options = null;
        }
      }
    } else if (data.event === 'bye') {
      logger.debug(`bye: '${data.workerId}'`);
      this.workerInfos_.delete(data.workerId);
      this.activeWorkers_.delete(data.workerId);
    }
  }

  handleMessage_(data) {
    if (this.activeWorkers_.has(data.from)) {
      this.activeWorkers_.get(data.from).handleMessage(data.message);
    }
  }

  socketConnected_() {
    logger.debug(`${this.socket_.id} connected`);
    logger.timeEnd('socket connected');
    // create the session.
    if (this.socket_) {
      this.socket_.emit('create');
    }
  }

  socketDisconnected_() {
    logger.debug(`${this.socket_.id} disconnected`);
    this.signalingStatus = false;
  }

  getOrCreateWorker(workerId) {
    if (this.activeWorkers_.has(workerId)) {
      return this.activeWorkers_.get(workerId);
    }

    const newWorker = new Worker(workerId);
    this.activeWorkers_.set(workerId, newWorker);
    return newWorker;
  }

  getWorkerInfos() {
    return this.workerInfos_;
  }

  getSupportedWorkers(feature) {
    const workers = [];

    this.capabilities_.forEach((value, key, map) => {
      if (value.features.indexOf(feature) >= 0) {
        workers.push({ id: key, name: value.name });
      }
    });

    // Add manually joined workers.
    this.workerInfos_.forEach((value, key, map) => {
      if (
        !this.capabilities_.has(key) &&
        value.features.indexOf(feature) >= 0
      ) {
        workers.push({ id: key, name: value.name });
      }
    });

    return workers;
  }

  getId() {
    return this.id_;
  }

  requestService(workerId, options) {
    if (this.workerInfos_.has(workerId)) {
      logger.debug(
        `Already existed in workInfos. Resolve successCallback directly`
      );
      options.successCallback();
      return;
    }

    this.capabilities_.get(workerId).options = options;
    this.socket_.emit('requestService', workerId);
  }

  updateCapability(callback) {
    this.socket_.emit('getcapabilities');
    this.callbacks_['capability'].push(callback);
  }

  sendMessage(workerId, message) {
    if (!this.socket_) {
      logger.error(`socket is null`);
      return;
    }
    this.socket_.emit('message', {
      to: workerId,
      from: this.socket_.id,
      message: message
    });
  }

  getQrCode() {
    return new Promise(resolve => {
      if (!this.qrCode_) {
        this.callbacks_['qrcode'].push(resolve);
        return;
      }
      resolve(this.qrCode_);
    });
  }
}
