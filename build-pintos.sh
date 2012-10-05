#!/usr/bin/env bash

SOURCE="${BASH_SOURCE[0]}"
DIR="$( dirname "$SOURCE" )"
while [ -h "$SOURCE" ]
do 
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
  DIR="$( cd -P "$( dirname "$SOURCE"  )" && pwd )"
done
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

PINTOS_DIR="$DIR/pintos"
BOCHS_LOCATION="$DIR"
TOOLS_DIR="$DIR/tools"

echo "Making \$TOOLS_DIR:" $TOOLS_DIR
mkdir -p $TOOLS_DIR

echo "Building pintos and dependencies ..."
$PINTOS_DIR/src/misc/build-pintos-tools $PINTOS_DIR $TOOLS_DIR $BOCHS_LOCATION

echo "Exporting executable path ..."
PATH="$TOOLS_DIR/share":$PATH
PATH="$TOOLS_DIR/bin":$PATH
export PATH
