#!/bin/bash

export TARGET_TYPE=TIZEN_TV_PRODUCT
export OS_TYPE=TIZEN
export BUILD_DIR=$(readlink -e $(dirname $0))

if [ ! -d "$BUILD_DIR/../../packaging" ]; then
  ln -s $BUILD_DIR/../packaging $BUILD_DIR/../../packaging
fi

touch env.tmp
printf "TARGET_TYPE=%s\nOS_TYPE=%s\nexport TARGET_TYPE\nexport OS_TYPE" $TARGET_TYPE $OS_TYPE > env.tmp
mv env.tmp $BUILD_DIR/.env

gbs -c $BUILD_DIR/gbs.conf build -A armv7l -P tztv_5.5_arm-musem --incremental --include-all "$@"
