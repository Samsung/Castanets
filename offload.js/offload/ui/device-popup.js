import { WorkerManager } from '../core/worker-manager';
import { EventEmitter } from 'events';
import Logger from '~/util/logger';

import deviceEntry from './html/device-entry.html';
import devicePopup from './html/device-popup.html';
import styles from './style/offload.css';

import SIGNAL_IMG from '~/images/signal.png';
import ENABLE_IMG from '~/images/green.png';
import DISABLE_IMG from '~/images/red.png';

const logger = new Logger('device-popup.js');

const KeyCode = {
  BACKSPACE: 8,
  ENTER: 13,
  SPACE: 32,
  LEFT: 37,
  UP: 38,
  RIGHT: 39,
  DOWN: 40,
  XF86_BACK: 10009
};

const UPDATE_DEVICE_INTERVAL = 1000;
const UPDATE_DEVICE_TIMEOUT = 10000;

const POPUP_ID = '__oFfLoAdPoPuP210426__';

export class DevicePopup extends EventEmitter {
  constructor() {
    super();
    this.updateIntervalID_ = 0;
    this.signalingIntervalID_ = 0;
    this.createElement();
    this.addQrImage();
  }

  createElement() {
    styles.use();
    this.element_ = document.createElement('div');
    this.element_.classList.add('modal');
    this.element_.id = POPUP_ID;
    this.element_.classList.add('overlay');
    this.element_.setAttribute('tabindex', '-1');
    this.element_.setAttribute('role', 'dialog');

    this.element_.innerHTML = devicePopup({
      SIGNAL_IMG: SIGNAL_IMG,
      ENABLE_IMG: ENABLE_IMG,
      alwaysUseSameDevice: true
    });

    this.keydownEventListener_ = e => {
      let isHandled = false;
      switch (e.which) {
        case KeyCode.LEFT:
        case KeyCode.UP:
        case KeyCode.RIGHT:
        case KeyCode.DOWN:
          this.moveElement(e.which);
          isHandled = true;
          break;
        case KeyCode.BACKSPACE:
        case KeyCode.XF86_BACK:
          this.element_.querySelector('#closeBtn').click();
          isHandled = true;
          break;
        case KeyCode.ENTER:
        case KeyCode.SPACE:
          isHandled = this.selectElement();
          break;
      }

      if (isHandled) {
        e.preventDefault();
        e.stopPropagation();
      } else {
        logger.debug(
          `Not handled keydown. ${e.which} ${e.code} ${e.currentTarget.id}`
        );
      }
    };

    this.keyupEventListener_ = e => {
      const isHandled = Object.keys(KeyCode).some(key => {
        if (e.which === KeyCode[key]) {
          e.preventDefault();
          e.stopPropagation();
          return true;
        }
        return false;
      });
      if (!isHandled) {
        logger.debug(
          `Not handled keyup. ${e.which} ${e.code} ${e.currentTarget.id}`
        );
      }
    };

    this.focusInEventListener_ = e => {
      this.checkAndBackFocus_();
    };

    this.focusOutEventListener_ = e => {
      // Check focus and back focus to popup after focus out 100ms.
      setTimeout(() => {
        this.checkAndBackFocus_();
      }, 100);
    };

    this.element_
      .querySelector('#refreshBtn')
      .addEventListener('click', e => this.startUpdateInterval());

    this.element_.querySelector('#closeBtn').addEventListener('click', e => {
      if (typeof this.options_.errorCallback === 'function') {
        this.options_.errorCallback(
          new DOMException('Requested device not found')
        );
      }
      this.hide();
    });

    this.element_.querySelector('#connectBtn').addEventListener('click', e => {
      const checked = this.element_.querySelector('input[type=radio]:checked');
      if (!checked) {
        return;
      }
      if (typeof this.options_.successCallback === 'function') {
        this.options_.successCallback({
          workerId: checked.value,
          always: this.element_.querySelector('#alwaysCb').checked
        });
      }
      this.hide();
    });
  }

