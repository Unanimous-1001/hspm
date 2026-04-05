#include "manifest.hpp"

vector<string> scan_manifest(const string& store_dir) {
    vector<string> files;

    if (!fs::exists(store_dir)) {
        throw runtime_error("Store directory does not exist: " + store_dir);
    }

    for (auto& entry : fs::recursive_directory_iterator(store_dir)) {
        if (fs::is_regular_file(entry) || fs::is_symlink(entry)) {
            files.push_back(entry.path().string());
        }
    }

    return files;
}
