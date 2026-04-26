#!/usr/bin/env python3
"""
BLFS Recipe Scraper for HSPM
Scrapes the BLFS stable book and generates .recipe files and blfs-urls.txt

Usage:
    python3 tools/blfs-scraper.py [--output-dir DIR] [--urls-file FILE] [--book-version stable]

Alternative URL sources suggested on failure:
    - https://archive.org/         (Wayback Machine)
    - https://github.com/          (Many projects mirror here)
    - https://pkgs.org/            (Package metadata aggregator)
    - https://repology.org/        (Cross-distro package tracker)
    - https://ftp.gnu.org/gnu/     (GNU packages)
    - https://kernel.org/pub/      (Kernel.org mirrors)
"""

import urllib.request
import urllib.error
import html.parser
import re
import os
import sys
import json
import time
import argparse
from pathlib import Path

BLFS_BASE      = "https://www.linuxfromscratch.org/blfs/view/stable"
BLFS_INDEX     = BLFS_BASE + "/index.html"
REQUEST_DELAY  = 1.0
USER_AGENT     = "HSPM-BLFS-Scraper/1.0 (Linux From Scratch package manager)"

PROBLEMATIC_URL_PATTERNS = [
    "sourceforge.net",
    "sf.net",
    "ftp://",
    "downloads.sourceforge",
    "prdownloads.sourceforge",
]

ALTERNATIVE_SOURCES = [
    ("Wayback Machine",    "https://web.archive.org/web/*/{}"),
    ("GitHub",             "https://github.com/search?q={}"),
    ("pkgs.org",           "https://pkgs.org/search/?q={}"),
    ("repology.org",       "https://repology.org/projects/?search={}"),
    ("GNU FTP",            "https://ftp.gnu.org/gnu/{}/"),
    ("kernel.org",         "https://www.kernel.org/pub/"),
]

BUILD_SYSTEM_PATTERNS = [
    (r"meson\s+setup|meson\s+\.",           "meson"),
    (r"cmake\s+\.\.|cmake\s+-[SB]",         "cmake"),
    (r"perl\s+Makefile\.PL|perl\s+Build",   "perl"),
    (r"python3?\s+setup\.py|pip3?\s+",      "python"),
    (r"\./waf\s+configure",                 "waf"),
    (r"\./configure",                       "autotools"),
    (r"^make\b",                            "make"),
]

class BlfsIndexParser(html.parser.HTMLParser):
    """Parses the BLFS index page to extract package page URLs."""

    def __init__(self):
        super().__init__()
        self.packages = []
        self._in_link = False
        self._current_href = ""

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            attrs_dict = dict(attrs)
            href = attrs_dict.get("href", "")
            if href and not href.startswith("http") and ".html" in href:
                self._current_href = href
                self._in_link = True

    def handle_data(self, data):
        if self._in_link and self._current_href:
            name = data.strip()
            if name and len(name) > 1:
                url = BLFS_BASE + "/" + self._current_href.lstrip("./")
                self.packages.append((name, url))

    def handle_endtag(self, tag):
        if tag == "a":
            self._in_link = False
            self._current_href = ""


class BlfsPackageParser(html.parser.HTMLParser):
    """Parses a single BLFS package page to extract build information."""

    def __init__(self):
        super().__init__()
        self.title        = ""
        self.version      = ""
        self.download_url = ""
        self.sha256       = ""
        self.md5          = ""
        self.dependencies = []
        self.install_cmds = []

        self._in_title      = False
        self._in_pre        = False
        self._in_para       = False
        self._current_text  = []
        self._all_text      = []
        self._links         = []
        self._current_tag   = ""

    def handle_starttag(self, tag, attrs):
        self._current_tag = tag
        attrs_dict = dict(attrs)

        if tag == "title":
            self._in_title = True
        elif tag == "pre":
            self._in_pre = True
            self._current_text = []
        elif tag == "p":
            self._in_para = True
            self._current_text = []
        elif tag == "a":
            href = attrs_dict.get("href", "")
            if href:
                self._links.append(href)

    def handle_data(self, data):
        if self._in_title:
            self.title += data
        if self._in_pre or self._in_para:
            self._current_text.append(data)
        self._all_text.append(data)

    def handle_endtag(self, tag):
        if tag == "title":
            self._in_title = False
            self._extract_version_from_title()
        elif tag == "pre":
            self._in_pre = False
            block = "".join(self._current_text)
            self.install_cmds.append(block)
            self._current_text = []
        elif tag == "p":
            self._in_para = False
            self._current_text = []

    def _extract_version_from_title(self):
        match = re.search(r'(\d+\.\d+[\.\d]*)', self.title)
        if match:
            self.version = match.group(1)

    def get_full_text(self):
        return "".join(self._all_text)

