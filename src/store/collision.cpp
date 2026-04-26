#include "collision.hpp"
#include "db/database.hpp"

static string store_to_live(const string& store_path,
                             const string& store_root,
                             const string& live_root) {
    string prefix = store_root;
    if (prefix.back() != '/') prefix += '/';

    size_t pos = store_path.find(prefix);
    if (pos == string::npos)
        throw runtime_error("Path not under store root: " + store_path);

    size_t next_slash = store_path.find('/', prefix.size());
    if (next_slash == string::npos)
        throw runtime_error("Unexpected store path format: " + store_path);

    string relative = store_path.substr(next_slash);

    size_t usr_pos = relative.find("/usr");
    if (usr_pos != string::npos)
        relative = relative.substr(usr_pos + 4);

    return live_root + relative;
}

CollisionResult check_collision(const string& target_path,
                                bool force_symlink,
                                bool adopt_collision) {
    if (!fs::exists(target_path) && !fs::is_symlink(target_path))
        return CollisionResult::Safe;

    if (fs::is_symlink(target_path)) {
        if (force_symlink) {
            fs::remove(target_path);
            std::cout << "  [force] removed unknown symlink: "
                      << target_path << "\n";
            return CollisionResult::Safe;
        }
        return CollisionResult::UnknownSymlink;
    }

    if (adopt_collision) {
        std::cout << "  [adopt] recording real file as adopted: "
                  << target_path << "\n";
        return CollisionResult::Safe;
    }
    return CollisionResult::RealFile;
}

pair<string, CollisionResult> check_manifest_collisions(
    const vector<string>& store_paths,
    const string& store_root,
    const string& live_root,
    bool force_symlink,
    bool adopt_collision) {

    for (const auto& store_path : store_paths) {
        string live_path = store_to_live(store_path, store_root, live_root);
        CollisionResult result = check_collision(
            live_path, force_symlink, adopt_collision);

        if (result != CollisionResult::Safe &&
            result != CollisionResult::OwnedSymlink)
            return {live_path, result};
    }

    return {"", CollisionResult::Safe};
}
