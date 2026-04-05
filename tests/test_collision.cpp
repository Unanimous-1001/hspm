// Build and run:
//   g++ -std=c++17 -Isrc tests/test_collision.cpp
//       src/store/collision.cpp src/db/database.cpp -o test_collision
//       -lsqlite3
//   ./test_collision

#include "test_helpers.hpp"
#include "store/collision.hpp"
#include "db/database.hpp"
#include <cstdlib>

int main() {
    std::cout << "=== Collision Detection Tests ===\n\n";

    std::string tmp = "/tmp/hspm-test-collision/";
    system(("mkdir -p " + tmp).c_str());

    // use in-memory database for tests
    db_open(":memory:");

    test("safe: path does not exist", [&]() {
        CollisionResult r = check_collision(tmp + "nonexistent_file");
        assert_true(r == CollisionResult::Safe,
                    "missing path should be Safe");
    });

    test("real file: returns RealFile", [&]() {
        std::string path = tmp + "realfile.txt";
        system(("touch " + path).c_str());
        CollisionResult r = check_collision(path);
        assert_true(r == CollisionResult::RealFile,
                    "real file should return RealFile");
        system(("rm " + path).c_str());
    });

    test("unknown symlink: returns UnknownSymlink", [&]() {
        std::string target = tmp + "target.txt";
        std::string link   = tmp + "unknown_link";
        system(("touch " + target).c_str());
        system(("ln -sf " + target + " " + link).c_str());

        CollisionResult r = check_collision(link);
        assert_true(r == CollisionResult::UnknownSymlink,
                    "unknown symlink should return UnknownSymlink");

        system(("rm " + link + " " + target).c_str());
    });

    test("manifest with no collisions returns empty path", [&]() {
        // create proper store structure that collision checker expects
        std::string store = tmp + "store/";
        std::string live  = tmp + "live/";
        system(("mkdir -p " + store + "testpkg-1.0/usr/bin").c_str());
        system(("mkdir -p " + live).c_str());
        system(("touch " + store + "testpkg-1.0/usr/bin/nonexistent1").c_str());
        system(("touch " + store + "testpkg-1.0/usr/bin/nonexistent2").c_str());

        vector<std::string> paths = {
            store + "testpkg-1.0/usr/bin/nonexistent1",
            store + "testpkg-1.0/usr/bin/nonexistent2"
        };

        // these paths don't exist in live/ so should be safe
        auto [col_path, result] = check_manifest_collisions(
            paths, store, live);
        assert_true(col_path.empty(),
                    "no collision should return empty path");
    });

    // cleanup
    db_close();
    system(("rm -rf " + tmp).c_str());

    print_results();
    return tests_failed > 0 ? 1 : 0;
}
