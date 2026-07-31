#!/usr/bin/env sh

cd "$(dirname "$0")" || exit 1

verbose=0
debug_bin="./build/linux/x86_64/debug/dotty"
release_bin="./build/linux/x86_64/release/dotty"
copy_bin=""


# set submodules up
if ping -c1 cloudfare.com >/dev/null 2>&1; then
    git submodule update --init

    cd "deps/dotline"
    git fetch --quiet

    if git status -uno | grep -q "behind"; then
        cd ../..
        git submodule update --init --remote "deps/dotline"
    else
        cd ../..
    fi
fi



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
    xmake config --mode=debug
    xmake config --toolchain=dotty.llvm
    copy_bin="$debug_bin"
    verbose="."
    shift
else
    [ "$(xmake config --show 2>/dev/null | grep mode)" != *release* ] && \
    xmake config --mode=release
    xmake config --toolchain=dotty.gnu
    copy_bin="$release_bin"
    verbose=""
fi


xmake build -j"$JOBS" ${verbose:+-v} dotty
cp "$copy_bin" ./dotty
# ./dotty "$@"
