// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <random>
#include <string>
#include <string_view>

#include "HttpClient.h"

namespace net {

// multipart/form-data request body (RFC 7578)
class Multipart {
 public:
    Multipart() :
        _boundary(_randomBoundary()) {}

    void addField(std::string_view name, std::string_view value) {
        _beginPart(name, {}, {});
        _body += value;
        _body += "\r\n";
    }

    void addFile(std::string_view name, std::string_view fileName, std::string_view contentType, std::string_view data) {
        _beginPart(name, fileName, contentType);
        _body += data;
        _body += "\r\n";
    }

    // fills the request content type and body
    void applyTo(Request& request) const {
        request.headers.push_back({ .name = "Content-Type", .value = "multipart/form-data; boundary=" + _boundary });
        request.body = _body + "--" + _boundary + "--\r\n";
    }

 private:
    std::string _boundary;

    static std::string _randomBoundary() {
        static constexpr std::string_view alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        std::random_device random;
        std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);

        std::string boundary = "----SoundBuddyBoundary";
        for (int i = 0; i < 24; ++i) boundary += alphabet[pick(random)];
        return boundary;
    }
    std::string _body;

    void _beginPart(std::string_view name, std::string_view fileName, std::string_view contentType) {
        _body += "--";
        _body += _boundary;
        _body += "\r\nContent-Disposition: form-data; name=\"";
        _body += name;
        _body += '"';

        if (!fileName.empty()) {
            _body += "; filename=\"";
            _body += fileName;
            _body += '"';
        }

        if (!contentType.empty()) {
            _body += "\r\nContent-Type: ";
            _body += contentType;
        }

        _body += "\r\n\r\n";
    }
};

}  // namespace net
