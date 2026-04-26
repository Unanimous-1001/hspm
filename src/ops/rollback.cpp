#include "rollback.hpp"
#include "db/database.hpp"
#include "config.hpp"

static const string STORE = HSPM_STORE;
static const string LIVE  = HSPM_LIVE;

void run_rollback(const string& pkg_name) {
    if (pkg_name.empty())
        throw runtime_error("Usage: hspm rollback <name>");

    PackageRecord rec = db_get_package(pkg_name);
    if (rec.id == -1)
        throw runtime_error("Package not found: " + pkg_name);

    if (rec.state != "partial") {
        std::cout << pkg_name << " is not in a partial state "
                  << "(current state: " << rec.state << ")\n"
                  << "Nothing to roll back.\n";
        return;
    }

    std::cout << "Rolling back partial install of "
              << rec.name << " " << rec.version << "...\n";
    
    int removed = 0;

    auto pending = db_pending_get_done(rec.id);
    if (!pending.empty()) {
        std::cout << "  Found " << pending.size()
                  << " pending symlinks from interrupted transaction\n";
        for (const auto& [store, live] : pending) {
            if (fs::is_symlink(live)) {
                fs::remove(live);
                std::cout << "  removed pending symlink: " << live << "\n";
                removed++;
            }
        }
        db_pending_clear(rec.id);
    }

    vector<string> files = db_get_files(rec.id);
    for (const auto& path : files) {
        if (fs::is_symlink(path)) {
            fs::remove(path);
            std::cout << "  removed symlink: " << path << "\n";
            removed++;
        }
    }

    if (!rec.store_path.empty() && fs::exists(rec.store_path)) {
        fs::remove_all(rec.store_path);
        std::cout << "  removed store: " << rec.store_path << "\n";
    }

    db_log("rollback", rec.id,
           "rolled back partial install of "
           + rec.name + "-" + rec.version);

    std::cout << "Rollback complete. Removed "
              << removed << " symlink(s).\n"
              << "You can retry with: hspm install "
              << rec.name << "\n";
}
