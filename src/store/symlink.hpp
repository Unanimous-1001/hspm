#pragma once
#include "common.hpp"

bool symlink_transaction(int package_id,
                         const vector<string>& store_paths,
                         const string& store_root,
                         const string& live_root);
