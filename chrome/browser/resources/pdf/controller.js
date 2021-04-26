// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {SaveRequestType} from './constants.js';
import {PartialPoint, Point, Viewport} from './viewport.js';

/** @typedef {{ type: string }} */
export let MessageData;

/**
 * @typedef {{
 *   dataToSave: Array,
 *   token: string,
 *   fileName: string
 * }}
 */
let SaveDataMessageData;

/**
 * @typedef {{
 *   type: string,
 *   to: string,
 *   cc: string,
 *   bcc: string,
 *   subject: string,
 *   body: string,
 * }}
 */
let EmailMessageData;

/**
 * @typedef {{
 *   type: string,
 *   url: string,
 *   grayscale: boolean,
 *   modifiable: boolean,
 *   pageNumbers: !Array<number>
 * }}
 */
export let PrintPreviewParams;

/**
 * Creates a cryptographically secure pseudorandom 128-bit token.
 * @return {string} The generated token as a hex string.
 */
function createToken() {
  const randomBytes = new Uint8Array(16);
  return window.crypto.getRandomValues(randomBytes)
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
}

/** @abstract */
export class ContentController {
  constructor() {}

  beforeZoom() {}

  afterZoom() {}

  viewportChanged() {}

  /** @abstract */
  rotateClockwise() {}

  /** @abstract */
  rotateCounterclockwise() {}

  /** @abstract */
  setTwoUpView(enableTwoUpView) {}

  /** Triggers printing of the current document. */
  print() {}

  /** Undo an edit action. */
  undo() {}

  /** Redo an edit action. */
  redo() {}

  /**
   * Requests that the current document be saved.
   * @param {!SaveRequestType} requestType The type of save request. If
   *     ANNOTATION, a response is required, otherwise the controller may save
   *     the document to disk internally.
   * @return {Promise<{fileName: string, dataToSave: ArrayBuffer}>}
   * @abstract
   */
  save(requestType) {}

  /**
   * Loads PDF document from `data` activates UI.
   * @param {string} fileName
   * @param {!ArrayBuffer} data
   * @return {Promise<void>}
   * @abstract
   */
  load(fileName, data) {}

  /**
   * Unloads the current document and removes the UI.
   * @abstract
   */
  unload() {}
}

// PDF plugin controller, responsible for communicating with the embedded plugin
// element. Dispatches a 'plugin-message' event containing the message from the
// plugin, if a message type not handled by this controller is received.
export class PluginController extends ContentController {
  /**
   * @param {!HTMLEmbedElement} plugin
   * @param {!Viewport} viewport
   * @param {function():boolean} getIsUserInitiatedCallback
   * @param {function():?Promise} getLoadedCallback
   */
  constructor(plugin, viewport, getIsUserInitiatedCallback, getLoadedCallback) {
    super();

    /** @private {!HTMLEmbedElement} */
    this.plugin_ = plugin;

    /** @private {!Viewport} */
    this.viewport_ = viewport;

    /** @private {!function():boolean} */
    this.getIsUserInitiatedCallback_ = getIsUserInitiatedCallback;

    /** @private {!function():?Promise} */
    this.getLoadedCallback_ = getLoadedCallback;

    /** @private {!Map<string, PromiseResolver>} */
    this.pendingTokens_ = new Map();
    this.plugin_.addEventListener(
        'message', e => this.handlePluginMessage_(e), false);

    /** @private {!EventTarget} */
    this.eventTarget_ = new EventTarget();
  }

  /** @return {!EventTarget} */
  getEventTarget() {
    return this.eventTarget_;
  }

  /**
   * Notify the plugin to stop reacting to scroll events while zoom is taking
   * place to avoid flickering.
   * @override
   */
  beforeZoom() {
    this.postMessage_({type: 'stopScrolling'});

    if (this.viewport_.pinchPhase === Viewport.PinchPhase.PINCH_START) {
      const position = this.viewport_.position;
      const zoom = this.viewport_.getZoom();
      const pinchPhase = this.viewport_.pinchPhase;
      const layoutOptions = this.viewport_.getLayoutOptions();
      this.postMessage_({
        type: 'viewport',
        userInitiated: true,
        zoom: zoom,
        layoutOptions: layoutOptions,
        xOffset: position.x,
        yOffset: position.y,
        pinchPhase: pinchPhase
      });
    }
  }

  /**
   * Notify the plugin of the zoom change and to continue reacting to scroll
   * events.
   * @override
   */
  afterZoom() {
    const position = this.viewport_.position;
    const zoom = this.viewport_.getZoom();
    const layoutOptions = this.viewport_.getLayoutOptions();
    const pinchVector = this.viewport_.pinchPanVector || {x: 0, y: 0};
    const pinchCenter = this.viewport_.pinchCenter || {x: 0, y: 0};
    const pinchPhase = this.viewport_.pinchPhase;

    this.postMessage_({
      type: 'viewport',
      userInitiated: this.getIsUserInitiatedCallback_(),
      zoom: zoom,
      layoutOptions: layoutOptions,
      xOffset: position.x,
      yOffset: position.y,
      pinchPhase: pinchPhase,
      pinchX: pinchCenter.x,
      pinchY: pinchCenter.y,
      pinchVectorX: pinchVector.x,
      pinchVectorY: pinchVector.y
    });
  }

  /**
   * Post a message to the PPAPI plugin. Some messages will cause an async reply
   * to be received through handlePluginMessage_().
   * @param {!MessageData} message Message to post.
   * @private
   */
  postMessage_(message) {
    this.plugin_.postMessage(message);
  }

