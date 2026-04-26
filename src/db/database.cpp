#include "database.hpp"
#include <sqlite3.h>
#include "config.hpp"

static sqlite3* g_db = nullptr;

static string g_log_path = "";

void db_set_log_path(const string& path) {
    g_log_path = path;
}

static void write_flat_log(const string& operation,
                           const string& detail) {
    if (g_log_path.empty()) return;

    std::ofstream log(g_log_path, std::ios::app);
    if (!log) return;

    // get current timestamp
    time_t now = time(nullptr);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    log << "[" << ts << "] "
        << operation << " — "
        << detail    << "\n";
}

static void check(int rc, const string& context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        string msg = context + ": " + sqlite3_errmsg(g_db);
        throw runtime_error(msg);
    }
}

void db_open(const string& db_path) {
    check(sqlite3_open(db_path.c_str(), &g_db), "db_open");

    std::ifstream schema_file(HSPM_SCHEMA);
    if (!schema_file) throw runtime_error("Cannot open schema.sql");

    std::ostringstream ss;
    ss << schema_file.rdbuf();
    string schema = ss.str();

    char* err = nullptr;
    sqlite3_exec(g_db, schema.c_str(), nullptr, nullptr, &err);
    if (err) {
        string msg = string("Schema apply failed: ") + err;
        sqlite3_free(err);
        throw runtime_error(msg);
    }
}

void db_close() {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

int db_insert_package(const string& name,
                      const string& version,
                      const string& type,
                      const string& state,
                      const string& store_path) {
    const char* sql =
        "INSERT INTO packages (name, version, type, state, store_path) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "insert_package prepare");
    sqlite3_bind_text(stmt, 1, name.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, version.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, state.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, store_path.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(stmt), "insert_package step");
    sqlite3_finalize(stmt);

    return (int)sqlite3_last_insert_rowid(g_db);
}

PackageRecord db_get_package(const string& name) {
    const char* sql =
        "SELECT id, name, version, type, state, store_path "
        "FROM packages WHERE name = ? LIMIT 1";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "get_package prepare");
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    PackageRecord rec;
    rec.id = -1;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rec.id         = sqlite3_column_int(stmt, 0);
        rec.name       = (const char*)sqlite3_column_text(stmt, 1);
        rec.version    = (const char*)sqlite3_column_text(stmt, 2);
        rec.type       = (const char*)sqlite3_column_text(stmt, 3);
        rec.state      = (const char*)sqlite3_column_text(stmt, 4);
        rec.store_path = (const char*)sqlite3_column_text(stmt, 5);
    }

    sqlite3_finalize(stmt);
    return rec;
}

void db_set_package_state(int package_id, const string& state) {
    const char* sql = "UPDATE packages SET state = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "set_state prepare");
    sqlite3_bind_text(stmt, 1, state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, package_id);
    check(sqlite3_step(stmt), "set_state step");
    sqlite3_finalize(stmt);
}

void db_insert_file(int package_id, const string& path, bool is_symlink) {
    const char* sql =
        "INSERT INTO files (package_id, path, is_symlink) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "insert_file prepare");
    sqlite3_bind_int(stmt,  1, package_id);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  3, is_symlink ? 1 : 0);
    check(sqlite3_step(stmt), "insert_file step");
    sqlite3_finalize(stmt);
}

vector<string> db_get_files(int package_id) {
    const char* sql = "SELECT path FROM files WHERE package_id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "get_files prepare");
    sqlite3_bind_int(stmt, 1, package_id);

    vector<string> paths;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        paths.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return paths;
}

void db_log(const string& operation, int package_id, const string& detail) {
    const char* sql =
        "INSERT INTO log (operation, package_id, detail) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "db_log prepare");
    sqlite3_bind_text(stmt, 1, operation.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, package_id);
    sqlite3_bind_text(stmt, 3, detail.c_str(),    -1, SQLITE_TRANSIENT);
    check(sqlite3_step(stmt), "db_log step");
    write_flat_log(operation, detail);
    sqlite3_finalize(stmt);
}

vector<LogRecord> db_get_log(int limit) {
    const char* sql =
        "SELECT l.id, l.timestamp, l.operation, "
        "COALESCE(l.package_id, -1), l.detail "
        "FROM log l "
        "ORDER BY l.id DESC LIMIT ?";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr), "get_log prepare");
    sqlite3_bind_int(stmt, 1, limit);

    vector<LogRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LogRecord r;
        r.id         = sqlite3_column_int(stmt, 0);
        r.timestamp  = (const char*)sqlite3_column_text(stmt, 1);
        r.operation  = (const char*)sqlite3_column_text(stmt, 2);
        r.package_id = sqlite3_column_int(stmt, 3);
        r.detail     = sqlite3_column_text(stmt, 4)
                       ? (const char*)sqlite3_column_text(stmt, 4)
                       : "";
        records.push_back(r);
    }
    sqlite3_finalize(stmt);
    return records;
}

