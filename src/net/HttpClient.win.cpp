// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>
#include <memory>
#include <stop_token>
#include <string_view>
#include <vector>
#ifdef _WIN32

#include "HttpClient.h"

#include <windows.h>
#include <winhttp.h>

#include <mutex>
#include <string>

namespace net {

namespace {

std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    const auto size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    const auto size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// description of the last WinHTTP error, like "Cannot connect : The server name or address could not be resolved (12007)"
std::string lastError(const char* step) {
    const auto code = GetLastError();

    wchar_t* message = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        GetModuleHandleW(L"winhttp.dll"), code, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr
    );

    std::string description = message ? narrow(message) : std::string {};
    if (message) LocalFree(message);

    while (!description.empty() && (description.back() == '\n' || description.back() == '\r' || description.back() == ' ')) {
        description.pop_back();
    }

    return std::string { step } + " : " + description + " (" + std::to_string(code) + ")";
}

// "Name: value" lines, after the status line
std::vector<Header> parseHeaders(std::wstring_view raw) {
    std::vector<Header> headers;

    std::size_t lineStart = raw.find(L"\r\n");
    while (lineStart != std::wstring_view::npos) {
        lineStart += 2;
        const auto lineEnd = raw.find(L"\r\n", lineStart);
        const auto line = raw.substr(lineStart, lineEnd == std::wstring_view::npos ? std::wstring_view::npos : lineEnd - lineStart);

        const auto colon = line.find(L':');
        if (colon != std::wstring_view::npos) {
            auto value = line.substr(colon + 1);
            while (!value.empty() && value.front() == L' ') value.remove_prefix(1);
            headers.push_back({ .name = narrow(line.substr(0, colon)), .value = narrow(value) });
        }

        lineStart = lineEnd;
    }

    return headers;
}

}  // namespace

class HttpClient::Impl {
 public:
    Impl(const Impl&) = delete;
    void operator=(const Impl&) = delete;

