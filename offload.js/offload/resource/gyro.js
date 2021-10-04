import { Resource } from '../core';
import Logger from '~/util/logger';

const logger = new Logger('gyro.js');

export class GYROResource extends Resource {
  constructor() {
    super();

    this.checkCapability();
  }

  getGyroscopeRotationVectorSensorData(...params) {
    const args = [].slice.call(params);
    this.startJobWithExistingWorker({
      feature: 'GYRO',
      arguments: ['getGyroscopeRotationVectorSensorData'],
      successCallback: args[0],
      errorCallback: args[1]
    });
  }

  gyroStart(...params) {
    const args = [].slice.call(params);
    this.startJob({
      feature: 'GYRO',
      arguments: ['start'],
      successCallback: args[0],
      errorCallback: args[1]
    });
  }

  gyroStop() {
    this.stopJob('GYRO');
  }

  checkCapability() {
    const self = this;
    logger.debug(`checkCapability`);
    if (typeof tizen === 'undefined') {
      logger.debug(`tizen is undefined`);
      window.tizen = {};
    }

    if (typeof tizen.sensorservice === 'undefined') {
      logger.debug(`tizen.sensorservice is undefined`);
      tizen.sensorservice = {};
      tizen.sensorservice.getDefaultSensor = function (...params) {
        const sensor = {};
        sensor.sensorType = params[0];

        sensor.start = self.gyroStart.bind(self);
        sensor.getGyroscopeRotationVectorSensorData = self.getGyroscopeRotationVectorSensorData.bind(
          self
        );
        sensor.stop = self.gyroStop.bind(self);
        return sensor;
      };
    }
  }
}
