#include "ShoutPayload.h"
#include "src/helpers/Hash.h"
#include "src/helpers/Json.hpp"
#include "src/helpers/Platform.h"
#include "src/helpers/Timestamps.hpp"

namespace {

void addString(yyjson_mut_doc * doc, yyjson_mut_val * object, const char * key, const std::string& value) {
    yyjson_mut_obj_add_strncpy(doc, object, key, value.data(), value.size());
}

}  // namespace

std::string ShoutPayload::hasChangedHash() const {
    return std::to_string(playing) +
        name +
        album +
        artist +
        std::to_string(positionMs) +
        (playedDate >= skippedDate ? playedDate : skippedDate);
}

std::string ShoutPayload::getFileHash() const {
    //
    if (fileLocation.empty()) {
        return {};
    }

    //
    return hash::md5File(platform::fromUtf8(fileLocation));
}

ShoutJSONParsingResult ShoutPayload::toJSON() const
{
    auto audioFileHash = getFileHash();
    const auto isEmptyShout = audioFileHash.empty();

    const auto doc = json::create();
    const auto obj = yyjson_mut_obj(doc.get());
    yyjson_mut_doc_set_root(doc.get(), obj);

    // Important ! we should not use local time, only UTC+0 !
    const auto date = timestamps::utcISO();
    yyjson_mut_obj_add_strncpy(doc.get(), obj, "date", date.data(), date.size());

    if (!isEmptyShout) {
        addString(doc.get(), obj, "name", name);
        addString(doc.get(), obj, "album", album);
        addString(doc.get(), obj, "artist", artist);
        addString(doc.get(), obj, "genre", genre);
        yyjson_mut_obj_add_int(doc.get(), obj, "duration", duration);
        yyjson_mut_obj_add_int(doc.get(), obj, "playerPositionMS", positionMs);
        yyjson_mut_obj_add_bool(doc.get(), obj, "playerState", playing);
        yyjson_mut_obj_add_int(doc.get(), obj, "year", year);
        addString(doc.get(), obj, "md5", audioFileHash);
    }

    return {
        .json = json::write(doc),
        .audioFileHash = std::move(audioFileHash),
        .isEmpty = isEmptyShout
    };
}
