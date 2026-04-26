#pragma once
#include "common.hpp"
#include "core/package.hpp"

struct BuildOverride {
    string build_cmd;
    string patch_cmds;
    bool save_to_recipe;
};

BuildOverride interactive_prompt(const Package& pkg, const string& src_dir);
