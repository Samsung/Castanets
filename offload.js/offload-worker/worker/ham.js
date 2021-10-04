import { isDebugMode } from '~/util';
import { Worker } from './worker';
import Logger from '~/util/logger';

const logger = new Logger('ham.js');

export class HAMWorker extends Worker {
  constructor() {
    super();

    this.listenerId = 0;
    this.intervalId = 0;
  }

  checkCapability() {
    if (typeof tizen !== 'undefined' && tizen.humanactivitymonitor) {
      this.features.add('HRM');
      this.features.add('PEDOMETER');
      this.features.add('GESTURE');
    }

    return this.features;
  }

  handleMessage(message) {
    switch (message.feature) {
      case 'PEDOMETER':
      case 'HRM':
        if (message.type === 'start') {
          this.startHam(message.feature);
        } else {
          this.stopHam(message.feature);
        }
        break;
      case 'GESTURE':
        if (message.type === 'start') {
          this.startGestureRecognition();
        } else {
          this.stopGestureRecognition();
        }
        break;
      default:
        logger.debug('unsupported feature', message.feature);
    }
  }

  startHam(feature) {
    logger.debug('start HAM');
    const hamFeature = feature;
    const hamData = {};
    const self = this;

    if (isDebugMode()) {
      this.intervalId = setInterval(() => {
        if (hamFeature === 'HRM') {
          hamData.heartRate = Math.floor(Math.random() * 150 + 50);
          hamData.rRInterval = 0;
        } else if (hamFeature === 'PEDOMETER') {
          hamData.stepStatus = 'WALKING';
          hamData.cumulativeTotalStepCount = Math.floor(
            Math.random() * 150 + 50
          );
        }

        this.dataChannel.send(
          JSON.stringify({
            type: 'data',
            feature: hamFeature,
            data: hamData
          })
        );
      }, 1000);
      return;
    }

    function onSuccess() {
      tizen.humanactivitymonitor.start(hamFeature, hamInfo => {
        if (hamFeature === 'HRM') {
          hamData.heartRate = hamInfo.heartRate;
          hamData.rRInterval = hamInfo.rRInterval;
        } else if (hamFeature === 'PEDOMETER') {
          hamData.stepStatus = hamInfo.stepStatus;
          hamData.speed = hamInfo.speed;
          hamData.walkingFrequency = hamInfo.walkingFrequency;
          hamData.cumulativeDistance = hamInfo.cumulativeDistance;
          hamData.cumulativeCalorie = hamInfo.cumulativeCalorie;
          hamData.cumulativeTotalStepCount = hamInfo.cumulativeTotalStepCount;
          hamData.cumulativeWalkStepCount = hamInfo.cumulativeWalkStepCount;
          hamData.cumulativeRunStepCount = hamInfo.cumulativeRunStepCount;
          hamData.stepCountDifferences = hamInfo.stepCountDifferences.slice();
        }

        self.dataChannel.send(
          JSON.stringify({
            type: 'data',
            feature: hamFeature,
            data: hamData
          })
        );
      });
    }

    tizen.ppm.requestPermission(
      'http://tizen.org/privilege/healthinfo',
      onSuccess,
      error => logger.error('error: ' + JSON.stringify(error))
    );
  }

  stopHam(feature) {
    tizen.humanactivitymonitor.stop(feature);
  }

  startGestureRecognition() {
    if (this.listenerId > 0) {
      return;
    }

    if (isDebugMode()) {
      this.intervalId = setInterval(() => {
        this.dataChannel.send(
          JSON.stringify({
            type: 'data',
            feature: 'GESTURE',
            data: {
              type: 'GESTURE_WRIST_UP',
              event: 'GESTURE_EVENT_DETECTED',
              timestamp: new Date().getTime()
            }
          })
        );
      }, 3000);
      return;
    }

    try {
      this.listenerId = tizen.humanactivitymonitor.addGestureRecognitionListener(
        'GESTURE_WRIST_UP',
        data => {
          logger.debug(
            'Received ' +
              data.event +
              ' event on ' +
              new Date(data.timestamp * 1000) +
              ' for ' +
              data.type +
              ' type'
          );
          this.dataChannel.send(
            JSON.stringify({
              type: 'data',
              feature: 'GESTURE',
              data: {
                type: data.type,
                event: data.event,
                timestamp: data.timestamp
              }
            })
          );
        },
        error => logger.error('error: ' + JSON.stringify(error)),
        true
      );
      logger.debug('Listener with id ' + this.listenerId + ' has been added');
    } catch (error) {
      logger.error('error: ' + JSON.stringify(error));
    }
  }

  stopGestureRecognition() {
    if (this.listenerId > 0) {
      tizen.humanactivitymonitor.removeGestureRecognitionListener(
        this.listenerId
      );
    }
  }
}
