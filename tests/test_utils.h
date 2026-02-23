/*
 * File: tests/test_utils.h
 *
 * Purpose:
 *   Minimal internal test harness for registering tests, running assertions, and reporting pass/fail outcomes.
 *
 * Design Notes:
 *   - This file is part of the production-grade refactor where responsibilities are intentionally split.
 *   - The intent is to keep logic predictable, observable, and recoverable under partial-runtime scenarios.
 *   - Error paths are expected in real user environments (missing Oculus runtime, missing SteamVR, permissions).
 *
 * Maintenance Guidance:
 *   - Keep behavior deterministic and avoid hidden side effects.
 *   - Prefer explicit logging and explicit return values over implicit assumptions.
 *   - If a change alters runtime behavior, update tests and diagnostics messaging in the same change.
 */
#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace odtkra::test {

using TestFn = std::function<void()>;

struct Registry {
    std::vector<std::pair<std::string, TestFn>> tests;

    static Registry& instance() {
        static Registry value;
        return value;
    }

    void add(const std::string& name, TestFn fn) {
        tests.emplace_back(name, std::move(fn));
    }
};

class TestFailure : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message) : std::runtime_error(message) {}
};

inline int run_all() {
    int failures = 0;
    for (const auto& [name, fn] : Registry::instance().tests) {
        try {
            fn();
            std::cout << "[PASS] " << name << "\n";
        } catch (const std::exception& ex) {
            ++failures;
            std::cout << "[FAIL] " << name << ": " << ex.what() << "\n";
        }
    }
    return failures;
}

} // namespace odtkra::test

#define TEST_CASE(name) \
    static void test_##name(); \
    namespace { \
    struct reg_##name { \
        reg_##name() { ::odtkra::test::Registry::instance().add(#name, test_##name); } \
    } reg_instance_##name; \
    } \
    static void test_##name()

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            throw ::odtkra::test::TestFailure(std::string("Requirement failed: ") + #cond); \
        } \
    } while (false)
