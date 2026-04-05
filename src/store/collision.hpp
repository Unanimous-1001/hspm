#pragma once
#include "common.hpp"

enum class CollisionResult {
    Safe,
    OwnedSymlink,
    UnknownSymlink,
    RealFile
};

CollisionResult check_collision(const string& target_path,
                                bool force_symlink    = false,
                                bool adopt_collision  = false);

pair<string, CollisionResult> check_manifest_collisions(
    const vector<string>& store_paths,
    const string& store_root,
    const string& live_root,
    bool force_symlink   = false,
    bool adopt_collision = false);