def fetch_url(url, retries=3):
    """Fetch a URL with retries and a polite delay."""
    for attempt in range(retries):
        try:
            req = urllib.request.Request(
                url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=30) as resp:
                return resp.read().decode("utf-8", errors="replace")
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None  # not found, don't retry
            if attempt < retries - 1:
                time.sleep(REQUEST_DELAY * 2)
        except Exception as e:
            if attempt < retries - 1:
                time.sleep(REQUEST_DELAY * 2)
    return None


def extract_download_info(page_text, links):
    """Extract download URL and checksum from page text and links."""
    download_url = ""
    sha256       = ""
    md5          = ""

    for link in links:
        if any(ext in link for ext in
               [".tar.gz", ".tar.xz", ".tar.bz2", ".tgz"]):
            if not download_url:
                download_url = link
                break

    sha256_match = re.search(
        r'SHA256\s*(?:Sum|sum|checksum)?:?\s*([a-f0-9]{64})',
        page_text, re.IGNORECASE)
    if sha256_match:
        sha256 = sha256_match.group(1)

    md5_match = re.search(
        r'MD5\s*(?:Sum|sum|checksum)?:?\s*([a-f0-9]{32})',
        page_text, re.IGNORECASE)
    if md5_match:
        md5 = md5_match.group(1)

    return download_url, sha256, md5


def detect_build_system(install_cmds):
    """Detect which build system a package uses from its install commands."""
    all_cmds = "\n".join(install_cmds)
    for pattern, builder in BUILD_SYSTEM_PATTERNS:
        if re.search(pattern, all_cmds, re.MULTILINE | re.IGNORECASE):
            return builder
    return "autotools"

def extract_dependencies(page_html):
    """Extract required and recommended deps separately."""
    required = []
    recommended = []

    for section in re.findall(
            r'<p\s+class="required"[^>]*>(.*?)</p>',
            page_html, re.DOTALL | re.IGNORECASE):
        for href in re.findall(
                r'href=["\']([^"\']+\.html)["\']', section):
            pkg = href.rstrip('/').split('/')[-1]\
                      .replace('.html','').lower()
            pkg = re.sub(r'[^a-z0-9\-\+]', '', pkg)
            if pkg and len(pkg) > 1 and pkg not in required:
                required.append(pkg)

    for section in re.findall(
            r'<p\s+class="recommended"[^>]*>(.*?)</p>',
            page_html, re.DOTALL | re.IGNORECASE):
        for href in re.findall(
                r'href=["\']([^"\']+\.html)["\']', section):
            pkg = href.rstrip('/').split('/')[-1]\
                      .replace('.html','').lower()
            pkg = re.sub(r'[^a-z0-9\-\+]', '', pkg)
            if (pkg and len(pkg) > 1 and
                pkg not in required and
                pkg not in recommended):
                recommended.append(pkg)

    return required[:15], recommended[:10]

TEST_KEYWORDS = [
    'make check', 'make test', 'make -k check',
    'ninja test', 'pytest', 'python -m pytest',
    'make installcheck', 'make distcheck',
    './run_tests', 'ctest'
]

SIMPLE_BUILD_PATTERN = re.compile(
    r'^\s*\./configure\s+--prefix.*&&\s*make\s*&&\s*make\s+install',
    re.MULTILINE | re.DOTALL
)

def is_test_command(cmd):
    """Check if a command is test-related and should be skipped."""
    cmd_lower = cmd.lower().strip()
    return any(kw in cmd_lower for kw in TEST_KEYWORDS)

def extract_build_notes(install_cmds):
    """Extract the actual build commands from install sections,
    filtering out test commands and normalizing whitespace."""
    notes_lines = []
    for block in install_cmds:
        lines = block.strip().split('\n')
        for line in lines:
            line = line.strip()
            if not line: continue
            if is_test_command(line): continue
            if line.startswith('#'): continue
            if re.search(r'\[CONFIG_|\[FUSE|\[.*_FS\]|--->', line):
                continue
            notes_lines.append(line)

    return ' && '.join(notes_lines[:10])

