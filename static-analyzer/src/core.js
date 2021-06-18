'use strict';
const parser = require('@babel/parser');
const path = require('path');
const program = require('commander');
const traverse = require('@babel/traverse').default;
const _forEach = require('lodash').forEach;
const utils = require('./utils');

const { writeDataToFile } = utils;
const PROHIBITTED_GLOBALS = [
  'document',
  'window',
  'console'
];
let typeMap = {};
let isFnAsync = {};

function getData(file, code) {
  const ast = parser.parse(code);
  let config = {
    id: file,
    bindings: [],
    functions: [],
    globals: []
  };

  // output AST to file
  if (program.ast) {
    writeDataToFile(ast, path.basename(file).split(".").shift() + '-ast.json', true);
  }

  traverse(ast, {
    Program: function (path) {
      // Collect bindings in program scope as global
      for (let id in path.scope.bindings) {
        let glob = {
          id: id,
          kind: path.scope.bindings[id].kind,
          type: path.scope.bindings[id].path.type,
          refCount: path.scope.bindings[id].references
        }
        config.bindings.push(glob);
        if (glob.type === "VariableDeclarator")
          config.globals.push(glob.id);
        typeMap[glob.id] = glob.type;
      }

      // Function
      path.traverse({
        FunctionDeclaration: function (funcPath) {
          isFnAsync[funcPath.node.id.name] = funcPath.node.async;
          let func = {
            id: funcPath.node.id.name,
            bindings: [],
            paramsCount: 0,
            params: [],
            references: [],
            deps: [],
            asyncDeps: [],
            localVars: [],
            globalVars: [],
            isAsync: funcPath.node.async,
          };
          for (let id in funcPath.scope.bindings) {
            let local = {
              id: id,
              kind: funcPath.scope.bindings[id].kind,
              type: funcPath.scope.bindings[id].path.type,
              refCount: funcPath.scope.bindings[id].references
            }
            typeMap[local.id] = local.type;
            func.bindings.push(local);
          }

          let paramCount = 0;
          const params = [];
          for (let id in func.bindings) {
            if (func.bindings[id].type === 'Identifier') {
              paramCount++;
              params.push(func.bindings[id].id);
            }
          }
          func.paramsCount = paramCount;
          func.params = params;
          funcPath.traverse({
            Identifier(innerPath) {
              const innerPathNodeName = innerPath.node.name;
              if (innerPathNodeName === func.id)
                return;
              for (let idx in func.bindings)
                if (innerPathNodeName === func.bindings[idx].id)
                  return;
              for (let idx in func.references)
                if (innerPathNodeName === func.references[idx].id)
                  return;
              if (func.references.indexOf(innerPathNodeName) == -1)
                func.references.push(innerPathNodeName);
            },
            VariableDeclarator: function (varPath) {
              varPath.traverse({
                Identifier(innerPath) {
                  const innerPathNodeName = innerPath.node.name;
                  if (!func.params.includes(innerPathNodeName))
                    func.localVars.push(innerPathNodeName);
                }
              })
            },
            FunctionDeclaration(innerPath) {
              innerPath.skip();
            }
          });
          const newLocalVars = [];
          for (let x of func.localVars) {
            if (!func.references.includes(x) && !newLocalVars.includes(x))
              newLocalVars.push(x);
          }
          func.localVars = newLocalVars;
          for (let x of func.references) {
            if (config.globals.includes(x))
              func.globalVars.push(x);
          }
          config.functions.push(func);
        }
      });
    },
  });

  // Fill up the dependencies array.
  _forEach(config.functions, func => {
    _forEach(func.references, reference => {
      if (typeMap[reference] === "FunctionDeclaration") {
        const index = config.functions.findIndex(obj => obj.id === reference);
        if (isFnAsync[func.id])
          config.functions[index]['asyncDeps'].push(func.id);
        else
          config.functions[index]['deps'].push(func.id);
      }
    });
  });
  return config;
};

function isOffloadableFunc(currentData, funcIdx) {
  let isOffloadableFunc = true;
  const currentFunction = currentData.functions[funcIdx];
  for (let refIdx in currentFunction.references) {
    const currentRef = currentFunction.references[refIdx];
    if (typeMap[currentRef] === 'FunctionDeclaration' || PROHIBITTED_GLOBALS.indexOf(currentRef) !== -1)
      isOffloadableFunc = false;
  }
  if (isOffloadableFunc)
    for (let bindIdx in currentFunction.bindings) {
      const currentBinding = currentFunction.bindings[bindIdx];
      if (currentBinding.type === 'FunctionDeclaration')
        isOffloadableFunc = false;
    }
  return isOffloadableFunc;
}

module.exports = {
  getData,
  isOffloadableFunc
};
