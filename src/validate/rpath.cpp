#include "rpath.hpp"
#include <array>
#include <cstdio>
#include "config.hpp"

static bool is_elf(const string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    unsigned char magic[4] = {};
    fread(magic, 1, 4, f);
    fclose(f);
    return magic[0] == 0x7f && magic[1] == 'E'
                             && magic[2] == 'L'
                             && magic[3] == 'F';
}

static string run_ldd(const string& binary, const string& store_dir) {
    string lib_path = store_dir + "/usr/lib:" +
                      store_dir + "/usr/lib64:" +
                      store_dir + "/lib";
    string cmd = "LD_LIBRARY_PATH=" + lib_path +
                 " ldd " + binary + " 2>&1";
    std::array<char, 512> buf;
    string output;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe.get()))
        output += buf.data();
    return output;
}

vector<RpathResult> validate_rpath(const string& store_dir) {
    vector<RpathResult> results;

    for (auto& entry : fs::recursive_directory_iterator(store_dir)) {
        if (!fs::is_regular_file(entry)) continue;
        string path = entry.path().string();
        if (!is_elf(path)) continue;

        string ldd_output = run_ldd(path, store_dir);
        std::istringstream ss(ldd_output);
        string line;

        while (std::getline(ss, line)) {
            if (line.find("not found") != string::npos) {
                results.push_back({
                    RpathStatus::Fatal,
                    path,
                    "Library not found: " + line
                });
                continue;
            }

            if (line.find("=>") != string::npos) {
                if (line.find("linux-vdso") != string::npos)
                    continue;

                size_t arrow = line.find("=>");
                size_t paren = line.find("(", arrow);
                if (arrow == string::npos || paren == string::npos)
                    continue;

                string resolved = line.substr(arrow + 3,
                                  paren - arrow - 4);
                size_t s = resolved.find_first_not_of(" \t");
                size_t e = resolved.find_last_not_of(" \t");
                if (s == string::npos) continue;
                resolved = resolved.substr(s, e - s + 1);

                if (resolved.empty() || resolved == "not found")
                    continue;

                if (resolved.find(HSPM_STORE)
                    != string::npos)
                    continue;

                if (resolved.find("/lib")  != string::npos ||
                    resolved.find("/usr")  != string::npos) {
                    continue;
                }

                results.push_back({
                    RpathStatus::Warning,
                    path,
                    "Resolves outside store: " + resolved
                });
            }
        }
    }

    return results;
}
