// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <yyjson.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

// Thin helpers over yyjson : owning documents, and lenient value accessors
namespace json {

using Document = std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)>;
using MutableDocument = std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)>;

// parsed "text", or a null document if it is not valid JSON
inline Document parse(std::string_view text) {
    return { yyjson_read(text.data(), text.size(), 0), &yyjson_doc_free };
}

// root value of "doc", or null
inline yyjson_val* root(const Document& doc) {
    return yyjson_doc_get_root(doc.get());
}

// new empty document, to build with yyjson_mut_* functions
inline MutableDocument create() {
    return { yyjson_mut_doc_new(nullptr), &yyjson_mut_doc_free };
}

// "doc" serialized, or an empty string on failure
inline std::string write(const MutableDocument& doc, yyjson_write_flag flags = YYJSON_WRITE_NOFLAG) {
    std::size_t length = 0;
    const std::unique_ptr<char, decltype(&std::free)> text { yyjson_mut_write(doc.get(), flags, &length), &std::free };
    if (!text) return {};
    return { text.get(), length };
}

// string content of "value", or "fallback" if it is not a string
inline std::string_view string(yyjson_val* value, std::string_view fallback = {}) {
    if (!yyjson_is_str(value)) return fallback;
    return { yyjson_get_str(value), yyjson_get_len(value) };
}

// "value" as an integer (decimals truncated), or "fallback" if it is not a number
inline std::int64_t integer(yyjson_val* value, std::int64_t fallback = 0) {
    if (yyjson_is_int(value)) return yyjson_get_sint(value);
    if (yyjson_is_real(value)) return static_cast<std::int64_t>(yyjson_get_real(value));
    return fallback;
}

// "value" as a boolean, or "fallback" if it is not one
inline bool boolean(yyjson_val* value, bool fallback = false) {
    return yyjson_is_bool(value) ? yyjson_get_bool(value) : fallback;
}

}  // namespace json
