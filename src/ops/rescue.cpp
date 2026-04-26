#include "rescue.hpp"
#include "config.hpp"
#include "db/database.hpp"
#include "core/package.hpp"
#include "store/manifest.hpp"
#include "store/symlink.hpp"
#include "store/collision.hpp"
#include "validate/rpath.hpp"
#include <algorithm>

// --rebuild-db

void run_rescue_rebuild_db(bool force) {
    std::cout << "Rebuilding database from store sidecars...\n\n";

    // safety check
    vector<PackageRecord> existing = db_get_all_packages();
    bool has_managed = false;
    for (const auto& p : existing)
        if (p.type == "managed") { has_managed = true; break; }

    if (has_managed && !force) {
        throw runtime_error(
            "Database already contains managed packages.\n"
            "Use --force to override.\n"
            "Run: hspm rescue --rebuild-db --force");
    }

    if (!fs::exists(HSPM_STORE)) {
        throw runtime_error("Store directory not found: " + HSPM_STORE);
    }

    unordered_map<string, int> name_to_id;
    unordered_map<string, Package> name_to_pkg;

    int count = 0;
    for (auto& entry : fs::directory_iterator(HSPM_STORE)) {
        if (!fs::is_directory(entry)) continue;

        string store_dir  = entry.path().string();
        string sidecar    = store_dir + "/.hspm-meta";

        if (!fs::exists(sidecar)) {
            std::cout << "  skip (no sidecar): "
                      << entry.path().filename().string() << "\n";
            continue;
        }

        try {
            string tmp_recipe = "/tmp/hspm-rescue-" +
                entry.path().filename().string() + ".recipe";
            fs::copy_file(sidecar, tmp_recipe,
                fs::copy_options::overwrite_existing);

            string dir_name = entry.path().filename().string();
            size_t ver_pos = string::npos;
            for (int i = dir_name.size() - 1; i >= 0; i--) {
                if (dir_name[i] == '-' && i + 1 < (int)dir_name.size()
                    && isdigit(dir_name[i+1])) {
                    ver_pos = i;
                    break;
                }
            }
            string pkg_name = (ver_pos != string::npos)
                            ? dir_name.substr(0, ver_pos)
                            : dir_name;

            string named_recipe = "/tmp/" + pkg_name + ".recipe";
            fs::rename(tmp_recipe, named_recipe);

            Package pkg = load_recipe(pkg_name, "/tmp/");
            fs::remove(named_recipe);

            int pkg_id = db_insert_package(
                pkg.name, pkg.version,
                "managed", "active", store_dir);

            vector<string> store_files = scan_manifest(store_dir);
            for (const auto& store_path : store_files) {
                string prefix = HSPM_STORE;
                size_t next_slash = store_path.find('/', prefix.size());
                if (next_slash == string::npos) continue;
                string relative = store_path.substr(next_slash);
                size_t usr_pos = relative.find("/usr");
                if (usr_pos != string::npos)
                    relative = relative.substr(usr_pos + 4);
                string live_path = HSPM_LIVE + relative;
                if (fs::is_symlink(live_path))
                    db_insert_file(pkg_id, live_path, true);
            }
            db_log("rescue", pkg_id,
                   "rebuilt from sidecar: " + pkg.name);

            name_to_id[pkg.name]  = pkg_id;
            name_to_pkg[pkg.name] = pkg;

            std::cout << "  restored: " << pkg.name
                      << " " << pkg.version << "\n";
            count++;

        } catch (const std::exception& e) {
            std::cerr << "  error parsing sidecar for "
                      << entry.path().filename().string()
                      << ": " << e.what() << "\n";
        }
    }

    std::cout << "\nResolving dependency edges...\n";
    for (const auto& [name, pkg] : name_to_pkg) {
        int pkg_id = name_to_id[name];
        for (const auto& dep_name : pkg.depends) {
            auto it = name_to_id.find(dep_name);
            if (it != name_to_id.end())
                db_insert_dependency(pkg_id, it->second);
        }
    }

    std::cout << "\nDatabase rebuilt successfully from "
              << count << " store sidecar(s).\n\n";
    std::cout << "WARNING: Adopted packages were not restored.\n";
    std::cout << "Run 'hspm adopt all' to re-adopt your LFS base system.\n";
}

// --fix-links

