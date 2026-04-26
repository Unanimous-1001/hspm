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

echo "[build-perl] Building $PKG_NAME into $DESTDIR"

cd "$SRC_DIR"

if [ -f "Makefile.PL" ]; then
    perl Makefile.PL \
        PREFIX="$PREFIX" \
        INSTALLDIRS=vendor \
        $EXTRA_ARGS
    make -j"$(nproc)"
    make install DESTDIR="$DESTDIR"

elif [ -f "Build.PL" ]; then
    perl Build.PL \
        --prefix="$PREFIX" \
        --installdirs=vendor \
        $EXTRA_ARGS
    ./Build
    ./Build install \
        --destdir="$DESTDIR"
else
    echo "ERROR: No Makefile.PL or Build.PL found in $SRC_DIR"
    exit 1
fi

echo "MANIFEST_READY"
