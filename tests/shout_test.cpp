// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// ShoutPayload and ShoutPayloadConsumer

#include <yyjson.h>

#include <fstream>
#include <ios>

#include "Check.h"
#include "TestData.h"
#include "src/helpers/Json.h"
#include "src/helpers/Platform.h"
#include "src/workers/shout/ShoutPayload.h"
#include "src/workers/shout/ShoutPayloadConsumer.h"

int main() {
    // nothing playing : only the date
    const auto empty = ShoutPayload {}.toJSON();
    const auto emptyDoc = json::parse(empty.json);
    CHECK(empty.isEmpty && yyjson_obj_size(json::root(emptyDoc)) == 1, "empty shout : only the date");
    CHECK(json::string(yyjson_obj_get(json::root(emptyDoc), "date")).ends_with('Z'), "UTC date");

    // a track, from a file at a non-ASCII path
    const auto file = testdata::scratchDirectory("shout é") / platform::fromUtf8("caf\xC3\xA9.mp3");
    {
        std::ofstream { file, std::ios::binary } << "abc";
    }

    const ShoutPayload payload { .playing = true,
                                 .positionMs = 12000,
                                 .duration = 215,
                                 .year = 1999,
                                 .fileLocation = platform::toUtf8(file),
                                 .name = "Caf\xC3\xA9 \"Live\"",
                                 .album = "Album",
                                 .artist = "Artist",
                                 .genre = "Genre",
                                 .skippedDate = "2026-01-01",
                                 .playedDate = "2026-02-01" };
    const auto result = payload.toJSON();
    const auto doc = json::parse(result.json);
    auto* const root = json::root(doc);

    CHECK(!result.isEmpty && result.audioFileHash == "900150983cd24fb0d6963f7d28e17f72", "md5 of the audio file");
    CHECK(json::string(yyjson_obj_get(root, "name")) == "Caf\xC3\xA9 \"Live\"", "name : UTF-8, quotes");
    CHECK(
        json::integer(yyjson_obj_get(root, "duration")) == 215 && json::integer(yyjson_obj_get(root, "playerPositionMS")) == 12000 &&
            json::integer(yyjson_obj_get(root, "year")) == 1999,
        "numbers"
    );
    CHECK(
        json::boolean(yyjson_obj_get(root, "playerState")) && json::string(yyjson_obj_get(root, "md5")) == result.audioFileHash,
        "state and md5"
    );
    CHECK(
        payload.hasChangedHash() ==
            "1Caf\xC3\xA9 \"Live\"AlbumArtist12000"
            "2026-02-01",
        "change footprint : latest of played / skipped dates"
    );

    // only meaningful changes are shouted
    ShoutPayloadConsumer consumer;
    CHECK(consumer.consume(payload).hasMeaningfulChange, "first shout");
    CHECK(!consumer.consume(payload).hasMeaningfulChange, "same shout : no change");

    auto paused = payload;
    paused.playing = false;
    CHECK(consumer.consume(paused).hasMeaningfulChange, "paused : change");

    testdata::removeScratchDirectories();
    return check::summary();
}
