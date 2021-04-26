// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * This behavior bundles functionality required to show a password to the user.
 * It is used by both <password-list-item> and <password-edit-dialog>.
 *
 * @polymerBehavior
 */
export const ShowPasswordBehavior = {

  properties: {
    /**
     * @type {!MultiStorePasswordUiEntry}
     */
    entry: Object,

    /** The password that is being displayed. */
    password: {
      type: String,
      value: '',
    },

    // <if expr="chromeos">
    /** @type BlockingRequestManager */
    tokenRequestManager: Object
    // </if>
  },

  observers: [
    'resetPlaintextPasswordOnEntryChanged_(entry)',
  ],

  /** @private */
  resetPlaintextPasswordOnEntryChanged_() {
    this.password = '';
  },

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * or when there's federated text otherwise 'password'.
   * @private
   */
  getPasswordInputType_() {
    return this.password || this.entry.federationText ? 'text' : 'password';
  },

  /**
   * Gets the title text for the show/hide icon.
   * @param {string} password
   * @param {string} hide The i18n text to use for 'Hide'
   * @param {string} show The i18n text to use for 'Show'
   * @private
   */
  showPasswordTitle_(password, hide, show) {
    return password ? hide : show;
  },

  /**
   * Get the right icon to display when hiding/showing a password.
   * @return {string}
   * @private
   */
  getIconClass_() {
    return this.password ? 'icon-visibility-off' : 'icon-visibility';
  },

  /**
   * Gets the text of the password. Will use the value of |password| unless it
   * cannot be shown, in which case it will be a fixed number of spaces. It can
   * also be the federated text.
   * @private
   */
  getPassword_() {
    if (!this.entry) {
      return '';
    }

    const NUM_PLACEHOLDERS = 10;
    return this.entry.federationText || this.password ||
        ' '.repeat(NUM_PLACEHOLDERS);
  },

  /**
   * Handler for tapping the show/hide button.
   * @private
   */
  onShowPasswordButtonTap_() {
    if (this.password) {
      this.password = '';
      return;
    }
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.entry.getAnyId(), chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(
            password => {
              this.password = password;
            },
            error => {
              // <if expr="chromeos">
              // If no password was found, refresh auth token and retry.
              this.tokenRequestManager.request(
                  this.onShowPasswordButtonTap_.bind(this));
              // </if>
            });
  },
};
