#pragma once
#include "common.hpp"

struct CliArgs {
    string subcommand;
    string package_name;
    string version;
    bool   force_symlink   = false;
    bool   adopt_collision = false;
};

CliArgs parse_args(int argc, char* argv[]);
void    print_usage();
