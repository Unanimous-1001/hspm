#pragma once
#include "common.hpp"

using DepGraph = unordered_map<string, vector<string>>;

DepGraph build_graph(const string& root_name);

vector<string> resolve_order(const DepGraph& graph, const string& root);
