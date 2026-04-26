#include "prompt.hpp"
#include "config.hpp"
#include <fstream>
#include <unistd.h>

static string read_line() {
    string line;
    std::getline(std::cin, line);
    return line;
}

static bool ask_yes_no(const string& prompt) {
    std::cout << prompt << " [y/n]: ";
    string answer = read_line();
    return (answer == "y" || answer == "Y");
}

static string show_editor(const string& initial) {
    string tmp = "/tmp/hspm-edit-" + std::to_string(getpid()) + ".sh";
    std::ofstream f(tmp);
    f << initial;
    f.close();
    
    string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "nano";
    string cmd = editor + " " + tmp;
    system(cmd.c_str());
    
    std::ifstream f2(tmp);
    string result((std::istreambuf_iterator<char>(f2)),
                  std::istreambuf_iterator<char>());
    remove(tmp.c_str());
    return result;
}

BuildOverride interactive_prompt(const Package& pkg, const string& src_dir) {
    BuildOverride ov;
    ov.save_to_recipe = false;
    
    std::cout << "\n=== Interactive build: " << pkg.name << " " << pkg.version << " ===\n";
    
    if (!pkg.notes.empty()) {
        std::cout << "\nBLFS notes:\n  " << pkg.notes << "\n";
    }
    
    if (!pkg.patches.empty()) {
        std::cout << "\nPatches detected:\n";
        std::stringstream ss(pkg.patches);
        string patch;
        while (ss >> patch) {
            std::cout << "  " << patch << "\n";
        }
    } else if (pkg.notes.find("patch -Np") != string::npos) {
        std::cout << "\nPatch referenced in build notes but URL unknown.\n";
        std::cout << "Check BLFS page for patch download URL.\n";
        std::cout << "Enter patch URL (or leave blank to skip): ";
        string patch_url = read_line();
        if (!patch_url.empty()) {
            std::cout << "  Will download: " << patch_url << "\n";
            const_cast<Package&>(pkg).patches = patch_url;
            string patch_file = patch_url.substr(patch_url.rfind('/') + 1);
            const_cast<Package&>(pkg).patch_cmds =
                "patch -Np1 -i ../" + patch_file;
        }
    }
    
    std::cout << "\nCurrent build command:\n";
    if (!pkg.build_cmd.empty()) {
        std::cout << "  " << pkg.build_cmd << "\n";
    } else {
        std::cout << "  (default builder: " << pkg.builder << ")\n";
    }
    
    std::cout << "\nOptions:\n";
    std::cout << "  [1] Use current build command\n";
    std::cout << "  [2] Edit build command\n";
    std::cout << "  [3] Enter custom build command\n";
    std::cout << "  [4] Skip this package\n";
    std::cout << "Choice: ";
    
    string choice = read_line();
    
    if (choice == "2") {
        string current = pkg.build_cmd.empty() ? 
            "cd " + src_dir + " && ./configure --prefix=/usr && make && make install" :
            pkg.build_cmd;
        string edited = show_editor(current);
        ov.build_cmd = edited;
        ov.save_to_recipe = ask_yes_no("Save these commands to recipe for future installs?");
    } else if (choice == "3") {
        std::cout << "Enter build commands (will run in " << src_dir << "):\n";
        ov.build_cmd = read_line();
        ov.save_to_recipe = ask_yes_no("Save these commands to recipe for future installs?");
    } else if (choice == "4") {
        throw runtime_error("User skipped package: " + pkg.name);
    } else {
        if (!pkg.build_cmd.empty()) {
            ov.build_cmd = pkg.build_cmd;
        }
    }
    
    return ov;
}
