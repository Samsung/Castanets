#!/bin/bash

export TARGET_TYPE=TIZEN_STANDARD_ARMV7L
export OS_TYPE=TIZEN

touch env.tmp
printf "TARGET_TYPE=%s\nOS_TYPE=%s\nexport TARGET_TYPE\nexport OS_TYPE" $TARGET_TYPE $OS_TYPE > env.tmp
mv env.tmp `dirname $0`/.env

gbs -c `dirname $0`/gbs.conf build -A armv7l -P tz_standard "$@" --include-all