void run_rescue_fix_links() {
    std::cout << "Fixing symlinks for all active managed packages...\n\n";

    vector<PackageRecord> packages = db_get_all_packages();
    int fixed   = 0;
    int skipped = 0;
    int ok      = 0;

    for (const auto& pkg : packages) {
        if (pkg.type != "managed" || pkg.state != "active") continue;
        if (pkg.store_path.empty()) continue;

        vector<string> store_files = scan_manifest(pkg.store_path);

        for (const auto& store_path : store_files) {
            string prefix = HSPM_STORE;
            size_t next_slash = store_path.find('/', prefix.size());
            if (next_slash == string::npos) continue;
            string relative = store_path.substr(next_slash);
            size_t usr_pos  = relative.find("/usr");
            if (usr_pos != string::npos)
                relative = relative.substr(usr_pos + 4);
            string live_path = HSPM_LIVE + relative;

            if (fs::is_symlink(live_path)) {
                fs::path target = fs::read_symlink(live_path);
                if (target.string() == store_path) {
                    ok++;
                    continue;
                }
            }

            CollisionResult result = check_collision(live_path);
            if (result == CollisionResult::RealFile) {
                std::cerr << "  SKIP (real file): " << live_path << "\n";
                skipped++;
                continue;
            }
            if (result == CollisionResult::UnknownSymlink) {
                std::cerr << "  SKIP (unknown symlink): "
                          << live_path << "\n";
                skipped++;
                continue;
            }

            try {
                fs::create_directories(
                    fs::path(live_path).parent_path());
                if (fs::is_symlink(live_path))
                    fs::remove(live_path);
                fs::create_symlink(store_path, live_path);
                std::cout << "  fixed: " << live_path << "\n";
                fixed++;
            } catch (const std::exception& e) {
                std::cerr << "  error: " << live_path
                          << ": " << e.what() << "\n";
                skipped++;
            }
        }
    }

    std::cout << "\nResults:\n"
              << "  Fixed:   " << fixed   << "\n"
              << "  OK:      " << ok      << "\n"
              << "  Skipped: " << skipped << "\n";
}

// --validate-rpath

void run_rescue_validate_rpath() {
    std::cout << "Validating RPATH for all managed packages...\n\n";

    vector<PackageRecord> packages = db_get_all_packages();
    int fatal_count   = 0;
    int warning_count = 0;
    int ok_count      = 0;

    for (const auto& pkg : packages) {
        if (pkg.type != "managed" || pkg.state != "active") continue;
        if (pkg.store_path.empty() || !fs::exists(pkg.store_path))
            continue;

        vector<RpathResult> results = validate_rpath(pkg.store_path);

        bool pkg_has_issues = false;
        for (const auto& r : results) {
            if (r.status == RpathStatus::Fatal) {
                if (!pkg_has_issues) {
                    std::cout << "  [" << pkg.name << "]\n";
                    pkg_has_issues = true;
                }
                std::cerr << "    FATAL: " << r.binary << "\n"
                          << "           " << r.message << "\n";
                fatal_count++;
            } else if (r.status == RpathStatus::Warning) {
                if (!pkg_has_issues) {
                    std::cout << "  [" << pkg.name << "]\n";
                    pkg_has_issues = true;
                }
                std::cout << "    WARN:  " << r.binary << "\n"
                          << "           " << r.message << "\n";
                warning_count++;
            } else {
                ok_count++;
            }
        }
    }

    std::cout << "\nResults:\n"
              << "  OK:       " << ok_count      << "\n"
              << "  Warnings: " << warning_count << "\n"
              << "  Fatal:    " << fatal_count   << "\n";

    if (fatal_count == 0)
        std::cout << "\nAll binaries have valid library linkage.\n";
    else
        std::cout << "\nFatal issues found. Some packages may not work.\n"
                  << "Try reinstalling affected packages.\n";
}

// dispatch

void run_rescue(const string& flag, bool yes, bool force) {
    if (flag == "--rebuild-db") {
        run_rescue_rebuild_db(force);
    } else if (flag == "--fix-links") {
        run_rescue_fix_links();
    } else if (flag == "--validate-rpath") {
        run_rescue_validate_rpath();
    } else if (flag.empty()) {
        std::cout << "\n hspm rescue — Disaster recovery tools\n"
                  << " =====================================\n\n"
                  << " Flags:\n"
                  << "   --rebuild-db        Rebuild database from store sidecars\n"
                  << "   --fix-links         Restore missing/broken symlinks\n"
                  << "   --validate-rpath    Check library linkage for all packages\n"
                  << "   --prune-orphans     (coming soon)\n\n"
                  << " Example: hspm rescue  --rebuild-db\n"
                  << "          hspm rescue  --fix-links\n\n";
    } else {
        throw runtime_error(" Unknown rescue flag: " + flag +
                            "\n Run 'hspm rescue' to see available flags.");
    }
}
