#include "resolver.hpp"
#include "graph.hpp"
#include "circular.hpp"
#include "db/database.hpp"

vector<string> get_install_queue(const string& name) {
    DepGraph graph = build_graph(name);

    vector<string> ordered;

    try {
        ordered = resolve_order(graph, name);
    } catch (const runtime_error& e) {
        string msg = e.what();
        if (msg.find("Circular dependency") != string::npos) {
            // hand off to circular resolver
            std::cout << "[resolver] " << msg << "\n";
            CycleInfo cycle = detect_cycle(name);
            if (cycle.cycle_nodes.empty())
                throw; // re-throw if we can't detect it properly
            return resolve_circular(cycle, name);
        }
        throw; // re-throw other errors
    }

    // filter out already-active packages
    vector<string> queue;
    for (const auto& pkg_name : ordered) {
        PackageRecord rec = db_get_package(pkg_name);
        if (rec.id == -1 || rec.state != "active")
            queue.push_back(pkg_name);
    }

    return queue;
}
