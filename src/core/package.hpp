#pragma once
#include "common.hpp"

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
    string notes;           // scraped BLFS build commands
    string patches;         // space-separated patch URLs
    string patch_cmds;      // commands to apply patches
    string extra_urls;      // space-separated extra download URLs
    bool   complex = false; // true if non-standard build detected
};

Package load_recipe(const string& name);
