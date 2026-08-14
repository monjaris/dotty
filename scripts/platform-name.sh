#!/usr/bin/env sh

RAW_PLAT=$(uname -s | tr '[:upper:]' '[:lower:]')
PLAT=""

case "$RAW_PLAT" in
    *bsd*) PLAT="bsd" ;;
    *)     PLAT="$RAW_PLAT" ;;
esac

echo "$PLAT"
