#include "core/json_doc.h"

#include <cstdlib>
#include <cstring>

#include "core/log.h"

namespace rf {

JsonDoc::~JsonDoc() {
    std::free(nodes_);
    std::free(pool_);
}

u32 JsonDoc::addNode() {
    if (count_ == capacity_) {
        const u32 cap = capacity_ ? capacity_ * 2 : 1024;
        auto* grown = static_cast<JsonValue*>(std::realloc(nodes_, sizeof(JsonValue) * cap));
        if (!grown) {
            oom_ = true;
            return 0;
        }
        nodes_ = grown;
        capacity_ = cap;
    }
    nodes_[count_] = JsonValue{};
    return count_++;
}

u32 JsonDoc::addString(const char* s, size_t n) {
    if (poolUsed_ + n + 1 > poolCapacity_) {
        u32 cap = poolCapacity_ ? poolCapacity_ : 4096;
        while (poolUsed_ + n + 1 > cap) cap *= 2;
        auto* grown = static_cast<char*>(std::realloc(pool_, cap));
        if (!grown) {
            oom_ = true;
            return 0;
        }
        pool_ = grown;
        poolCapacity_ = cap;
    }
    const u32 at = poolUsed_;
    std::memcpy(pool_ + at, s, n);
    pool_[at + n] = '\0';
    poolUsed_ += static_cast<u32>(n + 1);
    return at;
}

bool JsonDoc::fail(const char* what) {
    u32 line = 1;
    for (size_t i = 0; i < pos_ && i < len_; ++i) line += src_[i] == '\n' ? 1u : 0u;
    RF_ERR("json %s: %s at line %u", name_, what, line);
    return false;
}

void JsonDoc::skipSpace() {
    while (pos_ < len_ && (src_[pos_] == ' ' || src_[pos_] == '\n' || src_[pos_] == '\r' || src_[pos_] == '\t')) ++pos_;
}

namespace {
// Reads a JSON string body (after the opening quote) into `out` (unescaped); returns false on error.
bool readString(const char* s, size_t len, size_t& pos, char* out, size_t& outLen) {
    outLen = 0;
    while (pos < len) {
        const char c = s[pos++];
        if (c == '"') return true;
        if (c != '\\') {
            out[outLen++] = c;
            continue;
        }
        if (pos >= len) return false;
        const char e = s[pos++];
        switch (e) {
            case 'n': out[outLen++] = '\n'; break;
            case 't': out[outLen++] = '\t'; break;
            case 'r': out[outLen++] = '\r'; break;
            case 'b': out[outLen++] = '\b'; break;
            case 'f': out[outLen++] = '\f'; break;
            case 'u': {
                if (pos + 4 > len) return false;
                unsigned v = 0;
                for (int i = 0; i < 4; ++i) {
                    const char h = s[pos++];
                    v = v * 16 + static_cast<unsigned>(h >= '0' && h <= '9' ? h - '0' : (h | 32) - 'a' + 10);
                }
                // UTF-8 encode (the design files are ASCII apart from rare symbols in notes).
                if (v < 0x80) {
                    out[outLen++] = static_cast<char>(v);
                } else if (v < 0x800) {
                    out[outLen++] = static_cast<char>(0xC0 | (v >> 6));
                    out[outLen++] = static_cast<char>(0x80 | (v & 0x3F));
                } else {
                    out[outLen++] = static_cast<char>(0xE0 | (v >> 12));
                    out[outLen++] = static_cast<char>(0x80 | ((v >> 6) & 0x3F));
                    out[outLen++] = static_cast<char>(0x80 | (v & 0x3F));
                }
                break;
            }
            default: out[outLen++] = e; break;  // \" \\ \/
        }
    }
    return false;
}
}  // namespace

bool JsonDoc::parseValue(u32 node, u32 depth) {
    if (depth > 64) return fail("nesting too deep");
    skipSpace();
    if (pos_ >= len_) return fail("unexpected end");
    const char c = src_[pos_];
    if (c == '{' || c == '[') {
        const bool object = c == '{';
        nodes_[node].type = object ? JsonValue::Type::Object : JsonValue::Type::Array;
        ++pos_;
        skipSpace();
        const char close = object ? '}' : ']';
        if (pos_ < len_ && src_[pos_] == close) {
            ++pos_;
            return true;
        }
        u32 prev = 0;
        char scratch[512];
        for (;;) {
            u32 keyAt = 0;
            if (object) {
                skipSpace();
                if (pos_ >= len_ || src_[pos_] != '"') return fail("expected a member name");
                ++pos_;
                size_t n = 0;
                const size_t start = pos_;
                // Member names are short; longer ones are an error in our data.
                size_t end = start;
                while (end < len_ && src_[end] != '"') end += src_[end] == '\\' ? 2 : 1;
                if (end - start >= sizeof scratch) return fail("member name too long");
                if (!readString(src_, len_, pos_, scratch, n)) return fail("bad member name");
                keyAt = addString(scratch, n);
                skipSpace();
                if (pos_ >= len_ || src_[pos_] != ':') return fail("expected ':'");
                ++pos_;
            }
            const u32 child = addNode();
            if (oom_) return fail("out of memory");
            nodes_[child].keyAt = keyAt;
            if (prev) {
                nodes_[prev].next = child;
            } else {
                nodes_[node].first = child;
            }
            ++nodes_[node].count;
            prev = child;
            if (!parseValue(child, depth + 1)) return false;
            skipSpace();
            if (pos_ < len_ && src_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < len_ && src_[pos_] == close) {
                ++pos_;
                return true;
            }
            return fail(object ? "expected ',' or '}'" : "expected ',' or ']'");
        }
    }
    if (c == '"') {
        ++pos_;
        size_t end = pos_;
        while (end < len_ && src_[end] != '"') end += src_[end] == '\\' ? 2 : 1;
        const size_t maxLen = end - pos_ + 1;
        char* tmp = static_cast<char*>(std::malloc(maxLen * 3 + 1));
        if (!tmp) return fail("out of memory");
        size_t n = 0;
        const bool ok = readString(src_, len_, pos_, tmp, n);
        if (ok) {
            nodes_[node].type = JsonValue::Type::String;
            nodes_[node].strAt = addString(tmp, n);
        }
        std::free(tmp);
        return ok ? true : fail("bad string");
    }
    if (c == 't' && len_ - pos_ >= 4 && std::strncmp(src_ + pos_, "true", 4) == 0) {
        nodes_[node].type = JsonValue::Type::Bool;
        nodes_[node].boolean = true;
        pos_ += 4;
        return true;
    }
    if (c == 'f' && len_ - pos_ >= 5 && std::strncmp(src_ + pos_, "false", 5) == 0) {
        nodes_[node].type = JsonValue::Type::Bool;
        pos_ += 5;
        return true;
    }
    if (c == 'n' && len_ - pos_ >= 4 && std::strncmp(src_ + pos_, "null", 4) == 0) {
        pos_ += 4;
        return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char buf[64];
        size_t n = 0;
        while (pos_ < len_ && n < sizeof buf - 1) {
            const char d = src_[pos_];
            if (!((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.' || d == 'e' || d == 'E')) break;
            buf[n++] = d;
            ++pos_;
        }
        buf[n] = '\0';
        char* end = nullptr;
        nodes_[node].type = JsonValue::Type::Number;
        nodes_[node].number = std::strtod(buf, &end);
        return end != buf ? true : fail("bad number");
    }
    return fail("unexpected character");
}

bool JsonDoc::parse(const char* text, size_t length, const char* name) {
    src_ = text;
    len_ = length;
    pos_ = 0;
    name_ = name;
    count_ = 0;
    poolUsed_ = 0;
    addString("", 0);  // offset 0 = empty string
    const u32 rootNode = addNode();
    if (oom_ || !parseValue(rootNode, 0)) return false;
    skipSpace();
    return pos_ == len_ || fail("trailing characters");
}

const JsonValue* JsonDoc::member(const JsonValue* object, const char* k) const {
    if (!object || object->type != JsonValue::Type::Object) return nullptr;
    for (const JsonValue* c = firstChild(object); c; c = nextSibling(c))
        if (std::strcmp(pool_ + c->keyAt, k) == 0) return c;
    return nullptr;
}

const JsonValue* JsonDoc::at(const JsonValue* array, u32 index) const {
    if (!array || (array->type != JsonValue::Type::Array && array->type != JsonValue::Type::Object)) return nullptr;
    const JsonValue* c = firstChild(array);
    for (u32 i = 0; c && i < index; ++i) c = nextSibling(c);
    return c;
}

const JsonValue* JsonDoc::find(const char* path, const JsonValue* from) const {
    const JsonValue* v = from ? from : root();
    char part[64];
    while (v && *path) {
        size_t n = 0;
        while (path[n] && path[n] != '.' && n < sizeof part - 1) {
            part[n] = path[n];
            ++n;
        }
        part[n] = '\0';
        path += n;
        if (*path == '.') ++path;
        if (v->type == JsonValue::Type::Array) {
            char* end = nullptr;
            const unsigned long index = std::strtoul(part, &end, 10);
            v = (end && *end == '\0') ? at(v, static_cast<u32>(index)) : nullptr;
        } else {
            v = member(v, part);
        }
    }
    return v;
}

double JsonDoc::number(const char* path, double fallback, bool* ok, const JsonValue* from) const {
    const JsonValue* v = find(path, from);
    if (v && v->type == JsonValue::Type::Number) return v->number;
    if (ok) *ok = false;
    return fallback;
}

u32 JsonDoc::numbers(const char* path, float* out, u32 max, const JsonValue* from) const {
    const JsonValue* v = find(path, from);
    if (!v) return 0;
    if (v->type == JsonValue::Type::Number) {
        if (max) out[0] = static_cast<float>(v->number);
        return max ? 1u : 0u;
    }
    u32 n = 0;
    for (const JsonValue* c = firstChild(v); c && n < max; c = nextSibling(c))
        if (c->type == JsonValue::Type::Number) out[n++] = static_cast<float>(c->number);
    return n;
}

bool JsonDoc::text(const char* path, char* out, size_t capacity, const JsonValue* from) const {
    const JsonValue* v = find(path, from);
    if (!v || v->type != JsonValue::Type::String || capacity == 0) return false;
    const char* s = pool_ + v->strAt;
    size_t n = std::strlen(s);
    if (n >= capacity) n = capacity - 1;
    std::memcpy(out, s, n);
    out[n] = '\0';
    return true;
}

}  // namespace rf
