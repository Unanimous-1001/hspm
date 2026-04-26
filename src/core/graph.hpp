#pragma once
#include "common.hpp"
#include "config.hpp"

using DepGraph = unordered_map<string, vector<string>>;

DepGraph build_graph(const string& root_name,
                     const string& recipe_dir = HSPM_RECIPES);

vector<string> resolve_order(const DepGraph& graph, const string& root);