vector<LogRecord> db_get_log_by_package(const string& pkg_name) {
    const char* sql =
        "SELECT l.id, l.timestamp, l.operation, "
        "COALESCE(l.package_id, -1), l.detail "
        "FROM log l "
        "JOIN packages p ON l.package_id = p.id "
        "WHERE p.name = ? "
        "ORDER BY l.id DESC";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "get_log_by_package prepare");
    sqlite3_bind_text(stmt, 1, pkg_name.c_str(), -1, SQLITE_TRANSIENT);

    vector<LogRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LogRecord r;
        r.id         = sqlite3_column_int(stmt, 0);
        r.timestamp  = (const char*)sqlite3_column_text(stmt, 1);
        r.operation  = (const char*)sqlite3_column_text(stmt, 2);
        r.package_id = sqlite3_column_int(stmt, 3);
        r.detail     = sqlite3_column_text(stmt, 4)
                       ? (const char*)sqlite3_column_text(stmt, 4)
                       : "";
        records.push_back(r);
    }
    sqlite3_finalize(stmt);
    return records;
}

vector<LogRecord> db_get_log_by_operation(const string& operation) {
    const char* sql =
        "SELECT l.id, l.timestamp, l.operation, "
        "COALESCE(l.package_id, -1), l.detail "
        "FROM log l "
        "WHERE l.operation = ? "
        "ORDER BY l.id DESC";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "get_log_by_op prepare");
    sqlite3_bind_text(stmt, 1, operation.c_str(), -1, SQLITE_TRANSIENT);

    vector<LogRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LogRecord r;
        r.id         = sqlite3_column_int(stmt, 0);
        r.timestamp  = (const char*)sqlite3_column_text(stmt, 1);
        r.operation  = (const char*)sqlite3_column_text(stmt, 2);
        r.package_id = sqlite3_column_int(stmt, 3);
        r.detail     = sqlite3_column_text(stmt, 4)
                       ? (const char*)sqlite3_column_text(stmt, 4)
                       : "";
        records.push_back(r);
    }
    sqlite3_finalize(stmt);
    return records;
}

void db_delete_package(int package_id) {
    const char* sql1 = "DELETE FROM files WHERE package_id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql1, -1, &stmt, nullptr),
          "delete_files prepare");
    sqlite3_bind_int(stmt, 1, package_id);
    check(sqlite3_step(stmt), "delete_files step");
    sqlite3_finalize(stmt);

    const char* sql2 = "DELETE FROM packages WHERE id = ?";
    check(sqlite3_prepare_v2(g_db, sql2, -1, &stmt, nullptr),
          "delete_package prepare");
    sqlite3_bind_int(stmt, 1, package_id);
    check(sqlite3_step(stmt), "delete_package step");
    sqlite3_finalize(stmt);
}

vector<string> db_get_dependents(const string& pkg_name) {
    const char* sql =
        "SELECT DISTINCT p.name FROM packages p "
        "JOIN dependencies d ON d.package_id = p.id "
        "JOIN packages dep ON d.depends_on = dep.id "
        "WHERE dep.name = ? "
        "AND p.state = 'active' "
        "AND p.type = 'managed'";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "get_dependents prepare");
    sqlite3_bind_text(stmt, 1, pkg_name.c_str(), -1, SQLITE_TRANSIENT);

    vector<string> dependents;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        dependents.push_back((const char*)sqlite3_column_text(stmt, 0));

    sqlite3_finalize(stmt);
    return dependents;
}

void db_insert_dependency(int package_id, int depends_on_id) {
    const char* sql =
        "INSERT OR IGNORE INTO dependencies "
        "(package_id, depends_on) VALUES (?, ?)";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "insert_dependency prepare");
    sqlite3_bind_int(stmt, 1, package_id);
    sqlite3_bind_int(stmt, 2, depends_on_id);
    check(sqlite3_step(stmt), "insert_dependency step");
    sqlite3_finalize(stmt);
}

PackageRecord db_get_package_version(const string& name,
                                     const string& version) {
    const char* sql =
        "SELECT id, name, version, type, state, store_path "
        "FROM packages WHERE name = ? AND version = ? LIMIT 1";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "get_package_version prepare");
    sqlite3_bind_text(stmt, 1, name.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT);

    PackageRecord rec;
    rec.id = -1;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rec.id         = sqlite3_column_int(stmt, 0);
        rec.name       = (const char*)sqlite3_column_text(stmt, 1);
        rec.version    = (const char*)sqlite3_column_text(stmt, 2);
        rec.type       = (const char*)sqlite3_column_text(stmt, 3);
        rec.state      = (const char*)sqlite3_column_text(stmt, 4);
        rec.store_path = (const char*)sqlite3_column_text(stmt, 5);
    }

    sqlite3_finalize(stmt);
    return rec;
}

