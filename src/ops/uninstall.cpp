#include "uninstall.hpp"
#include "db/database.hpp"
#include "config.hpp"

static const string LIVE = HSPM_LIVE;

void run_uninstall(const string& pkg_name) {
    if (pkg_name.empty())
        throw runtime_error("Usage: hspm uninstall <name>");

    PackageRecord rec = db_get_package(pkg_name);
    if (rec.id == -1)
        throw runtime_error("Package not found: " + pkg_name);

    if (rec.type == "adopted")
        throw runtime_error(
            "Cannot uninstall adopted package: " + pkg_name +
            "\nAdopted packages are managed by LFS, not HSPM.");
    
    vector<string> dependents = db_get_dependents(pkg_name);
    if (!dependents.empty()) {
        string msg = "Cannot uninstall " + pkg_name
                   + " — the following packages depend on it:\n";
        for (const auto& d : dependents)
            msg += "  " + d + "\n";
        msg += "Uninstall those first.";
        throw runtime_error(msg);
    }

    std::cout << "Uninstalling " << rec.name
              << " " << rec.version << "...\n";

    vector<string> files = db_get_files(rec.id);
    int removed = 0;
    for (const auto& path : files) {
        if (fs::is_symlink(path)) {
            fs::remove(path);
            std::cout << "  removed: " << path << "\n";
            removed++;
        }
    }

    if (!rec.store_path.empty() && fs::exists(rec.store_path)) {
        fs::remove_all(rec.store_path);
        std::cout << "  store removed: " << rec.store_path << "\n";
    }

    db_log("uninstall", rec.id,
           "uninstalled " + rec.name + "-" + rec.version);
    db_delete_package(rec.id);

    std::cout << "Done. Removed " << removed << " symlinks.\n";
}
