// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {BrowserProxy} from './browser_proxy_interface.js';

/* eslint-disable new-cap */

/** @throws {Error} */
function NOTIMPLEMENTED() {
  throw Error('Browser proxy method not implemented!');
}

/**
 * The WebUI implementation of the CCA's interaction with the browser.
 * @implements {BrowserProxy}
 */
class WebUIBrowserProxy {
  /** @override */
  async getVolumeList() {
    NOTIMPLEMENTED();
    return null;
  }

  /** @override */
  async requestFileSystem(options) {
    NOTIMPLEMENTED();
    return null;
  }

  /** @override */
  async localStorageGet(keys) {
    let result = {};
    let sanitizedKeys = [];
    if (typeof keys === 'string') {
      sanitizedKeys = [keys];
    } else if (Array.isArray(keys)) {
      sanitizedKeys = keys;
    } else if (keys !== null && typeof keys === 'object') {
      sanitizedKeys = Object.keys(keys);

      // If any key does not exist, use the default value specified in the
      // input.
      result = Object.assign({}, keys);
    } else {
      throw new Error('WebUI localStorageGet() cannot be run with ' + keys);
    }

    for (const key of sanitizedKeys) {
      const value = window.localStorage.getItem(key);
      if (value !== null) {
        result[key] = JSON.parse(value);
      } else if (result[key] === undefined) {
        // For key that does not exist and does not have a default value, set it
        // to null.
        result[key] = null;
      }
    }
    return result;
  }

  /** @override */
  async localStorageSet(items) {
    for (const [key, val] of Object.entries(items)) {
      window.localStorage.setItem(key, JSON.stringify(val));
    }
  }

  /** @override */
  async localStorageRemove(items) {
    if (typeof items === 'string') {
      items = [items];
    }
    for (const key of items) {
      window.localStorage.removeItem(key);
    }
  }

  /** @override */
  async getBoard() {
    NOTIMPLEMENTED();
    return '';
  }

  /** @override */
  getI18nMessage(name, substitutions = undefined) {
    NOTIMPLEMENTED();
    return '';
  }

  /** @override */
  async isCrashReportingEnabled() {
    NOTIMPLEMENTED();
    return false;
  }

  /** @override */
  async openGallery(file) {
    NOTIMPLEMENTED();
  }

  /** @override */
  openInspector(type) {
    NOTIMPLEMENTED();
  }

  /** @override */
  getAppId() {
    NOTIMPLEMENTED();
    return '';
  }

  /** @override */
  getAppVersion() {
    NOTIMPLEMENTED();
    return '';
  }

  /** @override */
  addOnMessageExternalListener(listener) {
    NOTIMPLEMENTED();
  }

  /** @override */
  addOnConnectExternalListener(listener) {
    NOTIMPLEMENTED();
  }

  /** @override */
  sendMessage(extensionId, message) {
    NOTIMPLEMENTED();
  }

  /** @override */
  addDummyHistoryIfNotAvailable() {
    // no-ops
  }

  /** @override */
  isMp4RecordingEnabled() {
    return false;
  }
}

export const browserProxy = new WebUIBrowserProxy();

/* eslint-enable new-cap */
