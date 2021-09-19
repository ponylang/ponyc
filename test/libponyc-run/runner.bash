#!/bin/bash

# shellcheck disable=SC2119,SC2120

# TODO: currently requires you to be in the directory with this script when run

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
    return 1
  fi

  return 0
}

exit_code=0

while IFS= read -r -d '' directory
do
  pushd "${directory}" || exit 1
  echo "Testing ${directory}"

  if [ -e Makefile ]
  then
    make program 1> /dev/null
    if check_build "${directory}";
    then
      popd || exit 1
      continue
    fi
  else
    ponyc --verbose 0 --bin-name program 1> /dev/null
    if check_build "${directory}";
    then
      popd || exit 1
      continue
    fi
  fi
  ./program
  if [ $? -ne 1 ]
  then
    echo "${directory}: FAILED"
    exit_code=1
  else
    echo "${directory}: PASSED"
  fi
  popd || exit 1
done < <(find ./ -maxdepth 1 -type d ! -iname ".*" -print0)

exit ${exit_code}
