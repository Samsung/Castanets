/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

const dimensions = document.querySelector('#dimensions');
const video = document.querySelector('video');
let stream;

const zoomUpButton = document.querySelector('#zoomUp');
const zoomDownButton = document.querySelector('#zoomDown');

const vgaButton = document.querySelector('#vga');
const qvgaButton = document.querySelector('#qvga');
const hdButton = document.querySelector('#hd');
const fullHdButton = document.querySelector('#full-hd');
const fourKButton = document.querySelector('#fourK');
const eightKButton = document.querySelector('#eightK');

const videoblock = document.querySelector('#videoblock');
const messagebox = document.querySelector('#errormessage');

const widthInput = document.querySelector('div#width input');
const widthOutput = document.querySelector('div#width span');
const aspectLock = document.querySelector('#aspectlock');
const sizeLock = document.querySelector('#sizelock');

const facingModeSelect = document.querySelector('select#facingMode');
const codecSelect = document.querySelector('select#codec');
const frameRateSelect = document.querySelector('select#frameRate');
const audioInputSelect = document.querySelector('select#audioSource');
const audioOutputSelect = document.querySelector('select#audioOutput');
const videoSelect = document.querySelector('select#videoSource');
const selectors = [audioInputSelect, audioOutputSelect, videoSelect];

const targetSelect = document.querySelector('select#target');
const numberSelect = document.querySelector('select#number');
const focusSelect = document.querySelector('select#focus');
const animationSelect = document.querySelector('select#animation');
const aiZoomOnButton = document.querySelector('#aiZoomOn');
const aiZoomOffButton = document.querySelector('#aiZoomOff');

const autoConnect = document.querySelector('#autoConnect');
const getConstraints = document.querySelector('#getConstraints');
const constraintsValue = document.querySelector('#constraintsValue');

let currentWidth = 0;
let currentHeight = 0;
let statsIntervalId = 0;
let zoomSettings = { min: -1, max: -1, step: -1, value: -1, };

zoomUpButton.onclick = () => {
  if (zoomSettings.value < zoomSettings.max)
    zoomSettings.value += zoomSettings.step;
  stream.getVideoTracks()[0].applyConstraints({ advanced: [{ zoom: zoomSettings.value }] });
  displayVideoDimensions('zoom');
};

zoomDownButton.onclick = () => {
  if (zoomSettings.min < zoomSettings.value)
    zoomSettings.value -= zoomSettings.step;
  stream.getVideoTracks()[0].applyConstraints({ advanced: [{ zoom: zoomSettings.value }] });
  displayVideoDimensions('zoom');
};

aiZoomOnButton.onclick = () => {
  stream.getVideoTracks()[0].applyConstraints({
    "tizenAiAutoZoomTarget": { "exact": targetSelect.value },
    "tizenAiAutoZoomTargetNumber": { "exact": numberSelect.value },
    "tizenAiAutoZoomFocusPriority": { "exact": focusSelect.value },
    "tizenAiAutoZoomAnimation": { "exact": animationSelect.value }
  });
};

aiZoomOffButton.onclick = () => {
  stream.getVideoTracks()[0].applyConstraints({ "tizenAiAutoZoomTarget": { "exact": "none" } });
};

getConstraints.onclick = () => {
  const constraints = stream.getVideoTracks()[0].getConstraints();
  constraintsValue.innerText = JSON.stringify(constraints, null, 2);
  console.log('constraints: ' + JSON.stringify(constraints));
}

vgaButton.onclick = () => {
  getMedia(vgaConstraints);
};

qvgaButton.onclick = () => {
  getMedia(qvgaConstraints);
};

hdButton.onclick = () => {
  getMedia(hdConstraints);
};

fullHdButton.onclick = () => {
  getMedia(fullHdConstraints);
};

fourKButton.onclick = () => {
  getMedia(fourKConstraints);
};

eightKButton.onclick = () => {
  getMedia(eightKConstraints);
};

const qvgaConstraints = {
  video: { width: { exact: 320 }, height: { exact: 240 } }
};

const vgaConstraints = {
  video: { width: { exact: 640 }, height: { exact: 480 } }
};

const hdConstraints = {
  video: { width: { exact: 1280 }, height: { exact: 720 } }
};

const fullHdConstraints = {
  video: { width: { exact: 1920 }, height: { exact: 1080 } }
};

const fourKConstraints = {
  video: { width: { exact: 4096 }, height: { exact: 2160 } }
};

const eightKConstraints = {
  video: { width: { exact: 7680 }, height: { exact: 4320 } }
};

