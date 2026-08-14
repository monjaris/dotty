#!/usr/bin/env sh

cd "$(dirname "$0")" || exit 1

# internet connected     -> exit 1
# internet not connected -> exit 0

if ./cmd-valid.sh nc; then
    nc -z -w 3 1.1.1.1 53 >/dev/null 2>&1 && exit 1
    nc -z -w 3 8.8.8.8 53 >/dev/null 2>&1 && exit 1
fi

# Fallback to ping just in case
ping -c 1 1.1.1.1 >/dev/null 2>&1 && exit 1
ping -c 1 8.8.8.8 >/dev/null 2>&1 && exit 1

exit 0
