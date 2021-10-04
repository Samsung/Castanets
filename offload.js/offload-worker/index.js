/* global localStorage, RTCPeerConnection */
import io from 'socket.io-client';
import { getDeviceName, getUniqueId, isDebugMode } from '~/util';
import * as Workers from './worker';
import { version } from '../../package.json';
import Logger from '~/util/logger';
import EventEmitter from 'events';

const logger = new Logger('index.js');

// Separate the log according to the development mode or production mode
if (process.env.NODE_ENV === 'development') {
  window.localStorage.setItem('debug', 'offload:*');
} else {
  window.localStorage.setItem('debug', 'offload:INFO*, offload:ERROR*');
}

logger.info(`offloadWorker version: ${version}`);

// for debugging
const debugMode = isDebugMode();

const SUPPORTED_FEATURES = [
  'CAMERA',
  'PEDOMETER',
  'HRM',
  'MIC',
  'GESTURE',
  'GYRO',
  'COMPUTE'
];

const CONNECTION_STATUS = Object.freeze({
  IDLE: 0,
  CONNECTING: 1,
  CONNECTED: 2
});

class OffloadWorkerImpl extends EventEmitter {
  constructor() {
    super();
    this.socket_ = null;
    this.peerConnection_ = null;
    this.dataChannel_ = null;
    this.id_ = null;
    this.peerId_ = null;
    this.currentUrl_ = null;
    this.features_ = null;
    this.mediaDeviceInfos_ = null;
    this.deviceName_ = null;
    this.workers_ = [];
    this.connectionStatus_ = CONNECTION_STATUS.IDLE;

    this.initialize_();
  }

  initialize_() {
    this.id_ = localStorage.getItem('workerId');
    if (this.id_ === null) {
      this.id_ = getUniqueId();
      logger.debug(`New workerId : ${this.id_}`);
      localStorage.setItem('workerId', this.id_);
    }

    for (const Worker of Object.values(Workers)) {
      this.workers_.push(new Worker());
    }

    if (debugMode) {
      this.features_ = new Set(SUPPORTED_FEATURES);
    }
  }

  connect(url, options) {
    this.deviceName_ = (options && options.deviceName) || getDeviceName();

    if (url === null || url.includes('file://')) {
      logger.error('No valid server URL found.');
      return;
    }

    if (!url.endsWith('/offload-js')) {
      url += '/offload-js';
    }

    if (this.connectionStatus_ !== CONNECTION_STATUS.IDLE) {
      if (options && options.forceConnect) {
        this.socket_.disconnect();
        this.connectionStatus_ = CONNECTION_STATUS.IDLE;
      } else {
        logger.debug(`Already connected or connecting to ${this.currentUrl_}`);
        return;
      }
    }

    logger.debug(`Try to connect to ${url}`);
    this.createSocket_(url);
    this.connectionStatus_ = CONNECTION_STATUS.CONNECTING;
    this.currentUrl_ = url;
  }

  onConfirmationResult(id, allowed) {
    this.workers_.some(worker => {
      return worker.onConfirmationResult(id, allowed);
    });
  }

  async checkCapability() {
    logger.debug('checkCapability');
    let features = '';
    await this.getCapability_();
    if (typeof android !== 'undefined') {
      features = Array.from(this.features_).toString();
      android.emit(
        'writeCapability',
        JSON.stringify({
          id: this.id_,
          name: getDeviceName(),
          features: features
        })
      );
    }
  }

  createSocket_(url) {
    this.socket_ = io(url, {
      transports: ['websocket'],
      reconnectionAttempts: 5
    });

    this.socket_.on('connect', async () => {
      logger.debug(`${this.socket_.id} connected`);
      this.connectionStatus_ = CONNECTION_STATUS.CONNECTED;
      await this.getCapability_();
      this.join_();
      this.workers_.forEach(worker => {
        const serverUrl = new URL(url);
        worker.onConnected(serverUrl.origin);
      });
      this.emit('connect', this.currentUrl_);
    });
    this.socket_.on('reconnect_attempt', () => {
      logger.debug('reconnect attempt');
      this.socket_.io.opts.transports = ['polling', 'websocket'];
    });
    this.socket_.on('connect_error', error => {
      logger.error('connect error: %o', error);
    });
    this.socket_.on('reconnect_failed', () => {
      logger.error('reconnect failed');
      this.connectionStatus_ = CONNECTION_STATUS.IDLE;
    });
    this.socket_.on('disconnect', reason => {
      logger.debug(`disconnect ${reason}`);
      this.connectionStatus_ = CONNECTION_STATUS.IDLE;
    });

    this.socket_.on('client', this.handleClient_.bind(this));
    this.socket_.on('message', this.handleMessage_.bind(this));
  }

  async getCapability_() {
    logger.debug('getCapability_');
    if (this.features_ !== null) {
      return;
    }

    this.features_ = new Set();
    await Promise.all(
      this.workers_.map(async worker => {
        const capabilities = await worker.checkCapability();
        capabilities.forEach(capability => {
          this.features_.add(capability);
        });
      })
    );

    if (this.features_.has('CAMERA') || this.features_.has('MIC')) {
      try {
        this.mediaDeviceInfos_ = await navigator.mediaDevices.enumerateDevices();
      } catch (err) {
        logger.error(err.name + ': ' + err.message);
      }
    }
  }

  join_() {
    this.socket_.emit('join', {
      id: this.id_,
      name: this.deviceName_,
      features: Array.from(this.features_),
      mediaDeviceInfos: this.mediaDeviceInfos_
    });
  }