  checkAndBackFocus_() {
    if (this.lastFocusedElm_ === document.activeElement) {
      return;
    }

    // check if activeElement is the focusable element in popup.
    const isPopupFocused = Array.from(
      this.element_.querySelectorAll('[tabindex]')
    ).some(elm => elm === document.activeElement);

    // focus back to popup
    if (!isPopupFocused) {
      if (
        this.lastFocusedElm_ &&
        this.lastFocusedElm_.closest(`#${POPUP_ID}`)
      ) {
        this.lastFocusedElm_.focus();
      } else {
        this.lastFocusedElm_ = this.setFocus(0);
      }
      logger.debug(`[focus] >> focus back to ${this.lastFocusedElm_.id}`);
    } else {
      this.lastFocusedElm_ = document.activeElement;
    }
  }

  addQrImage() {
    WorkerManager.getInstance()
      .getQrCode()
      .then(qrCode => {
        const qrCodeImage = this.element_.querySelector('#qrCodeImage');
        qrCodeImage.src = qrCode;
        qrCodeImage.style.display = 'block';
        const newP = document.createElement('span');
        const text = document.createTextNode('Scan QR code for connecting');
        newP.appendChild(text);
        qrCodeImage.parentNode.appendChild(newP);
      });
  }

  show(options) {
    this.options_ = options;

    const popup = document.getElementById(POPUP_ID);
    if (!popup) {
      document.body.appendChild(this.element_);
    }

    this.element_.style.display = 'block';
    this.element_.addEventListener('keydown', this.keydownEventListener_);
    this.element_.addEventListener('keyup', this.keyupEventListener_);
    document.addEventListener('focusin', this.focusInEventListener_);
    document.addEventListener('focusout', this.focusOutEventListener_);

    const selected = this.element_.querySelector('input[type=radio]:checked');
    if (selected) {
      selected.focus();
    } else {
      this.element_.querySelector('#refreshBtn').focus();
    }

    this.startUpdateInterval();
    this.startSignalingStatusInterval();
    this.emit('showDevicePopup', this);
  }

  hide() {
    this.clearUpdateInterval();
    this.clearSignalingStatusInterval();

    document.body.removeChild(this.element_);

    this.element_.style.display = 'none';
    this.element_.removeEventListener('keydown', this.keydownEventListener_);
    this.element_.removeEventListener('keyup', this.keyupEventListener_);
    document.removeEventListener('focusin', this.focusInEventListener_);
    document.removeEventListener('focusout', this.focusOutEventListener_);
    this.emit('hideDevicePopup', this);
  }

  setFocus(index) {
    const focusableElms = Array.from(
      this.element_.querySelectorAll('[tabindex]')
    ).filter(elm => elm.offsetParent !== null);

    if (index >= focusableElms.length) {
      index = 0;
    } else if (index < 0) {
      index = focusableElms.length - 1;
    }

    focusableElms[index].focus();
    logger.debug(
      `setFocus() >> [${index}] ${focusableElms[index].id} : ${document.activeElement.id}`
    );
    return focusableElms[index];
  }

  moveElement(direction) {
    const focusableElms = Array.from(
      this.element_.querySelectorAll('[tabindex]')
    ).filter(elm => elm.offsetParent !== null);

    // Find current focused element
    let currentIndex = 0;
    for (let i = 0; i < focusableElms.length; i++) {
      if (document.activeElement.id === focusableElms[i].id) {
        currentIndex = i;
      }
    }

    switch (direction) {
      case KeyCode.LEFT:
      case KeyCode.UP:
        currentIndex--;
        break;
      case KeyCode.RIGHT:
      case KeyCode.DOWN:
        currentIndex++;
        break;
    }

    this.setFocus(currentIndex);
  }

  selectElement() {
    const focusableElms = Array.from(
      this.element_.querySelectorAll('[tabindex]')
    ).filter(elm => elm.offsetParent !== null);

    // Verify if the current focused element is ours
    for (let i = 0; i < focusableElms.length; i++) {
      if (document.activeElement.id === focusableElms[i].id) {
        focusableElms[i].click();
        if (focusableElms[i].className === 'form-device-input') {
          this.element_.querySelector('#connectBtn').click();
        }
        return true;
      }
    }

    logger.debug('selectElement() : Not Handled ' + document.activeElement.id);
    return false;
  }