function gotStream(mediaStream) {
  stream = window.stream = mediaStream; // stream available to console
  video.srcObject = mediaStream;
  messagebox.style.display = 'none';

  const track = mediaStream.getVideoTracks()[0];
  const constraints = track.getConstraints();
  const settings = track.getSettings();
  const capabilities = track.getCapabilities();

  console.log('constraints: ' + JSON.stringify(constraints));
  console.log('settings: ' + JSON.stringify(settings));
  console.log('capabilities: ' + JSON.stringify(capabilities));

  // width
  if (capabilities && capabilities.width && capabilities.width.min && capabilities.width.max) {
    widthInput.min = capabilities.width.min;
    widthInput.max = capabilities.width.max;
  } else {
    console.error('capabilities is null or has no width!');
  }
  if (constraints && constraints.width && constraints.width.exact) {
    widthInput.value = constraints.width.exact;
    widthOutput.textContent = constraints.width.exact;
  } else if (constraints && constraints.width && constraints.width.min) {
    widthInput.value = constraints.width.min;
    widthOutput.textContent = constraints.width.min;
  }

  // zoom
  if (settings && capabilities) {
    zoomSettings = {
      min: capabilities.zoom ? capabilities.zoom.min : -1,
      max: capabilities.zoom ? capabilities.zoom.max : -1,
      step: capabilities.zoom ? capabilities.zoom.step : -1,
      value: settings.zoom ? settings.zoom : -1,
    };
  } else {
    console.error('track has no settings or no capabilities!');
  }
  if (settings && settings.zoom) {
    zoomUpButton.disabled = false;
    zoomDownButton.disabled = false;
  } else {
    console.log('zoom is not supported by ' + track.label);
    zoomUpButton.disabled = true;
    zoomDownButton.disabled = true;
  }

  // WebRTC Report
  if (statsIntervalId)
    clearInterval(statsIntervalId);
  let statsOutput = "";
  let inboundRtpCount = 0;
  statsIntervalId = setInterval(function() {
    if (!mediaStream.peerConnection) {
      console.warn("MediaStream has no peerConnection");
      clearInterval(statsIntervalId);
      statsIntervalId = 0;
      return;
    }
    mediaStream.peerConnection.getStats(track).then(stats => {
      statsOutput = "";
      stats.forEach(report => {
        // report only codec and inbound-rtp
        if (report.type !== 'track' && report.type !== 'codec' && report.type !==
          'inbound-rtp')
          return;
        statsOutput +=
          `<h2>Report: ${report.type}</h2>\n<strong>ID:</strong> ${report.id}<br>\n` +
          `<strong>Timestamp:</strong> ${report.timestamp}<br>\n`;

        // Now the statistics for this report; we intentially drop the ones we
        // sorted to the top above

        Object.keys(report).forEach(statName => {
          if (statName !== "id" && statName !== "timestamp" &&
            statName !== "type") {
            statsOutput +=
              `<strong>${statName}:</strong> ${report[statName]}<br>\n`;
          }
        });
      });
    });

    mediaStream.peerConnection.getStats().then(stats => {
      inboundRtpCount = 0;
      stats.forEach(report => {
        // report only codec and inbound-rtp
        if (report.type === 'inbound-rtp')
          inboundRtpCount++;
      });
    });

    document.querySelector("#webrtcstats").innerHTML =
      `<h2>inbound-rtp Count: ${inboundRtpCount}</h2>\n` +
      statsOutput;
  }, 1000);
}

function errorMessage(who, what, msg) {
  const message = who + ': ' + what + (what == "" ? msg : ", " + msg);
  messagebox.innerText += message + "\n";
  messagebox.style.display = 'block';
  console.log(message);
}

function clearErrorMessage() {
  messagebox.innerText = "";
  messagebox.style.display = 'none';
}

function displayVideoDimensions(whereSeen) {
  if (video.videoWidth) {
    const track = stream.getVideoTracks()[0];
    let stepPoint = zoomSettings.step.toString().indexOf('.');
    let stepFixedCount = (stepPoint > -1) ? (zoomSettings.step.toString().length - stepPoint - 1) :
      0;
    let zoomInfo =
      `zoom:${zoomSettings.value.toFixed(stepFixedCount)}(${zoomSettings.min}/${zoomSettings.max}:${zoomSettings.step})`;
    if (track.getSettings()) {
      dimensions.innerText = `${track.getSettings().width}x${track.getSettings().height}` +
        ` ${track.getSettings().frameRate}fps` +
        ", " + zoomInfo;
    }
    if (currentWidth !== video.videoWidth || currentHeight !== video.videoHeight) {
      console.log(whereSeen + ': ' + dimensions.innerText);
      currentWidth = video.videoWidth;
      currentHeight = video.videoHeight;
    }
  } else {
    dimensions.innerText = 'Video not ready';
  }
}

video.onloadedmetadata = () => {
  displayVideoDimensions('loadedmetadata');
};

video.onresize = () => {
  displayVideoDimensions('resize');
};

