'use strict';
var Ajv = require('ajv');
var path = require('path');
var program = require('commander');

program
    .arguments('<schema-JSON> <data-JSON>')
    .parse();

var ajv = new Ajv();
var validate = ajv.compile(require(path.resolve(program.args[0])));
var valid = validate(require(path.resolve(program.args[1])));
if (!valid)
    console.log(validate.errors);