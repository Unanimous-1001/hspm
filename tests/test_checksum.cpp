
#include "test_helpers.hpp"
#include "fetch/checksum.hpp"
#include <fstream>
#include <cstdlib>

int main() {
    std::cout << "=== Checksum Tests ===\n\n";

    std::string tmp = "/tmp/hspm-test-checksum/";
    system(("mkdir -p " + tmp).c_str());

    test("sha256 of known content", [&]() {
        std::string path = tmp + "hello.txt";
        std::ofstream f(path);
        f << "hello";
        f.close();

        std::string hash = sha256_of_file(path);
        assert_eq(hash,
            "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
            "sha256 of 'hello'");
    });

    test("verify_checksum returns true on match", [&]() {
        std::string path = tmp + "hello2.txt";
        std::ofstream f(path);
        f << "hello";
        f.close();

        bool ok = verify_checksum(path,
            "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
        assert_true(ok, "checksum should match");
    });

    test("verify_checksum returns false on mismatch", [&]() {
        std::string path = tmp + "hello3.txt";
        std::ofstream f(path);
        f << "hello";
        f.close();

        bool ok = verify_checksum(path, "wronghash");
        assert_true(!ok, "checksum should not match");
    });

    test("sha256 of empty file is known value", [&]() {
        std::string path = tmp + "empty.txt";
        std::ofstream f(path);
        f.close();

        std::string hash = sha256_of_file(path);
        assert_eq(hash,
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
            "sha256 of empty file");
    });

    system(("rm -rf " + tmp).c_str());

    print_results();
    return tests_failed > 0 ? 1 : 0;
}
