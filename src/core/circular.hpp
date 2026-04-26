#pragma once
#include "common.hpp"

struct CycleInfo {
    vector<string> cycle_nodes;
    string         stub_package;
};

CycleInfo detect_cycle(const string& root_name);

vector<string> resolve_circular(const CycleInfo& cycle,
                                const string& root_name);
