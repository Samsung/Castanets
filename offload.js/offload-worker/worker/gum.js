import { isDebugMode } from '~/util';
import { Worker } from './worker';
import Logger from '~/util/logger';

import * as blazeface from '@tensorflow-models/blazeface';
import * as tf from '@tensorflow/tfjs-core';
// import * as tfjsWebgl from '@tensorflow/tfjs-backend-webgl';
import * as tfjsWasm from '@tensorflow/tfjs-backend-wasm';
import sunglassSource from '../images/sunglass.png';
import santaSource from '../images/santa_new.png';
import sooSource from '../images/soo.png';
import chelseaSource from '../images/chelsea-hat.png';
import mancitySource from '../images/mancity.png';
import realmadridSource from '../images/realmadrid.png';

import CodecHelper from '~/util/codec-helper';

const logger = new Logger('gum.js');

export class GUMWorker extends Worker {
  constructor() {
    super();

    this.mediaStream = null;
    this.sunglass = null;
    this.enableEffect = false;
    this.reqId = null;
  }

  onConnected(url) {
    logger.debug('onConnected() ' + url);
    try {
      tfjsWasm.setWasmPaths(url + '/');
      tf.setBackend('wasm');
      tf.ready().then(() => {
        blazeface.load().then(model => {
          this.model = model;
        });
      });
    } catch (e) {
      logger.error('setbackend(wasm) failed. ' + e);
    }
  }

  checkCapability() {
    logger.debug('checkCapability');

    if (typeof android !== 'undefined') {
      // On android webview, H264 is missed at the beginning of loading.
      // It causes the leak of H264 from the sdp of the offer.
      // Calling RTCRtpSender.getCapabilities here can be a solution to include
      // H264 in the sdp. But, it seems to be a bit of trick.
      alert(RTCRtpSender.getCapabilities('video'));
    }
    return new Promise((resolve, reject) => {
      navigator.mediaDevices
        .enumerateDevices()
        .then(devices => {
          devices.forEach(device => {
            if (device.kind === 'videoinput') {
              this.features.add('CAMERA');
            }
            if (device.kind === 'audioinput') {
              this.features.add('MIC');
            }
          });

          resolve(this.features);
        })
        .catch(err => {
          logger.error('err.code: ' + err.code);
          logger.error(err.name + ': ' + err.message);
          reject(this.features);
        });
    }).catch(err => {
      // Return the empty features if getting reject promise
      return this.features;
    });
  }

  handleMessage(message) {
    if (message.type === 'start') {
      this.requestConfirmation(
        message.feature,
        message.clientId,
        message.deviceName
      ).then(allowed => {
        if (allowed) {
          this.startGum(message.arguments[0], message.feature);
        } else {
          this.dataChannel.send(
            JSON.stringify({
              type: 'error',
              feature: message.feature,
              error: {
                message: 'Permission denied',
                name: 'NotAllowedError'
              }
            })
          );
        }
      });
    } else if (message.type === 'applyConstraints') {
      this.applyConstraints(message.constraints, message.feature);
    } else if (message.type === 'stopTrack') {
      const transceiver = this.peerConnection.getTransceivers().find(t => {
        return t.sender && t.sender.track.id === message.trackId;
      });
      if (transceiver && transceiver.sender) {
        transceiver.sender.track.stop();
        transceiver.stop();
        this.peerConnection.removeTrack(transceiver.sender);
      } else {
        logger.log('Failed to stopTrack() ' + message.trackId);
      }
    }
  }

