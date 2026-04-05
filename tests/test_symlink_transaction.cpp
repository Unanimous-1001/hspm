// Build and run:
//   g++ -std=c++17 -Isrc tests/test_symlink_transaction.cpp
//       src/store/symlink.cpp src/store/manifest.cpp
//       src/db/database.cpp -o test_symlink -lsqlite3
//   ./test_symlink

#include "test_helpers.hpp"
#include "store/symlink.hpp"
#include "store/manifest.hpp"
#include "db/database.hpp"
#include <fstream>
#include <cstdlib>

int main() {
    std::cout << "=== Symlink Transaction Tests ===\n\n";

    std::string store = "/tmp/hspm-test-symlink/store/";
    std::string live  = "/tmp/hspm-test-symlink/live/";
    std::string db    = "/tmp/hspm-test-symlink/test.db";

    system(("rm -rf /tmp/hspm-test-symlink && "
            "mkdir -p " + store + "testpkg-1.0/usr/bin " +
            "          " + store + "testpkg-1.0/usr/lib " +
            "          " + live).c_str());

    // create fake store files
    system(("touch " + store + "testpkg-1.0/usr/bin/testbin").c_str());
    system(("touch " + store + "testpkg-1.0/usr/lib/libtest.so").c_str());

    db_open(db);
    db_set_log_path("/tmp/hspm-test-symlink/test.log");

    test("successful transaction creates symlinks", [&]() {
        int pkg_id = db_insert_package(
            "testpkg", "1.0", "managed", "partial",
            store + "testpkg-1.0");

        vector<std::string> store_paths = {
            store + "testpkg-1.0/usr/bin/testbin",
            store + "testpkg-1.0/usr/lib/libtest.so"
        };

        bool ok = symlink_transaction(
            pkg_id, store_paths, store, live);
        assert_true(ok, "transaction should succeed");

        // verify symlinks exist
        assert_true(
            std::filesystem::is_symlink(live + "bin/testbin"),
            "bin symlink should exist");
        assert_true(
            std::filesystem::is_symlink(live + "lib/libtest.so"),
            "lib symlink should exist");

        // verify pending_links is clean
        auto pending = db_pending_get_all(pkg_id);
        assert_eq((int)pending.size(), 0,
                  "pending_links should be empty after commit");
    });

    test("rollback removes created symlinks on failure", [&]() {
        // create a real file at the target to cause a collision
        system(("touch " + live + "bin/testbin2").c_str());
        system(("touch " + store +
                "testpkg-1.0/usr/bin/testbin2").c_str());
        system(("touch " + store +
                "testpkg-1.0/usr/bin/testbin3").c_str());

        int pkg_id = db_insert_package(
            "testpkg2", "1.0", "managed", "partial",
            store + "testpkg-1.0");

        vector<std::string> store_paths = {
            // testbin3 should succeed
            store + "testpkg-1.0/usr/bin/testbin3",
            // testbin2 already exists as real file — will fail
            store + "testpkg-1.0/usr/bin/testbin2"
        };

        bool ok = symlink_transaction(
            pkg_id, store_paths, store, live);
        assert_true(!ok, "transaction should fail");

        // testbin3 symlink should have been rolled back
        assert_true(
            !std::filesystem::exists(live + "bin/testbin3"),
            "testbin3 should be rolled back");

        // pending_links should be clean after rollback
        auto pending = db_pending_get_all(pkg_id);
        assert_eq((int)pending.size(), 0,
                  "pending_links should be empty after rollback");
    });

    // cleanup
    db_close();
    system("rm -rf /tmp/hspm-test-symlink");

    print_results();
    return tests_failed > 0 ? 1 : 0;
}
