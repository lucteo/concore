#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "Clang compilation"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} -e INPUT_CC='clang' -e INPUT_CONANFLAGS='-s compiler.libcxx=libstdc++11' -e INPUT_CHECKS='build test install' action-cxx-toolkit
status=$?
printStatus $status