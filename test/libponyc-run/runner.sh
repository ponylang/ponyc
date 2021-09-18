#!/bin/bash

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd "$@" > /dev/null
}

exit_code=0

for directory in $(find ./ -maxdepth 1 -type d ! -iname ".*")
do
  pushd ${directory}
  if [ -e Makefile ]
  then
    make program > /dev/null 2>&1
  else
    ponyc --bin-name program > /dev/null 2>&1
  fi
  ./program
  if [ $? -ne 1 ]; then
    echo "${directory}: FAILED"
    exit_code=1
  else
    echo "${directory}: PASSED"
  fi
  popd
done

exit ${exit_code}
