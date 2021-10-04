'use strict';

(function () {
  let coffConfigs;
  let offloadInfoMap = new Map();
  const ELAPSED_TIME_THRESHOLD_MS = 3000;

  loadConfigs();

  window.onload = function() {
    if (coffConfigs) {
      for (const func of coffConfigs.functions) {
        makeOffloadable(func.id);
      }
    } else {
      console.log("Config file has not been loaded. Offloading will not work.");
    }
  }

  function loadConfigs(callback) {
    try {
      let xobj = new XMLHttpRequest("application/json");
      // Use synchronous request in order to guarantee that
      // loading configs is completed before onload event.
      xobj.open('GET', 'coff_config.json', false);
      xobj.send(null);
      if (xobj.status == "200") {
        coffConfigs = JSON.parse(xobj.responseText);
      } else {
        console.log("Unable to load config file: " + xobj.status);
      }
    } catch (e) {
      console.log(e);
    }
  }

  function makeOffloadable(functionName) {
    // Find an object that is owned the function.
    const names = functionName.split('.');
    let object = window;
    for (let i = 0; i < names.length - 1; i++) {
      object = object[names[i]];
    }

    if (typeof object[names[names.length - 1]] === 'function') {
      const realName = names[names.length - 1];
      const offloadId = realName + "_" +
                        Math.random().toString(36).substr(2, 9);
      // Create information for offloading.
      offloadInfoMap[offloadId] = {
        funcObject : object[realName],
        elapsedTime : 0,
      };

      // Rewrite original function to make offloadable.
      const funcString = object[realName].toString();
      const params = funcString.slice(
                         funcString.indexOf('('), funcString.indexOf(')') + 1);
      console.log(funcString);
      console.log(params);
      eval(
        "object[realName] = function " + realName + params + " {\n" +
        "  let ret;\n" +
        "  const args = Array.prototype.slice.call(arguments);\n" +
        "  if (1 || offloadInfoMap[\"" + offloadId + "\"].elapsedTime" +
        " > ELAPSED_TIME_THRESHOLD_MS) {\n" +
        "    console.log(' here here here '); \n" +
        "    const t0 = performance.now();\n" +
        "    ret = offloadTo(\"" + offloadId + "\", args);\n" +
        "    const t1 = performance.now();\n" +
        "    console.log(\"elapsed time for " + realName + ": \" + (t1 - t0));\n" +
        "  } else {\n" +
        "    console.log(' here here '); \n" +
        "    const t0 = performance.now();\n" +
        "    ret = offloadInfoMap[\"" + offloadId + "\"].funcObject.\n" +
        "        apply(null, args);\n" +
        "    const t1 = performance.now();\n" +
        "    console.log(\"elapsed time for " + realName + ": \" + (t1 - t0));\n" +
        "    offloadInfoMap[\"" + offloadId + "\"].elapsedTime = t1 - t0;\n" +
        "  }\n" +
        "  return ret;\n" +
        "};");

      console.log("From now on, " + functionName + " is offloadable.");
    } else {
      console.log(functionName + "is not function object.");
    }
  }

  function offloadTo(offloadId, params) {
    console.log("Function " + offloadInfoMap[offloadId].funcObject.name +
                " will be offloaded with " + params.toString());
    try {
      let xobj = new XMLHttpRequest("application/json");
      // Use synchronous request because the function should be returned
      // synchronously.
      xobj.open("POST", "http://127.0.0.1:5080/api/js-offloading", false);

      let bodyData = new Object();
      bodyData.execStatement = offloadInfoMap[offloadId].funcObject.toString() +
                                "\n" + offloadInfoMap[offloadId].funcObject.name +
                                ".call(null" + (params.length > 0 ?
                                ", " + serialize(params) : "") + ");";
      bodyData.timeout = 3000;

      xobj.send(JSON.stringify(bodyData));

      let ret = undefined;
      if (xobj.status == "200") {
        let response = JSON.parse(xobj.responseText);
        ret = response.result;
      } else {
        console.log("Unable to offload " +
                    offloadInfoMap[offloadId].funcObject.name + ": " +
                    xobj.status);
      }
      console.log('result is ' + ret);
      return ret;
    } catch (e) {
      console.log(e);
      return 0;
    }
  }
}());
