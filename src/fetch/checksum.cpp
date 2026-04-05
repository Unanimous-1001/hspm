#include "checksum.hpp"
#include <array>
#include <cstdio>

string sha256_of_file(const string& filepath) {
    string cmd = "sha256sum " + filepath;
    std::array<char, 256> buffer;
    string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw runtime_error("popen failed for sha256sum");

    if (fgets(buffer.data(), buffer.size(), pipe.get()))
        result = string(buffer.data()).substr(0, 64);

    return result;
}

bool verify_checksum(const string& filepath, const string& expected) {
    return sha256_of_file(filepath) == expected;
}

string md5_of_file(const string& filepath) {
    string cmd = "md5sum " + filepath;
    std::array<char, 256> buffer;
    string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw runtime_error("popen failed for md5sum");
    if (fgets(buffer.data(), buffer.size(), pipe.get()))
        result = string(buffer.data()).substr(0, 32);
    return result;
}

bool verify_md5(const string& filepath, const string& expected) {
    return md5_of_file(filepath) == expected;
}
