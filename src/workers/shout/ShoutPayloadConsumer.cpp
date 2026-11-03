// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ShoutPayloadConsumer.h"
#include "src/workers/shout/ShoutPayload.h"

ShoutConsumeResult ShoutPayloadConsumer::consume(const ShoutPayload& incomingPayload) {
    auto output = ShoutConsumeResult {};

    //
    const auto currHash = incomingPayload.hasChangedHash();       // hash blueprint
    output.hasMeaningfulChange = lastHasChangedHash != currHash;  // check if strings are identical
    lastHasChangedHash = currHash;                                // replace old hash with new

    //
    output.parsingResult = incomingPayload.toJSON();

    //
    return output;
}
