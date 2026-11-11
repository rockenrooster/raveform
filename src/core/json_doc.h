#pragma once
// Minimal JSON reader for the design data (design/*.json, embedded in the binary) at start-up. Parses
// into a node array with linked children and looks values up by dotted path ("bloom.weights",
// "variants.1.id"). Storage comes from malloc and is released with the document: never use it in the
// frame loop.

#include <cstddef>

#include "core/types.h"

namespace rf {

struct JsonValue {
    enum class Type : u8 { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    u32 count = 0;      // children of an array or object
    u32 first = 0;      // node index of the first child (0: none; node 0 is the root)
    u32 next = 0;       // node index of the next sibling (0: none)
    u32 keyAt = 0;      // offset of the member name in the string pool (objects only)
    u32 strAt = 0;      // offset of the string value in the string pool
    double number = 0.0;
};

class JsonDoc {
public:
    JsonDoc() = default;
    JsonDoc(const JsonDoc&) = delete;
    JsonDoc& operator=(const JsonDoc&) = delete;
    ~JsonDoc();

    // Returns false (and logs where) on a syntax error. `name` is used in messages.
    bool parse(const char* text, size_t length, const char* name);

    const JsonValue* root() const { return count_ ? &nodes_[0] : nullptr; }
    // Dotted path from the root (or from `from`); numeric parts index arrays. nullptr when missing.
    const JsonValue* find(const char* path, const JsonValue* from = nullptr) const;
    const JsonValue* member(const JsonValue* object, const char* key) const;
    const JsonValue* at(const JsonValue* array, u32 index) const;
    const JsonValue* firstChild(const JsonValue* v) const { return v && v->first ? &nodes_[v->first] : nullptr; }
    const JsonValue* nextSibling(const JsonValue* v) const { return v && v->next ? &nodes_[v->next] : nullptr; }
    const char* key(const JsonValue* v) const { return pool_ + v->keyAt; }
    const char* string(const JsonValue* v) const { return v && v->type == JsonValue::Type::String ? pool_ + v->strAt : ""; }

    // Convenience lookups with defaults (the design files are the single source of the numbers: a
    // missing value is logged once by the caller through `ok`).
    double number(const char* path, double fallback, bool* ok = nullptr, const JsonValue* from = nullptr) const;
    // Fills up to `max` numbers from an array (or one from a number); returns how many were read.
    u32 numbers(const char* path, float* out, u32 max, const JsonValue* from = nullptr) const;
    bool text(const char* path, char* out, size_t capacity, const JsonValue* from = nullptr) const;

private:
    u32 addNode();
    u32 addString(const char* s, size_t n);
    bool parseValue(u32 node, u32 depth);
    void skipSpace();
    bool fail(const char* what);

    const char* src_ = nullptr;
    size_t len_ = 0;
    size_t pos_ = 0;
    const char* name_ = "";
    JsonValue* nodes_ = nullptr;
    u32 count_ = 0;
    u32 capacity_ = 0;
    char* pool_ = nullptr;
    u32 poolUsed_ = 0;
    u32 poolCapacity_ = 0;
    bool oom_ = false;
};

}  // namespace rf
