'use strict';

const videoCheckBox = document.getElementById('videoCheckBox');
const videoBlock = document.getElementById('videoBlock');
const video = document.querySelector('video');

const statsCheckBox = document.getElementById('statsCheckBox');
const statsInfo = document.getElementById('stats');

let myStream;
let statsIntervalId = 0;
let prevFramesSent = 0;

document.getElementById('connectBtn').onclick = function () {
  const url = document.getElementById('serverURL').innerHTML;
  offloadWorker.connect(url, { forceConnect: true });
};

document.getElementById('logClearBtn').onclick = function () {
  document.getElementById('offloadLogText').innerHTML = '';
};

if (videoCheckBox && videoBlock && video) {
  videoBlock.style.display = 'none';
  videoCheckBox.onchange = event => {
    if (event.currentTarget.checked) {
      videoBlock.style.display = 'block';
      video.srcObject = myStream;
    } else {
      videoBlock.style.display = 'none';
      video.srcObject = null;
    }
  };
}

if (statsCheckBox && statsInfo) {
  statsCheckBox.onchange = event => {
    if (event.currentTarget.checked) {
      if (statsIntervalId > 0) {
        return;
      }
      prevFramesSent = 0;
      statsIntervalId = setInterval(function () {
        if (!myStream || !myStream.peerConnection) {
          return;
        }
        myStream.peerConnection.getStats().then(stats => {
          let outboundCnt = 0;
          statsInfo.innerHTML = '';
          stats.forEach(stat => {
            if (!(stat.type === 'outbound-rtp' && stat.kind === 'video')) {
              return;
            }
            outboundCnt++;
            const statTrack = stats.get(stat.trackId);
            if (
              statTrack &&
              statTrack.trackIdentifier === myStream.getVideoTracks()[0].id
            ) {
              const statCodec = stats.get(stat.codecId);
              statsInfo.innerHTML += 'codec:' + statCodec.mimeType + ', ';
              statsInfo.innerHTML +=
                'size:' +
                statTrack.frameWidth +
                'x' +
                statTrack.frameHeight +
                ' ' +
                stat.framesPerSecond +
                'fps, ';
              statsInfo.innerHTML +=
                'framesSent:' +
                (statTrack.framesSent - prevFramesSent) +
                '/' +
                statTrack.framesSent +
                ', ';
              prevFramesSent = statTrack.framesSent;
            }
          });
          statsInfo.innerHTML =
            'rtp cnt:' + outboundCnt + ', ' + statsInfo.innerHTML;
        });
      }, 1000);
    } else {
      clearInterval(statsIntervalId);
      statsIntervalId = 0;
      statsInfo.innerHTML = '';
    }
  };
}

offloadWorker.on('connect', url => {
  console.log('onconnect : ' + url);
  const serverURL = document.getElementById('serverURL');
  serverURL.innerHTML = serverURL.value = url;
});

offloadWorker.on('stream', stream => {
  console.log('onstream : ' + stream);
  myStream = stream;
  if (video) {
    video.onresize = () => {
      const track = myStream.getVideoTracks()[0];
      statsInfo.innerHTML =
        `${track.getSettings().width}x${track.getSettings().height}` +
        ` ${track.getSettings().frameRate}fps`;
    };
    if (!videoCheckBox || videoCheckBox.checked) {
      video.srcObject = myStream;
    }
  }
});

const params = new URLSearchParams(document.location.search);
let serverUrl = params.get('server');
if (serverUrl === null && location.protocol !== 'file:') {
  serverUrl = localStorage.getItem('serverURL') || location.origin;
}
offloadWorker.connect(serverUrl);