def is_complex_build(install_cmds):
    """Detect if a package needs non-standard build steps."""
    all_cmds = ' && '.join(
        block.strip() for block in install_cmds if block.strip())

    filtered_lines = [
        l.strip() for l in all_cmds.replace('&&', '\n').split('\n')
        if l.strip() and not is_test_command(l)
    ]
    filtered = ' && '.join(filtered_lines)

    complex_indicators = [
        r'sed\s+-[ei]',
        r'patch\s+-[Np]',
        r'export\s+\w+=',
        r'install\s+-v\s+-m',
        r'ln\s+-sfv?\s+',
        r'case\s+\$',
        r'if\s+\[',
        r'make\s+install\s+\w+=',
        r'^\s*\.\/config\s+',
        r'make\s+\w+=(?!DESTDIR)',
    ]
    for pattern in complex_indicators:
        if re.search(pattern, filtered, re.IGNORECASE | re.MULTILINE):
            return True

    has_make    = bool(re.search(r'\bmake\b|\bninja\b', filtered))
    has_install = bool(re.search(
        r'make\s+install|ninja\s+install|meson\s+install|'
        r'python.*install|./waf\s+install', filtered, re.IGNORECASE))

    if has_make and has_install:
        return False
    return True

def extract_patches(page_html, page_url):
    """Extract patch URLs from the Additional Downloads section."""
    patches = []
    patch_cmds = []

    additional = re.search(
        r'Additional\s+Downloads.*?(?=Installation|$)',
        page_html, re.DOTALL | re.IGNORECASE)

    if additional:
        section = additional.group(0)
        urls = re.findall(
            r'href=["\']([^"\']*\.patch)["\']', section)
        for url in urls:
            if not url.startswith('http'):
                base = '/'.join(page_url.split('/')[:3])
                url = base + '/' + url.lstrip('/')
            patches.append(url)
            patch_file = url.split('/')[-1]
            patch_cmds.append(f'patch -Np1 -i ../{patch_file}')

    if not patches:
        patch_refs = re.findall(
            r'href=["\']([^"\']*\.patch)["\']', page_html)
        for url in patch_refs:
            if 'patch' in url.lower():
                if not url.startswith('http'):
                    base = '/'.join(page_url.split('/')[:3])
                    url = base + '/' + url.lstrip('/')
                if url not in patches:
                    patches.append(url)
                    patch_file = url.split('/')[-1]
                    patch_cmds.append(f'patch -Np1 -i ../{patch_file}')

    return patches, patch_cmds

def extract_extra_urls(page_html, main_url):
    """Extract additional download URLs (non-patch, non-test)."""
    extra = []

    additional = re.search(
        r'Additional\s+Downloads.*?(?=Installation|$)',
        page_html, re.DOTALL | re.IGNORECASE)

    if not additional:
        return extra

    section = additional.group(0)
    urls = re.findall(
        r'href=["\']([^"\']+\.(?:tar\.gz|tar\.xz|tar\.bz2|tgz|zip))["\']',
        section)

    for url in urls:
        if url == main_url: continue
        if not url.startswith('http'):
            base = '/'.join(main_url.split('/')[:3])
            url = base + '/' + url.lstrip('/')
        extra.append(url)

    return extra
    
def extract_kernel_hint(page_html):
    kernel_div_match = re.search(
        r'<div[^>]*class="kernel"[^>]*>(.*?)</div\s*>',
        page_html,
        re.DOTALL | re.IGNORECASE
    )
    if not kernel_div_match:
        return None

    kernel_section = kernel_div_match.group(1)
    pre_blocks = re.findall(
        r'<pre[^>]*>(.*?)</pre>',
        kernel_section,
        re.DOTALL | re.IGNORECASE
    )
    if not pre_blocks:
        return None

    hints = []
    for block in pre_blocks:
        # Step 1: remove real HTML tags FIRST (before unescaping)
        # This preserves &lt;*&gt; and &lt;M&gt; as entities
        block = re.sub(r'<[a-zA-Z][^>]*>', '', block)   # opening tags
        block = re.sub(r'</[a-zA-Z][^>]*>', '', block)  # closing tags

        # Step 2: NOW unescape HTML entities
        # &lt;*&gt; becomes <*>, &lt;M&gt; becomes <M>, etc.
        import html as html_module
        block = html_module.unescape(block)

        block = block.strip()
        if block:
            hints.append(block)

    return '\n'.join(hints) if hints else None

