#include "adopt.hpp"
#include "db/database.hpp"
#include <cstdlib>
#include <cctype>

// Adopt a single named package
static void adopt_one(const string& pkg_name, const string& version) {
    PackageRecord existing = db_get_package(pkg_name);
    if (existing.id != -1) {
        std::cout << "  already recorded: " << pkg_name << "\n";
        return;
    }

    int pkg_id = db_insert_package(
        pkg_name, version,
        "adopted", "active", "");

    db_log("adopt", pkg_id, "adopted " + pkg_name + "-" + version);
    std::cout << "  adopted: " << pkg_name << " " << version << "\n";
}

static string extract_version(const string& output) {
    // extract first X.Y or X.Y.Z pattern from a string
    size_t i = 0;
    while (i < output.size()) {
        if (isdigit(output[i])) {
            size_t start = i;
            while (i < output.size() &&
                   (isdigit(output[i]) || output[i] == '.'))
                i++;
            string candidate = output.substr(start, i - start);
            // must have at least one dot to be a version
            if (candidate.find('.') != string::npos)
                return candidate;
        }
        i++;
    }
    return "unknown";
}

static string detect_version(const string& name) {
    vector<string> cmds = {
        "timeout 2 " + name + " --version 2>&1 | head -1",
        "timeout 2 " + name + " -version 2>&1 | head -1",
        "timeout 2 " + name + " -V 2>&1 | head -1",
        "timeout 2 pkg-config --modversion " + name + " 2>/dev/null",
    };

    for (const auto& cmd : cmds) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) continue;
        char buf[256] = {};
        fgets(buf, sizeof(buf), pipe);
        pclose(pipe);
        string output(buf);
        if (output.empty()) continue;
        string ver = extract_version(output);
        if (ver != "unknown" && !ver.empty())
            return ver;
    }
    return "unknown";
}
static void adopt_lfs_base() {
    std::cout << "Scanning installed system binaries...\n\n";

    // scan /usr/bin and /usr/sbin for installed binaries
    // for each unique package, detect version and adopt
    vector<string> scan_dirs = {
        "/usr/bin", "/usr/sbin", "/bin", "/sbin"
    };

    // well-known binary -> package name mapping
    // maps the binary name to the package it belongs to
    unordered_map<string, string> bin_to_pkg = {
        {"bash",       "bash"},
        {"gcc",        "gcc"},
        {"g++",        "gcc"},
        {"make",       "make"},
        {"python3",    "python3"},
        {"perl",       "perl"},
        {"openssl",    "openssl"},
        {"gzip",       "gzip"},
        {"bzip2",      "bzip2"},
        {"xz",         "xz"},
        {"tar",        "tar"},
        {"sed",        "sed"},
        {"awk",        "gawk"},
        {"grep",       "grep"},
        {"find",       "findutils"},
        {"diff",       "diffutils"},
        {"patch",      "patch"},
        {"m4",         "m4"},
        {"autoconf",   "autoconf"},
        {"automake",   "automake"},
        {"libtool",    "libtool"},
        {"flex",       "flex"},
        {"bison",      "bison"},
        {"gperf",      "gperf"},
        {"bc",         "bc"},
        {"less",       "less"},
        {"vim",        "vim"},
        {"file",       "file"},
        {"gettext",    "gettext"},
        {"msgfmt",     "gettext"},
        {"ninja",      "ninja"},
        {"meson",      "meson"},
        {"pkg-config", "pkgconf"},
        {"pkgconf",    "pkgconf"},
        {"ldconfig",   "glibc"},
        {"ldd",        "glibc"},
        {"e2fsck",     "e2fsprogs"},
        {"tune2fs",    "e2fsprogs"},
        {"ip",         "iproute2"},
        {"ss",         "iproute2"},
        {"kmod",       "kmod"},
        {"lsmod",      "kmod"},
        {"udevadm",    "eudev"},
        {"shadow",     "shadow"},
        {"passwd",     "shadow"},
        {"useradd",    "shadow"},
        {"man",        "man-db"},
        {"texinfo",    "texinfo"},
        {"makeinfo",   "texinfo"},
        {"groff",      "groff"},
        {"ps",         "procps-ng"},
        {"top",        "procps-ng"},
        {"fuser",      "psmisc"},
        {"killall",    "psmisc"},
        {"ncurses6-config", "ncurses"},
        {"readline",   "readline"},
    };

    // track what we've already adopted to avoid duplicates
    unordered_set<string> adopted;
    int count = 0;

    for (const auto& [binary, pkg_name] : bin_to_pkg) {
        if (adopted.count(pkg_name)) continue;

        // check if binary exists
        string check = "which " + binary + " >/dev/null 2>&1";
        if (system(check.c_str()) != 0) continue;

        // check if already in database
        PackageRecord existing = db_get_package(pkg_name);
        if (existing.id != -1) {
            std::cout << "  skip (already recorded): "
                      << pkg_name << "\n";
            adopted.insert(pkg_name);
            continue;
        }

        // detect version
        string version = detect_version(binary);

        adopt_one(pkg_name, version);
        adopted.insert(pkg_name);
        count++;
    }

    std::cout << "\nScanning pkg-config libraries...\n";
    FILE* pipe = popen("timeout 10 pkg-config --list-all 2>/dev/null | awk '{print $1}'",
                       "r");
    if (pipe) {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) {
            string pkg_name(buf);
            if (!pkg_name.empty() && pkg_name.back() == '\n')
                pkg_name.pop_back();
            if (pkg_name.empty()) continue;

            if (adopted.count(pkg_name)) continue;

            PackageRecord existing = db_get_package(pkg_name);
            if (existing.id != -1) continue;

            // get version from pkg-config
            string ver_cmd = "timeout 2 pkg-config --modversion " +
                             pkg_name + " 2>/dev/null";
            FILE* vpipe = popen(ver_cmd.c_str(), "r");
            string version = "unknown";
            if (vpipe) {
                char vbuf[64] = {};
                if (fgets(vbuf, sizeof(vbuf), vpipe)) {
                    version = string(vbuf);
                    if (!version.empty() && version.back() == '\n')
                        version.pop_back();
                }
                pclose(vpipe);
            }

            adopt_one(pkg_name, version);
            adopted.insert(pkg_name);
            count++;
        }
        pclose(pipe);
    }

    std::cout << "\nAdopted " << count << " system package(s).\n"
              << "Run 'hspm list' to see all recorded packages.\n"
              << "Run 'hspm adopt <name> <version>' to add any missing ones.\n";
}

void run_adopt(const string& pkg_name, const string& version) {
    if (pkg_name.empty())
        throw runtime_error(
            "Usage: hspm adopt <name> <version>\n"
            "       hspm adopt all");

    // special case: adopt all LFS base packages
    if (pkg_name == "all") {
        adopt_lfs_base();
        return;
    }

    if (version.empty())
        throw runtime_error(
            "Usage: hspm adopt <name> <version>");

    // check not already recorded
    PackageRecord existing = db_get_package(pkg_name);
    if (existing.id != -1)
        throw runtime_error(
            "Package already recorded: " + pkg_name +
            " (state: " + existing.state + ")");

    int pkg_id = db_insert_package(
        pkg_name, version,
        "adopted", "active", "");

    db_log("adopt", pkg_id,
           "adopted " + pkg_name + "-" + version);

    std::cout << "Adopted " << pkg_name
              << " " << version
              << " (id=" << pkg_id << ")\n"
              << "Note: HSPM will never modify or delete"
              << " files belonging to this package.\n";
}
