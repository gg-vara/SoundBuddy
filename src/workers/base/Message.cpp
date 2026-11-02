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

#include "Message.h"

#include <utility>

#include "src/logs/Logs.h"

namespace workers {

void report(const MessageSink& sink, std::string_view source, const Message& message) {
    const auto level =
        message.kind == MessageKind::Issue ? logs::Level::Error :
        message.kind == MessageKind::Warning ? logs::Level::Warning :
        logs::Level::Info;
    logs::write(level, source, message.text);

    if (sink) sink(message);
}

}  // namespace workers
