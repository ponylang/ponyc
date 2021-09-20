#!/bin/bash

# shellcheck disable=SC2059,SC2119,SC2120,SC2181

# TODO: debug and release builds?

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd "$@" > /dev/null
}

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

check_build () {
  if [ $? -ne 0 ];
  then
    printf "${1}: ${RED}FAILED${NC}\n"
    return 0
  fi

  return 1
}

CC="${CC:-cc}"
CXX="${CXX:-c++}"

tests_failed=0
tests_run=0

path_to_me=$(realpath "$0")
dir_im_in=$(dirname "$path_to_me")

while IFS= read -r -d '' directory
do
  pushd "${directory}" || exit 1
  under_test=$(basename "${directory}")
  printf "${YELLOW}TESTING${NC} ${under_test}\n"

  if [ -e Makefile ]
  then
    make program 1> /dev/null
    if check_build "${under_test}";
    then
      tests_failed=$((tests_failed + 1))
      popd || exit 1
      continue
    fi
  else
    ponyc --verbose 0 --bin-name program 1> /dev/null
    if check_build "${under_test}";
    then
      tests_failed=$((tests_failed + 1))
      popd || exit 1
      continue
    fi
  fi

  expected_exit_code=1
  if [ -e expected-exit-code.txt ]
  then
    expected_exit_code=$(<expected-exit-code.txt)
  fi

  tests_run=$((tests_run + 1))
  ./program
  if [[ $? -ne expected_exit_code ]]
  then
    printf "${under_test}: ${RED}FAILED${NC}\n"
    tests_failed=$((tests_failed + 1))
  else
    printf "${under_test}: ${GREEN}PASSED${NC}\n"
  fi

  popd || exit 1
done < <(find "${dir_im_in}"/* -maxdepth 1 -type d ! -iname ".*" -print0)

if [[ ${tests_failed} -ne 0 ]]
then
  printf "${tests_failed} of ${tests_run} tests ${RED}FAILED${NC}\n"
else
  printf "All ${tests_run} tests ${GREEN}PASSED${NC}\n"
fi

exit ${tests_failed}
