# static-analyzer.js
Static Analyzer for JS Offloading. This script is used to analyze the given Javascript file(s) and generate a list of all the entities that needs monitoring to get overloaded in the form of JSON.

The main purpose of this script are as follows:
 * To generate a configuration file of all the entities that can be further monitor to overload.
 * To validate the output config file against the expected config file.

```
offload.js/static-analyzer/src/
├── core.js
├── function-tree-schema.json
├── json-schema-validator.js
├── output-validator.js
├── static-analyzer.js
├── test
└── utils.js
```

Currently, the following types of functions cannot be offload
* Has a direct or indirect dependency on browser APIs, for example `console`, `window`, `document`, `LocalStorage`, etc. The static-analyser cannot currently catch all such dependencies and the developer has to be aware of this limitation. In the example below, `foo` cannot be offloaded for the above reason.
    ```js
    function foo(){
        console.log('Hello World!')
    }
    ```
* Functions that call other function or have nested functions. In the examples below, `foo` cannot be offloaded for the above reason.
    ```js
    function foo(){
        function bar(){
        }
    }
    ```
    ```js
    function bar(){
    }
    function foo(){
        bar();
    }
    ```
* Functions that depend on variables declared in outer functions. In the example below, `bar` cannot be offloaded for the above reason.
    ```js
    function foo(){
        let outer;
        function bar(){
            let inner;
            return inner + outer;
        }
    }
    ```

## How to run
### static-analyzer.js
1. Install the dependent node modules
```sh
static-analyzer$ npm install
```

2. Run Static Analyzer script
 * Run static analyzer
```sh
static-analyzer$ node src/static-analyzer.js <JS files... or Directory containing all JS files...>

'-a, --ast', 'output AST to file'
'-t, --test <test_file>', 'test output with a file'
'-o, --out <dir_path>', 'set output directory', './out'
```

Note: If the directory contains expected JSON file (<same_name>.json)  then it will automatically validate output against it.

* Run static analyzer with expected output JSON file in another directory
```sh
static-analyzer$ node src/static-analyzer.js <JS file...> -t <Expected JSON output... >
```

### json-schema-validator.js
```sh
static-analyzer$ node src/json-schema-validator.js <JSON schema> <JSON data>
```

## Example
Source code :
<pre>
var myglobal = "My Global";
function myfunc(p) {
  return p + " " + myglobal;
}

console.log(myfunc("Test"));
</pre>

Internal analysis output :
<pre>
{
  "files": [
    {
      "id": "test/global_var_test.js",
      "bindings": [
        {
          "id": "myglobal",
          "kind": "var",
          "type": "VariableDeclarator",
          "references": 1
        },
        {
          "id": "myfunc",
          "kind": "hoisted",
          "type": "FunctionDeclaration",
          "references": 1
        }
      ],
      "functions": [
        {
          "id": "myfunc",
          "bindings": [
            {
              "id": "p",
              "kind": "param",
              "type": "Identifier",
              "references": 1
            }
          ],
          "references": [
            "myglobal"
          ]
        }
      ]
    }
  ]
}
</pre>
