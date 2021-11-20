#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "GCC compilation with concepts"

# Create a temporary directory to store results between runs
BUILDDIR="build/gh-checks/gcc-concepts/"
mkdir -p "${CURDIR}/../../${BUILDDIR}"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_PREBUILD_COMMAND="conan config set storage.path=/github/workspace/${BUILDDIR}/.conan" \
    -e INPUT_CONANFLAGS="-e CONAN_RUN_TESTS=1" \
    -e INPUT_MAKEFLAGS='-j 4' \
    -e INPUT_CC='gcc' -e INPUT_CHECKS='build test' -e INPUT_CMAKEFLAGS='-Dconcore.use2020=yes' \
    $IMAGENAME
status=$?
printStatus $status
