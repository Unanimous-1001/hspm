#pragma once
#include "common.hpp"

struct CycleInfo {
    vector<string> cycle_nodes;  // packages involved in the cycle
    string         stub_package; // which package gets the stub build
};

// Detect if a cycle exists and return info about it
// Returns empty CycleInfo if no cycle
CycleInfo detect_cycle(const string& root_name);

// Execute the 3-stage circular dependency resolution
// Returns ordered install queue with cycle resolved
vector<string> resolve_circular(const CycleInfo& cycle,
                                const string& root_name);
