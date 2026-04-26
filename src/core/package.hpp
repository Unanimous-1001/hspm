#pragma once
#include "common.hpp"
#include "config.hpp"

struct Package {
    string name;
    string version;
    string builder;
    string sha256;
    string md5;
    string url;
    string url_fallback;
    string configure_args;
    string bootstrap_script;
    string build_cmd;
    vector<string> depends;
    vector<string> recommends;
    string notes;
    string patches;
    string patch_cmds;
    string extra_urls;
    bool   complex = false;
    string kernel_hint;//new dlt
};

void save_recipe(const Package& pkg,
                 const string& recipe_dir = HSPM_RECIPES);

Package load_recipe(const string& name,
                    const string& recipe_dir = HSPM_RECIPES);
