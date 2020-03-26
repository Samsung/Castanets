#!/bin/bash

BASEDIR=$(dirname "$0")
cd $BASEDIR/../../
GCLIENTDIR=$(pwd)
printf "solutions = [\n {\n  \"url\": \"https://chromium.googlesource.com/chromium/src.git\",\n  \"managed\": False,\n  \"name\": \"src\",\n  \"deps_file\": \".DEPS.git\",\n  \"custom_deps\": {},\n },\n]\n" > .gclient
echo "$GCLIENTDIR/.gclient has been created."
