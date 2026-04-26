#include "package.hpp"
#include "config.hpp"

static string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end   = s.find_last_not_of(" \t");
    if (start == string::npos) return "";
    return s.substr(start, end - start + 1);
}

Package load_recipe(const string& name, const string& recipe_dir) {
    string path = recipe_dir + name + ".recipe";
    std::ifstream file(path);
    if (!file) throw runtime_error("Recipe not found: " + path);

    Package pkg;
    string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t colon = line.find(':');
        if (colon == string::npos) continue;

        string key = trim(line.substr(0, colon));
        string val = trim(line.substr(colon + 1));

        if      (key == "name")             pkg.name             = val;
        else if (key == "version")          pkg.version          = val;
        else if (key == "builder")          pkg.builder          = val;
        else if (key == "sha256")           pkg.sha256           = val;
        else if (key == "md5")              pkg.md5              = val;
        else if (key == "url")              pkg.url              = val;
        else if (key == "url_fallback")     pkg.url_fallback     = val;
        else if (key == "configure_args")   pkg.configure_args   = val;
        else if (key == "bootstrap_script") pkg.bootstrap_script = val;
        else if (key == "build_cmd")        pkg.build_cmd        = val;
        else if (key == "notes")            pkg.notes            = val;
        else if (key == "patches")          pkg.patches          = val;
        else if (key == "patch_cmds")       pkg.patch_cmds       = val;
        else if (key == "extra_urls")       pkg.extra_urls       = val;
        else if (key == "complex")          pkg.complex          = (val == "true");
        else if (key == "depends") {
            std::istringstream iss(val);
            string dep;
            while (iss >> dep) pkg.depends.push_back(dep);
        }
        else if (key == "recommends") {
            std::istringstream iss(val);
            string dep;
            while (iss >> dep) pkg.recommends.push_back(dep);
        }//kernel hint is new, dlt if not work
        else if (key == "kernel_hint") {
			pkg.kernel_hint = val;
			while (std::getline(file, line)) {
				if (line.empty() || line[0] != ' ') break;
				pkg.kernel_hint += "\n" + line.substr(2);
			}
		}
    }

    if (pkg.name.empty())    throw runtime_error("Recipe missing 'name': "    + path);
    if (pkg.version.empty()) throw runtime_error("Recipe missing 'version': " + path);
    if (pkg.builder.empty()) throw runtime_error("Recipe missing 'builder': " + path);
	if (pkg.sha256.empty() && pkg.md5.empty())
		throw runtime_error("Recipe missing checksum (sha256 or md5): " + path);
    if (pkg.url.empty())     throw runtime_error("Recipe missing 'url': "     + path);

    return pkg;
}

void save_recipe(const Package& pkg, const string& recipe_dir) {
    string path = recipe_dir + pkg.name + ".recipe";
    std::ofstream f(path);
    if (!f) throw runtime_error("Cannot write recipe: " + path);

    f << "# " << pkg.name << " " << pkg.version
      << " — updated by hspm interactive install\n";
    f << "name:    " << pkg.name    << "\n";
    f << "version: " << pkg.version << "\n";
    f << "builder: " << pkg.builder << "\n";

    if (!pkg.sha256.empty())
        f << "sha256:  " << pkg.sha256 << "\n";
    if (!pkg.md5.empty())
        f << "md5:     " << pkg.md5 << "\n";

    f << "url:     " << pkg.url << "\n";

    if (!pkg.url_fallback.empty())
        f << "url_fallback: " << pkg.url_fallback << "\n";
    if (!pkg.depends.empty()) {
        f << "depends:";
        for (const auto& d : pkg.depends) f << " " << d;
        f << "\n";
    }
    if (!pkg.recommends.empty()) {
        f << "recommends:";
        for (const auto& r : pkg.recommends) f << " " << r;
        f << "\n";
    }
    if (!pkg.configure_args.empty())
        f << "configure_args: " << pkg.configure_args << "\n";
    if (!pkg.build_cmd.empty())
        f << "build_cmd: " << pkg.build_cmd << "\n";
    if (pkg.complex)
        f << "complex: true\n";
    if (!pkg.notes.empty())
        f << "notes:   " << pkg.notes << "\n";
    if (!pkg.patches.empty())
        f << "patches: " << pkg.patches << "\n";
    if (!pkg.patch_cmds.empty())
        f << "patch_cmds: " << pkg.patch_cmds << "\n";
    if (!pkg.extra_urls.empty())
        f << "extra_urls: " << pkg.extra_urls << "\n";
    if (!pkg.bootstrap_script.empty())
        f << "bootstrap_script: " << pkg.bootstrap_script << "\n";
}
