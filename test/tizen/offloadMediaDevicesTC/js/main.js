var tc_pathnames = [];

function readAndParseXMLFile(filePath) {
  let xml = new XMLHttpRequest();
  xml.open("GET", filePath, false);
  xml.onreadystatechange = function() {
    if (xml.readyState === 4) {
      if (xml.status === 200 || xml.status === 0) {
        let responseText = xml.responseText;
        console.log("responseText: " + responseText);
        if (responseText) {
          var xmlDoc = new DOMParser().parseFromString(responseText, "text/xml");
          let testcases = xmlDoc.getElementsByTagName("testcase");
          for (let i = 0; i < testcases.length; i++) {
              tc_pathnames.push(testcases[i].getAttribute("filename"));
          }
        }
      }
    }
  }
  xml.send();
}

window.onload = function() {
  // add eventListener for tizenhwkey
  document.addEventListener('tizenhwkey', function(e) {
    if (e.keyName === "back") {
        try {
            tizen.application.getCurrentApplication().exit();
        } catch (ignore) {}
    }
  });

  readAndParseXMLFile("resources/tests.xml");

  let numOfTCs = tc_pathnames.length;
  let currentIdx = 0;
  let iframe = document.getElementsByTagName('iframe')[0];

  document.getElementById("testRun").addEventListener("click", function() {
    iframe.src = tc_pathnames[currentIdx];
  });
  document.getElementById("previousTest").addEventListener("click", function() {
    if (currentIdx <= 0) {
        alert("No previous tc.");
        return;
    }
    iframe.src = tc_pathnames[--currentIdx];
  });
  document.getElementById("nextTest").addEventListener("click", function() {
    if (currentIdx >= numOfTCs - 1) {
        alert("No next tc.");
        return;
    }
    iframe.src = tc_pathnames[++currentIdx];
  });
};
