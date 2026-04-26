#pragma once
#include "common.hpp"

struct PackageRecord {
    int    id;
    string name;
    string version;
    string type;
    string state;
    string store_path;
};

struct LogRecord {
    int    id;
    string timestamp;
    string operation;
    int    package_id;
    string detail;
};

vector<LogRecord> db_get_log(int limit);
vector<LogRecord> db_get_log_by_package(const string& pkg_name);
vector<LogRecord> db_get_log_by_operation(const string& operation);

void db_open(const string& path);
void db_close();

int db_insert_package(const string& name,
                      const string& version,
                      const string& type,
                      const string& state,
                      const string& store_path);

PackageRecord db_get_package(const string& name);

void db_set_package_state(int package_id, const string& state);

void db_insert_file(int package_id, const string& path, bool is_symlink);

vector<string> db_get_files(int package_id);

void db_log(const string& operation, int package_id, const string& detail);

void db_delete_package(int package_id);

vector<string> db_get_dependents(const string& pkg_name);

void db_insert_dependency(int package_id, int depends_on_id);

PackageRecord db_get_package_version(const string& name,
                                     const string& version);

void db_set_package_inactive(int package_id);

vector<PackageRecord> db_get_all_packages();

void db_delete_inactive_package(const string& name, const string& version);

void db_set_log_path(const string& path);

void db_pending_begin(int package_id,
                      const vector<string>& store_paths,
                      const vector<string>& live_paths);
void db_pending_mark_done(const string& live_path);
void db_pending_clear(int package_id);
vector<pair<string,string>> db_pending_get_done(int package_id);
vector<pair<string,string>> db_pending_get_all(int package_id);
