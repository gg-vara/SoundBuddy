// src/net/SseParser

#include <string>
#include <vector>

#include "Check.h"
#include "src/net/SseParser.h"

namespace {

struct Parsed {
    std::vector<net::SseEvent> events;
    net::SseParser parser { [this](const net::SseEvent& event) { events.push_back(event); } };

    explicit Parsed(const std::vector<std::string>& chunks) {
        for (const auto& chunk : chunks) parser.feed(chunk);
    }
};

}  // namespace

int main() {
    {
        const Parsed parsed { { "data: hello\n\n" } };
        CHECK(parsed.events.size() == 1 && parsed.events[0].type == "message" && parsed.events[0].data == "hello", "simple event");
    }
    {
        const Parsed parsed { { "event: score\ndata: line1\r", "\ndata: line2\r\n\r\n" } };
        CHECK(parsed.events.size() == 1 && parsed.events[0].type == "score" && parsed.events[0].data == "line1\nline2",
            "CRLF split between chunks, multi-line data, typed event");
    }
    {
        const Parsed parsed { { ": comment\n", "id: 42\ndata:no-space\n\n", "data: x\n\n" } };
        CHECK(parsed.events.size() == 2 && parsed.events[0].id == "42" && parsed.events[0].data == "no-space" && parsed.events[1].id == "42",
            "comment ignored, id kept for the next events");
    }
    {
        const Parsed parsed { { "\xEF\xBB\xBF" "retry: 3000\nevent: ignored\n\n", "data: a\n" } };
        CHECK(parsed.events.empty() && parsed.parser.retry() && parsed.parser.retry()->count() == 3000,
            "BOM, retry, event without data discarded, unterminated event pending");
    }
    {
        const Parsed parsed { { "data\n\ndata:  two spaces\n\n" } };
        CHECK(parsed.events.size() == 2 && parsed.events[0].data.empty() && parsed.events[1].data == " two spaces",
            "field without colon, only one leading space removed");
    }

    return check::summary();
}
