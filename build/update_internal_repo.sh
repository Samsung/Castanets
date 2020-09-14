#!/bin/bash

[ -d third_party/offload-js ] || git clone git@github.sec.samsung.net:HighPerformanceWeb/offload.js.git third_party/offload-js
git -C third_party/offload-js pull
