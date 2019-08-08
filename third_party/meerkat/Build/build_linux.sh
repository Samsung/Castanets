#!/bin/bash

export TARGET_TYPE=X64
export OS_TYPE=LINUX

touch env.tmp
printf "TARGET_TYPE=%s\nOS_TYPE=%s\nexport TARGET_TYPE\nexport OS_TYPE" $TARGET_TYPE $OS_TYPE > env.tmp
mv env.tmp `dirname $0`/.env

make -C `dirname $0` "$@"
