import Logger from '~/util/logger';

const logger = new Logger('worker.js');

let confirmationId = 1000;

export class Worker {
  constructor() {
    this.dataChannel = null;
    this.peerConnection = null;
    this.features = new Set();
    this.confirmations = new Map();
  }

  setPeerConnection(connection) {
    this.peerConnection = connection;
  }

  setDataChannel(channel) {
    this.dataChannel = channel;
  }

  hasFeature(feature) {
    return this.features.has(feature);
  }

  requestConfirmation(feature, clientId, deviceName) {
    logger.debug(
      `requestConfirmation feature:${feature} clientId:${clientId} deviceName:${deviceName}`
    );

    return new Promise(resolve => {
      if (typeof android !== 'undefined') {
        android.emit(
          'requestConfirmation',
          JSON.stringify({
            id: confirmationId,
            feature: feature,
            clientId: clientId,
            deviceName: deviceName
          })
        );
        this.confirmations.set(confirmationId, resolve);
        confirmationId++;
      } else {
        // TODO: Need to implement confirmation logic for Tizen.
        resolve(true);
      }
    });
  }

  onConfirmationResult(id, allowed) {
    logger.debug(`onConfirmationResult id:${id} allowed:${allowed}`);

    if (this.confirmations.has(id)) {
      this.confirmations.get(id)(allowed);
      this.confirmations.delete(id);
      return true;
    }
    return false;
  }

  onConnected(url) {}
}
