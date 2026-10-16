#pragma once

#include <cstdio>
#include <functional>
#include <stdexcept>

namespace test {

inline int failures = 0;

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
        ++failures;
    }
}

inline bool throws(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}  // namespace test

#define CHECK(expression) ::test::check((expression), #expression, __FILE__, __LINE__)
