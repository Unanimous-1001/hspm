#pragma once
#include <iostream>
#include <string>
#include <functional>

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static void test(const std::string& name, std::function<void()> fn) {
    tests_run++;
    try {
        fn();
        std::cout << "  PASS: " << name << "\n";
        tests_passed++;
    } catch (const std::exception& e) {
        std::cout << "  FAIL: " << name << "\n"
                  << "        " << e.what() << "\n";
        tests_failed++;
    }
}

static void assert_true(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error("Assertion failed: " + msg);
}

static void assert_eq(const std::string& a, const std::string& b,
                      const std::string& msg) {
    if (a != b)
        throw std::runtime_error(
            "Assertion failed: " + msg
            + "\n          expected: " + b
            + "\n          got:      " + a);
}

static void assert_eq(int a, int b, const std::string& msg) {
    if (a != b)
        throw std::runtime_error(
            "Assertion failed: " + msg
            + "\n          expected: " + std::to_string(b)
            + "\n          got:      " + std::to_string(a));
}

static void print_results() {
    std::cout << "\n--- Results ---\n"
              << "  Run:    " << tests_run    << "\n"
              << "  Passed: " << tests_passed << "\n"
              << "  Failed: " << tests_failed << "\n";
    if (tests_failed == 0)
        std::cout << "  ALL TESTS PASSED\n";
}
