// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

struct ShoutJSONParsingResult {
    std::string json;
    std::string audioFileHash;
    bool isEmpty;
};

/** Shape of a Shout; texts are UTF-8 */
struct ShoutPayload {
    bool playing;
    int positionMs;
    int duration;
    int year;
    std::string fileLocation;
    std::string name;
    std::string album;
    std::string artist;
    std::string genre;
    std::string skippedDate;
    std::string playedDate;

    /** generates shout data footprint, used to compare shout payloads for changes */
    std::string hasChangedHash() const;

    /** gets the associated file's footprint */
    std::string getFileHash() const;

    /** serializes the shout, timestamped; only the date if no track is playing */
    struct ShoutJSONParsingResult toJSON() const;
};