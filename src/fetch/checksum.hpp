#pragma once
#include "common.hpp"

string sha256_of_file(const string& filepath);

bool verify_checksum(const string& filepath, const string& expected);

string md5_of_file(const string& filepath);
bool verify_md5(const string& filepath, const string& expected);
