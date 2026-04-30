<div align="center">

# HSPM — Hybrid Symlink Package Manager for LFS

**A source-based package manager for Linux From Scratch.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![BLFS](https://img.shields.io/badge/BLFS-13.0-green.svg)](https://www.linuxfromscratch.org/blfs/)

</div>

## What is HSPM?

If you have built an LFS system, you know the problem. By the time your base system is running, dozens of packages are already installed and there is no package manager tracking any of them. You cannot just drop in a new package manager and have it know about bash, gcc, glibc, and everything else you already compiled.

HSPM is built around this reality. It works in two modes at the same time.

**Adopted packages** are things already on your system that HSPM records in its database but never touches. Your LFS base packages, anything you compiled manually before installing HSPM, all of that gets adopted. HSPM will never modify or delete an adopted file under any circumstance.

**Managed packages** are things HSPM installs itself. Every managed package gets built into an isolated directory under `/opt/hspm/store/name-version/` and activated by creating symlinks into `/usr/bin`, `/usr/lib`, and so on. Uninstalling is just removing those symlinks and deleting the store directory.

This means HSPM can coexist with your existing LFS system from day one without requiring a reinstall or any modification to what you already have.

---

## How it works

```
Source tarball
  -> build into /opt/hspm/store/curl-8.7.1/
  -> symlink /usr/bin/curl    -> /opt/hspm/store/curl-8.7.1/usr/bin/curl
  -> symlink /usr/lib/libcurl -> /opt/hspm/store/curl-8.7.1/usr/lib/libcurl.so
  -> ...and so on for every file
```

The actual files never leave the store directory. The symlinks in `/usr/` are just pointers. Upgrading a package means building the new version alongside the old one, swapping the symlinks, and keeping the old store directory around in case you need to roll back.

---

## Key Features

- Hybrid tracking: HSPM knows about your whole system, not just what it installed
- Isolated store per package: each package lives in its own directory with no file conflicts between packages
- Full dependency resolution with cycle detection and automatic cycle breaking for recommended dependencies
- Multiple build systems supported out of the box: autotools, cmake, meson, make, perl, python, openssl, and waf
- BLFS scraper that auto-generates recipes for around 650 packages directly from the BLFS book, including build complexity detection, patch files, and kernel config requirements
- Interactive install mode for complex packages: shows you the BLFS build notes, lets you edit or override the build command, and saves your customization for future installs
- Crash-safe symlink transactions: if HSPM is interrupted mid-install, the rollback system can clean up from exactly where it stopped
- RPATH validation: runs ldd on every ELF binary after building to catch missing library dependencies before they reach the live system
- Collision protection: before symlinking, checks every target path and refuses to overwrite files that belong to adopted packages
- Metadata sidecars: every installed package gets a `.hspm-meta` file in its store directory, enabling full database reconstruction from scratch if something goes wrong
- Full audit log: every operation is written to both the SQLite database and a plain text log at `/opt/hspm/logs/hspm.log`
- 17 unit tests covering the recipe parser, dependency graph, checksum verification, collision detection, and symlink transactions

---

## What HSPM cannot do

- It cannot manage packages on other Linux distributions. It was designed for LFS and tested on LFS.
- It does not download pre-built binaries. Everything is compiled from source.
- It does not support per-user installs. It is a root-only tool that writes to `/usr/` and `/opt/hspm/`.
- It does not sandbox builds. Packages build as root in the host environment.
- It does not use content-addressed store paths like Nix. Store paths are just `name-version`.
- Some complex packages will fail with the default build commands. The `--interactive` flag handles this, but you may need to look at the BLFS page and provide the right commands yourself the first time.

---

## Dependencies

Everything here is part of a standard LFS build, so you should already have all of it.

| Dependency | Used for |
|---|---|
| g++ with C++17 support | Compiling HSPM |
| make | Build system |
| sqlite3 with dev headers | Package database |
| python3 | BLFS recipe scraper |
| wget | Downloading tarballs |
| tar | Extracting source archives |
| sha256sum / md5sum | Checksum verification |
| ldd | Library linkage validation |
| patch | Applying source patches |

---

## Installation

Clone the repository and build it:

```bash
git clone https://github.com/Unanimous-1001/hspm.git
cd hspm
make
sudo make install
```

`make install` copies the binary to `/usr/bin/hspm` and copies all recipes, builder scripts, tools, and the database schema to `/opt/hspm/`.

If you want to install to a different location:

```bash
sudo make install PREFIX=/opt/hspm LIVE=/usr DISTFILES=/usr/src/distfiles/
```

| Variable | Default | Description |
|---|---|---|
| PREFIX | /opt/hspm | Where the store, database, and recipes live |
| LIVE | /usr | Where symlinks get created |
| DISTFILES | /usr/src/distfiles/ | Where downloaded tarballs are cached |

---

## Initialization

Run this once after installing:

```bash
# create the database and store directories
sudo hspm init

# detect and record everything already installed on your system
sudo hspm adopt all
```

`adopt all` scans all directories in your `$PATH`, common system directories, and `pkg-config --list-all` to find installed packages and detect their versions. Anything it misses you can add manually:

```bash
sudo hspm adopt bash 5.2.37
```

Then pull down recipes for BLFS packages. This takes around 15 minutes and generates recipes for roughly 650 packages:

```bash
sudo hspm sync
```

---

## Configuration

There is no configuration file. Paths are set at compile time via the Makefile variables above. If you need to change them after installing, set the variables and recompile.

HSPM is a root-only tool. Commands that modify the system require sudo or a root shell.

---

## Commands

### Installing and removing packages

```bash
# install a package and all its dependencies
sudo hspm install curl

# install with interactive prompts for complex packages
sudo hspm install apache --interactive

# remove a managed package
sudo hspm uninstall curl

# upgrade to a newer version (old version stays in store)
sudo hspm upgrade curl 8.8.0

# switch back to an older stored version
sudo hspm activate curl 8.7.1

# delete an old stored version you no longer need
sudo hspm prune curl 8.7.1

# clean up a failed or partial install
sudo hspm rollback curl
```

### Adopting existing packages

```bash
# scan the system and adopt everything installed
sudo hspm adopt all

# adopt a specific package manually
sudo hspm adopt bash 5.2.37
```

### Checking things

```bash
# list everything HSPM knows about
hspm list

# show the recipe fields for a package
hspm show curl

# show what order packages would be installed in
hspm resolve curl

# check that all symlinks are valid
sudo hspm verify

# show the operation log
hspm log
hspm log curl        # filter by package name
hspm log install     # filter by operation type
```

### Recipes and syncing

```bash
# sync latest recipes from the BLFS book
sudo hspm sync

# sync from a specific book version
sudo hspm sync 13.0-systemd
```

### Recovery

```bash
# show available recovery operations
hspm rescue

# rebuild the database from store sidecars
sudo hspm rescue --rebuild-db

# restore missing symlinks for managed packages
sudo hspm rescue --fix-links

# check library linkage for all managed packages
sudo hspm rescue --validate-rpath
```

### Install flags

| Flag | What it does |
|---|---|
| --interactive | Show build notes and prompt before building complex packages |
| --force-symlink | Replace unknown symlinks instead of aborting |
| --adopt-collision | Adopt files that would otherwise cause a collision |
| --yes or -y | Automatically confirm any prompts |

---

## Recipe Format

Recipes live in `/opt/hspm/recipes/name.recipe` and use a simple key-value format. Most of them are generated automatically by `hspm sync`. For packages the scraper misses, you can write one manually in a couple of minutes using the URL and checksum from the BLFS page.

```
# curl 8.7.1
name:           curl
version:        8.7.1
builder:        autotools
sha256:         b813316b...
url:            https://curl.se/download/curl-8.7.1.tar.gz
url_fallback:   https://github.com/curl/curl/releases/download/curl-8_7_1/curl-8.7.1.tar.gz
depends:        openssl zlib
configure_args: --with-openssl --with-zlib --enable-ipv6
```

### All recipe fields

| Field | Required | Description |
|---|---|---|
| name | Yes | Package name |
| version | Yes | Version string |
| builder | Yes | Build system: autotools, cmake, meson, make, perl, python, openssl, waf |
| sha256 | Yes* | SHA256 checksum of the source tarball |
| md5 | Yes* | MD5 checksum (used when SHA256 is not available) |
| url | Yes | Primary download URL |
| url_fallback | No | Backup URL if the primary fails |
| depends | No | Required dependencies, space-separated |
| recommends | No | Recommended dependencies (installed but dropped if they cause cycles) |
| configure_args | No | Extra arguments passed to the build script |
| build_cmd | No | Fully custom build command that replaces the builder script |
| complex | No | Set to true if the package needs interactive configuration |
| notes | No | BLFS build commands shown during interactive install |
| patches | No | Patch file URLs to download before building |
| patch_cmds | No | Commands to apply the patches |
| kernel_hint | No | Kernel configuration options this package requires |

*Either sha256 or md5 is required.

---

## BLFS Scraper

Running `hspm sync` scrapes the BLFS book and generates recipes automatically. The scraper:

- Fetches all roughly 860 pages from the BLFS book
- Generates recipes for around 650 packages (the rest are SourceForge-hosted, FTP-only, or non-package pages)
- Detects the build system from the install instructions on each page
- Separates required and recommended dependencies
- Finds and records patch file URLs
- Extracts kernel configuration hints for packages that need kernel options enabled
- Marks complex packages so the interactive prompt activates automatically
- Never overwrites a recipe that was written by hand
- Updates a recipe when a newer version is found on the BLFS page

Packages it cannot handle are listed in a failure log with suggestions for where to find working download URLs.

---

## Rescue and Recovery

### Metadata sidecars

Every package HSPM installs gets a `.hspm-meta` file written into its store directory. This file contains the package recipe at install time, including any custom build commands you provided during interactive install. If the database is lost or corrupted, HSPM can rebuild it entirely from these sidecar files.

For packages installed before this feature was added, you can generate sidecars using the included migration script:

```bash
sudo bash tools/migrate-sidecars.sh
```

### Rebuilding the database

```bash
sudo hspm rescue --rebuild-db
```

Scans all store directories for `.hspm-meta` files and reconstructs the packages, files, and dependencies tables. After rebuilding, re-adopt the base system packages since they have no store directory and cannot be restored automatically:

```bash
sudo hspm adopt all
```

### Fixing broken symlinks

```bash
sudo hspm rescue --fix-links
```

Checks every active managed package and recreates any missing symlinks. Skips paths that belong to adopted packages.

---

## License

MIT - see [LICENSE](LICENSE) for details.

Built for and tested on Linux From Scratch 12.x.
Recipe data sourced from the [BLFS book](https://www.linuxfromscratch.org/blfs/).
