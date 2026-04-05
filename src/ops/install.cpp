#include "install.hpp"
#include "db/database.hpp"
#include "core/package.hpp"
#include "core/resolver.hpp"
#include "fetch/fetcher.hpp"
#include "store/manifest.hpp"
#include "store/collision.hpp"
#include "store/symlink.hpp"
#include "validate/rpath.hpp"
#include "config.hpp"

static const string STORE    = HSPM_STORE;
static const string BUILD    = HSPM_BUILD;
static const string LIVE     = HSPM_LIVE;
static const string BUILDERS = HSPM_BUILDERS;

static void install_single(const string& pkg_name,
                           bool force_symlink   = false,
                           bool adopt_collision = false) {
    Package pkg = load_recipe(pkg_name);
    std::cout << "\n=== Installing " << pkg.name
              << " " << pkg.version << " ===\n";

    // check if already installed
    PackageRecord existing = db_get_package(pkg.name);
    if (existing.id != -1 && existing.state == "active") {
        std::cout << "[skip] " << pkg.name
                  << " is already installed\n";
        return;
    }

    // Step 1: fetch
    string tarball = fetch_tarball(pkg);
    std::cout << "[fetch] OK\n";

    // Step 2: extract
    string src_dir = BUILD + pkg.name + "-" + pkg.version;
    fs::create_directories(src_dir);
    string extract_cmd = "tar xf " + tarball + " -C " + BUILD;
    if (system(extract_cmd.c_str()) != 0)
        throw runtime_error("Extraction failed");
    std::cout << "[extract] OK\n";

    // Step 3: build using the correct builder
    string store_dir = STORE + pkg.name + "-" + pkg.version;
    fs::create_directories(store_dir);
    string build_cmd;
	if (!pkg.build_cmd.empty()) {
		// custom build command — substitute variables
		build_cmd = pkg.build_cmd;
		// replace placeholders
		auto replace_all = [](string& s,
							  const string& from,
							  const string& to) {
			size_t pos = 0;
			while ((pos = s.find(from, pos)) != string::npos) {
				s.replace(pos, from.size(), to);
				pos += to.size();
			}
		};
		replace_all(build_cmd, "{srcdir}",  src_dir);
		replace_all(build_cmd, "{destdir}", store_dir);
		replace_all(build_cmd, "{prefix}",  "/usr");
		replace_all(build_cmd, "{store}",   STORE);
	} else {
		string builder_script = BUILDERS + "/build-" + pkg.builder + ".sh";
		build_cmd =
			builder_script +
			" --name "    + pkg.name +
			" --version " + pkg.version +
			" --srcdir "  + src_dir +
			" --destdir " + store_dir +
			" --prefix /usr";
		if (!pkg.configure_args.empty())
			build_cmd += " --extra-args \"" + pkg.configure_args + "\"";
	}
	if (system(build_cmd.c_str()) != 0)
		throw runtime_error("Build failed for: " + pkg.name);
    std::cout << "[build] OK\n";

    // Step 4: scan manifest
    vector<string> files = scan_manifest(store_dir);
    std::cout << "[manifest] " << files.size() << " files\n";
    
    // Step 5: RPATH validation
        std::cout << "[rpath] Validating binaries...\n";
        vector<RpathResult> rpath_results = validate_rpath(store_dir);
        bool has_fatal = false;
        for (const auto& r : rpath_results) {
            if (r.status == RpathStatus::Fatal) {
                std::cerr << "  [FATAL] " << r.binary << "\n"
                          << "         " << r.message << "\n";
                has_fatal = true;
            } else if (r.status == RpathStatus::Warning) {
                std::cout << "  [WARN]  " << r.binary << "\n"
                          << "         " << r.message << "\n";
            }
        }
        if (has_fatal)
            throw runtime_error(
                "RPATH validation failed for: " + pkg_name +
                "\nBinaries have unresolved library dependencies.");
        std::cout << "[rpath] OK\n";

    // Step 6: collision check
    auto [collision_path, collision_result] =
            check_manifest_collisions(files, STORE, LIVE,
                force_symlink, adopt_collision);
    if (!collision_path.empty())
        throw runtime_error("Collision at: " + collision_path);
    std::cout << "[collision] OK\n";

    // Step 7: register in DB as partial
    int pkg_id = db_insert_package(
        pkg.name, pkg.version,
        "managed", "partial", store_dir);
    
    // record dependency edges in the database
        for (const auto& dep_name : pkg.depends) {
            PackageRecord dep_rec = db_get_package(dep_name);
            if (dep_rec.id != -1)
                db_insert_dependency(pkg_id, dep_rec.id);
        }

    // Step 8: symlink transaction
    std::cout << "[symlink] Linking files...\n";
    bool ok = symlink_transaction(pkg_id, files, STORE, LIVE);
    if (!ok) throw runtime_error("Symlink transaction failed");

    db_log("install", pkg_id,
           "installed " + pkg.name + "-" + pkg.version);

    std::cout << "[done] " << pkg.name
              << " " << pkg.version
              << " installed successfully\n";
}

