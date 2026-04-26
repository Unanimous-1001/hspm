#include "fetcher.hpp"
#include "checksum.hpp"
#include "config.hpp"

static bool verify_pkg_checksum(const string& filepath,
                                 const Package& pkg) {
    if (!pkg.sha256.empty())
        return verify_checksum(filepath, pkg.sha256);
    if (!pkg.md5.empty())
        return verify_md5(filepath, pkg.md5);
    std::cerr << "[checksum] WARNING: no checksum for "
              << pkg.name << " — skipping verification\n";
    return true;
}

static const string DISTFILES = HSPM_DISTFILES;
static const string URL_CACHE = HSPM_URLS;

static string find_in_distfiles(const Package& pkg) {
    if (!fs::exists(DISTFILES)) return "";
    for (auto& entry : fs::directory_iterator(DISTFILES)) {
        string fname = entry.path().filename().string();
        if (fname.find(pkg.name + "-" + pkg.version) == 0)
            return entry.path().string();
    }
    return "";
}

static string download(const string& url, const Package& pkg) {
    fs::create_directories(DISTFILES);
    string dest = DISTFILES + pkg.name + "-" + pkg.version + ".tar.gz";
    string cmd  = "wget -O " + dest + " " + url;
    if (system(cmd.c_str()) != 0)
        throw runtime_error("wget failed for: " + url);
    return dest;
}

static string find_in_url_cache(const Package& pkg) {
    std::ifstream file(URL_CACHE);
    if (!file) return "";

    string key = pkg.name + "-" + pkg.version;
    string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t space = line.find(' ');
        if (space == string::npos) continue;
        string entry_key = line.substr(0, space);
        string entry_url = line.substr(space + 1);
        if (entry_key == key) return entry_url;
    }
    return "";
}

string fetch_tarball(const Package& pkg) {
    string found = find_in_distfiles(pkg);
    if (!found.empty()) {
        std::cout << "[fetch] Found in distfiles: " << found << "\n";
        if (!verify_pkg_checksum(found, pkg))
            throw runtime_error("Checksum mismatch for cached file: " + found);
        return found;
    }

    string cached_url = find_in_url_cache(pkg);

    vector<string> urls_to_try;
    if (!cached_url.empty()) {
        std::cout << "[fetch] Found URL in cache: " << cached_url << "\n";
        urls_to_try.push_back(cached_url);
    }
    urls_to_try.push_back(pkg.url);
    if (!pkg.url_fallback.empty())
        urls_to_try.push_back(pkg.url_fallback);

    for (const auto& url : urls_to_try) {
        if (url.empty()) continue;
        std::cout << "[fetch] Downloading: " << url << "\n";
        try {
            string path = download(url, pkg);
            if (!verify_pkg_checksum(path, pkg)) {
                fs::remove(path);
                throw runtime_error("Checksum mismatch after download");
            }
            return path;
        } catch (const std::exception& e) {
            std::cerr << "[fetch] Failed: " << e.what() << "\n";
        }
    }

    std::cout << "[fetch] Cannot find tarball for "
              << pkg.name << "-" << pkg.version << "\n"
              << "        Enter a download URL (or leave blank to abort): ";
    string user_url;
    std::getline(std::cin, user_url);
    if (user_url.empty()) throw runtime_error("No URL provided, aborting.");

    string path = download(user_url, pkg);
    if (!verify_pkg_checksum(path, pkg)) {
        fs::remove(path);
        throw runtime_error("Checksum mismatch for user-provided URL");
    }
    return path;
}
