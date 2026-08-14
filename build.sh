#!/usr/bin/env sh

cd "$(dirname "$0")" || exit 1

run-script () { script="$1"; shift; ./scripts/"$script" "$@"; }

PLAT=$(run-script platform-name.sh)
DEBUG_BIN="./build/${PLAT}/x86_64/debug/dotty"
RELEASE_BIN="./build/${PLAT}/x86_64/release/dotty"
run-script update-submodules.sh


# run make if xmake doesn't exist
if ! run-script cmd-valid.sh xmake; then
    make
fi


# build with dev profile if the first argument is "dev"
if [ "$1" = "dev" ]; then
    xmake config --mode=debug --toolchain=dotty.llvm
    COPY_BIN="$DEBUG_BIN"
    VERBOSE="."
    shift
else
    xmake config --mode=release --toolchain=dotty.gnu
    COPY_BIN="$RELEASE_BIN"
    VERBOSE=""
fi


JOBS=$(run-script max-jobs.sh)
xmake build -j"$JOBS" ${VERBOSE:+-v} dotty
cp "$COPY_BIN" ./dotty
# ./dotty "$@"
