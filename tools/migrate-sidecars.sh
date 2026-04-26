#!/bin/bash
# migrate-sidecars.sh
# One-time script to generate .hspm-meta sidecars for managed packages
# that were installed before the sidecar feature was added.
# Run as root: sudo bash tools/migrate-sidecars.sh

set -e

DB="/opt/hspm/db/hspm.db"
STORE="/opt/hspm/store"
RECIPES="/opt/hspm/recipes"

echo "HSPM Sidecar Migration Tool"
echo "==========================="
echo ""

if [ "$(id -u)" != "0" ]; then
    echo "ERROR: must run as root"
    exit 1
fi

# get all managed active packages with their store paths
sqlite3 "$DB" "SELECT name, version, store_path FROM packages WHERE type='managed' AND state='active';" | \
while IFS='|' read -r name version store_path; do
    if [ -z "$store_path" ] || [ ! -d "$store_path" ]; then
        echo "  skip (no store dir): $name-$version"
        continue
    fi

    sidecar="$store_path/.hspm-meta"
    if [ -f "$sidecar" ]; then
        echo "  skip (already has sidecar): $name-$version"
        continue
    fi

    recipe="$RECIPES/$name.recipe"
    if [ ! -f "$recipe" ]; then
        echo "  skip (no recipe): $name-$version"
        continue
    fi

    # copy recipe as sidecar
    cp "$recipe" "$sidecar"
    
    # update the version in sidecar to match what's actually installed
    # (recipe might have been updated since install)
    sed -i "s/^version:.*/version: $version/" "$sidecar"
    
    # update the comment line
    sed -i "1s/.*/# $name $version — migrated sidecar/" "$sidecar"

    echo "  created: $name-$version -> $sidecar"
done

echo ""
echo "Migration complete."
echo "Run 'hspm rescue --rebuild-db --force' to test recovery."
