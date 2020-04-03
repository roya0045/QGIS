#!/bin/bash

# Location of current script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

docker  run  \
    -v $(pwd):$(pwd) \
    -w $(pwd) --rm  \
    --user $(id -u):$(id -g) \
    qgis/qgis3-build-deps \
    ${DIR}/build-mxe.sh
