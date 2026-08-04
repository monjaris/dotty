#!/usr/bin/env sh

cd "$(dirname "$0")" || exit 1


RAW_PLAT=$(uname -s | tr '[:upper:]' '[:lower:]')
case "$RAW_PLAT" in
    *bsd*) PLAT="bsd" ;;
    *)     PLAT="$RAW_PLAT" ;;
esac

VERBOSE=0
DEBUG_BIN="./build/${PLAT}/x86_64/debug/dotty"
RELEASE_BIN="./build/${PLAT}/x86_64/release/dotty"
COPY_BIN=""


# set submodules up
git submodule update --init --remote


# portable nproc
if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=1
fi



# either debug or release
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


xmake build -j"$JOBS" ${VERBOSE:+-v} dotty
cp "$COPY_BIN" ./dotty
# ./dotty "$@"
