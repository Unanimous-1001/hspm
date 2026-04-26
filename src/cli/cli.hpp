#pragma once
#include "common.hpp"

struct CliArgs {
    string subcommand;
    string package_name;
    string version;
    bool   force_symlink   = false;
    bool   adopt_collision = false;
    bool   interactive     = false;
    bool   yes             = false;
    bool   force           = false;
    bool   dry_run         = false;
};

CliArgs parse_args(int argc, char* argv[]);
void    print_usage();
