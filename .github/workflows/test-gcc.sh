#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "GCC compilation"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} -e INPUT_CC='gcc' -e INPUT_CHECKS='build test install warnings' action-cxx-toolkit
status=$?
printStatus $status
