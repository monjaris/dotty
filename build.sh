#!/usr/bin/env sh

cd "$(dirname "$0")" || exit 1

verbose=0
debug_bin="./build/linux/x86_64/debug/dotty"
release_bin="./build/linux/x86_64/release/dotty"
copy_bin=""


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
    copy_bin="$debug_bin"
    verbose="."
    shift
else
    xmake config --mode=release --toolchain=dotty.gnu
    copy_bin="$release_bin"
    verbose=""
fi


xmake build -j"$JOBS" ${verbose:+-v} dotty
cp "$copy_bin" ./dotty
# ./dotty "$@"