function constraintChange(e) {
  widthOutput.textContent = e.target.value;
  const track = window.stream.getVideoTracks()[0];
  let constraints;
  if (aspectLock.checked) {
    constraints = {
      width: { exact: e.target.value },
      aspectRatio: {
        exact: video.videoWidth / video.videoHeight
      }
    };
  } else {
    constraints = { width: { exact: e.target.value } };
  }
  // clearErrorMessage();
  console.log('applying ' + JSON.stringify(constraints));
  track.applyConstraints(constraints)
    .then(() => {
      console.log('applyConstraint success');
      displayVideoDimensions('applyConstraints');
    })
    .catch(err => {
      errorMessage('applyConstraints', err.name);
    });
}

widthInput.onchange = constraintChange;

sizeLock.onchange = () => {
  if (sizeLock.checked) {
    console.log('Setting fixed size');
    video.style.width = '98%';
  } else {
    console.log('Setting auto size');
    video.style.width = 'auto';
  }
};
sizeLock.onchange();

function getMedia(constraints) {
  if (stream) {
    stream.getTracks().forEach(track => {
      console.log(`Stop track ${track.kind}`)
      track.stop();
    });
  }

  constraints.video.facingMode = facingModeSelect.value;
  constraints.video.zoom = true;
  constraints.video.frameRate = frameRateSelect.value;
  if (videoSelect.length !== 0 && autoConnect.checked) {
    constraints.video.deviceId = videoSelect.value;
  }
  // Set offload.videoCodecs constraint
  constraints.offload = { videoCodecs: [codecSelect.value] };
  console.log('constraints: ' + JSON.stringify(constraints));
  navigator.mediaDevices.getUserMedia(constraints)
    .then(gotStream)
    .catch(e => {
      errorMessage('getUserMedia', e.message, e.name);
    });
}

function gotDevices(deviceInfos) {
  // Handles being called several times to update labels. Preserve values.
  const values = selectors.map(select => select.value);
  selectors.forEach(select => {
    while (select.firstChild) {
      select.removeChild(select.firstChild);
    }
  });
  for (let i = 0; i !== deviceInfos.length; ++i) {
    const deviceInfo = deviceInfos[i];
    const option = document.createElement('option');
    option.value = deviceInfo.deviceId;
    if (deviceInfo.kind === 'audioinput') {
      option.text = deviceInfo.label || `microphone ${audioInputSelect.length + 1}`;
      audioInputSelect.appendChild(option);
    } else if (deviceInfo.kind === 'audiooutput') {
      option.text = deviceInfo.label || `speaker ${audioOutputSelect.length + 1}`;
      audioOutputSelect.appendChild(option);
    } else if (deviceInfo.kind === 'videoinput') {
      option.text = deviceInfo.label || `camera ${videoSelect.length + 1}`;
      videoSelect.appendChild(option);
    } else {
      console.log('Some other kind of source/device: ', deviceInfo);
    }
  }
  selectors.forEach((select, selectorIndex) => {
    if (Array.prototype.slice.call(select.childNodes).some(n => n.value ===
        values[selectorIndex])) {
      select.value = values[selectorIndex];
    }
  });
}

navigator.mediaDevices.enumerateDevices().then(gotDevices).catch(handleError);

// Attach audio output device to video element using device/sink ID.
function attachSinkId(element, sinkId) {
  if (typeof element.sinkId !== 'undefined') {
    element.setSinkId(sinkId).then(() => {
      console.log(`Success, audio output device attached: ${sinkId}`);
    }).catch(error => {
      let errorMessage = error;
      if (error.name === 'SecurityError') {
        errorMessage = `You need to use HTTPS for selecting audio output device: ${error}`;
      }
      console.error(errorMessage);
      // Jump back to first output device in the list as it's the default.
      audioOutputSelect.selectedIndex = 0;
    });
  } else {
    console.warn('Browser does not support output device selection.');
  }
}

function changeAudioDestination() {
  const audioDestination = audioOutputSelect.value;
  attachSinkId(videoElement, audioDestination);
}

function handleError(error) {
  console.log('navigator.MediaDevices.getUserMedia error: ', error.message, error.name);
}

// Stop Watch
var timerStart = setInterval(function() {
  let nowTime = new Date
  document.getElementById('postTestMin').innerText = addZero(nowTime.getMinutes())
  document.getElementById('postTestSec').innerText = addZero(nowTime.getSeconds())
  document.getElementById('postTestMilisec').innerText = addZero(Math.floor(nowTime.getMilliseconds()),
    true)
}, 1)

function addZero(num, isMS) {
  if (isMS) {
    return (num < 10 ? '00' + num : (num < 100 ? '0' + num : '' + num))
  }
  return (num < 10 ? '0' + num : '' + num)
}

offload.on("showDevicePopup", () => console.log("showDevicePopup"));
offload.on("hideDevicePopup", () => console.log("hideDevicePopup"));
