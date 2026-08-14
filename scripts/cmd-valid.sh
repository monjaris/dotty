#!/usr/bin/env sh

# command available -> exit 0
# command missing   -> exit 1

if command -v "$1" >/dev/null 2>&1; then
    exit 0
fi

exit 1
