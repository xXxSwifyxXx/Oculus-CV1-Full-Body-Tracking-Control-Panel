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