static void install_stub(const string& pkg_name) {
    Package pkg = load_recipe(pkg_name);

    if (pkg.bootstrap_script.empty())
        throw runtime_error(
            "No bootstrap_script in recipe for: " + pkg_name);

    string store_dir = STORE + pkg.name + "-" + pkg.version + "-stub";
    fs::create_directories(store_dir);

    // run the bootstrap script instead of the normal builder
    string build_cmd =
        HSPM_ROOT + "/" + pkg.bootstrap_script +
        " --name "    + pkg.name +
        " --version " + pkg.version +
        " --srcdir "  + BUILD + pkg.name + "-" + pkg.version +
        " --destdir " + store_dir +
        " --prefix /usr";

    if (system(build_cmd.c_str()) != 0)
        throw runtime_error("Stub build failed for: " + pkg_name);

    // register stub in DB
    int pkg_id = db_insert_package(
        pkg.name + "-stub", pkg.version,
        "managed", "partial", store_dir);

    vector<string> files = scan_manifest(store_dir);
    bool ok = symlink_transaction(pkg_id, files, STORE, LIVE);
    if (!ok) throw runtime_error("Stub symlink transaction failed");

    std::cout << "[circular] Stub installed for " << pkg_name << "\n";
}

static void install_rebuild(const string& pkg_name) {
    // uninstall the stub first
    string stub_name = pkg_name + "-stub";
    PackageRecord stub_rec = db_get_package(stub_name);
    if (stub_rec.id != -1) {
        vector<string> stub_files = db_get_files(stub_rec.id);
        for (const auto& path : stub_files)
            if (fs::is_symlink(path)) fs::remove(path);
        if (!stub_rec.store_path.empty())
            fs::remove_all(stub_rec.store_path);
        db_delete_package(stub_rec.id);
        std::cout << "[circular] Stub removed for " << pkg_name << "\n";
    }

    // now do a full install
    install_single(pkg_name, false, false);
}

void run_install(const string& pkg_name, const CliArgs& args) {
    if (pkg_name.empty())
        throw runtime_error("Usage: hspm install <name>");

    // ensure live dirs exist
    fs::create_directories(LIVE + "/bin");
    fs::create_directories(LIVE + "/lib");
    fs::create_directories(LIVE + "/include");

    // resolve full dependency queue
    vector<string> queue = get_install_queue(pkg_name);

    std::cout << "Packages to install (" << queue.size() << "):\n";
    for (const auto& n : queue)
        std::cout << "  " << n << "\n";

    // install each in order
	// handle circular dep markers: "pkg:stub" and "pkg:rebuild"
	for (const auto& entry : queue) {
		size_t colon = entry.find(':');
		if (colon != string::npos) {
			string pkg_name = entry.substr(0, colon);
			string mode     = entry.substr(colon + 1);

			if (mode == "stub") {
				std::cout << "\n[circular] Building stub for "
						  << pkg_name << "...\n";
				install_stub(pkg_name);
			} else if (mode == "rebuild") {
				std::cout << "\n[circular] Rebuilding "
						  << pkg_name << " fully...\n";
				install_rebuild(pkg_name);
			}
		} else {
            install_single(entry,
                args.force_symlink, args.adopt_collision);
        }
	}

    std::cout << "\nDone. " << queue.size()
              << " package(s) installed.\n";
}
