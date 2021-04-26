// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from './constants.js';

// Parses the open pdf parameters passed in the url to set initial viewport
// settings for opening the pdf.
export class OpenPdfParamsParser {
  /**
   * @param {function(string):void} getNamedDestinationCallback
   *     Function called to fetch information for a named destination.
   */
  constructor(getNamedDestinationCallback) {
    /** @private {!Array<!Object>} */
    this.outstandingRequests_ = [];

    /** @private {!function(string):void} */
    this.getNamedDestinationCallback_ = getNamedDestinationCallback;
  }

  /**
   * Parse zoom parameter of open PDF parameters. The PDF should be opened at
   * the specified zoom level.
   * @param {string} paramValue zoom value.
   * @return {Object} Map with zoom parameters (zoom and position).
   * @private
   */
  parseZoomParam_(paramValue) {
    const paramValueSplit = paramValue.split(',');
    if (paramValueSplit.length !== 1 && paramValueSplit.length !== 3) {
      return {};
    }

    // User scale of 100 means zoom value of 100% i.e. zoom factor of 1.0.
    const zoomFactor = parseFloat(paramValueSplit[0]) / 100;
    if (Number.isNaN(zoomFactor)) {
      return {};
    }

    // Handle #zoom=scale.
    if (paramValueSplit.length === 1) {
      return {'zoom': zoomFactor};
    }

    // Handle #zoom=scale,left,top.
    const position = {
      x: parseFloat(paramValueSplit[1]),
      y: parseFloat(paramValueSplit[2])
    };
    return {'position': position, 'zoom': zoomFactor};
  }

  /**
   * Parse view parameter of open PDF parameters. The PDF should be opened at
   * the specified fitting type mode and position.
   * @param {string} paramValue view value.
   * @return {Object} Map with view parameters (view and viewPosition).
   * @private
   */
  parseViewParam_(paramValue) {
    const viewModeComponents = paramValue.toLowerCase().split(',');
    if (viewModeComponents.length < 1) {
      return {};
    }

    const params = {};
    const viewMode = viewModeComponents[0];
    let acceptsPositionParam;
    if (viewMode === 'fit') {
      params['view'] = FittingType.FIT_TO_PAGE;
      acceptsPositionParam = false;
    } else if (viewMode === 'fith') {
      params['view'] = FittingType.FIT_TO_WIDTH;
      acceptsPositionParam = true;
    } else if (viewMode === 'fitv') {
      params['view'] = FittingType.FIT_TO_HEIGHT;
      acceptsPositionParam = true;
    }

    if (!acceptsPositionParam || viewModeComponents.length < 2) {
      return params;
    }

    const position = parseFloat(viewModeComponents[1]);
    if (!Number.isNaN(position)) {
      params['viewPosition'] = position;
    }

    return params;
  }

  /**
   * Parse the parameters encoded in the fragment of a URL.
   * @param {string} url to parse
   * @return {!URLSearchParams}
   * @private
   */
  parseUrlParams_(url) {
    const hash = new URL(url).hash;
    const params = new URLSearchParams(hash.substring(1));

    // Handle the case of http://foo.com/bar#NAMEDDEST. This is not
    // explicitly mentioned except by example in the Adobe
    // "PDF Open Parameters" document.
    if (Array.from(params).length === 1) {
      const key = Array.from(params.keys())[0];
      if (params.get(key) === '') {
        params.append('nameddest', key);
        params.delete(key);
      }
    }

    return params;
  }

  /**
   * @param {string} url that needs to be parsed.
   * @return {boolean} Whether the toolbar UI element should be shown.
   */
  shouldShowToolbar(url) {
    return this.parseUrlParams_(url).get('toolbar') !== '0';
  }

  /**
   * Parse PDF url parameters. These parameters are mentioned in the url
   * and specify actions to be performed when opening pdf files.
   * See http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/
   * pdfs/pdf_open_parameters.pdf for details.
   * @param {string} url that needs to be parsed.
   * @param {Function} callback function to be called with viewport info.
   */
  getViewportFromUrlParams(url, callback) {
    const params = {};
    params['url'] = url;

    const urlParams = this.parseUrlParams_(url);

    if (urlParams.has('page')) {
      // |pageNumber| is 1-based, but goToPage() take a zero-based page number.
      const pageNumber = parseInt(urlParams.get('page'), 10);
      if (!Number.isNaN(pageNumber) && pageNumber > 0) {
        params['page'] = pageNumber - 1;
      }
    }

    if (urlParams.has('view')) {
      Object.assign(
          params,
          this.parseViewParam_(/** @type {string} */ (urlParams.get('view'))));
    }

    if (urlParams.has('zoom')) {
      Object.assign(
          params,
          this.parseZoomParam_(/** @type {string} */ (urlParams.get('zoom'))));
    }

    if (params.page === undefined && urlParams.has('nameddest')) {
      this.outstandingRequests_.push({callback: callback, params: params});
      this.getNamedDestinationCallback_(
          /** @type {string} */ (urlParams.get('nameddest')));
    } else {
      callback(params);
    }
  }

  /**
   * This is called when a named destination is received and the page number
   * corresponding to the request for which a named destination is passed.
   * @param {number} pageNumber The page corresponding to the named destination
   *    requested.
   */
  onNamedDestinationReceived(pageNumber) {
    const outstandingRequest = this.outstandingRequests_.shift();
    if (pageNumber !== -1) {
      outstandingRequest.params.page = pageNumber;
    }
    outstandingRequest.callback(outstandingRequest.params);
  }
}
