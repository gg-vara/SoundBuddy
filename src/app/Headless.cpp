// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Headless.h"

#include <yyjson.h>

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "src/helpers/Json.h"
#include "src/app/Controller.h"
#include "src/app/Post.h"
#include "version.h"

namespace ui {

namespace {

// tasks of the workers and of the input, run in turn
struct Tasks {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::function<void()>> queue;
    bool accepting = false;
};

Tasks& tasks() {
    static Tasks instance;
    return instance;
}

void postToQueue(std::function<void()> task) {
    auto& all = tasks();
    {
        const std::scoped_lock lock { all.mutex };
        if (!all.accepting) return;
        all.queue.push_back(std::move(task));
    }
    all.changed.notify_one();
}

void setAccepting(bool accepting) {
    auto& all = tasks();
    const std::scoped_lock lock { all.mutex };
    all.accepting = accepting;
    if (!accepting) all.queue.clear();
}

std::function<void()> nextTask() {
    auto& all = tasks();
    std::unique_lock lock { all.mutex };
    all.changed.wait(lock, [&all] { return !all.queue.empty(); });
    auto task = std::move(all.queue.front());
    all.queue.pop_front();
    return task;
}

// a whole line, then flushed : another program waits for it
void writeLine(std::string_view line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// JSON on one line : only whitespace between its values may hold line breaks, as its texts escape them
std::string oneLine(std::string json) {
    std::erase_if(json, [](char character) { return character == '\n' || character == '\r'; });
    return json;
}

std::string dialog(std::string_view kind, std::string_view title, std::string_view text) {
    const auto doc = json::create();
    auto* const root = yyjson_mut_obj(doc.get());
    yyjson_mut_doc_set_root(doc.get(), root);
    yyjson_mut_obj_add_str(doc.get(), root, "type", "dialog");
    yyjson_mut_obj_add_strncpy(doc.get(), root, "kind", kind.data(), kind.size());
    yyjson_mut_obj_add_strncpy(doc.get(), root, "title", title.data(), title.size());
    yyjson_mut_obj_add_strncpy(doc.get(), root, "text", text.data(), text.size());
    return json::write(doc);
}

class HeadlessShell final : public Shell {
 public:
    bool sendToPage(const std::string& event) override {
        writeLine(oneLine(event));
        return true;
    }

    void showWindow() override {}

    void setTrayState(TrayState state) override {
        const char* dot = "none";
        if (state.dot == TrayState::Dot::Ok) dot = "ok";
        if (state.dot == TrayState::Dot::Issue) dot = "issue";
        writeLine(std::string { R"({"type":"tray","shouting":)" } + (state.shouting ? "true" : "false") + R"(,"dot":")" + dot + R"("})");
    }

    bool ask(std::string_view title, std::string_view question, Answer /*byDefault*/) override {
        writeLine(dialog("ask", title, question));
        return true;
    }

    void tell(std::string_view title, std::string_view text, Severity severity) override {
        writeLine(dialog(severity == Severity::Error ? "error" : "information", title, text));
    }

    void quit() override { _running = false; }

    bool isRunning() const { return _running; }

 private:
    bool _running = true;
};

void handleLine(Controller& controller, std::size_t number, std::string_view line) {
    const auto separator = line.find(' ');
    const auto verb = line.substr(0, separator);
    const auto request = separator == std::string_view::npos ? std::string_view {} : line.substr(separator + 1);
    const auto prefix = R"(,"line":)" + std::to_string(number);

    if (verb == "query") {
        writeLine(R"({"type":"answer")" + prefix + R"(,"value":)" + oneLine(controller.query(request)) + "}");
    } else if (verb == "command") {
        controller.command(request);
        writeLine(R"({"type":"done")" + prefix + "}");
    } else {
        writeLine(R"({"type":"error")" + prefix + R"(,"message":"Expected: query [...] or command [...]"})");
    }
}

// some bytes of the standard input, none at its end. Not through its C++ or C streams, whose locks would hold the process back at exit
std::size_t readInput(std::array<char, 4096>& buffer) {
#ifdef _WIN32
    DWORD count = 0;
    const auto read = ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr);
    return read != FALSE ? count : 0;
#else
    const auto count = ::read(STDIN_FILENO, buffer.data(), buffer.size());
    return count > 0 ? static_cast<std::size_t>(count) : 0;
#endif
}

// reads lines until the end of the input, which it may never reach : never joined, it ends with the process
void readLines(Controller& controller) {
    std::string pending;
    std::size_t number = 0;
    std::array<char, 4096> buffer {};

    while (const auto count = readInput(buffer)) {
        pending.append(buffer.data(), count);

        for (auto end = pending.find('\n'); end != std::string::npos; end = pending.find('\n')) {
            auto line = pending.substr(0, end);
            pending.erase(0, end + 1);
            if (line.ends_with('\r')) line.pop_back();
            if (line.empty()) continue;

            post([&controller, number = ++number, line = std::move(line)] { handleLine(controller, number, line); });
        }
    }

    post([&controller] { controller.requestQuit(); });
}

}  // namespace

int runHeadless() {
#ifdef _WIN32
    // lines end the same on every platform
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    setPoster(&postToQueue);
    setAccepting(/*accepting=*/true);

    {
        HeadlessShell shell;
        Controller controller { shell };

        writeLine(R"({"type":"started","version":")" APP_CURRENT_VERSION R"("})");

        std::thread { [&controller] { readLines(controller); } }.detach();

        while (shell.isRunning()) nextTask()();

        // the workers' last messages have nowhere to go
        setAccepting(/*accepting=*/false);
    }

    return 0;
}

}  // namespace ui
