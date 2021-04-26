# Chrome Android Dependency Analysis Tool
## Overview
As part of Chrome Modularization, this directory contains various tools for
analyzing the dependencies contained within the Chrome Android project.

## Usage
Start by generating a JSON dependency file with a snapshot of the dependencies 
for your JAR using the **JSON dependency generator** command-line tool.

This snapshot file can then be used as input for various other 
analysis tools listed below.

## Command-line tools
The usage information for any of the following tools is also accessible via 
`toolname -h` or `toolname --help`.
#### JSON Dependency Generator
Runs [jdeps](https://docs.oracle.com/javase/8/docs/technotes/tools/unix/jdeps.html) 
on a given JAR and writes the resulting dependency graph into a JSON file.
```
usage: generate_json_dependency_graph.py [-h] -t TARGET -o OUTPUT [-j JDEPS_PATH]

optional arguments:
  -j JDEPS_PATH, --jdeps-path JDEPS_PATH
                        Path to the jdeps executable.

required arguments:
  -t TARGET, --target TARGET
                        Path to the JAR file to run jdeps on.
  -o OUTPUT, --output OUTPUT
                        Path to the file to write JSON output to. Will be
                        created if it does not yet exist and overwrite
                        existing content if it does.
```
#### Class Dependency Audit
Given a JSON dependency graph, output the class-level dependencies for a given
class.
```
usage: print_class_dependencies.py [-h] -f FILE -c CLASS_NAME

required arguments:
  -f FILE, --file FILE  Path to the JSON file containing the dependency graph.
                        See the README on how to generate this file.
  -c CLASS_NAME, --class CLASS_NAME
                        Case-insensitive name of the class to print
                        dependencies for. Matches names of the form ...input,
                        for example `apphooks` matches
                        `org.chromium.browser.AppHooks`.
```
#### Package Dependency Audit
Given a JSON dependency graph, output the package-level dependencies for a
given package and the class dependencies comprising those dependencies.
```
usage: print_package_dependencies.py [-h] -f FILE -p PACKAGE

required arguments:
  -f FILE, --file FILE  Path to the JSON file containing the dependency graph.
                        See the README on how to generate this file.
  -p PACKAGE, --package PACKAGE
                        Case-insensitive name of the package to print
                        dependencies for. Matches names of the form ...input,
                        for example `browser` matches `org.chromium.browser`.
```

## Example Usage
This Linux example assumes Chromium is contained in a directory `~/cr` 
and that Chromium has been built as per the instructions 
[here](https://chromium.googlesource.com/chromium/src/+/master/docs/linux/build_instructions.md),
although the only things these assumptions affect are the file paths.
```
cd ~/cr/src/tools/android/dependency_analysis

./generate_json_dependency_graph.py --target ~/cr/src/out/Default/obj/chrome/android/chrome_java__process_prebuilt.desugar.jar --output ./json_graph.txt
>>> Running jdeps and parsing output...
>>> Parsed class-level dependency graph, got 3239 nodes and 19272 edges.
>>> Created package-level dependency graph, got 500 nodes and 4954 edges.
>>> Dumping JSON representation to ./json_graph.txt.

./print_class_dependencies.py --file ./json_graph.txt --class apphooks
>>> Printing class dependencies for org.chromium.chrome.browser.AppHooks:
>>> 35 inbound dependency(ies) for org.chromium.chrome.browser.AppHooks:
>>> 	org.chromium.chrome.browser.AppHooksImpl
>>> 	org.chromium.chrome.browser.ChromeActivity
>>> ...

./print_package_dependencies.py --file ./json_graph.txt --package chrome.browser
>>> Printing package dependencies for org.chromium.chrome.browser:
>>> 121 inbound dependency(ies) for org.chromium.chrome.browser:
>>> 	org.chromium.chrome.browser.about_settings -> org.chromium.chrome.browser
>>> 	1 class edge(s) comprising the dependency:
>>> 		AboutChromeSettings -> ChromeVersionInfo
>>> ...
```