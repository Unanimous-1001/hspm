#include "graph.hpp"
#include "package.hpp"
#include <stack>
#include "db/database.hpp"
#include <algorithm>

DepGraph build_graph(const string& root_name,
                     const string& recipe_dir) {
    DepGraph graph;
    std::stack<string> to_visit;
    to_visit.push(root_name);

    while (!to_visit.empty()) {
        string current = to_visit.top();
        to_visit.pop();

        if (graph.count(current)) continue;

        
        PackageRecord rec = db_get_package(current);
        if (rec.id != -1 && rec.state == "active") {
            graph[current] = {};
            continue;
        }

        Package pkg;
        try {
            pkg = load_recipe(current, recipe_dir);
        } catch (const std::exception& e) {
            if (current == root_name) throw;
            std::cerr << "[graph] Warning: no recipe for '"
                      << current << "' — treating as leaf.\n"
                      << "        Run: hspm adopt "
                      << current << " <version>\n";
            graph[current] = {};
            continue;
        }

        vector<string> all_deps = pkg.depends;
        for (const auto& rec_dep : pkg.recommends) {
            if (std::find(all_deps.begin(), all_deps.end(), rec_dep)
                == all_deps.end())
                all_deps.push_back(rec_dep);
        }

        graph[current] = all_deps;

        for (const auto& dep : all_deps)
            to_visit.push(dep);
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
