#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ff6 {

// A parsed JSON value. Numbers are integers, which is all that a save needs.
struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    long number = 0;
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;

    // The member with this name, or null.
    const Json* find(const std::string& key) const;
};

// Throws std::runtime_error with the position of the error.
Json parseJson(const std::string& text);

// A string with its quotes and the escapes that JSON needs.
std::string jsonQuote(const std::string& text);

}  // namespace ff6
