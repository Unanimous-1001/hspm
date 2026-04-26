#!/usr/bin/env python3
"""Scan all HSPM recipes and list packages with kernel hints."""

import os
import sys

RECIPE_DIR = "/opt/hspm/recipes"
OUTPUT_FILE = "/tmp/hspm_kernel_hints.txt"

def extract_recipe_info(filepath):
    """Return (pkg_name, kernel_hint) if kernel_hint exists, else None."""
    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()
    except Exception:
        return None

    pkg_name = None
    in_kernel_hint = False
    hint_lines = []

    for line in lines:
        line = line.rstrip('\n')

        if line.startswith("name:"):
            pkg_name = line.split(":", 1)[1].strip()

        if line.startswith("kernel_hint:"):
            in_kernel_hint = True
            continue

        if in_kernel_hint:
            # Multi-line hints are indented with two spaces
            if line.startswith("  "):
                hint_lines.append(line[2:])  # strip leading spaces
            else:
                # Reached end of hint block
                in_kernel_hint = False

    if pkg_name and hint_lines:
        return pkg_name, "\n".join(hint_lines)
    return None

def main():
    if not os.path.isdir(RECIPE_DIR):
        print(f"Recipe directory not found: {RECIPE_DIR}", file=sys.stderr)
        sys.exit(1)

    results = []
    for filename in os.listdir(RECIPE_DIR):
        if not filename.endswith(".recipe"):
            continue
        filepath = os.path.join(RECIPE_DIR, filename)
        info = extract_recipe_info(filepath)
        if info:
            results.append(info)

    # Sort by package name
    results.sort(key=lambda x: x[0])

    with open(OUTPUT_FILE, 'w') as f:
        for pkg_name, hint in results:
            f.write(f"Package: {pkg_name}\n")
            f.write(f"Kernel Hint:\n{hint}\n")
            f.write("-" * 50 + "\n")

    print(f"Found {len(results)} packages with kernel hints.")
    print(f"Results written to: {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
