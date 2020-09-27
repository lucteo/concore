#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "Run the tests with sanitizer"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} -e INPUT_CHECKS='sanitize=address sanitize=undefined' action-cxx-toolkit
status=$?
printStatus $status
