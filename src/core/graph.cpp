#include "graph.hpp"
#include "package.hpp"
#include <stack>

DepGraph build_graph(const string& root_name) {
    DepGraph graph;
    std::stack<string> to_visit;
    to_visit.push(root_name);

    while (!to_visit.empty()) {
        string current = to_visit.top();
        to_visit.pop();

        if (graph.count(current)) continue;

        Package pkg    = load_recipe(current);
        graph[current] = pkg.depends;

        for (const auto& dep : pkg.depends) {
            to_visit.push(dep);
        }
    }

    return graph;
}

static void dfs(const string& node,
                const DepGraph& graph,
                unordered_set<string>& visited,
                unordered_set<string>& in_stack,
                vector<string>& result) {

    in_stack.insert(node);

    auto it = graph.find(node);
    if (it != graph.end()) {
        for (const auto& dep : it->second) {
            if (in_stack.count(dep))
                throw runtime_error(
                    "Circular dependency detected: " + dep + " <-> " + node);
            if (!visited.count(dep))
                dfs(dep, graph, visited, in_stack, result);
        }
    }

    in_stack.erase(node);
    visited.insert(node);
    result.push_back(node);
}

vector<string> resolve_order(const DepGraph& graph, const string& root) {
    unordered_set<string> visited;
    unordered_set<string> in_stack;
    vector<string> result;
    dfs(root, graph, visited, in_stack, result);
    return result;
}