  /** @override */
  rotateClockwise() {
    this.postMessage_({type: 'rotateClockwise'});
  }

  /** @override */
  rotateCounterclockwise() {
    this.postMessage_({type: 'rotateCounterclockwise'});
  }

  /** @override */
  setTwoUpView(enableTwoUpView) {
    this.postMessage_({
      type: 'setTwoUpView',
      enableTwoUpView: enableTwoUpView,
    });
  }

  /** @override */
  print() {
    this.postMessage_({type: 'print'});
  }

  selectAll() {
    this.postMessage_({type: 'selectAll'});
  }

  getSelectedText() {
    this.postMessage_({type: 'getSelectedText'});
  }

  /** @param {!PrintPreviewParams} printPreviewParams */
  resetPrintPreviewMode(printPreviewParams) {
    this.postMessage_({
      type: 'resetPrintPreviewMode',
      url: printPreviewParams.url,
      grayscale: printPreviewParams.grayscale,
      // If the PDF isn't modifiable we send 0 as the page count so that no
      // blank placeholder pages get appended to the PDF.
      pageCount:
          (printPreviewParams.modifiable ?
               printPreviewParams.pageNumbers.length :
               0)
    });
  }

  /** @param {string} newColor New color, in hex, for the PDF plugin. */
  backgroundColorChanged(newColor) {
    this.postMessage_({
      type: 'backgroundColorChanged',
      backgroundColor: newColor,
    });
  }

  /**
   * @param {string} url
   * @param {number} index
   */
  loadPreviewPage(url, index) {
    this.postMessage_({type: 'loadPreviewPage', url: url, index: index});
  }

  /** @param {string} password */
  getPasswordComplete(password) {
    this.postMessage_({type: 'getPasswordComplete', password: password});
  }

  /** @param {string} destination */
  getNamedDestination(destination) {
    this.postMessage_({
      type: 'getNamedDestination',
      namedDestination: destination,
    });
  }

  /** @override */
  save(requestType) {
    const resolver = new PromiseResolver();
    const newToken = createToken();
    this.pendingTokens_.set(newToken, resolver);
    this.postMessage_({
      type: 'save',
      token: newToken,
      saveRequestType: requestType,
    });
    return resolver.promise;
  }

  /** @override */
  async load(fileName, data) {
    const url = URL.createObjectURL(new Blob([data]));
    this.plugin_.removeAttribute('headers');
    this.plugin_.setAttribute('stream-url', url);
    this.plugin_.setAttribute('has-edits', '');
    this.plugin_.style.display = 'block';
    try {
      await this.getLoadedCallback_();
    } finally {
      URL.revokeObjectURL(url);
    }
  }

  /** @override */
  unload() {
    this.plugin_.style.display = 'none';
  }

  /**
   * An event handler for handling message events received from the plugin.
   * @param {!Event} messageEvent a message event.
   * @private
   */
  handlePluginMessage_(messageEvent) {
    const messageData = /** @type {!MessageData} */ (messageEvent.data);
    switch (messageData.type) {
      case 'email':
        const emailData = /** @type {!EmailMessageData} */ (messageData);
        const href = 'mailto:' + emailData.to + '?cc=' + emailData.cc +
            '&bcc=' + emailData.bcc + '&subject=' + emailData.subject +
            '&body=' + emailData.body;
        window.location.href = href;
        break;
      case 'goToPage':
        this.viewport_.goToPage(
            /** @type {{type: string, page: number}} */ (messageData).page);
        break;
      case 'setScrollPosition':
        this.viewport_.scrollTo(/** @type {!PartialPoint} */ (messageData));
        break;
      case 'scrollBy':
        this.viewport_.scrollBy(/** @type {!Point} */ (messageData));
        break;
      case 'saveData':
        this.saveData_(/** @type {!SaveDataMessageData} */ (messageData));
        break;
      case 'consumeSaveToken':
        const saveTokenData =
            /** @type {{ type: string, token: string }} */ (messageData);
        const resolver = this.pendingTokens_.get(saveTokenData.token);
        assert(this.pendingTokens_.delete(saveTokenData.token));
        resolver.resolve(null);
        break;
      default:
        this.eventTarget_.dispatchEvent(
            new CustomEvent('plugin-message', {detail: messageData}));
    }
  }

  /**
   * Handles the pdf file buffer received from the plugin.
   * @param {!SaveDataMessageData} messageData data of the message event.
   * @private
   */
  saveData_(messageData) {
    assert(
        loadTimeData.getBoolean('pdfFormSaveEnabled') ||
        loadTimeData.getBoolean('pdfAnnotationsEnabled'));

    // Verify a token that was created by this instance is included to avoid
    // being spammed.
    const resolver = this.pendingTokens_.get(messageData.token);
    assert(this.pendingTokens_.delete(messageData.token));

    if (!messageData.dataToSave) {
      resolver.reject();
      return;
    }

    // Verify the file size and the first bytes to make sure it's a PDF. Cap at
    // 100 MB. This cap should be kept in sync with and is also enforced in
    // pdf/out_of_process_instance.cc.
    const MIN_FILE_SIZE = '%PDF1.0'.length;
    const MAX_FILE_SIZE = 100 * 1000 * 1000;

    const buffer = messageData.dataToSave;
    const bufView = new Uint8Array(buffer);
    assert(
        bufView.length <= MAX_FILE_SIZE,
        `File too large to be saved: ${bufView.length} bytes.`);
    assert(bufView.length >= MIN_FILE_SIZE);
    assert(
        String.fromCharCode(bufView[0], bufView[1], bufView[2], bufView[3]) ===
        '%PDF');

    resolver.resolve(messageData);
  }
}
