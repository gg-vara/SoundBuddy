#include "ShoutPayload.h"
#include "src/helpers/Hash.h"
#include "src/helpers/Json.hpp"
#include "src/helpers/Timestamps.hpp"

namespace {

void addString(yyjson_mut_doc * doc, yyjson_mut_val * object, const char * key, const QString& value) {
    const auto utf8 = value.toUtf8();
    yyjson_mut_obj_add_strncpy(doc, object, key, utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

}  // namespace

QString ShoutPayload::hasChangedHash() const {
    return QString::number(iPlayerState) + 
        tName + 
        tAlbum + 
        tArtist + 
        QString::number(iPlayerPosMS) +
        (tDatePlayed >= tDateSkipped ? tDatePlayed : tDateSkipped);
}

QString ShoutPayload::getFileHash() const {
    //
    if (tFileLocation.isEmpty()) {
        return QString();
    }

    //
    return QString::fromStdString(hash::md5File(std::filesystem::path(tFileLocation.toStdWString())));
}

ShoutJSONParsingResult ShoutPayload::toJSON() const
{
    auto audioFileHash = getFileHash();
    const auto isEmptyShout = audioFileHash.isEmpty();

    const auto doc = json::create();
    const auto obj = yyjson_mut_obj(doc.get());
    yyjson_mut_doc_set_root(doc.get(), obj);

    // Important ! we should not use local time, only UTC+0 !
    const auto date = timestamps::utcISO();
    yyjson_mut_obj_add_strncpy(doc.get(), obj, "date", date.data(), date.size());

    if (!isEmptyShout) {
        addString(doc.get(), obj, "name", tName);
        addString(doc.get(), obj, "album", tAlbum);
        addString(doc.get(), obj, "artist", tArtist);
        addString(doc.get(), obj, "genre", tGenre);
        yyjson_mut_obj_add_int(doc.get(), obj, "duration", iDuration);
        yyjson_mut_obj_add_int(doc.get(), obj, "playerPositionMS", iPlayerPosMS);
        yyjson_mut_obj_add_bool(doc.get(), obj, "playerState", iPlayerState);
        yyjson_mut_obj_add_int(doc.get(), obj, "year", tYear);
        addString(doc.get(), obj, "md5", audioFileHash);
    }

    return {
        json::write(doc),
        std::move(audioFileHash),
        isEmptyShout
    };
}