def extract_patch_cmds_from_notes(notes, patches):
    """Try to find explicit patch commands in the BLFS notes."""
    if not patches: return []
    patch_cmds = []
    found = re.findall(
        r'patch\s+-[Np0-9]+\s+-i\s+\S+', notes, re.IGNORECASE)
    if found:
        return found
    for patch_url in patches:
        patch_file = patch_url.split('/')[-1]
        patch_cmds.append(f'patch -Np1 -i ../{patch_file}')
    return patch_cmds

def extract_patches_from_notes(notes, page_url, existing_patches):
    """Extract patch filenames from notes and construct BLFS patch URLs."""
    if existing_patches:
        return existing_patches, []

    # find patch filenames mentioned in notes
    patch_files = re.findall(
        r'patch\s+-[Np0-9]+\s+-i\s+\.\./(\S+\.patch)',
        notes, re.IGNORECASE)

    if not patch_files:
        return [], []

    # construct BLFS patch URLs
    # format: https://www.linuxfromscratch.org/patches/blfs/<version>/<name>.patch
    # extract book version from page_url
    ver_match = re.search(r'/blfs/view/([^/]+)/', page_url)
    book_ver = ver_match.group(1) if ver_match else 'stable'

    patches = []
    patch_cmds = []
    for patch_file in patch_files:
        url = (f"https://www.linuxfromscratch.org/patches/blfs/"
               f"{book_ver}/{patch_file}")
        patches.append(url)
        patch_cmds.append(f'patch -Np1 -i ../{patch_file}')

    return patches, patch_cmds

def extract_package_name(title, url):
    """Extract clean package name from the BLFS page URL — most reliable."""
    # BLFS URLs are like: .../multimedia/alsa-lib.html
    # The filename without .html is the clean package name
    url_part = url.rstrip('/').split('/')[-1]
    url_part = url_part.replace('.html', '').strip().lower()
    url_part = re.sub(r'[^a-z0-9\-\+]', '', url_part)
    if url_part and len(url_part) > 1:
        return url_part
    name = re.sub(r'[-\s]+\d[\d\.]*.*$', '', title).strip()
    name = re.sub(r'\s+', '-', name).lower()
    name = re.sub(r'[^a-z0-9\-\+]', '', name)
    return name if name and len(name) > 1 else url_part

def is_problematic_url(url):
    """Check if a URL is unreliable for automated fetching."""
    url_lower = url.lower()
    return any(pat in url_lower for pat in PROBLEMATIC_URL_PATTERNS)


def suggest_alternatives(pkg_name):
    """Return suggested alternative URL sources for a package."""
    suggestions = []
    for source_name, url_template in ALTERNATIVE_SOURCES:
        if "{}" in url_template:
            url = url_template.format(pkg_name)
        else:
            url = url_template
        suggestions.append(f"  {source_name}: {url}")
    return "\n".join(suggestions)

def generate_recipe(pkg_name, version, builder, sha256, url,
                    dependencies, recommended=None,
                    configure_args="", complex=False, notes="",
                    patches=None, patch_cmds=None, extra_urls=None, md5=None, kernel_hint=None):
    checksum_line = None
    if sha256 and not sha256.startswith("md5:"):
        checksum_line = f"sha256: {sha256}"
    elif sha256 and sha256.startswith("md5:"):
        checksum_line = f"md5: {sha256[4:]}" 
    elif md5:
        checksum_line = f"md5: {md5}"

    lines = [
        f"# {pkg_name} {version} — auto-generated by blfs-scraper",
        f"name:    {pkg_name}",
        f"version: {version}",
        f"builder: {builder}",
    ]
    if checksum_line:
        lines.append(checksum_line)
    lines.append(f"url:     {url}")
    
    if dependencies:
        lines.append(f"depends:    {' '.join(dependencies)}")
    if recommended:
        lines.append(f"recommends: {' '.join(recommended)}")
    if configure_args:
        lines.append(f"configure_args: {configure_args}")
    if complex:
        lines.append(f"complex: true")
    if notes:
        lines.append(f"notes:   {notes.replace(chr(10), ' && ').strip()}")
    if patches:
        lines.append(f"patches: {' '.join(patches)}")
    if patch_cmds:
        lines.append(f"patch_cmds: {' '.join(patch_cmds)}")
    if extra_urls:
        lines.append(f"extra_urls: {' '.join(extra_urls)}")
    if kernel_hint:
        lines.append(f"kernel_hint: |")
        for line in kernel_hint.splitlines():
            lines.append(f"  {line}")
        
    return "\n".join(lines) + "\n"

