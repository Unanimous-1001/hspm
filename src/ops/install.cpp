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
#include "cli/prompt.hpp"
#include "core/package.hpp"

static const string STORE    = HSPM_STORE;
static const string BUILD    = HSPM_BUILD;
static const string LIVE     = HSPM_LIVE;
static const string BUILDERS = HSPM_BUILDERS;
static vector<pair<string, string>> collect_kernel_hints(const vector<string>& queue) {
    vector<pair<string, string>> hints;
    for (const auto& entry : queue) {
        string pkg_name = entry;
        size_t colon = entry.find(':');
        if (colon != string::npos)
            pkg_name = entry.substr(0, colon);
        
        PackageRecord rec = db_get_package(pkg_name);
        if (rec.id != -1 && rec.state == "active")
            continue;
        
        try {
            Package pkg = load_recipe(pkg_name);
            if (!pkg.kernel_hint.empty())
                hints.push_back({pkg_name, pkg.kernel_hint});
        } catch (...) {
        }
    }
    return hints;
}

static void install_single(const string& pkg_name,
                           bool force_symlink   = false,
                           bool adopt_collision = false,
                           bool interactive     = false) {
    Package pkg = load_recipe(pkg_name);
    std::cout << "\n=== Installing " << pkg.name
              << " " << pkg.version << " ===\n";

    // lil check
    PackageRecord existing = db_get_package(pkg.name);
    if (existing.id != -1 && existing.state == "active") {
        std::cout << "[skip] " << pkg.name
                  << " is already installed\n";
        return;
    }

    // 1.fetch
    string tarball = fetch_tarball(pkg);
    std::cout << "[fetch] OK\n";

    // 2. extract
    string extract_cmd = "tar xf " + tarball + " -C " + BUILD;
    if (system(extract_cmd.c_str()) != 0)
        throw runtime_error("Extraction failed");
    std::cout << "[extract] OK\n";

    string src_dir = BUILD + pkg.name + "-" + pkg.version;
    if (!fs::exists(src_dir)) {
        for (auto& entry : fs::directory_iterator(BUILD)) {
            if (fs::is_directory(entry)) {
                string dirname = entry.path().filename().string();
                if (dirname.find(pkg.version) != string::npos) {
                    src_dir = entry.path().string();
                    std::cout << "[extract] Source dir: " << src_dir << "\n";
                    break;
                }
            }
        }
    }
    
	// 3 .prompt when needed
	if (interactive && (pkg.complex || !pkg.build_cmd.empty())) {
		try {
			if (!pkg.patches.empty()) {
				std::cout << "[patches] Downloading patches...\n";
				string patch_dir = BUILD;
				std::istringstream pss(pkg.patches);
				string patch_url;
				while (pss >> patch_url) {
					string patch_file = patch_url.substr(
						patch_url.rfind('/') + 1);
					string patch_dest = patch_dir + patch_file;
					string wget_cmd = "wget -O " + patch_dest
									+ " " + patch_url;
					if (system(wget_cmd.c_str()) == 0)
						std::cout << "  downloaded: "
								  << patch_file << "\n";
					else
						std::cerr << "  failed: " << patch_url << "\n";
				}
			}

			BuildOverride ov = interactive_prompt(pkg, src_dir);

			if (!ov.patch_cmds.empty()) {
				std::cout << "[patches] Applying patches...\n";
				string patch_cmd = "cd " + src_dir + " && "
								 + ov.patch_cmds;
				if (system(patch_cmd.c_str()) != 0)
					throw runtime_error("Patch failed");
				std::cout << "[patches] OK\n";
			} else if (!pkg.patch_cmds.empty()) {
				std::cout << "[patches] Applying patches...\n";
				string patch_cmd = "cd " + src_dir + " && "
								 + pkg.patch_cmds;
				if (system(patch_cmd.c_str()) != 0)
					throw runtime_error("Patch failed");
				std::cout << "[patches] OK\n";
			}

			if (!ov.build_cmd.empty()) {
				pkg.build_cmd = ov.build_cmd;
			}

			if (ov.save_to_recipe) {
				pkg.complex   = false;
				save_recipe(pkg, HSPM_RECIPES);
				std::cout << "[recipe] Saved build commands.\n";
			}
		} catch (const runtime_error& e) {
			string msg = e.what();
			if (msg.find("User skipped") != string::npos)
				throw;
			std::cerr << "[prompt] " << msg << "\n";
		}
	} else if (!interactive && pkg.complex
			   && pkg.build_cmd.empty()) {
		std::cout << "[warn] " << pkg.name
				  << " is a complex package with no saved build commands.\n"
				  << "       Attempting default build — may fail.\n"
				  << "       Use --interactive to configure manually.\n";
	}

	// 4. build using corect builder
    string store_dir = STORE + pkg.name + "-" + pkg.version;
    fs::create_directories(store_dir);
    string build_cmd;
	if (!pkg.build_cmd.empty()) {
		build_cmd = pkg.build_cmd;
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

    // 5. scan manifest
    vector<string> files = scan_manifest(store_dir);
    std::cout << "[manifest] " << files.size() << " files\n";
    
    // 6. RPATH valid
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

    // 7. collision check
    auto [collision_path, collision_result] =
            check_manifest_collisions(files, STORE, LIVE,
                force_symlink, adopt_collision);
    if (!collision_path.empty())
        throw runtime_error("Collision at: " + collision_path);
    std::cout << "[collision] OK\n";

    // 8 .register in DB as partial
    int pkg_id = db_insert_package(
        pkg.name, pkg.version,
        "managed", "partial", store_dir);
    
        for (const auto& dep_name : pkg.depends) {
            PackageRecord dep_rec = db_get_package(dep_name);
            if (dep_rec.id != -1)
                db_insert_dependency(pkg_id, dep_rec.id);
        }
    //.hspm-meta sidecars- for disaster recovery
    save_recipe(pkg, store_dir + "/");
    string sidecar_src  = store_dir + "/" + pkg.name + ".recipe";
    string sidecar_dest = store_dir + "/.hspm-meta";
    if (fs::exists(sidecar_src))
        fs::rename(sidecar_src, sidecar_dest);
    
    // 9. symlink transaction
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

    string build_cmd =
        HSPM_ROOT + "/" + pkg.bootstrap_script +
        " --name "    + pkg.name +
        " --version " + pkg.version +
        " --srcdir "  + BUILD + pkg.name + "-" + pkg.version +
        " --destdir " + store_dir +
        " --prefix /usr";

    if (system(build_cmd.c_str()) != 0)
        throw runtime_error("Stub build failed for: " + pkg_name);

    int pkg_id = db_insert_package(
        pkg.name + "-stub", pkg.version,
        "managed", "partial", store_dir);

    vector<string> files = scan_manifest(store_dir);
    bool ok = symlink_transaction(pkg_id, files, STORE, LIVE);
    if (!ok) throw runtime_error("Stub symlink transaction failed");

    std::cout << "[circular] Stub installed for " << pkg_name << "\n";
}

static void install_rebuild(const string& pkg_name) {
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

    install_single(pkg_name, false, false);
}

void run_install(const string& pkg_name, const CliArgs& args) {
    if (pkg_name.empty())
        throw runtime_error("Usage: hspm install <name>");

    fs::create_directories(LIVE + "/bin");
    fs::create_directories(LIVE + "/lib");
    fs::create_directories(LIVE + "/include");

    vector<string> queue = get_install_queue(pkg_name);

    std::cout << "Packages to install (" << queue.size() << "):\n";
    
    auto kernel_hints = collect_kernel_hints(queue);
	if (!kernel_hints.empty()) {
		std::cout << "\n*** KERNEL CONFIGURATION NOTES ***\n";
		std::cout << "The following packages mention specific kernel options in BLFS.\n";
		std::cout << "Your kernel may need to be reconfigured for these packages to work.\n\n";
		
		for (const auto& [name, hint] : kernel_hints) {
			std::cout << "[" << name << "]\n";
			std::cout << hint << "\n\n";
		}
		
		if (!args.yes) {
			std::cout << "Continue with installation? [y/N] ";
			string answer;
			std::getline(std::cin, answer);
			if (answer != "y" && answer != "Y") {
				std::cout << "\nInstallation aborted. No changes were made.\n";
				std::cout << "Please reconfigure your kernel and try again.\n";
				return;
			}
		} else {
			std::cout << "[notice] --yes supplied: proceeding automatically.\n";
		}
	}
	
    for (const auto& n : queue)
        std::cout << "  " << n << "\n";

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
                args.force_symlink, args.adopt_collision,
                args.interactive);
        }
	}

    std::cout << "\nDone. " << queue.size()
              << " package(s) installed.\n";
}
