#!/bin/bash
function build_web() {
  rm -rf $1/.buildResult
  tizen build-web -- $1/
  tizen package -t wgt -- $1/.buildResult/
  for f in $1/.buildResult/*.wgt; do
    cp "$f" "$2/$(basename ${f//[[:space:]]/})"
  done
}

function build_native() {
  rm -rf $1/Debug
  tizen build-native -a arm -- $1/
  tizen package -t tpk -- $1/Debug/
  for f in $1/Debug/*.tpk; do
    cp "$f" "$2/$(basename ${f//[[:space:]]/})"
  done
}

function build_app() {
  case $1 in
    server)
      PATH_SRC=offload-server/tizen
      mkdir -p $PATH_SRC/service/gen/
      cp -r offload-server/src/* $PATH_SRC/service/gen/
      openssl req -newkey rsa:2048 -nodes -x509 -days 365 -subj "/CN=localhost" \
          -keyout $PATH_SRC/service/gen/key.pem \
          -out $PATH_SRC/service/gen/cert.pem
      build_web $PATH_SRC dist
    ;;
    worker)
      PATH_SRC=offload-worker/tizen
      build_web $PATH_SRC dist
    ;;
    sample)
      PATH_SRC=sample/tizen/roffdemo
      build_web $PATH_SRC dist
      PATH_SRC=sample/src/getusermedia
      build_web $PATH_SRC dist
    ;;
    test)
      PATH_SRC=test/tizen/offloadMediaDevicesTC
      build_web $PATH_SRC dist
      PATH_SRC=test/tizen/offloadTizenPPMTC
      build_web $PATH_SRC dist
      PATH_SRC=test/tizen/offloadTizenHAMTC
      build_web $PATH_SRC dist
  esac
}

ARGS=$@
if [ "$ARGS" == "" ] || [ "$ARGS" == "all" ]; then
  ARGS=(server worker sample test)
fi

for i in ${ARGS[@]};
do
  build_app $i
done
