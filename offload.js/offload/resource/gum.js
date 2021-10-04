import { Resource, WorkerManager } from '../core';
import Logger from '~/util/logger';

const logger = new Logger('gum.js');

const WORKER_INFOS_INTERVAL = 500;
const WORKER_INFOS_TIMEOUT = 3000;

export class GUMResource extends Resource {
  constructor() {
    super();

    this._hasLocalVideoInput = false;
    this._hasLocalAudioInput = false;
    this._nativeEnumerateDevices = null;
    this._nativeGetUserMedia = null;
    this._nativeGetUserMediaDeprecated = null;
    this._checkElapsedTime = 0;

    try {
      this.installOffloadGUM();
    } catch (e) {
      logger.error(e);
    }
  }

  checkIfLocalUserMediaExists() {
    return new Promise((resolve, reject) => {
      this._nativeEnumerateDevices()
        .then(devices => {
          devices.forEach(device => {
            switch (device.kind) {
              case 'videoinput':
                this._hasLocalVideoInput = true;
                break;
              case 'audioinput':
                this._hasLocalAudioInput = true;
                break;
              case 'audiooutput':
                break;
              default:
                logger.error('Unknown device kind.');
            }
          });
          resolve();
        })
        .catch(err => {
          reject(err);
        });
    });
  }

  checkConstraintsType(constraints) {
    if (constraints.video !== undefined && constraints.video !== false) {
      return 'CAMERA';
    }
    if (constraints.audio !== undefined && constraints.audio !== false) {
      return 'MIC';
    }

    return 'NONE';
  }

  checkLocalGumIsCallable(constraints) {
    return new Promise((resolve, reject) => {
      try {
        if (constraints.video !== undefined && constraints.video !== false) {
          if (this._hasLocalVideoInput === false) {
            resolve(false);
          }
        }
        if (constraints.audio !== undefined && constraints.audio !== false) {
          if (this._hasLocalAudioInput === false) {
            resolve(false);
          }
        }

        resolve(true);
      } catch (err) {
        reject(err);
      }
    });
  }

  printErrorMessage(err) {
    logger.error(err.name + ': ' + err.message);
  }

  getUserMediaDeprecated(...params) {
    const args = [].slice.call(params);
    const constraints = args[0] || {};
    const successCallback = args[1];
    const errorCallback = args[2];
    const type = this.checkConstraintsType(constraints);
    if (type === 'NONE') {
      errorCallback(
        new TypeError(
          'The list of constraints specified is empty, or has all constraints set to false.'
        )
      );
      return;
    }

    this.checkIfLocalUserMediaExists()
      .then(() => {
        this.checkLocalGumIsCallable(constraints)
          .then(localGumIsCallable => {
            this.startJob({
              feature: type,
              arguments: [constraints],
              successCallback: successCallback,
              errorCallback: errorCallback,
              localGumIsCallable: localGumIsCallable,
              nativeGetUserMedia: this._nativeGetUserMedia
            });
          })
          .catch(err => this.printErrorMessage(err));
      })
      .catch(err => this.printErrorMessage(err));
  }

  getUserMedia(...params) {
    const args = [].slice.call(params);
    const constraints = args[0] || {};
    const type = this.checkConstraintsType(constraints);

    return new Promise((resolve, reject) => {
      if (type === 'NONE') {
        reject(
          new TypeError(
            'The list of constraints specified is empty, or has all constraints set to false.'
          )
        );
        return;
      }

      this.checkIfLocalUserMediaExists()
        .then(() => {
          this.checkLocalGumIsCallable(constraints)
            .then(localGumIsCallable => {
              this.startJob({
                feature: type,
                arguments: [constraints],
                successCallback: resolve,
                errorCallback: reject,
                localGumIsCallable: localGumIsCallable,
                nativeGetUserMedia: this._nativeGetUserMedia
              });
            })
            .catch(err => this.printErrorMessage(err));
        })
        .catch(err => this.printErrorMessage(err));
    });
  }

  checkAndAddRemoteMediaDeviceInfos(devices, resolve, reject) {
    try {
      // add remote devices to |devices| list.
      const workerManager = WorkerManager.getInstance();
      let workerInfos = null;
      if (workerManager) {
        workerInfos = workerManager.getWorkerInfos();
      }

      // We wait to receive non empty workerInfos until the default timeout.
      if (
        !workerManager ||
        !workerInfos ||
        (!workerInfos.size && this._checkElapsedTime < WORKER_INFOS_TIMEOUT)
      ) {
        setTimeout(
          this.checkAndAddRemoteMediaDeviceInfos.bind(this),
          WORKER_INFOS_INTERVAL,
          devices,
          resolve,
          reject
        );
        this._checkElapsedTime += WORKER_INFOS_INTERVAL;
        return;
      }

      workerInfos.forEach(function (value, key, map) {
        value.mediaDeviceInfos.forEach(function (mediaDeviceInfo) {
          devices.push(mediaDeviceInfo);
        });
      });
      resolve(devices);
    } catch (err) {
      reject(err);
    }
  }

  installOffloadGUM() {
    const self = this;

    logger.debug(
      'getUserMedia for video is not usable on this device. Start using offload getUserMedia instead.'
    );

    // Store references for native function version of getUserMedia.
    this._nativeGetUserMediaDeprecated = navigator.getUserMedia.bind(navigator);
    this._nativeGetUserMedia = navigator.mediaDevices.getUserMedia.bind(
      navigator.mediaDevices
    );

    // Install offloaded version of getUserMedia.
    navigator.getUserMedia = this.getUserMediaDeprecated.bind(this);
    navigator.mediaDevices.getUserMedia = this.getUserMedia.bind(this);

    this._nativeEnumerateDevices = navigator.mediaDevices.enumerateDevices.bind(
      navigator.mediaDevices
    );
    navigator.mediaDevices.enumerateDevices = function () {
      return new Promise((resolve, reject) => {
        self
          ._nativeEnumerateDevices()
          .then(function (devices) {
            self.checkAndAddRemoteMediaDeviceInfos.call(
              self,
              devices,
              resolve,
              reject
            );
          })
          .catch(function (err) {
            reject(err);
          });
      });
    };
  }

  startJob(job) {
    const constraints = job.arguments[0];
    if (constraints.hasOwnProperty('video') && constraints.video.deviceId) {
      if (typeof constraints.video.deviceId === 'object') {
        if (constraints.video.deviceId.hasOwnProperty('exact')) {
          deviceId = constraints.video.deviceId.exact;
        }
      }

      const workers = WorkerManager.getInstance().getWorkerInfos();
      for (const workerId of workers.keys()) {
        if (typeof workers.get(workerId).mediaDeviceInfos !== 'object') {
          continue;
        }
        for (const mediaDeviceInfo of workers.get(workerId).mediaDeviceInfos) {
          if (mediaDeviceInfo && mediaDeviceInfo.deviceId === deviceId) {
            super.startJobImpl(workerId, job);
            return;
          }
        }
      }
    }

    super.startJob(job);
  }
}
