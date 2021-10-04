import { WorkerManager } from '../core/worker-manager';
import Logger from '~/util/logger';

import SIGNAL_IMG from '~/images/signal.png';
import ENABLE_IMG from '~/images/green.png';
import DISABLE_IMG from '~/images/red.png';

const logger = new Logger('device.js');

const BACKSPACE = 8;
const ENTER = 13;
const SPACE = 32;
const LEFT = 37;
const UP = 38;
const RIGHT = 39;
const DOWN = 40;
const XF86_BACK = 10009;

const UPDATE_DEVICE_INTERVAL = 1000;
const UPDATE_DEVICE_TIMEOUT = 10000;

let elapsedTime = 0;
let updateIntervalID = null;
let signalingIntervalID = null;
let alwaysUseSameDevice = true;
let signalingStatus = false;

const TAG = 'device.js ';

const getDevicePopupDeviceDiv = function (id, name, checked) {
  return `
    <div class="form-check">
      <input class="form-check-input" type="radio" name="radios" id="${id}" value="${id}" ${
    checked ? 'checked' : ''
  } tabindex=0>
      <label class="form-check-label" for="${id}">
        ${name}
      </label>
    </div>`;
};

const getdevicePopupContent = function () {
  return `
    <div class="modal-dialog" role="document">
      <div class="modal-content">
        <div class="modal-header">
          <span class="modal-title mr-auto">Available devices</span>
          <img width="24" src="${SIGNAL_IMG}" />
          <img width="24" id="signalingStatus" src="${ENABLE_IMG}" />
        </div>
        <div class="modal-body">
          <div class="row-container">
            <div id="deviceList">
            </div>
            <div class="vertical-align-center">
              <image id="qrCodeImage" width="132" height="132" style="display:none;"></image>
            </div>
          </div>
          <div class="form-check" id="alwaysForm">
            <input class="form-check-input" type="checkbox" id="alwaysCb" ${
              alwaysUseSameDevice ? 'checked' : ''
            } tabindex=0>
            <label class="form-check-label" for="alwaysCb">
              Always connect to selected device
            </label>
          </div>
        </div>
        <div class="modal-footer">
          <button type="button" class="btn btn-success mr-auto" id="refreshBtn" tabindex=0>Scan</button>
          <button type="button" class="btn btn-primary" id="connectBtn" tabindex=0>Connect</button>
          <button type="button" class="btn btn-secondary" id="closeBtn" tabindex=0>Close</button>
        </div>
      </div>
    </div>
  `;
};

function setFocus(root, index) {
  const focusableElms = Array.from(root.querySelectorAll('[tabindex]')).filter(
    elm => elm.offsetParent !== null
  );

  if (index >= focusableElms.length) {
    index = 0;
  } else if (index < 0) {
    index = focusableElms.length - 1;
  }

  focusableElms[index].focus();
  logger.debug(
    TAG +
      'setFocus() >> ' +
      index +
      ' : ' +
      focusableElms[index].id +
      '=' +
      document.activeElement.id
  );
}

function moveElement(direction) {
  const dialog = document.querySelector('.modal-dialog');
  const indexes = Array.from(dialog.querySelectorAll('[tabindex]')).filter(
    elm => elm.offsetParent !== null
  );

  // Find current focused element
  let currentIndex = 0;
  for (let i = 0; i < indexes.length; i++) {
    if (document.activeElement.id === indexes[i].id) {
      currentIndex = i;
    }
  }
  logger.debug(
    'focused elm : ' + currentIndex + ', ' + document.activeElement.id
  );

  switch (direction) {
    case LEFT:
    case UP:
      currentIndex--;
      break;
    case RIGHT:
    case DOWN:
      currentIndex++;
      break;
  }

  setFocus(dialog, currentIndex);
}

function close(popup) {
  clearUpdateInterval();
  clearSignalingStatusInterval();

  popup.innerHTML = '';
  popup.style.display = 'none';
  popup.removeEventListener('keydown', handleKeydown);
}

function selectElement() {
  const dialog = document.querySelector('.modal-dialog');
  const indexes = Array.from(dialog.querySelectorAll('[tabindex]')).filter(
    elm => elm.offsetParent !== null
  );

  // Verify if the current focused element is ours
  for (let i = 0; i < indexes.length; i++) {
    if (document.activeElement.id === indexes[i].id) {
      document.activeElement.click();
      if (
        document.activeElement.className === 'form-check-input' &&
        indexes[i].id !== 'alwaysCb'
      ) {
        popup.querySelector('#connectBtn').click();
      }
      return true;
    }
  }

  logger.debug('selectElement() : Not Handled ' + document.activeElement.id);
  return false;
}

function handleKeydown(e) {
  let isHandled = false;
  switch (e.which) {
    case LEFT:
    case UP:
    case RIGHT:
    case DOWN:
      moveElement(e.which);
      isHandled = true;
      break;
    case BACKSPACE:
    case XF86_BACK:
      if (e.currentTarget.id === 'popup') {
        close(e.currentTarget);
        isHandled = true;
      }
      break;
    case ENTER:
    case SPACE:
      isHandled = selectElement();
      break;
  }

  if (isHandled) {
    e.preventDefault();
  } else {
    logger.debug('handleKeydown() : Not Handled ' + e.which + ', ' + e.code);
  }
}