  runPrediction() {
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        if (predictions.length > 0) {
          const prediction = predictions[0];
          if (prediction.probability < 0.8) {
            return;
          }

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          const startX = Math.min(
            Math.max(start[0] - 100, 0),
            this.video.videoWidth - size[0] - 200
          );
          const startY = Math.min(
            Math.max(start[1] - 100, 0),
            this.video.videoHeight - size[1] - 200
          );

          if (!this.enableEffect) {
            this.context.drawImage(this.video, 0, 0);
          } else {
            this.context.drawImage(
              this.video,
              startX,
              startY,
              size[0] + 200,
              size[1] + 200,
              0,
              0,
              this.video.videoWidth,
              this.video.videoHeight
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runPrediction.bind(this));
  }

  runDrawGlasses() {
    this.context.drawImage(this.video, 0, 0);
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        if (predictions.length > 0) {
          const prediction = predictions[0];
          if (prediction.probability < 0.8) {
            return;
          }

          const landmarks = prediction.landmarks;
          const rightEyeX = parseInt(landmarks[0][0]);
          const rightEyeY = parseInt(landmarks[0][1]);

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          if (this.enableEffect) {
            this.context.drawImage(
              this.sunglass,
              rightEyeX - size[0] / 4,
              rightEyeY - size[1] / 2,
              size[0],
              size[1]
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runDrawGlasses.bind(this));
  }

  runDrawSanta() {
    this.context.drawImage(this.video, 0, 0);
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        if (predictions.length > 0) {
          const prediction = predictions[0];
          if (prediction.probability < 0.8) {
            return;
          }

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          if (this.enableEffect) {
            this.context.drawImage(
              this.santa,
              start[0],
              start[1],
              size[0],
              size[1]
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runDrawSanta.bind(this));
  }

  runDrawSoo() {
    this.context.drawImage(this.video, 0, 0);
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        if (predictions.length > 0) {
          const prediction = predictions[0];
          if (prediction.probability < 0.8) {
            return;
          }

          const landmarks = prediction.landmarks;
          const rightEyeX = parseInt(landmarks[0][0]);
          const rightEyeY = parseInt(landmarks[0][1]);

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          if (this.enableEffect) {
            this.context.drawImage(
              this.soo,
              rightEyeX - size[0] / 2,
              rightEyeY - size[1] / 2,
              size[0] * 1.5,
              size[1] * 1.5
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runDrawSoo.bind(this));
  }

  runDrawMancity() {
    this.context.drawImage(this.video, 0, 0);
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        for (const prediction of predictions) {
          if (prediction.probability[0] < 0.8) {
            return;
          }

          const landmarks = prediction.landmarks;
          const rightEyeX = parseInt(landmarks[0][0]);
          const rightEyeY = parseInt(landmarks[0][1]);

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          if (this.enableEffect) {
            this.context.drawImage(
              this.mancity,
              rightEyeX - size[0] / 1.3,
              rightEyeY - size[1] * 1.5,
              size[0] * 2,
              size[1] * 2
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runDrawMancity.bind(this));
  }

  runDrawMadrid() {
    this.context.drawImage(this.video, 0, 0);
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        for (const prediction of predictions) {
          if (prediction.probability[0] < 0.8) {
            return;
          }

          const landmarks = prediction.landmarks;
          const rightEyeX = parseInt(landmarks[0][0]);
          const rightEyeY = parseInt(landmarks[0][1]);

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          if (this.enableEffect) {
            this.context.drawImage(
              this.madrid,
              rightEyeX - size[0] / 1.3,
              rightEyeY - size[1] * 1.7,
              size[0] * 2,
              size[1] * 2
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runDrawMadrid.bind(this));
  }

  runDrawChelsea() {
    this.context.drawImage(this.video, 0, 0);
    this.model
      .estimateFaces(this.video, false, false, true)
      .then(predictions => {
        for (const prediction of predictions) {
          if (prediction.probability[0] < 0.8) {
            return;
          }

          const landmarks = prediction.landmarks;
          const rightEyeX = parseInt(landmarks[0][0]);
          const rightEyeY = parseInt(landmarks[0][1]);

          const start = prediction.topLeft;
          const end = prediction.bottomRight;
          const size = [end[0] - start[0], end[1] - start[1]];

          if (this.enableEffect) {
            this.context.drawImage(
              this.chelsea,
              rightEyeX - size[0] / 2,
              rightEyeY - size[1] * 1.5,
              size[0] * 1.5,
              size[1] * 1.5
            );
          }
        }
      });

    this.reqId = requestAnimationFrame(this.runDrawChelsea.bind(this));
  }

  applyAIZoom() {
    this.video = document.createElement('video');
    this.video.setAttribute('autoplay', '');
    this.video.style.display = 'none';

    this.canvas = document.createElement('canvas');

    this.video.addEventListener('loadedmetadata', () => {
      this.canvas.setAttribute('width', this.video.videoWidth);
      this.canvas.setAttribute('height', this.video.videoHeight);

      this.originStream = this.mediaStream;
      const videoTrack = this.mediaStream.getVideoTracks()[0];
      const sender = this.peerConnection.getSenders().find(s => {
        return s.track.kind === videoTrack.kind;
      });

      this.canvasStream = this.canvas.captureStream();
      this.canvasTrack = this.canvasStream.getVideoTracks()[0];
      sender.replaceTrack(this.canvasTrack);
    });

    this.context = this.canvas.getContext('2d');

    this.video.addEventListener('play', () => {
      this.runPrediction();
    });

    this.video.srcObject = this.mediaStream;
  }

  applyAIEmoji() {
    this.video = document.createElement('video');
    this.video.setAttribute('autoplay', '');
    this.video.style.display = 'none';

    this.canvas = document.createElement('canvas');

    this.video.addEventListener('loadedmetadata', () => {
      this.canvas.setAttribute('width', this.video.videoWidth);
      this.canvas.setAttribute('height', this.video.videoHeight);

      this.originStream = this.mediaStream;
      const videoTrack = this.mediaStream.getVideoTracks()[0];
      const sender = this.peerConnection.getSenders().find(s => {
        return s.track.kind === videoTrack.kind;
      });

      this.canvasStream = this.canvas.captureStream();
      this.canvasTrack = this.canvasStream.getVideoTracks()[0];
      sender.replaceTrack(this.canvasTrack);
    });

    this.context = this.canvas.getContext('2d');

    this.video.addEventListener('play', () => {
      this.runDrawGlasses();
    });

    this.video.srcObject = this.mediaStream;
  }

  applyAISanta() {
    this.video = document.createElement('video');
    this.video.setAttribute('autoplay', '');
    this.video.style.display = 'none';

    this.canvas = document.createElement('canvas');

    this.video.addEventListener('loadedmetadata', () => {
      this.canvas.setAttribute('width', this.video.videoWidth);
      this.canvas.setAttribute('height', this.video.videoHeight);

      this.originStream = this.mediaStream;
      const videoTrack = this.mediaStream.getVideoTracks()[0];
      const sender = this.peerConnection.getSenders().find(s => {
        return s.track.kind === videoTrack.kind;
      });

      this.canvasStream = this.canvas.captureStream();
      this.canvasTrack = this.canvasStream.getVideoTracks()[0];
      sender.replaceTrack(this.canvasTrack);
    });

    this.context = this.canvas.getContext('2d');

    this.video.addEventListener('play', () => {
      this.runDrawSanta();
    });

    this.video.srcObject = this.mediaStream;
  }

  applyAISoo() {
    this.video = document.createElement('video');
    this.video.setAttribute('autoplay', '');
    this.video.style.display = 'none';

    this.canvas = document.createElement('canvas');

    this.video.addEventListener('loadedmetadata', () => {
      this.canvas.setAttribute('width', this.video.videoWidth);
      this.canvas.setAttribute('height', this.video.videoHeight);

      this.originStream = this.mediaStream;
      const videoTrack = this.mediaStream.getVideoTracks()[0];
      const sender = this.peerConnection.getSenders().find(s => {
        return s.track.kind === videoTrack.kind;
      });

      this.canvasStream = this.canvas.captureStream();
      this.canvasTrack = this.canvasStream.getVideoTracks()[0];
      sender.replaceTrack(this.canvasTrack);
    });

    this.context = this.canvas.getContext('2d');

    this.video.addEventListener('play', () => {
      this.runDrawSoo();
    });

    this.video.srcObject = this.mediaStream;
  }

  applyAIEffect(name) {
    logger.debug('applyAIEffect:' + name);
    this.video = document.createElement('video');
    this.video.setAttribute('autoplay', '');
    this.video.style.display = 'none';

    this.canvas = document.createElement('canvas');

    this.video.addEventListener('loadedmetadata', () => {
      this.canvas.setAttribute('width', this.video.videoWidth);
      this.canvas.setAttribute('height', this.video.videoHeight);

      this.originStream = this.mediaStream;
      const videoTrack = this.mediaStream.getVideoTracks()[0];
      const sender = this.peerConnection.getSenders().find(s => {
        return s.track.kind === videoTrack.kind;
      });

      this.canvasStream = this.canvas.captureStream();
      this.canvasTrack = this.canvasStream.getVideoTracks()[0];
      sender.replaceTrack(this.canvasTrack);
    });

    this.context = this.canvas.getContext('2d');

    this.video.addEventListener('play', () => {
      if (name === 'mancity') {
        this.runDrawMancity();
      } else if (name === 'chelsea') {
        this.runDrawChelsea();
      } else if (name === 'madrid') {
        this.runDrawMadrid();
      }
    });

    this.video.srcObject = this.mediaStream;
  }

  setPreferredCodecs(track, codecs) {
    const transceiver = this.peerConnection.getTransceivers().find(t => {
      return t.sender && t.sender.track === track;
    });
    if (transceiver) {
      codecs = CodecHelper.getPreferredCodecs(codecs);
      transceiver.direction = 'sendonly';
      transceiver.setCodecPreferences(codecs);
    } else {
      logger.warn("Can't find transceiver for " + track.label);
      logger.debug(JSON.stringify(track));
    }
  }

  getStream(stream, reqConstraints, feature) {
    logger.debug(`getStream feature:${feature} ${stream.id}`);
    for (const track of stream.getTracks()) {
      if (track.kind === 'video') {
        logger.info(
          `[Track:video]` +
            ` ${track.label}` +
            `, size:${track.getSettings().width}x${
              track.getSettings().height
            }` +
            `, frameRate:${track.getSettings().frameRate}`
        );
      } else {
        console.info(`[Track:${track.kind}] id:${track.id}`);
      }
      logger.debug(`track:${JSON.stringify(track)}`);
      logger.debug(`settings:${JSON.stringify(track.getSettings())}`);
      logger.debug(`constraints:${JSON.stringify(track.getConstraints())}`);
      logger.debug(`capabilities:${JSON.stringify(track.getCapabilities())}`);
    }
    this.mediaStream = stream;

    stream.peerConnection = this.peerConnection;
    stream.onaddtrack = event =>
      logger.debug('onaddtrack' + JSON.stringify(event));
    stream.onremovetrack = event =>
      logger.debug('onremovetrack' + JSON.stringify(event));
    stream.onactive = event => logger.debug('onactive' + JSON.stringify(event));
    stream.oninactive = event =>
      logger.debug('oninactive' + JSON.stringify(event));

    stream.getTracks().forEach(track => {
      this.peerConnection.addTrack(track, stream);
    });

    // Apply offload.videoCodecs constraint
    if (reqConstraints.offload && reqConstraints.offload.videoCodecs) {
      this.setPreferredCodecs(
        stream.getVideoTracks()[0],
        reqConstraints.offload.videoCodecs
      );
    }

    // TODO: Currently, only 1 video track is required by demo scenario.
    // Need to support audio and other tracks.
    if (feature === 'CAMERA') {
      const mediaTrack = stream.getVideoTracks()[0];

      if (typeof mediaTrack.getCapabilities === 'undefined') {
        if (typeof mediaTrack.getConstraints !== 'undefined') {
          mediaTrack.getCapabilities = mediaTrack.getConstraints;
        } else {
          throw new Error('getCapabilities is not supported');
        }
      }

      this.dataChannel.send(
        JSON.stringify({
          type: 'data',
          feature: feature,
          data: {
            capabilities: mediaTrack.getCapabilities(),
            settings: mediaTrack.getSettings(),
            constraints: mediaTrack.getConstraints()
          }
        })
      );
    }

    offloadWorker.impl.emit('stream', stream);
  }

  startGum(constraints, feature) {
    logger.debug(
      `startGum constraints:${JSON.stringify(constraints)} feature:${feature}`
    );
    const self = this;

    if (isDebugMode()) {
      navigator.mediaDevices
        .getDisplayMedia({ video: true })
        .then(stream => this.getStream(stream, constraints, feature))
        .catch(error => {
          logger.error('error: ' + error);
        });
      return;
    }

    navigator.mediaDevices
      .getUserMedia(constraints)
      .then(stream => this.getStream(stream, constraints, feature))
      .catch(error => {
        logger.error('error: ' + error);
        self.dataChannel.send(
          JSON.stringify({
            type: 'error',
            feature: feature,
            error: {
              message: error.message,
              name: error.name
            }
          })
        );
      });
  }

  applyConstraints(constraints, feature) {
    logger.debug(
      `applyConstraints ${JSON.stringify(constraints)} feature:${feature}`
    );

    if (typeof constraints.aiMode !== 'undefined') {
      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAIZoom();
    }

    if (typeof constraints.santa !== 'undefined') {
      const image = new Image();
      image.src = santaSource;
      this.santa = image;

      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAISanta();
    }

    if (typeof constraints.emoji !== 'undefined') {
      const image = new Image();
      image.src = sunglassSource;
      this.sunglass = image;

      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAIEmoji();
    }

    if (typeof constraints.soo !== 'undefined') {
      const image = new Image();
      image.src = sooSource;
      this.soo = image;

      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAISoo();
    }

    if (typeof constraints.mancity !== 'undefined') {
      const image = new Image();
      image.src = mancitySource;
      this.mancity = image;

      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAIEffect('mancity');
    }

    if (typeof constraints.madrid !== 'undefined') {
      const image = new Image();
      image.src = realmadridSource;
      this.madrid = image;

      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAIEffect('madrid');
    }

    if (typeof constraints.chelsea !== 'undefined') {
      const image = new Image();
      image.src = chelseaSource;
      this.chelsea = image;

      if (this.reqId) {
        cancelAnimationFrame(this.reqId);
      }

      this.enableEffect = true;
      this.applyAIEffect('chelsea');
    }

    if (typeof constraints.off !== 'undefined') {
      this.enableEffect = false;
    }

    const mediaTrack = this.mediaStream.getVideoTracks()[0];
    mediaTrack
      .applyConstraints(constraints)
      .then(() => {
        this.dataChannel.send(
          JSON.stringify({
            type: 'applyConstraints',
            feature: feature,
            result: 'success',
            data: {
              settings: mediaTrack.getSettings(),
              constraints: mediaTrack.getConstraints()
            }
          })
        );
      })
      .catch(error => {
        this.dataChannel.send(
          JSON.stringify({
            type: 'applyConstraints',
            feature: feature,
            result: 'error',
            error: {
              message: error.message,
              name: error.name
            }
          })
        );
      });
  }
}
