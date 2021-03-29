#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "Static checks test"

SRCFILES=""

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} -e INPUT_CC='gcc-9' -e INPUT_CHECKS='cppcheck clang-tidy' \
    -e INPUT_CPPCHECKFLAGS='--enable=warning,style,performance,portability --inline-suppr' \
    -e INPUT_CLANGTIDYFLAGS="-quiet ${SRCFILES} -j 4" \
    action-cxx-toolkit
status=$?
printStatus $status