    explicit Impl(const std::string& userAgent) {
        // automatic proxy (system settings, WPAD) requires Windows 8.1
        _session =
            WinHttpOpen(widen(userAgent).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

        if (_session) {
            DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(_session, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
        }
    }

    ~Impl() {
        if (_session) WinHttpCloseHandle(_session);
    }

    Response perform(const Request& request, const OnBodyChunk* onChunk, const std::stop_token& stop) {
        Response response;

        if (stop.stop_requested()) {
            response.error = "Request cancelled";
            return response;
        }

        if (!_session) {
            response.error = lastError("Cannot initialize WinHTTP");
            return response;
        }

        // URL
        const auto url = widen(request.url);
        URL_COMPONENTS parts {};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = static_cast<DWORD>(-1);
        parts.dwUrlPathLength = static_cast<DWORD>(-1);
        parts.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (WinHttpCrackUrl(url.c_str(), 0, 0, &parts) == 0) {
            response.error = lastError("Invalid URL");
            return response;
        }

        const std::wstring host { parts.lpszHostName, parts.dwHostNameLength };
        std::wstring pathAndQuery { parts.lpszUrlPath, parts.dwUrlPathLength };
        pathAndQuery.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        if (pathAndQuery.empty()) pathAndQuery = L"/";

        // connection and request handles
        auto* const connection = WinHttpConnect(_session, host.c_str(), parts.nPort, 0);
        if (!connection) {
            response.error = lastError("Cannot connect");
            return response;
        }

        auto* const handle = WinHttpOpenRequest(
            connection, widen(request.method).c_str(), pathAndQuery.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0
        );

        if (!handle) {
            response.error = lastError("Cannot create request");
            WinHttpCloseHandle(connection);
            return response;
        }

        {
            const std::scoped_lock lock { _mutex };
            _request = handle;
            _cancelled = false;
        }

        {
            // once the request is registered : a stop already requested cancels it right away
            const std::stop_callback cancelOnStop(stop, [this] { cancel(); });
            _exchange(handle, request, onChunk, response);
        }

        {
            // the handle may already have been closed by cancel()
            const std::scoped_lock lock { _mutex };
            if (_request) WinHttpCloseHandle(_request);
            _request = nullptr;
        }

        WinHttpCloseHandle(connection);

        // false positive : the stop callback deregisters from the token when destroyed
        return response;  // NOLINT(clang-analyzer-core.StackAddressEscape)
    }

    void cancel() {
        const std::scoped_lock lock { _mutex };
        _cancelled = true;

        // closing the handle aborts the blocking call in progress on it
        if (_request) {
            WinHttpCloseHandle(_request);
            _request = nullptr;
        }
    }

 private:
    HINTERNET _session = nullptr;

    std::mutex _mutex;
    HINTERNET _request = nullptr;
    bool _cancelled = false;

    bool _isCancelled() {
        const std::scoped_lock lock { _mutex };
        return _cancelled;
    }

    // sets response.error from the last error, unless the request was cancelled
    void _fail(Response& response, const char* step) {
        response.error = _isCancelled() ? std::string { "Request cancelled" } : lastError(step);
    }

    void _exchange(HINTERNET handle, const Request& request, const OnBodyChunk* onChunk, Response& response) {
        // zero means no limit, for WinHTTP too
        const auto timeout = static_cast<int>(request.timeout.count());
        WinHttpSetTimeouts(handle, timeout, timeout, timeout, timeout);

        // send
        std::wstring headers;
        for (const auto& header : request.headers) {
            headers += widen(header.name) + L": " + widen(header.value) + L"\r\n";
        }

        const auto bodySize = static_cast<DWORD>(request.body.size());
        const BOOL sent = WinHttpSendRequest(
            handle, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), static_cast<DWORD>(-1),
            bodySize != 0U ? const_cast<char*>(request.body.data()) : WINHTTP_NO_REQUEST_DATA, bodySize, bodySize, 0
        );

        if (sent == 0) {
            _fail(response, "Cannot send request");
            return;
        }
        if (WinHttpReceiveResponse(handle, nullptr) == 0) {
            _fail(response, "No response");
            return;
        }

        // status and headers
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(
            handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
            WINHTTP_NO_HEADER_INDEX
        );
        response.status = static_cast<int>(status);

        size = 0;
        WinHttpQueryHeaders(
            handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX
        );
        if (size != 0U) {
            std::wstring raw(size / sizeof(wchar_t), L'\0');
            if (WinHttpQueryHeaders(
                    handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &size, WINHTTP_NO_HEADER_INDEX
                ) != 0) {
                response.headers = parseHeaders(raw);
            }
        }

        // body
        std::string chunk;
        while (true) {
            DWORD available = 0;
            if (WinHttpQueryDataAvailable(handle, &available) == 0) {
                _fail(response, "Cannot receive data");
                return;
            }
            if (available == 0) return;

            chunk.resize(available);
            DWORD read = 0;
            if (WinHttpReadData(handle, chunk.data(), available, &read) == 0) {
                _fail(response, "Cannot receive data");
                return;
            }
            if (read == 0) return;

            const std::string_view received { chunk.data(), read };
            if (onChunk) {
                if (!(*onChunk)(received)) return;
            } else {
                response.body += received;
            }
        }
    }
};

HttpClient::HttpClient(const std::string& userAgent) :
    _impl(std::make_unique<Impl>(userAgent)) {}
HttpClient::~HttpClient() = default;

Response HttpClient::send(const Request& request, const std::stop_token& stop) {
    return _impl->perform(request, nullptr, stop);
}

Response HttpClient::stream(const Request& request, const OnBodyChunk& onChunk, const std::stop_token& stop) {
    return _impl->perform(request, &onChunk, stop);
}

void HttpClient::cancel() {
    _impl->cancel();
}

}  // namespace net

#endif
