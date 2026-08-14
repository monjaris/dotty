#!/usr/bin/env sh

cd "$(dirname "$0")" || exit 1

if ./device-online.sh; then
    git submodule update --init --remote
fi
