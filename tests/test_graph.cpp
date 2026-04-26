#include "test_helpers.hpp"
#include "core/graph.hpp"
#include <fstream>
#include <cstdlib>
#include "db/database.hpp"

static const string TMP = "/tmp/hspm-test-graph/";

static void write_recipe(const string& name, const string& content) {
    system(("mkdir -p " + TMP).c_str());
    std::ofstream f(TMP + name + ".recipe");
    f << content;
}

static void remove_recipe(const string& name) {
    system(("rm -f " + TMP + name + ".recipe").c_str());
}

int main() {
    std::cout << "=== Graph / Resolver Tests ===\n\n";
    db_open(":memory:");
    system(("mkdir -p " + TMP).c_str());

    // write test recipes to tmp
    write_recipe("zlib",
        "name:    zlib\nversion: 1.3.1\nbuilder: autotools\n"
        "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
        "url:     https://example.com/zlib.tar.gz\n");

    write_recipe("openssl",
        "name:    openssl\nversion: 3.3.0\nbuilder: make\n"
        "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
        "url:     https://example.com/openssl.tar.gz\n"
        "depends: zlib\n");

    write_recipe("curl",
        "name:    curl\nversion: 8.7.1\nbuilder: autotools\n"
        "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
        "url:     https://example.com/curl.tar.gz\n"
        "depends: openssl zlib\n");

    test("linear resolution: zlib -> openssl -> curl", []() {
        DepGraph g = build_graph("curl", TMP);
        auto order = resolve_order(g, "curl");
        assert_eq((int)order.size(), 3, "queue size");
        assert_eq(order[0], "zlib",    "order[0]");
        assert_eq(order[1], "openssl", "order[1]");
        assert_eq(order[2], "curl",    "order[2]");
    });

    test("single package with no deps", []() {
        DepGraph g = build_graph("zlib", TMP);
        auto order = resolve_order(g, "zlib");
        assert_eq((int)order.size(), 1, "queue size");
        assert_eq(order[0], "zlib", "order[0]");
    });

    test("cycle detection throws", []() {
        write_recipe("cycle_a",
            "name:    cycle_a\nversion: 1.0\nbuilder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/a.tar.gz\n"
            "depends: cycle_b\n");
        write_recipe("cycle_b",
            "name:    cycle_b\nversion: 1.0\nbuilder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/b.tar.gz\n"
            "depends: cycle_a\n");

        bool threw = false;
        try {
            DepGraph g = build_graph("cycle_a", TMP);
            resolve_order(g, "cycle_a");
        } catch (const std::exception&) {
            threw = true;
        }
        remove_recipe("cycle_a");
        remove_recipe("cycle_b");
        assert_true(threw, "should throw on circular dependency");
    });

    test("diamond dependency resolves correctly", []() {
        write_recipe("dia_d",
            "name:    dia_d\nversion: 1.0\nbuilder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/d.tar.gz\n");
        write_recipe("dia_b",
            "name:    dia_b\nversion: 1.0\nbuilder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/b.tar.gz\n"
            "depends: dia_d\n");
        write_recipe("dia_c",
            "name:    dia_c\nversion: 1.0\nbuilder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/c.tar.gz\n"
            "depends: dia_d\n");
        write_recipe("dia_a",
            "name:    dia_a\nversion: 1.0\nbuilder: make\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/a.tar.gz\n"
            "depends: dia_b dia_c\n");

        DepGraph g = build_graph("dia_a", TMP);
        auto order = resolve_order(g, "dia_a");

        int d_count = 0, d_pos = -1, b_pos = -1,
            c_pos = -1, a_pos = -1;
        for (int i = 0; i < (int)order.size(); ++i) {
            if (order[i] == "dia_d") { d_count++; d_pos = i; }
            if (order[i] == "dia_b") b_pos = i;
            if (order[i] == "dia_c") c_pos = i;
            if (order[i] == "dia_a") a_pos = i;
        }

        remove_recipe("dia_a"); remove_recipe("dia_b");
        remove_recipe("dia_c"); remove_recipe("dia_d");

        assert_eq(d_count, 1, "dia_d appears exactly once");
        assert_true(d_pos < b_pos, "dia_d before dia_b");
        assert_true(d_pos < c_pos, "dia_d before dia_c");
        assert_true(b_pos < a_pos, "dia_b before dia_a");
        assert_true(c_pos < a_pos, "dia_c before dia_a");
    });

    system(("rm -rf " + TMP).c_str());
    db_close();
    print_results();
    return tests_failed > 0 ? 1 : 0;
}
