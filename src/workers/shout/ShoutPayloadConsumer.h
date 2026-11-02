// SoundBuddy
// Small companion app that feeds SoundVitrine with ITunes / Music library metadata
// Copyright (C) 2019-2023 Guillaume Vara <guillaume.vara@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Any graphical or audio resources available within the source code may
// use a different license and copyright : please refer to their metadata
// for further details. Resources without explicit references to a
// different license and copyright still refer to this GPL.

#pragma once

#include <string>

#include "ShoutPayload.h"

struct ShoutConsumeResult {
    bool hasMeaningfulChange = false;
    ShoutJSONParsingResult parsingResult;
};

struct ShoutPayloadConsumer {
    std::string lastHasChangedHash;

    /** updates state and produce a JSON output */
    ShoutConsumeResult consume(const ShoutPayload& incomingPayload);
};