function updateDeviceList(popup, feature, localGumIsCallable) {
  WorkerManager.getInstance().updateCapability(() => {
    elapsedTime += UPDATE_DEVICE_INTERVAL;

    const deviceList = popup.querySelector('#deviceList');
    const workers = WorkerManager.getInstance().getSupportedWorkers(feature);

    if (elapsedTime >= UPDATE_DEVICE_TIMEOUT) {
      if (workers.length === 0 && !localGumIsCallable) {
        deviceList.innerHTML = '<p>None</p>';
      }

      clearInterval(updateIntervalID);
      elapsedTime = 0;
      return;
    }

    const oldDevices = deviceList.querySelectorAll('.form-check-input');
    if (oldDevices.length === 0) {
      deviceList.innerHTML = '';
    }
    let deviceCount = oldDevices.length;
    // Remove invisible devices
    for (const device of oldDevices) {
      const foundIdx = workers.findIndex(elm => elm.id === device.value);
      if (device.value === 'localdevice') {
        continue;
      }
      if (foundIdx === -1) {
        device.parentNode.remove();
        deviceCount--;
      } else {
        workers.splice(foundIdx, 1);
      }
    }

    // Add new discovered devices
    for (let i = 0; i < workers.length; i++) {
      const newDiv = document
        .createRange()
        .createContextualFragment(
          getDevicePopupDeviceDiv(
            workers[i].id,
            workers[i].name,
            deviceCount === 0 && i === 0
          )
        );
      deviceList.appendChild(newDiv);
      deviceCount++;
    }

    popup.querySelector('#alwaysForm').style.display =
      deviceCount > 0 ? 'block' : 'none';

    if (!popup.qrGenerated) {
      addQrImage();
      popup.qrGenerated = true;
    }

    // Update focus when the first device is discovered or all discovered devices are closed.
    if (oldDevices.length > 0 !== deviceCount > 0) {
      setFocus(popup, 0);
    }
  });
}

function addQrImage() {
  WorkerManager.getInstance()
    .getQrCode()
    .then(qrCode => {
      const qrCodeImage = document.getElementById('qrCodeImage');
      qrCodeImage.src = qrCode;
      qrCodeImage.style.display = 'block';
      const newP = document.createElement('span');
      const text = document.createTextNode('Scan QR code for connecting');
      newP.appendChild(text);
      qrCodeImage.parentNode.appendChild(newP);
    });
}

export function showDevicePopup(popup, options) {
  popup.innerHTML = getdevicePopupContent();
  popup.addEventListener('load', () => {
    updateDeviceList(popup, options.feature, options.localGumIsCallable);
  });
  popup.qrGenerated = false;
  popup.style.display = 'block';

  startUpdateInterval(popup, options.feature, options.localGumIsCallable);
  startSignalingStatusInterval(popup);

  const dialog = popup.querySelector('.modal-dialog');
  const selected = dialog.querySelector('input[type=radio]:checked');
  if (selected) {
    selected.focus();
  } else {
    dialog.querySelector('#refreshBtn').focus();
  }

  popup.addEventListener('keydown', handleKeydown);

  popup.querySelector('#alwaysCb').addEventListener('change', e => {
    alwaysUseSameDevice = e.target.checked;
  });

  popup.querySelector('#refreshBtn').addEventListener('click', e => {
    startUpdateInterval(popup, options.feature, options.localGumIsCallable);
  });

  popup.querySelector('#closeBtn').addEventListener('click', function (e) {
    if (typeof options.errorCallback === 'function') {
      options.errorCallback(new DOMException('Requested device not found'));
    }

    close(popup);
  });

  popup.querySelector('#connectBtn').addEventListener('click', function (e) {
    const checked = popup.querySelector('input[type=radio]:checked');
    if (!checked) {
      return;
    }

    options.successCallback({
      workerId: checked.value,
      always: popup.querySelector('#alwaysCb').checked
    });

    close(popup);
  });
}

function updateSignalingStatus(popup) {
  const signalingImg = popup.querySelector('#signalingStatus');
  signalingStatus = WorkerManager.getInstance().signalingStatus;
  if (signalingImg && signalingStatus) {
    signalingImg.src = ENABLE_IMG;
  } else {
    signalingImg.src = DISABLE_IMG;
  }
}

function clearUpdateInterval() {
  if (updateIntervalID) {
    elapsedTime = 0;
    clearInterval(updateIntervalID);
    updateIntervalID = null;
  }
}

function startUpdateInterval(popup, feature, localGumIsCallable) {
  clearUpdateInterval();
  updateIntervalID = setInterval(
    updateDeviceList,
    UPDATE_DEVICE_INTERVAL,
    popup,
    feature,
    localGumIsCallable
  );
}

function clearSignalingStatusInterval() {
  if (signalingIntervalID) {
    clearInterval(signalingIntervalID);
    signalingIntervalID = null;
  }
}

function startSignalingStatusInterval(popup) {
  clearSignalingStatusInterval();
  signalingIntervalID = setInterval(
    updateSignalingStatus,
    UPDATE_DEVICE_INTERVAL,
    popup
  );
}
