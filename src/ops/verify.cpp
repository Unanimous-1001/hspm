#include "verify.hpp"
#include "config.hpp"
#include "db/database.hpp"

void run_verify() {
    std::cout << "Verifying HSPM installation...\n\n";

    int ok_count      = 0;
    int broken_count  = 0;
    int missing_count = 0;

    vector<PackageRecord> packages = db_get_all_packages();

    for (const auto& pkg : packages) {
        if (pkg.type != "managed" || pkg.state != "active")
            continue;

        vector<string> files = db_get_files(pkg.id);

        for (const auto& path : files) {
            if (!fs::exists(path) && !fs::is_symlink(path)) {
                std::cout << "  MISSING  [" << pkg.name << "] "
                          << path << "\n";
                missing_count++;
            } else if (fs::is_symlink(path)) {
                fs::path target = fs::read_symlink(path);
                if (!fs::exists(target)) {
                    std::cout << "  BROKEN   [" << pkg.name << "] "
                              << path << " -> "
                              << target.string() << "\n";
                    broken_count++;
                } else {
                    ok_count++;
                }
            } else {
                ok_count++;
            }
        }
    }

    std::cout << "\nResults:\n"
              << "  OK:      " << ok_count      << "\n"
              << "  Broken:  " << broken_count  << "\n"
              << "  Missing: " << missing_count << "\n";

    if (broken_count == 0 && missing_count == 0)
        std::cout << "\nAll symlinks are valid.\n";
    else
        std::cout << "\nIssues found. Run 'hspm rollback <name>'"
                  << " to fix broken packages.\n";
}
