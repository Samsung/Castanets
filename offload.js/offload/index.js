import * as Resources from './resource';
import { WorkerManager } from './core';
import Logger from '~/util/logger';
import { version } from '../../package.json';
import { devicePopup } from './ui';

const logger = new Logger('index.js');

// Separate the log according to the development mode or production mode
if (process.env.NODE_ENV === 'development') {
  window.localStorage.setItem('debug', 'offload:*');
} else {
  window.localStorage.setItem('debug', 'offload:INFO*, offload:ERROR*');
}

logger.info(`offload.js version: ${version}`);

export class Offload {
  constructor() {
    this.resources = [];
    this.popup = devicePopup;

    // Initialize resources in the resource folder
    for (const Resource of Object.values(Resources)) {
      this.resources.push(new Resource());
    }
  }

  /**
   * Connect to signaling server
   * @param {string} [address] - Signaling Server URL
   * @return {string} server URL
   */
  connect(address) {
    return WorkerManager.getInstance().connect(address);
  }

  /**
   * Add the event listener
   * @param {string} [name] - The name of event
   * @param {Function} [listener] - The callback function
   */
  on(name, listener) {
    switch (name) {
      case 'showDevicePopup':
      case 'hideDevicePopup':
        this.popup.on(name, listener);
        break;
      default:
        logger.error(`unhandled event name: ${name}`);
    }
  }

  /**
   * Remove the event listener
   * @param {string} [name] - The name of event
   * @param {Function} [listener] - The callback function
   */
  off(name, listener) {
    switch (name) {
      case 'showDevicePopup':
      case 'hideDevicePopup':
        this.popup.off(name, listener);
        break;
      default:
        logger.error(`unhandled event name: ${name}`);
    }
  }
}

const offload = new Offload();
offload.version = version;

export default offload;
