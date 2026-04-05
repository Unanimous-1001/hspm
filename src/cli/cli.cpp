#include "cli.hpp"

void print_usage() {
    std::cout << " \n HSPM - Hybrid Symlink Package Manager\n";
    std::cout << " ======================================\n\n";
    std::cout << " Usage: hspm <command> [options]\n\n";
    std::cout << " Commands:\n";
    std::cout << "   init                     Initialize store and database\n";
    std::cout << "   install  <name>          Install a package and its dependencies\n";
    std::cout << "   uninstall <name>         Remove a managed package\n";
    std::cout << "   adopt    <name> <ver>    Record a pre-installed package\n";
    std::cout << "   show     <name>          Print recipe fields\n";
    std::cout << "   resolve  <name>          Print dependency install order\n";
    std::cout << "   upgrade  <name> <ver>    Upgrade to a new version\n";
    std::cout << "   prune    <name> <ver>    Delete an inactive store dir\n";
    std::cout << "   log                      Show recent operations\n";
    std::cout << "   verify                   Check symlinks and database\n";
    std::cout << "   sync     [version]       Sync recipes from BLFS book\n";
    std::cout << "   list                     Show all installed packages\n";
    std::cout << "   activate <name> <ver>    Swap symlinks to a stored version\n";
    std::cout << "   help                     Show this help message\n\n";
    std::cout << " Options:\n";
    std::cout << "   --force-symlink          Replace unknown symlinks\n";
    std::cout << "   --adopt-collision        Adopt colliding real files\n\n";
}

CliArgs parse_args(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        exit(0);
    }

    CliArgs args;
    args.subcommand = argv[1];

    for (int i = 2; i < argc; ++i) {
        string a = argv[i];
        if      (a == "--force-symlink")    args.force_symlink   = true;
        else if (a == "--adopt-collision")  args.adopt_collision = true;
        else if (args.package_name.empty()) args.package_name    = a;
        else if (args.version.empty())      args.version         = a;
    }

    return args;
}
