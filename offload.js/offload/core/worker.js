/* global RTCPeerConnection */

import { WorkerManager } from './worker-manager';
import { getDeviceName } from '../../util';
import Logger from '~/util/logger';

const logger = new Logger('worker.js');

export class Worker {
  constructor(workerId) {
    this.workerId_ = workerId;
    this.clientId_ = WorkerManager.getInstance().getId();
    this.deviceName_ = getDeviceName();
    this.peerConnection_ = null;
    this.dataChannel_ = null;
    this.isConnected_ = false;

    this.runningJobs_ = new Map();
    this.pendingJobs_ = new Map();

    this.gumStream_ = null;
    this.gumCapabilities_ = null;
    this.gumSettings_ = null;
    this.gumConstraints_ = null;
    this.applyConstraintsResolve_ = null;
    this.applyConstraintsReject_ = null;
    this.nativeApplyConstraints_ = null;

    this.setUpPeerConnection_();
  }

  setUpPeerConnection_() {
    this.peerConnection_ = new RTCPeerConnection();
    this.peerConnection_.onicecandidate = this.handleIceCandidateEvent_.bind(
      this
    );
    this.peerConnection_.ontrack = this.handleTrackEvent_.bind(this);

    this.dataChannel_ = this.peerConnection_.createDataChannel('offload');
    this.dataChannel_.onopen = this.handleDataChannelOpenEvent_.bind(this);
    this.dataChannel_.onclose = this.handleDataChannelCloseEvent_.bind(this);
    this.dataChannel_.onmessage = this.handleDataChannelMessageEvent_.bind(
      this
    );

    this.peerConnection_
      .createOffer()
      .then(offer => this.peerConnection_.setLocalDescription(offer))
      .then(() => {
        logger.debug('send offer');
        this.sendSignalMessage_(this.peerConnection_.localDescription);
      })
      .catch(reason => logger.error('reason: ' + reason.toString()));
  }

  handleIceCandidateEvent_(event) {
    if (event.candidate) {
      logger.debug('send candidate');
      this.sendSignalMessage_({
        type: 'candidate',
        candidate: event.candidate
      });
    }
  }

  handleTrackEvent_(event) {
    logger.debug('got track');
    const stream = event.streams[0];
    this.gumStream_ = stream;

    // export peerConnection to client
    stream.peerConnection = this.peerConnection_;

    // Replace stop() to the offload stub
    stream.getTracks().forEach(track => {
      const stopFunc = track.stop;
      track.stop = () => {
        this.sendPeerMessage_({
          type: 'stopTrack',
          feature: 'CAMERA',
          trackId: track.id
        });
        stopFunc.call(track);
      };
    });

    let job = null;
    if (stream.getVideoTracks().length > 0) {
      // TODO: Currently, only 1 video track is required by demo scenario.
      // Need to support audio and other tracks.
      this.updateMediaStreamTrack_(stream.getVideoTracks()[0]);
      job = this.getRunningJob_('CAMERA');
    } else if (stream.getAudioTracks().length > 0) {
      job = this.getRunningJob_('MIC');
    }

    if (job) {
      logger.timeEnd('job complete');
      job.successCallback(stream);
    }
  }

  updateMediaStreamTrack_(track) {
    track.getCapabilities = () => this.gumCapabilities_;
    track.getSettings = () => this.gumSettings_;
    track.getConstraints = () => this.gumConstraints_;
    this.nativeApplyConstraints_ = track.applyConstraints.bind(track);
    track.applyConstraints = constraints => {
      if (this.hasAutoZoomConstraints_(constraints)) {
        const newConstraints = this.extractAutoZoomConstraints_(constraints);
        this.nativeApplyConstraints_(newConstraints);
      }

      return new Promise((resolve, reject) => {
        this.applyConstraintsResolve_ = resolve;
        this.applyConstraintsReject_ = reject;
        this.sendPeerMessage_({
          type: 'applyConstraints',
          feature: 'CAMERA',
          constraints: constraints
        });
      });
    };
  }

  handleDataChannelOpenEvent_() {
    logger.debug('data channel opened');
    this.isConnected_ = true;
    this.pendingJobs_.forEach(job => this.startJob(job));
    this.pendingJobs_.clear();
  }

