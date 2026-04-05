#include "activate.hpp"
#include "db/database.hpp"
#include "store/symlink.hpp"
#include "store/manifest.hpp"
#include "config.hpp"

static const string STORE = HSPM_STORE;
static const string LIVE  = HSPM_LIVE;

void run_activate(const string& pkg_name, const string& version) {
    if (pkg_name.empty() || version.empty())
        throw runtime_error("Usage: hspm activate <name> <version>");

    // find the target version in the database
    PackageRecord target = db_get_package_version(pkg_name, version);
    if (target.id == -1)
        throw runtime_error(
            "Version not found in database: " + pkg_name
            + " " + version + "\n"
            "Run 'hspm list' to see available versions.");

    if (target.state == "active") {
        std::cout << pkg_name << " " << version
                  << " is already active.\n";
        return;
    }

    if (target.state == "partial")
        throw runtime_error(
            pkg_name + " " + version + " is in partial state. "
            "Run 'hspm rollback " + pkg_name + "' first.");

    // verify store directory still exists
    if (target.store_path.empty() || !fs::exists(target.store_path))
        throw runtime_error(
            "Store directory missing for " + pkg_name
            + " " + version + ".\n"
            "Cannot activate — store was pruned.");

    // find currently active version
    PackageRecord current = db_get_package(pkg_name);

    std::cout << "Activating " << pkg_name << " " << version;
    if (current.id != -1 && current.state == "active")
        std::cout << " (replacing " << current.version << ")";
    std::cout << "\n";

    // remove current active symlinks
    if (current.id != -1 && current.state == "active") {
        vector<string> old_files = db_get_files(current.id);
        for (const auto& path : old_files) {
            if (fs::is_symlink(path)) {
                fs::remove(path);
            }
        }
        db_set_package_inactive(current.id);
        std::cout << "[activate] Old version deactivated\n";
    }

    // activate the target version
    vector<string> new_files = scan_manifest(target.store_path);
    bool ok = symlink_transaction(target.id, new_files, STORE, LIVE);
    if (!ok) {
        // restore old version if possible
        if (current.id != -1) {
            vector<string> old_files = db_get_files(current.id);
            symlink_transaction(current.id, old_files, STORE, LIVE);
            db_set_package_state(current.id, "active");
            std::cerr << "[activate] Failed — old version restored\n";
        }
        throw runtime_error("Activation failed");
    }

    db_log("activate", target.id,
           "activated " + pkg_name + "-" + version);

    std::cout << "[done] " << pkg_name << " "
              << version << " is now active.\n";
}
