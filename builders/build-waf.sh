#!/bin/bash
set -e

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)       PKG_NAME="$2";    shift 2 ;;
        --version)    PKG_VER="$2";     shift 2 ;;
        --srcdir)     SRC_DIR="$2";     shift 2 ;;
        --destdir)    DESTDIR="$2";     shift 2 ;;
        --prefix)     PREFIX="$2";      shift 2 ;;
        --store)      HSPM_STORE="$2";  shift 2 ;;
        --extra-args) EXTRA_ARGS="$2";  shift 2 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

: "${PKG_NAME:?--name is required}"
: "${DESTDIR:?--destdir is required}"
: "${PREFIX:?--prefix is required}"
: "${SRC_DIR:?--srcdir is required}"

echo "[build-waf] Building $PKG_NAME into $DESTDIR"

cd "$SRC_DIR"

./waf configure \
    --prefix="$PREFIX" \
    $EXTRA_ARGS

./waf build -j"$(nproc)"
./waf install --destdir="$DESTDIR"

echo "MANIFEST_READY"
