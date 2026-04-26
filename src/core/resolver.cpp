#include "resolver.hpp"
#include "graph.hpp"
#include "circular.hpp"
#include "db/database.hpp"
#include "package.hpp"
#include <algorithm>

static bool try_break_recommended_cycle(
        DepGraph& graph,
        const string& cycle_msg) {

    auto arrow = cycle_msg.find(" <-> ");
    if (arrow == string::npos) return false;

    string pkg_a = cycle_msg.substr(
        cycle_msg.rfind(": ") + 2, 
        arrow - cycle_msg.rfind(": ") - 2);
    string pkg_b = cycle_msg.substr(arrow + 5);

    try {
        Package pkg = load_recipe(pkg_a);
        for (const auto& rec : pkg.recommends) {
            if (rec == pkg_b) {
                auto& deps = graph[pkg_a];
                deps.erase(
                    std::remove(deps.begin(), deps.end(), pkg_b),
                    deps.end());
                std::cout << "[resolver] Breaking recommended cycle: "
                          << "skipping " << pkg_b
                          << " as dep of " << pkg_a << "\n";
                return true;
            }
        }
        Package pkgb = load_recipe(pkg_b);
        for (const auto& rec : pkgb.recommends) {
            if (rec == pkg_a) {
                auto& deps = graph[pkg_b];
                deps.erase(
                    std::remove(deps.begin(), deps.end(), pkg_a),
                    deps.end());
                std::cout << "[resolver] Breaking recommended cycle: "
                          << "skipping " << pkg_a
                          << " as dep of " << pkg_b << "\n";
                return true;
            }
        }
    } catch (...) {}

    return false;
}

vector<string> get_install_queue(const string& name) {
    DepGraph graph = build_graph(name);

    vector<string> ordered;
    int max_attempts = 20;

    while (max_attempts-- > 0) {
        try {
            ordered = resolve_order(graph, name);
            break;
        } catch (const runtime_error& e) {
            string msg = e.what();
            if (msg.find("Circular dependency") != string::npos) {
                if (try_break_recommended_cycle(graph, msg)) {
                    continue;
                }
                std::cout << "[resolver] " << msg << "\n";
                CycleInfo cycle = detect_cycle(name);
                if (!cycle.cycle_nodes.empty())
                    return resolve_circular(cycle, name);
            }
            throw;
        }
    }

    vector<string> queue;
    for (const auto& pkg_name : ordered) {
        PackageRecord rec = db_get_package(pkg_name);
        if (rec.id == -1 || rec.state != "active")
            queue.push_back(pkg_name);
    }

    return queue;
}
