#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "GCC compilation with concepts"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} -e INPUT_CC='gcc' -e INPUT_CHECKS='build test' -e INPUT_CMAKEFLAGS='-Dconcore.use2020=yes' action-cxx-toolkit
status=$?
printStatus $status
