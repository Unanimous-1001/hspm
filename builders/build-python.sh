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

echo "[build-python] Building $PKG_NAME into $DESTDIR"

cd "$SRC_DIR"

if [ -f "setup.py" ]; then
    python3 setup.py build $EXTRA_ARGS
    python3 setup.py install \
        --prefix="$PREFIX" \
        --root="$DESTDIR" \
        --optimize=1

elif [ -f "pyproject.toml" ]; then
    pip3 install \
        --no-deps \
        --no-build-isolation \
        --prefix="$PREFIX" \
        --root="$DESTDIR" \
        . $EXTRA_ARGS
else
    echo "ERROR: No setup.py or pyproject.toml found in $SRC_DIR"
    exit 1
fi

echo "MANIFEST_READY"
