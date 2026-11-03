// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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