#include "common.hpp"
#include "config.hpp"
#include "cli/cli.hpp"
#include "db/database.hpp"
#include "core/package.hpp"
#include "core/resolver.hpp"
#include "ops/install.hpp"
#include "ops/uninstall.hpp"
#include "ops/adopt.hpp"
#include "ops/rollback.hpp"
#include "ops/log.hpp"
#include "ops/verify.hpp"
#include "ops/upgrade.hpp"
#include "ops/list.hpp"
#include "ops/activate.hpp"
#include "ops/rescue.hpp"

static const string DB_PATH = HSPM_DB;

int main(int argc, char* argv[]) {
    CliArgs args = parse_args(argc, argv);

    if (args.subcommand == "help" || args.subcommand == "--help" ||
        args.subcommand == "-h") {
        print_usage();
        return 0;
    }
    
    if (args.subcommand == "init") {
        fs::create_directories(HSPM_STORE);
        fs::create_directories(HSPM_BUILD);
        fs::create_directories(HSPM_ROOT + "/logs");
        fs::create_directories(HSPM_ROOT + "/db");
        fs::create_directories(HSPM_ROOT + "/recipes");
        fs::create_directories(HSPM_ROOT + "/builders");
        fs::create_directories(HSPM_ROOT + "/tools");
        fs::create_directories(HSPM_DISTFILES);
        db_open(DB_PATH);
        db_close();
        std::cout << "HSPM initialized.\n";
        return 0;
    }
    
    try {
        db_open(DB_PATH);
        db_set_log_path(HSPM_LOG);
    } catch (const std::exception& e) {
        std::cerr << "Failed to open database: " << e.what() << "\n"
                  << "Run 'hspm init' first.\n";
        return 1;
    }

    int exit_code = 0;

    try {
        if (args.subcommand == "install") {
            run_install(args.package_name, args);
        }
        else if (args.subcommand == "uninstall") {
            run_uninstall(args.package_name);
        }
        else if (args.subcommand == "adopt") {
            run_adopt(args.package_name, args.version);
        }
        else if (args.subcommand == "show") {
            if (args.package_name.empty())
                throw runtime_error("Usage: hspm show <name>");
            Package pkg = load_recipe(args.package_name);
            std::cout << "Name:    " << pkg.name    << "\n"
                      << "Version: " << pkg.version << "\n"
                      << "Builder: " << pkg.builder << "\n"
                      << "URL:     " << pkg.url     << "\n"
                      << "Depends:";
            for (const auto& d : pkg.depends)
                std::cout << " " << d;
            std::cout << "\n";
        }
        else if (args.subcommand == "resolve") {
            if (args.package_name.empty())
                throw runtime_error("Usage: hspm resolve <name>");
            vector<string> queue = get_install_queue(args.package_name);
            std::cout << "Install order for "
                      << args.package_name << ":\n";
            for (const auto& n : queue)
                std::cout << "  " << n << "\n";
        }
        else if (args.subcommand == "log") {
            string pkg_filter;
            string op_filter;
            string filter = args.package_name;
            if (filter == "install"   || filter == "uninstall" ||
                filter == "adopt"     || filter == "upgrade"   ||
                filter == "rollback"  || filter == "activate"  ||
                filter == "sync")
                op_filter = filter;
            else
                pkg_filter = filter;
            run_log(pkg_filter, op_filter);
        }
        else if (args.subcommand == "verify") {
            run_verify();
        }
        else if (args.subcommand == "rollback") {
            run_rollback(args.package_name);
        }
        else if (args.subcommand == "upgrade") {
            run_upgrade(args.package_name, args.version);
        }
        else if (args.subcommand == "prune") {
            if (args.package_name.empty() || args.version.empty())
                throw runtime_error(
                    "Usage: hspm prune <name> <version>");
            PackageRecord rec = db_get_package_version(
                args.package_name, args.version);
            if (rec.id == -1)
                throw runtime_error(
                    "Not found: " + args.package_name
                    + "-" + args.version);
            if (!rec.store_path.empty()
                && fs::exists(rec.store_path)) {
                fs::remove_all(rec.store_path);
                std::cout << "Removed store: "
                          << rec.store_path << "\n";
            }
            db_delete_inactive_package(
                args.package_name, args.version);
            std::cout << "Pruned " << args.package_name
                      << " " << args.version << "\n";
        }
        else if (args.subcommand == "sync") {
            string book_version = args.version.empty()
                                ? "stable" : args.version;
            string cmd =
                "python3 " + HSPM_SCRAPER +
                " --book-version " + book_version;
            std::cout << "Syncing recipes from BLFS book ("
                      << book_version << ")...\n";
            int ret = system(cmd.c_str());
            if (ret != 0)
                throw runtime_error("Sync failed");
        }
        else if (args.subcommand == "list") {
            run_list();
        }
        else if (args.subcommand == "activate") {
            run_activate(args.package_name, args.version);
        }
        else if (args.subcommand == "rescue") {
            string flag = args.package_name.empty()
                        ? "" : args.package_name;
            run_rescue(flag, args.yes, args.force);
        }

        else {
            std::cerr << "Unknown command: "
                      << args.subcommand << "\n";
            print_usage();
            exit_code = 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        exit_code = 1;
    }

    db_close();
    return exit_code;
}
