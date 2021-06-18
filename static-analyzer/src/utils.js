'use strict';
const fs = require('fs');
const path = require('path');
const program = require('commander');

const writeDataToFile = (data, fileName, isJSON = false) => {
  try {
    let stringifyData = data;
    const absoluteFilePath = path.resolve(program.out, fileName);
    if (isJSON) {
      stringifyData = JSON.stringify(data, undefined, 2);
    }
    fs.writeFile(absoluteFilePath, stringifyData + "\n", function (err) {
      if (err) {
        console.error(`${fileName} could not be saved.`);
        throw err;
      }
      console.log(`${fileName} has been saved.`);
    });
  } catch (err) {
    console.log(err);
  }
};

module.exports = {
  writeDataToFile
};
