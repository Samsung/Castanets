import { WorkerManager } from '../core/worker-manager';
import { devicePopup } from '../ui';
import Logger from '~/util/logger';

const logger = new Logger('resource.js');

export class Resource {
  constructor() {
    this.worker_ = null;
    this.alwaysConnect_ = null;
  }

  // interface
  checkCapability() {}

  startJob(job) {
    if (this.alwaysConnect_) {
      this.startJobImpl(this.alwaysConnect_, job);
      return;
    }
    const feature = job.feature;
    const localGumIsCallable = job.localGumIsCallable;
    this.showResourcePopup(feature, localGumIsCallable)
      .then(result => {
        this.alwaysConnect_ = result.always ? result.workerId : null;
        this.startJobImpl(result.workerId, job);
      })
      .catch(exception => {
        logger.error(exception);
        if (job.errorCallback) {
          job.errorCallback(exception);
        }
      });
  }

  startJobImpl(workerId, job) {
    if (this.alwaysConnect_ === 'localdevice' || workerId === 'localdevice') {
      job
        .nativeGetUserMedia(job.arguments[0])
        .then(job.successCallback)
        .catch(job.errorCallback);
      return;
    }

    this.requestService(workerId)
      .then(() => {
        const workerManager = WorkerManager.getInstance();
        this.worker_ = workerManager.getOrCreateWorker(workerId);
        logger.time('job complete');
        this.worker_.startJob(job);
      })
      .catch(exception => {
        logger.error(exception);
        if (job.errorCallback) {
          job.errorCallback(exception);
        }
      });
  }

  startJobWithExistingWorker(job) {
    if (this.worker_) {
      this.worker_.startJob(job);
    }
  }

  stopJob(feature) {
    if (this.worker_) {
      this.worker_.stopJob(feature);
      this.worker_ = null;
    }
  }

  showResourcePopup(feature, localGumIsCallable) {
    return new Promise((resolve, reject) => {
      devicePopup.show({
        feature: feature,
        successCallback: resolve,
        errorCallback: reject,
        localGumIsCallable: localGumIsCallable
      });
    });
  }

  requestService(workerId) {
    return new Promise((resolve, reject) => {
      const workerManager = WorkerManager.getInstance();
      workerManager.requestService(workerId, {
        successCallback: resolve,
        errorCallback: reject
      });
    });
  }
}
