#!/bin/bash

# shellcheck disable=SC2119,SC2120,SC2181

# TODO: debug and release builds?
# TODO: colorize output

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd "$@" > /dev/null
}

check_build () {
  if [ $? -ne 0 ];
  then
    echo "${1}: FAILED"
    return 0
  fi

  return 1
}

tests_failed=0
tests_run=0

path_to_me=$(realpath "$0")
dir_im_in=$(dirname "$path_to_me")

while IFS= read -r -d '' directory
do
  pushd "${directory}" || exit 1
  under_test=$(basename "${directory}")
  echo "Testing ${under_test}"

  if [ -e Makefile ]
  then
    make program 1> /dev/null
    if check_build "${under_test}";
    then
      popd || exit 1
      continue
    fi
  else
    ponyc --verbose 0 --bin-name program 1> /dev/null
    if check_build "${under_test}";
    then      popd || exit 1
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
    echo "${under_test}: FAILED"
    tests_failed=$((tests_failed + 1))
  else
    echo "${under_test}: PASSED"
  fi

  popd || exit 1
done < <(find "${dir_im_in}"/* -maxdepth 1 -type d ! -iname ".*" -print0)

if [ ${tests_failed} -ne 0 ]
then
  echo "${tests_failed} of ${tests_run} tests FAILED"
else
  echo "All ${tests_run} tests PASSED"
fi

exit ${tests_failed}
