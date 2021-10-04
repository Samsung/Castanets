
function getServerIP() {
  const reqAppControl = tizen.application.getCurrentApplication().getRequestedAppControl();
  let serverIP = null;
  if (reqAppControl) {
    const retData = reqAppControl.appControl.data;
    for (let i = 0; i < retData.length; i++) {
      const data = retData[i];
      if (data.key === "http://tizen.org/appcontrol/data/text") {
        serverIP = String(data.value);
        serverIP = serverIP.substring(serverIP.indexOf('signaling-server') + 17);
        break;
      }
    }
  }
  return serverIP;
}

window.onload = function () {
  const serverIP = getServerIP();
  if (serverIP) {
    document.getElementById("serverURL").value = serverIP;
    offloadWorker.connect(serverIP);
  }

  // add eventListener for tizenhwkey
  document.addEventListener('tizenhwkey', function (e) {
    if (e.keyName == "back")
      try {
        tizen.application.getCurrentApplication().hide();
      } catch (ignore) {}
  });

};

window.addEventListener('appcontrol', function () {
  const serverIP = getServerIP();
  if (serverIP) {
    document.getElementById("serverURL").value = serverIP;
    offloadWorker.connect(serverIP);
  }
});
