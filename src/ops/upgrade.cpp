#include "upgrade.hpp"
#include "install.hpp"
#include "db/database.hpp"
#include "store/manifest.hpp"
#include "store/collision.hpp"
#include "store/symlink.hpp"
#include "fetch/fetcher.hpp"
#include "core/package.hpp"
#include "config.hpp"

static const string STORE    = HSPM_STORE;
static const string BUILD    = HSPM_BUILD;
static const string LIVE     = HSPM_LIVE;
static const string BUILDERS = HSPM_BUILDERS;

void run_upgrade(const string& pkg_name, const string& new_version) {
    if (pkg_name.empty() || new_version.empty())
        throw runtime_error("Usage: hspm upgrade <name> <version>");

    PackageRecord current = db_get_package(pkg_name);
    if (current.id == -1)
        throw runtime_error(
            pkg_name + " is not installed. "
            "Use 'hspm install' instead.");

    if (current.version == new_version)
        throw runtime_error(
            pkg_name + " " + new_version
            + " is already the active version.");

    if (current.type == "adopted")
        throw runtime_error(
            "Cannot upgrade adopted package: " + pkg_name);

    std::cout << "Upgrading " << pkg_name
              << " from " << current.version
              << " to "   << new_version << "\n";

    Package pkg = load_recipe(pkg_name);
    if (pkg.version != new_version)
        throw runtime_error(
            "Recipe version (" + pkg.version + ") does not match "
            "requested upgrade version (" + new_version + ").\n"
            "Update recipes/" + pkg_name + ".recipe first.");

    std::cout << "\n[upgrade] Fetching new version...\n";
    string tarball = fetch_tarball(pkg);
    std::cout << "[fetch] OK\n";

    string src_dir = BUILD + pkg.name + "-" + new_version;
    fs::create_directories(src_dir);
    string extract_cmd = "tar xf " + tarball + " -C " + BUILD;
    if (system(extract_cmd.c_str()) != 0)
        throw runtime_error("Extraction failed");
    std::cout << "[extract] OK\n";

    string new_store_dir = STORE + pkg.name + "-" + new_version;
    fs::create_directories(new_store_dir);
    string builder_script = BUILDERS + "/build-" + pkg.builder + ".sh";
    string build_cmd =
        builder_script +
        " --name "    + pkg.name +
        " --version " + new_version +
        " --srcdir "  + src_dir +
        " --destdir " + new_store_dir +
        " --prefix /usr";
    if (!pkg.configure_args.empty())
        build_cmd += " --extra-args \"" + pkg.configure_args + "\"";
    if (system(build_cmd.c_str()) != 0)
        throw runtime_error("Build failed");
    std::cout << "[build] OK\n";

    vector<string> new_files = scan_manifest(new_store_dir);
    std::cout << "[manifest] " << new_files.size() << " files\n";

    auto [collision_path, collision_result] =
        check_manifest_collisions(new_files, STORE, LIVE);
    if (!collision_path.empty())
        throw runtime_error("Collision at: " + collision_path);
    std::cout << "[collision] OK\n";

    int new_pkg_id = db_insert_package(
        pkg.name, new_version,
        "managed", "partial", new_store_dir);

    for (const auto& dep_name : pkg.depends) {
        PackageRecord dep_rec = db_get_package(dep_name);
        if (dep_rec.id != -1)
            db_insert_dependency(new_pkg_id, dep_rec.id);
    }

    std::cout << "[upgrade] Removing old symlinks...\n";
    vector<string> old_files = db_get_files(current.id);
    for (const auto& path : old_files) {
        if (fs::is_symlink(path)) {
            fs::remove(path);
        }
    }
    db_set_package_inactive(current.id);

    std::cout << "[upgrade] Creating new symlinks...\n";
    bool ok = symlink_transaction(new_pkg_id, new_files, STORE, LIVE);
    if (!ok) {
        std::cerr << "[upgrade] Failed — attempting to restore "
                  << "old version...\n";
        symlink_transaction(current.id, old_files, STORE, LIVE);
        db_set_package_state(current.id, "active");
        throw runtime_error("Upgrade failed — old version restored.");
    }

    db_log("upgrade", new_pkg_id,
           "upgraded " + pkg.name
           + " from " + current.version
           + " to "   + new_version);

    std::cout << "\n[done] " << pkg.name
              << " upgraded to " << new_version << "\n"
              << "Old store kept at: " << current.store_path << "\n"
              << "Run 'hspm prune " << pkg.name
              << " " << current.version
              << "' to remove it.\n";
}
