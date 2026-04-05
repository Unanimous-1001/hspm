#pragma once
#include "common.hpp"

enum class RpathStatus { OK, Warning, Fatal };

struct RpathResult {
    RpathStatus status;
    string      binary;
    string      message;
};

vector<RpathResult> validate_rpath(const string& store_dir);
