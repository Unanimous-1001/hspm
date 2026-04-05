#!/bin/bash
set -e

# --- Parse arguments ---
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

# --- Validate required args ---
: "${PKG_NAME:?--name is required}"
: "${DESTDIR:?--destdir is required}"
: "${PREFIX:?--prefix is required}"
: "${SRC_DIR:?--srcdir is required}"

echo "[build-autotools] Building $PKG_NAME into $DESTDIR"

cd "$SRC_DIR"

# --- Configure ---
./configure \
    --prefix="$PREFIX" \
    $EXTRA_ARGS

# --- Build ---
make -j"$(nproc)"

# --- Stage install into DESTDIR ---
make install DESTDIR="$DESTDIR"

# --- Signal completion to controller ---
echo "MANIFEST_READY"
