#include "data/json.hpp"

#include <cctype>
#include <stdexcept>

#define FF6_JSON_MAX_DEPTH 32

namespace ff6 {
namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Json parseDocument() {
        Json value = parseValue(0);
        skipSpace();
        if (pos_ != text_.size()) fail("unexpected text after the value");
        return value;
    }

private:
    [[noreturn]] void fail(const std::string& what) const {
        throw std::runtime_error("invalid JSON at byte " + std::to_string(pos_) + ": " + what);
    }

    void skipSpace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }

    char peek() {
        skipSpace();
        if (pos_ >= text_.size()) fail("unexpected end");
        return text_[pos_];
    }

    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    bool consumeWord(const char* word) {
        const std::string w = word;
        if (text_.compare(pos_, w.size(), w) != 0) return false;
        pos_ += w.size();
        return true;
    }

    Json parseValue(int depth) {
        if (depth > FF6_JSON_MAX_DEPTH) fail("nested too deep");
        const char c = peek();
        Json value;
        if (c == '{') {
            ++pos_;
            value.type = Json::Type::Object;
            if (peek() == '}') return ++pos_, value;
            for (;;) {
                if (peek() != '"') fail("expected a name");
                std::string key = parseString();
                expect(':');
                value.members.emplace_back(std::move(key), parseValue(depth + 1));
                if (peek() == ',') {
                    ++pos_;
                    continue;
                }
                expect('}');
                return value;
            }
        }
        if (c == '[') {
            ++pos_;
            value.type = Json::Type::Array;
            if (peek() == ']') return ++pos_, value;
            for (;;) {
                value.items.push_back(parseValue(depth + 1));
                if (peek() == ',') {
                    ++pos_;
                    continue;
                }
                expect(']');
                return value;
            }
        }
        if (c == '"') {
            value.type = Json::Type::String;
            value.text = parseString();
            return value;
        }
        if (consumeWord("true")) {
            value.type = Json::Type::Bool;
            value.boolean = true;
            return value;
        }
        if (consumeWord("false")) {
            value.type = Json::Type::Bool;
            return value;
        }
        if (consumeWord("null")) return value;
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            const bool negative = c == '-';
            if (negative) ++pos_;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) fail("expected a digit");
            long number = 0;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                number = number * 10 + (text_[pos_++] - '0');
                if (number > 0x7FFFFFFFL) fail("number too large");
            }
            if (pos_ < text_.size() && (text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E')) fail("only integers are accepted");
            value.type = Json::Type::Number;
            value.number = negative ? -number : number;
            return value;
        }
        fail("unexpected character");
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            char c = text_[pos_++];
            if (c == '\\') {
                if (pos_ >= text_.size()) fail("unfinished escape");
                c = text_[pos_++];
                switch (c) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '"': case '\\': case '/': break;
                    default: fail("unsupported escape");
                }
            }
            out.push_back(c);
        }
        if (pos_ >= text_.size()) fail("unfinished string");
        ++pos_;
        return out;
    }

    const std::string& text_;
    size_t pos_ = 0;
};

}  // namespace

const Json* Json::find(const std::string& key) const {
    for (const auto& member : members) {
        if (member.first == key) return &member.second;
    }
    return nullptr;
}

Json parseJson(const std::string& text) { return Parser(text).parseDocument(); }

std::string jsonQuote(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(c);
        }
    }
    return out + "\"";
}

}  // namespace ff6
