// Build and run:
//   g++ -std=c++17 -Isrc tests/test_graph.cpp src/core/graph.cpp src/core/package.cpp -o test_graph
//   ./test_graph

#include "test_helpers.hpp"
#include "core/graph.hpp"
#include <fstream>
#include <cstdlib>

static void write_recipe(const std::string& name,
                         const std::string& content) {
    std::string path = "/home/Dev/Projects/hspm/recipes/" + name + ".recipe";
    std::ofstream f(path);
    f << content;
}

static void remove_recipe(const std::string& name) {
    system(("rm -f /home/Dev/Projects/hspm/recipes/" + name + ".recipe").c_str());
}

int main() {
    std::cout << "=== Graph / Resolver Tests ===\n\n";

    test("linear resolution: zlib -> openssl -> curl", []() {
        // zlib has no deps, openssl depends on zlib,
        // curl depends on openssl and zlib
        DepGraph g = build_graph("curl");
        auto order = resolve_order(g, "curl");

        assert_eq((int)order.size(), 3, "queue size");
        assert_eq(order[0], "zlib",    "order[0]");
        assert_eq(order[1], "openssl", "order[1]");
        assert_eq(order[2], "curl",    "order[2]");
    });

    test("single package with no deps", []() {
        DepGraph g = build_graph("zlib");
        auto order = resolve_order(g, "zlib");
        assert_eq((int)order.size(), 1, "queue size");
        assert_eq(order[0], "zlib", "order[0]");
    });

    test("cycle detection throws", []() {
        // create two packages that depend on each other
        write_recipe("cycle_a",
            "name:    cycle_a\n"
            "version: 1.0\n"
            "builder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/a.tar.gz\n"
            "depends: cycle_b\n");

        write_recipe("cycle_b",
            "name:    cycle_b\n"
            "version: 1.0\n"
            "builder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/b.tar.gz\n"
            "depends: cycle_a\n");

        bool threw = false;
        try {
            DepGraph g = build_graph("cycle_a");
            resolve_order(g, "cycle_a");
        } catch (const std::exception&) {
            threw = true;
        }

        remove_recipe("cycle_a");
        remove_recipe("cycle_b");

        assert_true(threw, "should throw on circular dependency");
    });

    test("diamond dependency resolves correctly", []() {
        // A depends on B and C, B depends on D, C depends on D
        // D should only appear once in the result
        write_recipe("dia_d",
            "name:    dia_d\n"
            "version: 1.0\n"
            "builder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/d.tar.gz\n");

        write_recipe("dia_b",
            "name:    dia_b\n"
            "version: 1.0\n"
            "builder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/b.tar.gz\n"
            "depends: dia_d\n");

        write_recipe("dia_c",
            "name:    dia_c\n"
            "version: 1.0\n"
            "builder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/c.tar.gz\n"
            "depends: dia_d\n");

        write_recipe("dia_a",
            "name:    dia_a\n"
            "version: 1.0\n"
            "builder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/a.tar.gz\n"
            "depends: dia_b dia_c\n");

        DepGraph g = build_graph("dia_a");
        auto order = resolve_order(g, "dia_a");

        // dia_d must appear before dia_b and dia_c
        // and must appear only once
        int d_count = 0;
        int d_pos = -1, b_pos = -1, c_pos = -1, a_pos = -1;
        for (int i = 0; i < (int)order.size(); ++i) {
            if (order[i] == "dia_d") { d_count++; d_pos = i; }
            if (order[i] == "dia_b") b_pos = i;
            if (order[i] == "dia_c") c_pos = i;
            if (order[i] == "dia_a") a_pos = i;
        }

        remove_recipe("dia_a");
        remove_recipe("dia_b");
        remove_recipe("dia_c");
        remove_recipe("dia_d");

        assert_eq(d_count, 1, "dia_d appears exactly once");
        assert_true(d_pos < b_pos, "dia_d before dia_b");
        assert_true(d_pos < c_pos, "dia_d before dia_c");
        assert_true(b_pos < a_pos, "dia_b before dia_a");
        assert_true(c_pos < a_pos, "dia_c before dia_a");
    });

    print_results();
    return tests_failed > 0 ? 1 : 0;
}
