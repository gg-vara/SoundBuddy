// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// src/helpers/Json.h

#include <yyjson.h>

#include "Check.h"
#include "src/helpers/Json.h"

int main() {
    CHECK(!json::parse("{ nope"), "invalid JSON : null document");
    CHECK(json::root(json::parse("{ nope")) == nullptr, "root of a null document");

    const auto doc = json::parse(R"(["aé", 3, 2.9, true, null])");
    auto* const root = json::root(doc);
    CHECK(json::string(yyjson_arr_get(root, 0)) == "a\xC3\xA9", "string, unicode escape");
    CHECK(json::string(yyjson_arr_get(root, 1), "fallback") == "fallback", "string fallback");
    CHECK(json::integer(yyjson_arr_get(root, 1)) == 3 && json::integer(yyjson_arr_get(root, 2)) == 2, "integer, decimals truncated");
    CHECK(
        json::integer(yyjson_arr_get(root, 0), -1) == -1 && json::integer(yyjson_arr_get(root, 99)) == 0, "integer fallback, out of range"
    );
    CHECK(json::boolean(yyjson_arr_get(root, 3)) && !json::boolean(yyjson_arr_get(root, 4)), "boolean");

    const auto created = json::create();
    auto* const object = yyjson_mut_obj(created.get());
    yyjson_mut_doc_set_root(created.get(), object);
    yyjson_mut_obj_add_str(created.get(), object, "name", "\xC3\xA9t\xC3\xA9 \"live\"");
    yyjson_mut_obj_add_int(created.get(), object, "year", 1999);
    CHECK(json::write(created) == "{\"name\":\"\xC3\xA9t\xC3\xA9 \\\"live\\\"\",\"year\":1999}", "write, escapes");

    return check::summary();
}
