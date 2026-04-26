#include "symlink.hpp"
#include "db/database.hpp"

bool symlink_transaction(int package_id,
                         const vector<string>& store_paths,
                         const string& store_root,
                         const string& live_root) {

    vector<string> live_paths;
    for (const auto& store_path : store_paths) {
        string prefix = store_root;
        if (prefix.back() != '/') prefix += '/';

        size_t next_slash = store_path.find('/', prefix.size());
        if (next_slash == string::npos) continue;

        string relative = store_path.substr(next_slash);
        size_t usr_pos  = relative.find("/usr");
        if (usr_pos != string::npos)
            relative = relative.substr(usr_pos + 4);

        live_paths.push_back(live_root + relative);
    }

    db_pending_begin(package_id, store_paths, live_paths);

    for (size_t i = 0; i < store_paths.size(); ++i) {
        const string& src  = store_paths[i];
        const string& dest = live_paths[i];

        try {
            fs::create_directories(fs::path(dest).parent_path());
            fs::create_symlink(src, dest);

            db_pending_mark_done(dest);
            db_insert_file(package_id, dest, true);

            std::cout << "  linked: " << dest
                      << " -> " << src << "\n";

        } catch (const std::exception& e) {
            std::cerr << "[symlink] Failed at " << dest
                      << ": " << e.what() << "\n";

            auto done = db_pending_get_done(package_id);
            std::cerr << "[symlink] Rolling back "
                      << done.size() << " symlinks...\n";

            for (const auto& [store, live] : done) {
                if (fs::is_symlink(live)) {
                    fs::remove(live);
                    std::cout << "  removed: " << live << "\n";
                }
            }

            db_pending_clear(package_id);
            db_set_package_state(package_id, "partial");
            db_log("rollback", package_id,
                   "symlink transaction failed at: " + dest);
            return false;
        }
    }

    db_pending_clear(package_id);
    db_set_package_state(package_id, "active");
    db_log("install", package_id, "symlink transaction complete");
    return true;
}
