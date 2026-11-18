// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LoopbackRedirect.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace net {

namespace {

#ifdef _WIN32
using Socket = SOCKET;
using Length = int;

int pollSocket(Socket socket, int timeoutMs) {
    WSAPOLLFD watched { .fd = socket, .events = POLLRDNORM, .revents = 0 };
    return WSAPoll(&watched, 1, timeoutMs);
}

void closeSocket(Socket socket) {
    closesocket(socket);
}
#else
using Socket = int;
using Length = socklen_t;

int pollSocket(Socket socket, int timeoutMs) {
    pollfd watched { .fd = socket, .events = POLLIN, .revents = 0 };
    return poll(&watched, 1, timeoutMs);
}

void closeSocket(Socket socket) {
    close(socket);
}
#endif

const auto invalidSocket = static_cast<std::uintptr_t>(static_cast<Socket>(-1));

Socket native(std::uintptr_t socket) {
    return static_cast<Socket>(socket);
}

// how often a wait looks at its stop token
constexpr int pollMs = 200;

// what a browser takes to send its request line, once connected
constexpr auto requestTimeout = std::chrono::seconds { 5 };

// enough for a request line with its query : a code and a state, and what the platform adds
constexpr std::size_t maxRequestBytes = std::size_t { 16 } * 1024;

// the request line of a connection : "GET /callback?... HTTP/1.1", or empty
std::string requestLine(Socket client) {
    std::string received;
    std::array<char, 2048> buffer {};
    const auto until = std::chrono::steady_clock::now() + requestTimeout;

    while (received.find("\r\n") == std::string::npos && received.size() < maxRequestBytes) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - std::chrono::steady_clock::now()).count();
        if (left <= 0 || pollSocket(client, static_cast<int>(left)) <= 0) return {};

        const auto count = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (count <= 0) return {};
        received.append(buffer.data(), static_cast<std::size_t>(count));
    }

    return received.substr(0, received.find("\r\n"));
}

void answer(Socket client, std::string_view status, std::string_view page) {
    const auto response = std::format(
        "HTTP/1.1 {}\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {}\r\nCache-Control: no-store\r\n"
        "Connection: close\r\n\r\n{}",
        status, page.size(), page
    );

    std::size_t sent = 0;
    while (sent < response.size()) {
        const auto count = send(client, response.data() + sent, static_cast<int>(response.size() - sent), 0);
        if (count <= 0) return;
        sent += static_cast<std::size_t>(count);
    }
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// as application/x-www-form-urlencoded writes it
std::string decoded(std::string_view text) {
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') {
            result += ' ';
        } else if (text[i] == '%' && i + 2 < text.size() && hexValue(text[i + 1]) >= 0 && hexValue(text[i + 2]) >= 0) {
            result += static_cast<char>((hexValue(text[i + 1]) * 16) + hexValue(text[i + 2]));
            i += 2;
        } else {
            result += text[i];
        }
    }
    return result;
}

}  // namespace

LoopbackRedirect::LoopbackRedirect() :
    _socket(invalidSocket) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return;
#endif

    const auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    _socket = static_cast<std::uintptr_t>(listener);
    if (_socket == invalidSocket) return;

    // only this computer : never a port open to the network
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    auto* const generic = reinterpret_cast<sockaddr*>(&address);
    Length length = sizeof(address);
    if (bind(listener, generic, sizeof(address)) != 0 || listen(listener, 4) != 0 || getsockname(listener, generic, &length) != 0) return;

    _port = ntohs(address.sin_port);
}

LoopbackRedirect::~LoopbackRedirect() {
    if (_socket != invalidSocket) closeSocket(native(_socket));
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string LoopbackRedirect::redirectUri() const {
    return std::format("http://127.0.0.1:{}/callback", _port);
}

std::optional<std::string> LoopbackRedirect::wait(std::string_view page, std::chrono::seconds timeout, const std::stop_token& stop) const {
    if (!isListening()) return std::nullopt;

    const auto until = std::chrono::steady_clock::now() + timeout;
    while (!stop.stop_requested() && std::chrono::steady_clock::now() < until) {
        if (pollSocket(native(_socket), pollMs) <= 0) continue;

        const auto client = accept(native(_socket), nullptr, nullptr);
        if (static_cast<std::uintptr_t>(client) == invalidSocket) continue;

#ifdef __APPLE__
        // a browser closing first would otherwise end the whole process with SIGPIPE, as we answer
        const int noSignal = 1;
        setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif

        const auto line = requestLine(client);
        // "GET <target> HTTP/1.1"
        const auto targetStart = line.find(' ');
        const auto targetEnd = line.find(' ', targetStart + 1);
        const auto target = targetStart == std::string::npos || targetEnd == std::string::npos
                                ? std::string_view {}
                                : std::string_view { line }.substr(targetStart + 1, targetEnd - targetStart - 1);
        const auto path = target.substr(0, target.find('?'));

        if (line.starts_with("GET ") && path == "/callback") {
            answer(client, "200 OK", page);
            closeSocket(client);
            return std::string { target.find('?') == std::string_view::npos ? std::string_view {} : target.substr(target.find('?') + 1) };
        }

        answer(client, "404 Not Found", "");
        closeSocket(client);
    }

    return std::nullopt;
}

std::string LoopbackRedirect::parameter(std::string_view query, std::string_view name) {
    while (!query.empty()) {
        const auto end = query.find('&');
        const auto pair = query.substr(0, end);
        const auto equals = pair.find('=');
        if (decoded(pair.substr(0, equals)) == name) {
            return equals == std::string_view::npos ? std::string {} : decoded(pair.substr(equals + 1));
        }
        if (end == std::string_view::npos) break;
        query.remove_prefix(end + 1);
    }
    return {};
}

}  // namespace net
