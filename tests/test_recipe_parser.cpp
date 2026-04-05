// Build and run:
//   g++ -std=c++17 -Isrc tests/test_recipe_parser.cpp src/core/package.cpp -o test_recipe_parser
//   ./test_recipe_parser

#include "test_helpers.hpp"
#include "core/package.hpp"
#include <fstream>
#include <cstdlib>

// write a temp recipe file for testing
static void write_recipe(const std::string& path,
                         const std::string& content) {
    std::ofstream f(path);
    f << content;
}

int main() {
    std::cout << "=== Recipe Parser Tests ===\n\n";

    // use /tmp for test recipes
    std::string tmp = "/tmp/hspm-test-recipes/";
    system(("mkdir -p " + tmp).c_str());

    // patch load_recipe to use tmp dir by writing test files there
    // and temporarily symlinking recipes/ to /tmp
    // simpler: just write to the real recipes dir with test_ prefix
    // and clean up after

    test("parses all required fields", [&]() {
        write_recipe(tmp + "testpkg.recipe",
            "name:    testpkg\n"
            "version: 1.0.0\n"
            "builder: autotools\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/testpkg-1.0.0.tar.gz\n"
        );
        // copy to recipes dir
        system("cp /tmp/hspm-test-recipes/testpkg.recipe "
               "/home/Dev/Projects/hspm/recipes/testpkg.recipe");

        Package pkg = load_recipe("testpkg");
        assert_eq(pkg.name,    "testpkg",   "name");
        assert_eq(pkg.version, "1.0.0",     "version");
        assert_eq(pkg.builder, "autotools", "builder");
        assert_eq(pkg.url,
                  "https://example.com/testpkg-1.0.0.tar.gz", "url");
    });

    test("parses depends field into vector", [&]() {
        write_recipe(tmp + "testpkg2.recipe",
            "name:    testpkg2\n"
            "version: 1.0.0\n"
            "builder: cmake\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/testpkg2-1.0.0.tar.gz\n"
            "depends: zlib openssl curl\n"
        );
        system("cp /tmp/hspm-test-recipes/testpkg2.recipe "
               "/home/Dev/Projects/hspm/recipes/testpkg2.recipe");

        Package pkg = load_recipe("testpkg2");
        assert_eq((int)pkg.depends.size(), 3, "depends count");
        assert_eq(pkg.depends[0], "zlib",    "depends[0]");
        assert_eq(pkg.depends[1], "openssl", "depends[1]");
        assert_eq(pkg.depends[2], "curl",    "depends[2]");
    });

    test("ignores comments and blank lines", [&]() {
        write_recipe(tmp + "testpkg3.recipe",
            "# this is a comment\n"
            "\n"
            "name:    testpkg3\n"
            "# another comment\n"
            "version: 2.0.0\n"
            "builder: meson\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/testpkg3-2.0.0.tar.gz\n"
        );
        system("cp /tmp/hspm-test-recipes/testpkg3.recipe "
               "/home/Dev/Projects/hspm/recipes/testpkg3.recipe");

        Package pkg = load_recipe("testpkg3");
        assert_eq(pkg.name,    "testpkg3", "name");
        assert_eq(pkg.version, "2.0.0",    "version");
    });

    test("throws on missing required field", [&]() {
        write_recipe(tmp + "badpkg.recipe",
            "name:    badpkg\n"
            "version: 1.0.0\n"
            // missing builder, sha256, url
        );
        system("cp /tmp/hspm-test-recipes/badpkg.recipe "
               "/home/Dev/Projects/hspm/recipes/badpkg.recipe");

        bool threw = false;
        try {
            load_recipe("badpkg");
        } catch (const std::exception&) {
            threw = true;
        }
        assert_true(threw, "should throw on missing fields");
    });

    test("throws on missing recipe file", [&]() {
        bool threw = false;
        try {
            load_recipe("this_package_does_not_exist");
        } catch (const std::exception&) {
            threw = true;
        }
        assert_true(threw, "should throw on missing file");
    });

    // cleanup
    system("rm -f /home/Dev/Projects/hspm/recipes/testpkg*.recipe "
           "/home/Dev/Projects/hspm/recipes/badpkg.recipe");

    print_results();
    return tests_failed > 0 ? 1 : 0;
}
