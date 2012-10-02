#!/usr/bin/env bash

# Save current working directory
PWD="$(pwd)"

# Compute the directory of this script
SOURCE="${BASH_SOURCE[0]}"
THISDIR="$(dirname "$SOURCE")"
while [ -h "$SOURCE" ]; do
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$THISDIR/$SOURCE"
    THISDIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
done
THISDIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Work out important directories based on the directory of this script
TOOLDIR="$THISDIR/../tools"
BINDIR="$TOOLDIR/bin"
PINTOS="$BINDIR/pintos"

THREADDIR="$THISDIR/../pintos/src/threads"
KERNELDIR="$THISDIR/../pintos/src/threads/build/"

TESTDIR="$THISDIR/../pintos/src/tests/threads"

# Build pintos if it's not built
if [ ! -d $KERNELDIR ]; then
    cd "$THISDIR/../"
    ./build-pintos.sh
    make -C $THREADDIR
fi

# Run all tests
for file in $(find $TESTDIR -name "*.ck"); do
    make -C $THREADDIR
    cd $KERNELDIR &&
    $PINTOS run $(basename $file .ck)
done

# Return to current working directory
cd "$PWD"
