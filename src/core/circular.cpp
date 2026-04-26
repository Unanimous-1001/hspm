#include "circular.hpp"
#include "graph.hpp"
#include "package.hpp"

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
                vector<string> cycle;
                bool collecting = false;
                for (const auto& n : path) {
                    if (n == dep) collecting = true;
                    if (collecting) cycle.push_back(n);
                }
                cycle.push_back(dep);
                return cycle;
            }
        }
    }

    path.pop_back();
    in_stack[node] = false;
    return {};
}

CycleInfo detect_cycle(const string& root_name) {
    CycleInfo info;

    DepGraph graph;
    try {
        graph = build_graph(root_name);
    } catch (const runtime_error& e) {
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
                for (const auto& pkg_name : cycle) {
                    try {
                        Package pkg = load_recipe(pkg_name);
                        if (!pkg.bootstrap_script.empty()) {
                            info.stub_package = pkg_name;
                            break;
                        }
                    } catch (...) {}
                }
                if (info.stub_package.empty() && !cycle.empty())
                    info.stub_package = cycle[0];
                return info;
            }
        }
    }

    return info;
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

    Package stub_pkg = load_recipe(cycle.stub_package);
    if (stub_pkg.bootstrap_script.empty())
        throw runtime_error(
            "Circular dependency requires bootstrap_script "
            "in recipe for: " + cycle.stub_package +
            "\nAdd 'bootstrap_script: builders/bootstrap-"
            + cycle.stub_package + ".sh' to its recipe.");


    vector<string> queue;

    queue.push_back(cycle.stub_package + ":stub");

    for (const auto& node : cycle.cycle_nodes) {
        if (node != cycle.stub_package
            && node != cycle.cycle_nodes.back())
            queue.push_back(node);
    }

    queue.push_back(cycle.stub_package + ":rebuild");

    bool root_in_cycle = false;
    for (const auto& n : cycle.cycle_nodes)
        if (n == root_name) root_in_cycle = true;
    if (!root_in_cycle)
        queue.push_back(root_name);

    return queue;
}
