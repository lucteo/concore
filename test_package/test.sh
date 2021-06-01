#!/bin/bash

set -ex

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))

mkdir -p ${CURDIR}/build
pushd ${CURDIR}/build

# Ensure that the concore package is created for demo/testing
conan create ${CURDIR}/.. demo/testing

# Run the test package
conan install .. --build=missing -s build_type=Release
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run the two generated executables
bin/concore_test_conan
bin/concore_test_cmake

popd buld