import { Worker } from './worker';
import Logger from '~/util/logger';

const logger = new Logger('gyro.js');

export class GyroWorker extends Worker {
  constructor() {
    super();

    this.gyroSensor = null;
  }

  checkCapability() {
    if (typeof tizen !== 'undefined' && tizen.sensorservice) {
      this.features.add('GYRO');
    }

    return this.features;
  }

  handleMessage(message) {
    if (message.arguments[0] === 'start') {
      this.startGyro();
    } else if (
      message.arguments[0] === 'getGyroscopeRotationVectorSensorData'
    ) {
      this.getGyroscopeRotationVectorSensorData();
    } else {
      this.stopGyro();
    }
  }

  startGyro() {
    const self = this;

    function successCallback(result) {
      self.dataChannel.send(
        JSON.stringify({
          type: 'data',
          feature: 'GYRO',
          data: result
        })
      );
    }

    function errorCallback(error) {
      logger.error('error: ' + JSON.stringify(error));
      self.dataChannel.send(
        JSON.stringify({
          type: 'error',
          feature: 'GYRO',
          error: {
            message: error.message,
            name: error.name
          }
        })
      );
    }

    try {
      this.gyroSensor = tizen.sensorservice.getDefaultSensor(
        'GYROSCOPE_ROTATION_VECTOR'
      );
      this.gyroSensor.start(successCallback, errorCallback);
    } catch (error) {
      errorCallback(error);
    }
  }

  getGyroscopeRotationVectorSensorData() {
    const self = this;

    function successCallback(result) {
      self.dataChannel.send(
        JSON.stringify({
          type: 'data',
          feature: 'GYRO',
          data: result
        })
      );
    }

    function errorCallback(error) {
      logger.error('error: ' + JSON.stringify(error));
      self.dataChannel.send(
        JSON.stringify({
          type: 'error',
          feature: 'GYRO',
          error: {
            message: error.message,
            name: error.name
          }
        })
      );
    }

    try {
      this.gyroSensor.getGyroscopeRotationVectorSensorData(
        successCallback,
        errorCallback
      );
    } catch (error) {
      errorCallback(error);
    }
  }

  stopGyro() {
    if (this.gyroSensor) {
      this.gyroSensor.stop();
    }
    this.gyroSensor = null;
  }
}
