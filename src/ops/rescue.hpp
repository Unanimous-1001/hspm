#pragma once
#include "common.hpp"

void run_rescue_rebuild_db(bool force = false);
void run_rescue_fix_links();
void run_rescue_validate_rpath();
void run_rescue(const string& flag, bool yes, bool force);
