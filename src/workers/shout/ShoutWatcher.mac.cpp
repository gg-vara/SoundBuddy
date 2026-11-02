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

#include "ShoutWatcher.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "src/helpers/Json.hpp"
#include "src/logs/Logs.h"
#include "src/workers/base/Wait.h"

extern char** environ;  // NOLINT(readability-redundant-declaration) : of the process, for the scripts it runs

namespace {

// what the Music app plays : its current track, then the player position and state, as an AppleScript list
constexpr std::string_view currentlyPlayingScript = R"applescript(tell application "Music"
	if skipped date of current track is not missing value then
		set SkpDt to skipped date of current track as «class isot» as string
	else
		set SkpDt to ""
	end if
	if played date of current track is not missing value then
		set PlyDt to played date of current track as «class isot» as string
	else
		set PlyDt to ""
	end if
	get {file location, name, album, artist, genre, duration, year} of current track & player position & (player state as string) & SkpDt & PlyDt
end tell
)applescript";

// longer, the script is given up
constexpr auto scriptTimeout = std::chrono::seconds { 5 };

std::string toString(yyjson_val* value) {
    return std::string { json::string(value) };
}

// file descriptors of a pipe, closed once destroyed
class Pipe {
 public:
    Pipe() {
        if (pipe(_ends.data()) != 0) _ends = { -1, -1 };
    }
    ~Pipe() {
        closeReading();
        closeWriting();
    }
    Pipe(const Pipe&) = delete;
    void operator=(const Pipe&) = delete;

    bool isOpen() const { return _ends[0] >= 0 && _ends[1] >= 0; }
    int reading() const { return _ends[0]; }
    int writing() const { return _ends[1]; }

    void closeReading() { _close(_ends[0]); }
    void closeWriting() { _close(_ends[1]); }

 private:
    std::array<int, 2> _ends {};

    static void _close(int& end) {
        if (end >= 0) close(end);
        end = -1;
    }
};

// the result of "script", in its recompilable form (osascript -s s), or nothing if it failed, took too long, or "stop" was requested
std::optional<std::string> runScript(std::string_view script, const std::stop_token& stop) {
    Pipe input;
    Pipe output;
    if (!input.isOpen() || !output.isOpen()) return std::nullopt;

    posix_spawn_file_actions_t actions {};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, input.reading(), STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output.writing(), STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, input.writing());
    posix_spawn_file_actions_addclose(&actions, output.reading());
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    std::array<const char*, 6> arguments { "/usr/bin/osascript", "-l", "AppleScript", "-s", "s", nullptr };
    pid_t process = 0;
    const auto spawned = posix_spawn(
        &process, arguments[0], &actions, nullptr, const_cast<char* const*>(arguments.data()),
        environ  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    );
    posix_spawn_file_actions_destroy(&actions);

    // the child keeps its own ends
    input.closeReading();
    output.closeWriting();
    if (spawned != 0) return std::nullopt;

    // the script, on its standard input; a child gone early does not raise SIGPIPE
    fcntl(input.writing(), F_SETNOSIGPIPE, 1);
    for (std::size_t written = 0; written < script.size();) {
        const auto count = write(input.writing(), script.data() + written, script.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        written += static_cast<std::size_t>(count);
    }
    input.closeWriting();

    // its result, until it ends
    std::string result;
    bool complete = false;
    const auto deadline = std::chrono::steady_clock::now() + scriptTimeout;
    while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) {
        pollfd readable { .fd = output.reading(), .events = POLLIN, .revents = 0 };
        const auto ready = poll(&readable, 1, 100);
        if (ready < 0 && errno != EINTR) break;
        if (ready <= 0) continue;

        std::array<char, 4096> buffer {};
        const auto count = read(output.reading(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            complete = count == 0;
            break;
        }
        result.append(buffer.data(), static_cast<std::size_t>(count));
    }

    if (!complete) kill(process, SIGKILL);

    int status = 0;
    while (waitpid(process, &status, 0) < 0 && errno == EINTR) {
    }

    if (!complete || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return std::nullopt;
    return result;
}

// what the script tells, nothing playing if it tells nothing
ShoutPayload toPayload(std::string result) {
    ShoutPayload payload {};

    // the list, in braces, as a JSON array
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    if (result.size() < 2) return payload;
    result.front() = '[';
    result.back() = ']';

    const auto doc = json::parse(result);
    auto* const trackData = json::root(doc);
    const auto at = [trackData](std::size_t index) { return yyjson_arr_get(trackData, index); };

    payload.fileLocation = toString(at(0));
    payload.name = toString(at(1));
    payload.album = toString(at(2));
    payload.artist = toString(at(3));
    payload.genre = toString(at(4));
    payload.duration = static_cast<int>(json::integer(at(5)));
    payload.year = static_cast<int>(json::integer(at(6)));
    payload.positionMs = static_cast<int>(json::integer(at(7)) * 1000);  // in seconds
    payload.playing = json::string(at(8)) != "paused";
    payload.skippedDate = toString(at(9));
    payload.playedDate = toString(at(10));
    return payload;
}

}  // namespace

// the session is always available : the script asks the Music app, launched or not
bool ShoutWatcher::_waitForAvailableSession(const std::stop_token& stop) {
    return !stop.stop_requested();
}

void ShoutWatcher::_execWatchSession(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        if (auto result = runScript(currentlyPlayingScript, stop)) {
            // the same shout as before is not shouted again
            _processPayload(toPayload(std::move(*result)));
        } else if (!stop.stop_requested()) {
            logs::debug("ShoutWatcher", "Could not ask the Music app what it plays");
        }

        workers::waitFor(stop, std::chrono::seconds { 1 });
    }
}