void db_set_package_inactive(int package_id) {
    const char* sql = "UPDATE packages SET state = 'inactive' WHERE id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "set_inactive prepare");
    sqlite3_bind_int(stmt, 1, package_id);
    check(sqlite3_step(stmt), "set_inactive step");
    sqlite3_finalize(stmt);
}

vector<PackageRecord> db_get_all_packages() {
    const char* sql =
        "SELECT id, name, version, type, state, store_path "
        "FROM packages ORDER BY name";

    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "get_all_packages prepare");

    vector<PackageRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PackageRecord rec;
        rec.id         = sqlite3_column_int(stmt, 0);
        rec.name       = (const char*)sqlite3_column_text(stmt, 1);
        rec.version    = (const char*)sqlite3_column_text(stmt, 2);
        rec.type       = (const char*)sqlite3_column_text(stmt, 3);
        rec.state      = (const char*)sqlite3_column_text(stmt, 4);
        rec.store_path = sqlite3_column_text(stmt, 5)
                       ? (const char*)sqlite3_column_text(stmt, 5)
                       : "";
        records.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return records;
}

void db_delete_inactive_package(const string& name,
                                 const string& version) {
    PackageRecord rec = db_get_package_version(name, version);
    if (rec.id == -1)
        throw runtime_error("Package not found: " + name
                            + "-" + version);
    if (rec.state != "inactive")
        throw runtime_error(
            name + "-" + version
            + " is not inactive (state: " + rec.state + ")");

    const char* sql1 = "DELETE FROM files WHERE package_id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql1, -1, &stmt, nullptr),
          "delete_files prepare");
    sqlite3_bind_int(stmt, 1, rec.id);
    check(sqlite3_step(stmt), "delete_files step");
    sqlite3_finalize(stmt);

    const char* sql2 = "DELETE FROM packages WHERE id = ?";
    check(sqlite3_prepare_v2(g_db, sql2, -1, &stmt, nullptr),
          "delete_package prepare");
    sqlite3_bind_int(stmt, 1, rec.id);
    check(sqlite3_step(stmt), "delete_package step");
    sqlite3_finalize(stmt);
}

void db_pending_begin(int package_id,
                      const vector<string>& store_paths,
                      const vector<string>& live_paths) {
    const char* clear_sql =
        "DELETE FROM pending_links WHERE package_id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, clear_sql, -1, &stmt, nullptr),
          "pending_clear prepare");
    sqlite3_bind_int(stmt, 1, package_id);
    check(sqlite3_step(stmt), "pending_clear step");
    sqlite3_finalize(stmt);

    const char* sql =
        "INSERT INTO pending_links "
        "(package_id, store_path, live_path, state) "
        "VALUES (?, ?, ?, 'pending')";

    for (size_t i = 0; i < store_paths.size(); ++i) {
        check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
              "pending_insert prepare");
        sqlite3_bind_int(stmt,  1, package_id);
        sqlite3_bind_text(stmt, 2, store_paths[i].c_str(),
                          -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, live_paths[i].c_str(),
                          -1, SQLITE_TRANSIENT);
        check(sqlite3_step(stmt), "pending_insert step");
        sqlite3_finalize(stmt);
    }
}

void db_pending_mark_done(const string& live_path) {
    const char* sql =
        "UPDATE pending_links SET state='done' WHERE live_path=?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "pending_done prepare");
    sqlite3_bind_text(stmt, 1, live_path.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(stmt), "pending_done step");
    sqlite3_finalize(stmt);
}

void db_pending_clear(int package_id) {
    const char* sql =
        "DELETE FROM pending_links WHERE package_id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "pending_clear2 prepare");
    sqlite3_bind_int(stmt, 1, package_id);
    check(sqlite3_step(stmt), "pending_clear2 step");
    sqlite3_finalize(stmt);
}

vector<pair<string,string>> db_pending_get_done(int package_id) {
    const char* sql =
        "SELECT store_path, live_path FROM pending_links "
        "WHERE package_id = ? AND state = 'done'";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "pending_get_done prepare");
    sqlite3_bind_int(stmt, 1, package_id);

    vector<pair<string,string>> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back({
            (const char*)sqlite3_column_text(stmt, 0),
            (const char*)sqlite3_column_text(stmt, 1)
        });
    }
    sqlite3_finalize(stmt);
    return results;
}

vector<pair<string,string>> db_pending_get_all(int package_id) {
    const char* sql =
        "SELECT store_path, live_path FROM pending_links "
        "WHERE package_id = ?";
    sqlite3_stmt* stmt;
    check(sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr),
          "pending_get_all prepare");
    sqlite3_bind_int(stmt, 1, package_id);

    vector<pair<string,string>> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back({
            (const char*)sqlite3_column_text(stmt, 0),
            (const char*)sqlite3_column_text(stmt, 1)
        });
    }
    sqlite3_finalize(stmt);
    return results;
}


