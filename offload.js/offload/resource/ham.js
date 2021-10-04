import { Resource } from '../core';

export class HAMResource extends Resource {
  constructor() {
    super();

    // Need to check how to support for version-specific features
    // https://docs.tizen.org/application/web/api/5.5/device_api/wearable/tizen/humanactivitymonitor.html#HumanActivityType
    this.listenerId_ = 0;
    this.checkCapability();
  }

  start(...params) {
    const args = [].slice.call(params);
    this.startJob({
      feature: args[0],
      successCallback: args[1]
    });
  }

  stop(feature) {
    this.stopJob(feature);
  }

  addGestureRecognitionListener(...params) {
    const args = [].slice.call(params);
    this.startJob({
      feature: 'GESTURE',
      successCallback: args[1],
      errorCallback: args[2]
    });
    return ++this.listenerId_;
  }

  checkCapability() {
    if (typeof tizen === 'undefined') {
      window.tizen = {};
    }

    if (typeof tizen.ppm === 'undefined') {
      tizen.ppm = {};
      tizen.ppm.requestPermission = function (...params) {
        const privilege = params[0];
        const successCallback = params[1];
        successCallback('PPM_ALLOW_FOREVER', privilege);
      };
      tizen.ppm.requestPermissions = function (...params) {
        const privileges = params[0];
        const results = [];
        privileges.forEach(function (item) {
          const result = {};
          result.privilege = item;
          result.result = 'PPM_ALLOW_FOREVER';
          results.push(result);
        });
        const successCallback = params[1];
        successCallback(results);
        return results;
      };
      tizen.ppm.checkPermission = function () {
        return 'PPM_ALLOW';
      };
      tizen.ppm.checkPermissions = function (...params) {
        const privileges = params[0];
        const states = [];
        privileges.forEach(function (item) {
          const state = {};
          state.privilege = item;
          state.type = 'PPM_ALLOW';
          states.push(state);
        });
        return states;
      };
    }

    if (!tizen.humanactivitymonitor) {
      tizen.humanactivitymonitor = {};
      tizen.humanactivitymonitor.start = this.start.bind(this);
      tizen.humanactivitymonitor.stop = this.stop.bind(this);
      tizen.humanactivitymonitor.addGestureRecognitionListener = this.addGestureRecognitionListener.bind(
        this
      );
    }
  }
}