  showDeviceList(workers) {
    const deviceList = this.element_.querySelector('#deviceList');
    const oldDevices = deviceList.querySelectorAll('.form-device-input');
    const oldDeviceSet = new Set();
    let deviceCount = oldDevices.length;

    if (deviceCount === 0) {
      deviceList.innerHTML = '';
    } else {
      // Remove invisible devices
      for (const device of oldDevices) {
        const foundIdx = workers.findIndex(elm => elm.id === device.value);
        if (foundIdx === -1) {
          device.parentNode.remove();
          deviceCount--;
        } else {
          oldDeviceSet.add(device.id);
        }
      }
    }

    // Add new discovered devices
    for (let i = 0; i < workers.length; i++) {
      if (oldDeviceSet.has(workers[i].id)) {
        continue;
      }
      const newDiv = document.createRange().createContextualFragment(
        deviceEntry({
          id: workers[i].id,
          name: workers[i].name,
          checked: deviceCount === 0 && i === 0
        })
      );
      deviceList.appendChild(newDiv);
      deviceCount++;
    }

    this.element_.querySelector('#alwaysForm').style.display =
      deviceCount > 0 ? 'block' : 'none';

    // Update focus when the first device is discovered or all discovered devices are closed.
    if (oldDevices.length > 0 !== deviceCount > 0) {
      this.setFocus(0);
    }
  }

  updateDeviceList(callback) {
    const localWorker = { id: 'localdevice', name: 'Local Device' };

    // Show localDevice immediately
    if (!this.isCapabilityUpdated && this.options_.localGumIsCallable) {
      this.showDeviceList([localWorker]);
    }

    WorkerManager.getInstance().updateCapability(() => {
      this.isCapabilityUpdated = true;
      const workers = WorkerManager.getInstance().getSupportedWorkers(
        this.options_.feature
      );

      // Add localDevice to discovered workers
      if (this.options_.localGumIsCallable) {
        workers.unshift(localWorker);
      }

      this.showDeviceList(workers);

      // `callback` is function to test last time for device list update. If callback
      // returns true, it shows 'None' to list.
      if (callback && callback()) {
        if (workers.length === 0 && !this.options_.localGumIsCallable) {
          this.element_.querySelector('#deviceList').innerHTML = '<p>None</p>';
          return;
        }
      }
    });
  }

  updateSignalingStatus() {
    const signalingImg = this.element_.querySelector('#signalingStatus');
    if (signalingImg && WorkerManager.getInstance().signalingStatus) {
      signalingImg.src = ENABLE_IMG;
    } else {
      signalingImg.src = DISABLE_IMG;
    }
  }

  clearUpdateInterval() {
    if (this.updateIntervalID_) {
      logger.debug(`Capability Interval Ended.`);
      clearInterval(this.updateIntervalID_);
      this.updateIntervalID_ = 0;
    }
  }

  startUpdateInterval() {
    this.clearUpdateInterval();

    logger.debug(`Capability Interval Starting...`);
    let elapsedTime = 0;
    this.isCapabilityUpdated = false;
    this.updateIntervalID_ = setInterval(() => {
      this.updateDeviceList(() => {
        if ((elapsedTime += UPDATE_DEVICE_INTERVAL) >= UPDATE_DEVICE_TIMEOUT) {
          this.clearUpdateInterval();
          return true;
        }
        return false;
      });
    }, UPDATE_DEVICE_INTERVAL);

    // Update device list immediately
    this.updateDeviceList();
  }

  clearSignalingStatusInterval() {
    if (this.signalingIntervalID_) {
      clearInterval(this.signalingIntervalID_);
      this.signalingIntervalID_ = 0;
    }
  }

  startSignalingStatusInterval() {
    this.clearSignalingStatusInterval();
    this.signalingIntervalID_ = setInterval(
      this.updateSignalingStatus.bind(this),
      UPDATE_DEVICE_INTERVAL
    );
  }
}
