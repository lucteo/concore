#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

set -e

${CURDIR}/test-gcc.sh
${CURDIR}/test-clang.sh
${CURDIR}/test-clang-7.sh
${CURDIR}/test-static-checks.sh
# ${CURDIR}/test-clang-format.sh
# ${CURDIR}/test-sanitizer.sh

echo
echo -e "${LIGHTGREEN}All tests passed successfully.${CLEAR}"
echo
