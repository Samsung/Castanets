#!/bin/bash
function clean() {
  rm -f dist/*.apk
}

function build_app() {
  pushd $1
  ./gradlew build
  popd
  find $1 -name "*.apk" -exec cp {} $2/ \;
}

clean
build_app offload-worker/android dist
