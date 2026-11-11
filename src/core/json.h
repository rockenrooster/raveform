#pragma once
// Tiny JSON writer into a caller-provided buffer (benchmark result lines). No allocation.

#include <cstdarg>
#include <cstdio>

#include "core/types.h"

namespace rf {

class JsonWriter {
public:
    JsonWriter(char* buffer, size_t capacity) : buf_(buffer), cap_(capacity) {
        if (cap_) buf_[0] = '\0';
    }

    void beginObject(const char* key = nullptr) {
        prefix(key);
        raw("{");
        push();
    }
    void endObject() {
        raw("}");
        pop();
    }
    // Arrays: values inside take a null key (num(nullptr, v), beginObject(), ...).
    void beginArray(const char* key = nullptr) {
        prefix(key);
        raw("[");
        push();
    }
    void endArray() {
        raw("]");
        pop();
    }
    void str(const char* key, const char* value) {
        prefix(key);
        raw("\"");
        for (const char* c = value ? value : ""; *c; ++c) {
            if (*c == '"' || *c == '\\') {
                raw("\\%c", *c);
            } else if (static_cast<unsigned char>(*c) < 0x20) {
                raw("\\u%04x", static_cast<unsigned>(*c));
            } else {
                raw("%c", *c);
            }
        }
        raw("\"");
    }
    void num(const char* key, double value, int decimals = 3) {
        prefix(key);
        raw("%.*f", decimals, value);
    }
    void integer(const char* key, i64 value) {
        prefix(key);
        raw("%lld", static_cast<long long>(value));
    }
    void boolean(const char* key, bool value) {
        prefix(key);
        raw(value ? "true" : "false");
    }
    void pair(const char* key, i64 a, i64 b) {
        prefix(key);
        raw("[%lld,%lld]", static_cast<long long>(a), static_cast<long long>(b));
    }

    const char* text() const { return buf_; }
    size_t length() const { return len_; }
    bool overflowed() const { return overflow_; }

private:
    void prefix(const char* key) {
        if (depth_ > 0) {
            if (!first_[depth_]) raw(",");
            first_[depth_] = false;
        }
        if (key) raw("\"%s\":", key);
    }
    void push() {
        if (depth_ < 7) first_[++depth_] = true;
    }
    void pop() {
        if (depth_ > 0) --depth_;
    }
    void raw(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (len_ >= cap_) {
            overflow_ = true;
            return;
        }
        va_list ap;
        va_start(ap, fmt);
        const int n = std::vsnprintf(buf_ + len_, cap_ - len_, fmt, ap);
        va_end(ap);
        if (n < 0 || static_cast<size_t>(n) >= cap_ - len_) {
            overflow_ = true;
            len_ = cap_;
            return;
        }
        len_ += static_cast<size_t>(n);
    }

    char* buf_;
    size_t cap_;
    size_t len_ = 0;
    int depth_ = 0;
    bool first_[8] = {true, true, true, true, true, true, true, true};
    bool overflow_ = false;
};

}  // namespace rf
