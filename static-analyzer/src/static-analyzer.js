'use strict';
const fs = require('fs');
const path = require('path');
const program = require('commander');
const _forEach = require('lodash').forEach;
const utils = require('./utils');
const core = require('./core');
const outputValidator = require('./output-validator');

const { writeDataToFile } = utils;
const { getData, isOffloadableFunc } = core;

program
  .arguments('<src_file...>')
  .option('-a, --ast', 'output AST to file')
  .option('-t, --test <test_file>', 'test output with a file')
  .option('-o, --out <dir_path>', 'set output directory', './out')
  .parse();

if (program.out) {
  program.out = path.resolve(program.out);
  if (!fs.existsSync(program.out)) {
    fs.mkdirSync(program.out, { recursive: true });
  }
}

let offloadConfigs = {
  files: [],
  data: {}
};

new Promise(function (resolve, reject) {
  try {
    program.args.forEach(function (f) {
      const currentPath = path.resolve(f);
      const stats = fs.statSync(currentPath);
      if (stats.isFile() && path.extname(currentPath) === ".js") {
        const data = fs.readFileSync(currentPath, { encoding: 'utf8', flag: 'r' });
        offloadConfigs.files.push(currentPath);
        offloadConfigs.data[currentPath] = getData(currentPath, data)
      } else if (stats.isDirectory()) {
        const files = fs.readdirSync(currentPath);
        _forEach(files, (file) => {
          if (path.extname(file) === ".js") {
            const exactPath = path.join(currentPath, file);
            const data = fs.readFileSync(exactPath, { encoding: 'utf8', flag: 'r' });
            offloadConfigs.files.push(exactPath);
            offloadConfigs.data[exactPath] = getData(exactPath, data)
          }
        });
      }
    });
    resolve();
  } catch (e) {
    console.log(e);
    reject();
  }
}).then(function () {
  writeDataToFile(offloadConfigs, 'offload-debug.json', true);

  outputValidator.initialize();

  let globalOffloadConfig = {
    functions: [],
    globals: []
  };

  _forEach(offloadConfigs.files, (currentFile) => {
    const currentData = offloadConfigs.data[currentFile];
    let offloadConfig = {
      functions: [],
      globals: currentData.globals
    };

    globalOffloadConfig.globals = globalOffloadConfig.globals.concat(currentData.globals);

    for (let funcIdx in currentData.functions) {
      let funcObj = {};
      Object.assign(funcObj, currentData.functions[funcIdx]);

      delete funcObj.bindings;
      delete funcObj.references;
      delete funcObj.localVars;

      funcObj.testingParams = currentData.functions[funcIdx].paramsCount > 0 ? [] : null;

      if (isOffloadableFunc(currentData, funcIdx)) {
        offloadConfig.functions.push(funcObj);
        globalOffloadConfig.functions.push(funcObj);
      }
    }

    writeDataToFile(offloadConfig, path.basename(currentFile, ".js") + '-offload-config.json', true)

    const testFilePath = program.test || currentFile + "on";
    if (fs.existsSync(testFilePath)) {
      outputValidator.validate(testFilePath, offloadConfig, currentFile);
    }
  });

  writeDataToFile(globalOffloadConfig, 'coff_config.json', true);

  outputValidator.showResults();
});
