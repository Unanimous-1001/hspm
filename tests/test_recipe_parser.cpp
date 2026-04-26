#include "test_helpers.hpp"
#include "core/package.hpp"
#include <fstream>
#include <cstdlib>

static const string TMP = "/tmp/hspm-test-recipes/";

static void write_recipe(const string& name, const string& content) {
    system(("mkdir -p " + TMP).c_str());
    std::ofstream f(TMP + name + ".recipe");
    f << content;
}

int main() {
    std::cout << "=== Recipe Parser Tests ===\n\n";

    test("parses all required fields", []() {
        write_recipe("testpkg",
            "name:    testpkg\n"
            "version: 1.0.0\n"
            "builder: autotools\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/testpkg-1.0.0.tar.gz\n");

        Package pkg = load_recipe("testpkg", TMP);
        assert_eq(pkg.name,    "testpkg",   "name");
        assert_eq(pkg.version, "1.0.0",     "version");
        assert_eq(pkg.builder, "autotools", "builder");
        assert_eq(pkg.url,
                  "https://example.com/testpkg-1.0.0.tar.gz", "url");
    });

    test("parses depends field into vector", []() {
        write_recipe("testpkg2",
            "name:    testpkg2\n"
            "version: 1.0.0\n"
            "builder: cmake\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/testpkg2-1.0.0.tar.gz\n"
            "depends: zlib openssl curl\n");

        Package pkg = load_recipe("testpkg2", TMP);
        assert_eq((int)pkg.depends.size(), 3, "depends count");
        assert_eq(pkg.depends[0], "zlib",    "depends[0]");
        assert_eq(pkg.depends[1], "openssl", "depends[1]");
        assert_eq(pkg.depends[2], "curl",    "depends[2]");
    });

    test("ignores comments and blank lines", []() {
        write_recipe("testpkg3",
            "# this is a comment\n"
            "\n"
            "name:    testpkg3\n"
            "# another comment\n"
            "version: 2.0.0\n"
            "builder: meson\n"
            "sha256:  abc123def456abc123def456abc123def456abc123def456abc123def456abcd\n"
            "url:     https://example.com/testpkg3-2.0.0.tar.gz\n");

        Package pkg = load_recipe("testpkg3", TMP);
        assert_eq(pkg.name,    "testpkg3", "name");
        assert_eq(pkg.version, "2.0.0",    "version");
    });

    test("throws on missing required field", []() {
        write_recipe("badpkg",
            "name:    badpkg\n"
            "version: 1.0.0\n");

        bool threw = false;
        try {
            load_recipe("badpkg", TMP);
        } catch (const std::exception&) {
            threw = true;
        }
        assert_true(threw, "should throw on missing fields");
    });

    test("throws on missing recipe file", []() {
        bool threw = false;
        try {
            load_recipe("this_package_does_not_exist", TMP);
        } catch (const std::exception&) {
            threw = true;
        }
        assert_true(threw, "should throw on missing file");
    });

    system(("rm -rf " + TMP).c_str());
    print_results();
    return tests_failed > 0 ? 1 : 0;
}
