'use strict';
const chalk = require('chalk');
const path = require('path');
const jsonDiff = require('json-diff');
const Table = require('cli-table');
const _forEach = require('lodash').forEach;
const utils = require('./utils');

const {writeDataToFile} = utils;

let testStats = {}, tableData = [];

const initialize = () => {
  testStats = {
    index: 0,
    pass: 0,
    fail: 0
  };

  tableData = [[]];
};

const validate = (testFilePath, offloadConfig, currentFile) => {
  testStats.index++;
  const diff = getDifference(testFilePath, offloadConfig);
  if (diff) {
    testStats.fail++;
    tableData.push([testStats.index, path.basename(currentFile), path.basename(testFilePath), chalk.red("Fail")]);
  }
  else {
    testStats.pass++;
    tableData.push([testStats.index, path.basename(currentFile), path.basename(testFilePath), chalk.green("Pass")]);
  }
  writeDataToFile(diff, path.basename(currentFile, ".js") + '-offload-diff.txt', true);
};

const showResults = () => {
  tableData.push([], ["", "Total Tests: " + Number(testStats.index), chalk.red("Failed Tests: " + Number(testStats.fail)), chalk.green("Passed Tests: " + Number(testStats.pass))]);

  if (Number(testStats.index) > 0)
    generateConsoleTable(tableData);
}

const getDifference = (expectedOutputFile, outputJson) => {
  let expectedJson = {};
  try {
    expectedJson = require(path.resolve(expectedOutputFile));
  } catch (e) {
    console.log(e);
  }
  return jsonDiff.diff(expectedJson, outputJson);
};

const generateConsoleTable = (data) => {
  const table = new Table({
    head: ['S.no.', 'File', 'Expected JSON File', 'Result'],
    style: {
      head: ['yellow'],
      border: ['white']
    }
  });
  data.shift();
  _forEach(data, (tableRow => table.push(tableRow)));
  console.log(table.toString());
};

module.exports = {
  initialize,
  validate,
  showResults
};
