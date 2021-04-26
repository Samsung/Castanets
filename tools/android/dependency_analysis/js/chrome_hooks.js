// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Shortens a package name to be displayed in the svg.
 * @param {string} name The full package name to shorten.
 * @return {string} The shortened package name.
 */
function shortenPackageName(name) {
  return name
      .replace('org.chromium.', '.')
      .replace('chrome.browser.', 'c.b.');
}

export {
  shortenPackageName,
};