def scrape_blfs(output_dir, urls_file, book_version="stable"):
    """Main scraping function."""

    global BLFS_BASE, BLFS_INDEX
    BLFS_BASE  = f"https://www.linuxfromscratch.org/blfs/view/{book_version}"
    BLFS_INDEX = BLFS_BASE + "/index.html"

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    successes      = []
    failures       = []
    manual_needed  = []
    existing_urls  = set()

    urls_path = Path(urls_file)
    if urls_path.exists():
        with open(urls_path) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    parts = line.split(' ', 1)
                    if len(parts) == 2:
                        existing_urls.add(parts[0])

    print(f"HSPM BLFS Scraper")
    print(f"=================")
    print(f"Book:       {BLFS_BASE}")
    print(f"Output:     {output_dir}")
    print(f"URLs file:  {urls_file}")
    print()

    print("Fetching BLFS index...")
    index_html = fetch_url(BLFS_INDEX)
    if not index_html:
        print(f"ERROR: Cannot fetch index from {BLFS_INDEX}")
        sys.exit(1)

    index_parser = BlfsIndexParser()
    index_parser.feed(index_html)
    packages = index_parser.packages

    seen_urls = set()
    unique_packages = []
    for name, url in packages:
        if url not in seen_urls:
            seen_urls.add(url)
            unique_packages.append((name, url))

    print(f"Found {len(unique_packages)} package pages\n")

    new_url_entries = []

    for i, (page_name, page_url) in enumerate(unique_packages):
        progress = f"[{i+1}/{len(unique_packages)}]"
        print(f"{progress} Processing: {page_name}...", end=" ", flush=True)

        time.sleep(REQUEST_DELAY)

        page_html = fetch_url(page_url)
        if not page_html:
            print("SKIP (fetch failed)")
            failures.append({
                "name":   page_name,
                "url":    page_url,
                "reason": "Page fetch failed"
            })
            continue

        kernel_hint = extract_kernel_hint(page_html)

        page_html_clean = re.sub(
            r'<div[^>]*class="kernel"[^>]*>.*?</div\s*>',
            '',
            page_html,
            flags=re.DOTALL | re.IGNORECASE
        )

        parser = BlfsPackageParser()
        parser.feed(page_html_clean)
        page_text = parser.get_full_text()

        pkg_name = extract_package_name(parser.title or page_name, page_url)
        version  = parser.version

        if not version:
            print("SKIP (no version found)")
            failures.append({
                "name":   pkg_name,
                "url":    page_url,
                "reason": "Cannot determine version"
            })
            continue

        download_url, sha256, md5 = extract_download_info(
            page_text, parser._links)
        
        if download_url:
            url_basename = download_url.rstrip('/').split('/')[-1]
            url_basename = re.sub(r'\.tar\.(gz|xz|bz2|lz)$', '',
                                  url_basename)
            url_basename = re.sub(r'\.(zip|tgz)$', '', url_basename)
            url_ver_match = re.search(r'-(\d+[\.\d]+\d)$', url_basename)
            if url_ver_match:
                url_version = url_ver_match.group(1)
                if url_version != version:
                    version = url_version

        if not download_url:
            print("SKIP (no download URL)")
            failures.append({
                "name":    pkg_name,
                "version": version,
                "url":     page_url,
                "reason":  "No download URL found",
                "suggest": suggest_alternatives(pkg_name)
            })
            continue

        if is_problematic_url(download_url):
            print(f"MANUAL ({download_url[:50]}...)")
            manual_needed.append({
                "name":         pkg_name,
                "version":      version,
                "download_url": download_url,
                "reason":       "Problematic URL (SourceForge/FTP)",
                "suggest":      suggest_alternatives(pkg_name)
            })
            continue

        if not sha256 and not md5:
            print("SKIP (no checksum)")
            failures.append({
                "name":    pkg_name,
                "version": version,
                "reason":  "No checksum found"
            })
            continue

        notes        = extract_build_notes(parser.install_cmds)
        complex_build = is_complex_build(parser.install_cmds)
        patches, patch_cmds = extract_patches(page_html, page_url)
        extra_urls   = extract_extra_urls(page_html, download_url)

        if not patches:
            patches, patch_cmds = extract_patches_from_notes(
                notes, page_url, patches)
        elif not patch_cmds:
            patch_cmds = extract_patch_cmds_from_notes(notes, patches)

        builder = detect_build_system(parser.install_cmds)

        deps, recommended_deps = extract_dependencies(page_html)

        recipe_content = generate_recipe(
            pkg_name, version, builder, sha256,
            download_url, deps,
            md5=md5,
            recommended=recommended_deps if recommended_deps else None,
            complex=complex_build,
            notes=notes if complex_build else "",
            patches=patches if patches else None,
            patch_cmds=patch_cmds if patch_cmds else None,
            extra_urls=extra_urls if extra_urls else None, kernel_hint=kernel_hint)

        recipe_path = output_dir / f"{pkg_name}.recipe"
        should_write = True
        if recipe_path.exists():
            with open(recipe_path) as f:
                first_line = f.readline()
            if "auto-generated by blfs-scraper" not in first_line:
                should_write = False
                print(f"SKIP (hand-written recipe preserved)", end=" ")
            else:
                existing_version = ""
                with open(recipe_path) as f:
                    for line in f:
                        if line.startswith("version:"):
                            existing_version = line.split(":", 1)[1].strip()
                            break
                if existing_version == version:
                    should_write = False
                    print(f"SKIP (same version {version})", end=" ")
                else:
                    print(f"UPDATE ({existing_version} -> {version})", end=" ")

        if should_write:
            with open(recipe_path, 'w') as f:
                f.write(recipe_content)

        url_key = f"{pkg_name}-{version}"
        if url_key not in existing_urls:
            new_url_entries.append(f"{url_key} {download_url}")
            existing_urls.add(url_key)

        print(f"OK ({builder}, {version})")
        successes.append(pkg_name)

    if new_url_entries:
        with open(urls_path, 'a') as f:
            f.write(f"\n# Auto-generated by blfs-scraper — BLFS {book_version}\n")
            for entry in new_url_entries:
                f.write(entry + "\n")
        print(f"\nAdded {len(new_url_entries)} URLs to {urls_file}")

    log_path = Path(output_dir).parent / "scraper-failures.log"
    with open(log_path, 'w') as f:
        f.write(f"BLFS Scraper Failure Log\n")
        f.write(f"========================\n\n")

        if manual_needed:
            f.write(f"MANUAL INTERVENTION REQUIRED ({len(manual_needed)} packages)\n")
            f.write(f"These packages have SourceForge or FTP URLs.\n")
            f.write(f"Find working URLs manually and add to blfs-urls.txt.\n\n")
            for pkg in manual_needed:
                f.write(f"  Package:  {pkg['name']} {pkg.get('version','')}\n")
                f.write(f"  BLFS URL: {pkg.get('download_url','')}\n")
                f.write(f"  Reason:   {pkg['reason']}\n")
                f.write(f"  Try these alternative sources:\n")
                f.write(pkg.get('suggest','') + "\n\n")

        if failures:
            f.write(f"\nOTHER FAILURES ({len(failures)} packages)\n\n")
            for pkg in failures:
                f.write(f"  Package: {pkg['name']}\n")
                f.write(f"  Reason:  {pkg['reason']}\n")
                if 'suggest' in pkg:
                    f.write(f"  Suggestions:\n{pkg['suggest']}\n")
                f.write("\n")

    print(f"\n{'='*50}")
    print(f"SCRAPER SUMMARY")
    print(f"{'='*50}")
    print(f"  Succeeded:        {len(successes)}")
    print(f"  Manual needed:    {len(manual_needed)}")
    print(f"  Failed:           {len(failures)}")
    print(f"  Total processed:  {len(unique_packages)}")
    print(f"\n  Recipes written to: {output_dir}")
    print(f"  Failure log:        {log_path}")
    print(f"\nFor manual packages, see: {log_path}")
    print("Each failure entry includes alternative URL sources to check.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Scrape BLFS and generate HSPM recipe files")
    parser.add_argument(
        "--output-dir",
        default="/opt/hspm/recipes",
        help="Directory to write .recipe files (default: recipes/)")
    parser.add_argument(
        "--urls-file",
        default="/opt/hspm/blfs-urls.txt",
        help="Path to blfs-urls.txt to update")
    parser.add_argument(
        "--book-version",
        default="stable",
        help="BLFS book version: stable, systemd, or a number like 12.1")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and print what would be generated without writing files")

    args = parser.parse_args()

    scrape_blfs(
        output_dir    = args.output_dir,
        urls_file     = args.urls_file,
        book_version  = args.book_version,
    )
