#include "list.hpp"
#include "db/database.hpp"

void run_list() {
    vector<PackageRecord> packages = db_get_all_packages();

    if (packages.empty()) {
        std::cout << "No packages installed.\n";
        return;
    }

    // column widths
    std::cout << "\n"
              << std::left
              << "  " << "NAME                     "
              << "VERSION          "
              << "TYPE      "
              << "STATE\n"
              << "  " << string(65, '-') << "\n";

    for (const auto& pkg : packages) {
        // pad columns manually
        string name    = pkg.name;
        string version = pkg.version;
        string type    = pkg.type;
        string state   = pkg.state;

        // truncate long names
        if (name.size() > 24) name = name.substr(0, 21) + "...";

        std::cout << "  "
                  << name    << string(25 - name.size(),    ' ')
                  << version << string(17 - std::min(version.size(), size_t(16)), ' ')
                  << type    << string(10 - std::min(type.size(),    size_t(9)),  ' ')
                  << state   << "\n";
    }
    std::cout << "\n  Total: " << packages.size() << " package(s)\n\n";
}
