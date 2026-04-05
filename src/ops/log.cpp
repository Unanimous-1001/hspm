#include "log.hpp"
#include "db/database.hpp"

static void print_records(const vector<LogRecord>& records) {
    if (records.empty()) {
        std::cout << "No log entries found.\n";
        return;
    }

    std::cout << "\n"
              << std::left
              << "  " << "ID"   << "  "
              << "TIMESTAMP            " << "  "
              << "OPERATION   " << "  "
              << "DETAIL\n";
    std::cout << "  "
              << string(70, '-') << "\n";

    for (const auto& r : records) {
        std::cout << "  "
                  << r.id        << "  "
                  << r.timestamp << "  "
                  << r.operation << "  "
                  << r.detail    << "\n";
    }
    std::cout << "\n";
}

void run_log(const string& pkg_filter, const string& op_filter) {
    vector<LogRecord> records;

    if (!pkg_filter.empty()) {
        std::cout << "Log for package: " << pkg_filter << "\n";
        records = db_get_log_by_package(pkg_filter);
    } else if (!op_filter.empty()) {
        std::cout << "Log for operation: " << op_filter << "\n";
        records = db_get_log_by_operation(op_filter);
    } else {
        std::cout << "Last 50 operations:\n";
        records = db_get_log(50);
    }

    print_records(records);
}
