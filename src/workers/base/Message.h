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

#include <functional>
#include <string>
#include <string_view>

// Messages of the workers, shown in the user interface
namespace workers {

enum class MessageKind {
    Standard,
    Issue,
    Warning
};

// whether a message follows the previous ones, or replaces the last one (as a progress)
enum class LineMode {
    Append,
    ReplaceLast
};

struct Message {
    std::string text;  // UTF-8, translated
    MessageKind kind = MessageKind::Standard;
    LineMode line = LineMode::Append;
};

// receives the messages of a worker, on the worker's thread
using MessageSink = std::function<void(const Message&)>;

// writes "message" to the logs, under "source", then passes it to "sink" if any
void report(const MessageSink& sink, std::string_view source, const Message& message);

}  // namespace workers
