// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Message.h"

#include <string_view>

#include "src/logs/Logs.h"

namespace workers {

void report(const MessageSink& sink, std::string_view source, const Message& message) {
    auto level = logs::Level::Info;
    switch (message.kind) {
        case MessageKind::Issue:
            level = logs::Level::Error;
            break;
        case MessageKind::Warning:
            level = logs::Level::Warning;
            break;
        case MessageKind::Standard:
            break;
    }
    logs::write(level, source, message.text);

    if (sink) sink(message);
}

}  // namespace workers
