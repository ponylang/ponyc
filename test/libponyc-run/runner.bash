#!/bin/bash

# shellcheck disable=SC2119,SC2120

# TODO: use correct ponyc build from our local

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

exit_code=0

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
  ./program
  if [ $? -ne 1 ]
  then
    echo "${under_test}: FAILED"
    exit_code=1
  else
    echo "${under_test}: PASSED"
  fi
  popd || exit 1
done < <(find "${dir_im_in}"/* -maxdepth 1 -type d ! -iname ".*" -print0)

exit ${exit_code}
