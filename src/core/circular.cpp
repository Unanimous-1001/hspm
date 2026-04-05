#include "circular.hpp"
#include "graph.hpp"
#include "package.hpp"

// Find the cycle path using DFS
static vector<string> find_cycle_path(
        const string& node,
        const DepGraph& graph,
        unordered_map<string, bool>& visited,
        unordered_map<string, bool>& in_stack,
        vector<string>& path) {

    visited[node]  = true;
    in_stack[node] = true;
    path.push_back(node);

    auto it = graph.find(node);
    if (it != graph.end()) {
        for (const auto& dep : it->second) {
            if (!visited[dep]) {
                auto result = find_cycle_path(
                    dep, graph, visited, in_stack, path);
                if (!result.empty()) return result;
            } else if (in_stack[dep]) {
                // found the cycle — extract just the cycle portion
                vector<string> cycle;
                bool collecting = false;
                for (const auto& n : path) {
                    if (n == dep) collecting = true;
                    if (collecting) cycle.push_back(n);
                }
                cycle.push_back(dep); // close the loop
                return cycle;
            }
        }
    }

    path.pop_back();
    in_stack[node] = false;
    return {}; // no cycle from this node
}

CycleInfo detect_cycle(const string& root_name) {
    CycleInfo info;

    DepGraph graph;
    try {
        graph = build_graph(root_name);
    } catch (const runtime_error& e) {
        // build_graph itself throws on cycle
        // we need a version that doesn't throw
        // for now return empty — we'll detect via resolve_order
        return info;
    }

    unordered_map<string, bool> visited;
    unordered_map<string, bool> in_stack;
    vector<string> path;

    for (const auto& [node, deps] : graph) {
        if (!visited[node]) {
            auto cycle = find_cycle_path(
                node, graph, visited, in_stack, path);
            if (!cycle.empty()) {
                info.cycle_nodes = cycle;
                // choose the stub package — pick the first one
                // in the cycle that has a bootstrap_script defined
                for (const auto& pkg_name : cycle) {
                    try {
                        Package pkg = load_recipe(pkg_name);
                        if (!pkg.bootstrap_script.empty()) {
                            info.stub_package = pkg_name;
                            break;
                        }
                    } catch (...) {}
                }
                // if none have bootstrap_script, use the first one
                if (info.stub_package.empty() && !cycle.empty())
                    info.stub_package = cycle[0];
                return info;
            }
        }
    }

    return info; // no cycle found
}

vector<string> resolve_circular(const CycleInfo& cycle,
                                const string& root_name) {
    if (cycle.cycle_nodes.empty())
        throw runtime_error("resolve_circular called with no cycle");

    std::cout << "\n[circular] Cycle detected involving: ";
    for (const auto& n : cycle.cycle_nodes)
        std::cout << n << " ";
    std::cout << "\n";
    std::cout << "[circular] Stub package: "
              << cycle.stub_package << "\n";

    // verify stub package has a bootstrap_script
    Package stub_pkg = load_recipe(cycle.stub_package);
    if (stub_pkg.bootstrap_script.empty())
        throw runtime_error(
            "Circular dependency requires bootstrap_script "
            "in recipe for: " + cycle.stub_package +
            "\nAdd 'bootstrap_script: builders/bootstrap-"
            + cycle.stub_package + ".sh' to its recipe.");

    // build the install queue treating the cycle carefully:
    // 1. stub_package-stub  (minimal build)
    // 2. all other cycle members in order
    // 3. stub_package-full  (rebuild against real deps)
    // 4. everything that depends on the cycle

    vector<string> queue;

    // add stub marker — install.cpp will detect this suffix
    queue.push_back(cycle.stub_package + ":stub");

    // add the other cycle members
    for (const auto& node : cycle.cycle_nodes) {
        if (node != cycle.stub_package
            && node != cycle.cycle_nodes.back())
            queue.push_back(node);
    }

    // add the full rebuild of stub package
    queue.push_back(cycle.stub_package + ":rebuild");

    // add root if not already in cycle
    bool root_in_cycle = false;
    for (const auto& n : cycle.cycle_nodes)
        if (n == root_name) root_in_cycle = true;
    if (!root_in_cycle)
        queue.push_back(root_name);

    return queue;
}
