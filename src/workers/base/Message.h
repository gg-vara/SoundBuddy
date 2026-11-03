// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>
#include <string_view>

// Messages of the workers, shown in the user interface
namespace workers {

enum class MessageKind { Standard, Issue, Warning };

// whether a message follows the previous ones, or replaces the last one (as a progress)
enum class LineMode { Append, ReplaceLast };

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