  handleDataChannelCloseEvent_() {
    logger.debug('data channel closed');
    this.isConnected_ = false;
    this.dataChannel_ = null;
    this.closePeerConnection_();
  }

  closePeerConnection_() {
    if (this.peerConnection_) {
      this.peerConnection_.onicecandidate = null;
      this.peerConnection_.ontrack = null;
      this.peerConnection_.close();
      this.peerConnection_ = null;
    }
  }

  handleDataChannelMessageEvent_(event) {
    const message = JSON.parse(event.data);
    const job = this.getRunningJob_(message.feature);
    if (job === null) {
      logger.error('no job related to feature', message.feature);
      return;
    }
    switch (message.type) {
      case 'data':
        if (message.feature === 'CAMERA') {
          this.gumCapabilities_ = message.data.capabilities;
          this.gumSettings_ = message.data.settings;
          this.gumConstraints_ = message.data.constraints;
        } else {
          // logger.timeEnd('job complete');
          job.successCallback(message.data);
        }
        break;
      case 'error':
        job.errorCallback(
          new DOMException(message.error.message, message.error.name)
        );
        this.stopJob(message.feature);
        break;
      case 'applyConstraints':
        if (message.result === 'success') {
          this.gumSettings_ = message.data.settings;
          this.gumConstraints_ = message.data.constraints;
          this.applyConstraintsResolve_();
        } else {
          this.applyConstraintsReject_(
            new DOMException(message.error.message, message.error.name)
          );
        }
        break;
      default:
        logger.debug('unsupported message type', message.type);
    }
  }

  getRunningJob_(feature) {
    if (!this.runningJobs_.has(feature)) {
      return null;
    }
    return this.runningJobs_.get(feature);
  }

  sendSignalMessage_(message) {
    const workerManager = WorkerManager.getInstance();
    if (workerManager) {
      workerManager.sendMessage(this.workerId_, message);
    }
  }

  sendPeerMessage_(message) {
    if (this.isConnected_) {
      this.dataChannel_.send(JSON.stringify(message));
    } else {
      logger.error('data channel is not connected');
    }
  }

  startJob(job) {
    logger.debug(`startJob id:${this.workerId_} feature:${job.feature}`);
    if (this.isConnected_) {
      this.sendPeerMessage_({
        type: 'start',
        feature: job.feature,
        arguments: job.arguments,
        resolver: job.resolver,
        clientId: this.clientId_,
        deviceName: this.deviceName_
      });
      this.runningJobs_.set(job.feature, job);

      if (job.resolver && typeof job.resolver === 'function') {
        job.resolver();
      }
    } else {
      this.pendingJobs_.set(job.feature, job);
    }
  }

  stopJob(feature) {
    logger.debug('stopJob', this.workerId_, feature);
    if (this.runningJobs_.has(feature)) {
      this.sendPeerMessage_({
        type: 'stop',
        feature: feature
      });
      this.runningJobs_.delete(feature);
    } else if (this.pendingJobs_.has(feature)) {
      this.pendingJobs_.delete(feature);
    }
  }

  handleMessage(message) {
    if (message.type === 'offer') {
      this.handleOffer_(message);
    } else if (message.type === 'answer') {
      this.handleAnswer_(message);
    } else if (message.type === 'candidate') {
      this.handleCandidate_(message.candidate);
    }
  }

  handleOffer_(offer) {
    logger.debug('got offer');
    this.peerConnection_
      .setRemoteDescription(offer)
      .then(() => this.peerConnection_.createAnswer())
      .then(answer => this.peerConnection_.setLocalDescription(answer))
      .then(() => {
        logger.debug('send answer');
        this.sendSignalMessage_(this.peerConnection_.localDescription);
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

  hasAutoZoomConstraints_(constraints) {
    for (const constraint in constraints) {
      if (constraint.startsWith('tizenAiAutoZoom')) {
        return true;
      }
    }
    return false;
  }

  extractAutoZoomConstraints_(constraints) {
    const newConstraints = {};
    for (const constraint in constraints) {
      if (constraint.startsWith('tizenAiAutoZoom')) {
        newConstraints[constraint] = constraints[constraint];
      }
    }
    return newConstraints;
  }
}
