#include "collision.hpp"
#include "db/database.hpp"

// Strip store prefix and replace with live root
// e.g. /home/Dev/Projects/hspm/test-store/zlib-1.3.1/usr/lib/libz.a
//   -> /usr/lib/libz.a
static string store_to_live(const string& store_path,
                             const string& store_root,
                             const string& live_root) {
    // store_path starts with store_root + "/<n>-<version>/usr"
    // we want everything after store_root + "/<n>-<version>"
    // find the 2nd slash after store_root
    string prefix = store_root;
    if (prefix.back() != '/') prefix += '/';

    // skip past store_root
    size_t pos = store_path.find(prefix);
    if (pos == string::npos)
        throw runtime_error("Path not under store root: " + store_path);

    // skip past the package dir (e.g. "zlib-1.3.1/")
    size_t next_slash = store_path.find('/', prefix.size());
    if (next_slash == string::npos)
        throw runtime_error("Unexpected store path format: " + store_path);

    // everything after "zlib-1.3.1" is the relative install path
    string relative = store_path.substr(next_slash);
    // relative = "/usr/lib/libz.a"

    // replace leading /usr with live_root
    // strip the /usr prefix since live_root IS /usr
    size_t usr_pos = relative.find("/usr");
    if (usr_pos != string::npos)
        relative = relative.substr(usr_pos + 4);
    // relative = "/lib/libz.a"

    return live_root + relative;
    // "/usr/lib/libz.a"
}

CollisionResult check_collision(const string& target_path,
                                bool force_symlink,
                                bool adopt_collision) {
    if (!fs::exists(target_path) && !fs::is_symlink(target_path))
        return CollisionResult::Safe;

    if (fs::is_symlink(target_path)) {
        // TODO: replace with db_get_file_owner once added
        if (force_symlink) {
            // --force-symlink: remove unknown symlink and proceed
            fs::remove(target_path);
            std::cout << "  [force] removed unknown symlink: "
                      << target_path << "\n";
            return CollisionResult::Safe;
        }
        return CollisionResult::UnknownSymlink;
    }

    // real file
    if (adopt_collision) {
        // --adopt-collision: record this file as adopted and proceed
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