  handleClient_(data) {
    if (data.event === 'bye') {
      logger.debug(`client bye: '${data.socketId}'`);
      this.hangup_(data.socketId);
    } else if (data.event === 'forceQuit') {
      logger.debug('offload-worker will be closed now!');

      if (typeof tizen !== 'undefined') {
        tizen.application.getCurrentApplication().exit();
      } else if (typeof android !== 'undefined') {
        android.emit('destroyService', '');
      } else {
        window.open('', '_self').close();
      }
    }
  }

  hangup_(peerId) {
    if (peerId && peerId !== this.peerId_) {
      return;
    }
    logger.debug('hangup');

    if (this.peerConnection_ !== null) {
      if (this.dataChannel_ !== null) {
        this.dataChannel_.onopen = null;
        this.dataChannel_.onclose = null;
        this.dataChannel_.onmessage = null;
        this.dataChannel_.close();
        this.dataChannel_ = null;
        this.workers_.forEach(worker => {
          worker.setDataChannel(null);
        });
      }
      this.peerConnection_.onnegotiationneeded = null;
      this.peerConnection_.onicecandidate = null;
      this.peerConnection_.ondatachannel = null;
      this.peerConnection_.close();
      this.peerConnection_ = null;
      this.workers_.forEach(worker => {
        worker.setPeerConnection(null);
      });
    }

    this.peerId_ = null;
  }

  handleMessage_(data, callback) {
    if (data.message.type === 'offer') {
      this.setupPeerConnectionIfNeeded_(data.from);
      this.handleOffer_(data.message);
    } else if (data.message.type === 'answer') {
      this.handleAnswer_(data.message);
    } else if (data.message.type === 'candidate') {
      this.handleCandidate_(data.message.candidate);
    } else if (data.message.type === 'COMPUTE') {
      for (const worker of this.workers_) {
        if (worker.hasFeature('COMPUTE')) {
          worker.doComputingJob(data, callback);
          break;
        }
      }
    }
  }

  setupPeerConnectionIfNeeded_(peerId) {
    if (this.peerConnection_ !== null) {
      return;
    }

    logger.debug('create peer connection. ' + peerId);

    this.peerConnection_ = new RTCPeerConnection();
    this.peerConnection_.onnegotiationneeded = this.handleNegotiationNeeded_.bind(
      this
    );
    this.peerConnection_.onicecandidate = this.handleIceCandidate_.bind(this);
    this.peerConnection_.ondatachannel = this.handleDataChannel_.bind(this);

    this.workers_.forEach(worker => {
      worker.setPeerConnection(this.peerConnection_);
    });

    this.peerId_ = peerId;
  }

  handleNegotiationNeeded_() {
    this.peerConnection_
      .createOffer()
      .then(offer => {
        if (this.peerConnection_.signalingState !== 'stable') {
          return new DOMException(
            "The RTCPeerConnection's signalingState is not 'statble'",
            'InvalidStateError'
          );
        }
        return this.peerConnection_.setLocalDescription(offer);
      })
      .then(() => {
        logger.debug('send offer');
        this.sendPeerMessage_(this.peerConnection_.localDescription);
      })
      .catch(reason => logger.error(TAG + 'reason: ' + reason.message));
  }

  handleIceCandidate_(event) {
    if (event.candidate) {
      logger.debug('send candidate');
      this.sendPeerMessage_({
        type: 'candidate',
        candidate: event.candidate
      });
    }
  }

  handleDataChannel_(event) {
    this.dataChannel_ = event.channel;
    this.dataChannel_.onopen = () => {
      logger.debug('data channel opened');
      this.workers_.forEach(worker => {
        worker.setDataChannel(this.dataChannel_);
      });
    };
    this.dataChannel_.onclose = () => {
      logger.debug('data channel closed');
      this.hangup_();
    };
    this.dataChannel_.onmessage = event => {
      const message = JSON.parse(event.data);
      this.workers_.forEach(worker => {
        if (worker.hasFeature(message.feature)) {
          worker.handleMessage(message);
        }
      });
    };
  }

  handleOffer_(offer) {
    logger.debug('got offer');
    this.peerConnection_
      .setRemoteDescription(offer)
      .then(() => this.peerConnection_.createAnswer())
      .then(answer => this.peerConnection_.setLocalDescription(answer))
      .then(() => {
        logger.debug('send answer');
        this.sendPeerMessage_(this.peerConnection_.localDescription);
      })
      .catch(reason => logger.error('reason: ' + reason.toString()));
  }

  handleAnswer_(answer) {
    logger.debug('got answer');
    this.peerConnection_
      .setRemoteDescription(answer)
      .catch(reason => logger.error('reason: ' + reason.toString()));
  }

  handleCandidate_(candidate) {
    logger.debug('got candidate');
    this.peerConnection_
      .addIceCandidate(candidate)
      .catch(reason => logger.error('reason: ' + reason.toString()));
  }

  sendPeerMessage_(message) {
    if (this.socket_ === null) {
      return;
    }
    this.socket_.emit('message', {
      to: this.peerId_,
      from: this.id_,
      message: message
    });
  }
}

class OffloadWorker {
  constructor(impl) {
    this.impl_ = impl;
    this.version = version;
  }
  connect(url, options) {
    this.impl_.connect(url, options);
  }
  on(event, listener) {
    this.impl_.on(event, listener);
  }
  onConfirmationResult(id, allowed) {
    this.impl_.onConfirmationResult(id, allowed);
  }
  checkCapability() {
    this.impl_.checkCapability();
  }
  get impl() {
    return this.impl_;
  }
}

const offloadWorker = new OffloadWorker(new OffloadWorkerImpl());
window.offloadWorker = offloadWorker;
