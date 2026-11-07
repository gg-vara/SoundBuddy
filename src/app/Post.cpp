// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Post.h"

#include <atomic>
#include <functional>
#include <utility>

namespace ui {

namespace {

std::atomic<Poster>& currentPoster() {
    static std::atomic<Poster> instance { nullptr };
    return instance;
}

}  // namespace

void post(std::function<void()> task) {
    if (const auto poster = currentPoster().load()) poster(std::move(task));
}

void setPoster(Poster poster) {
    currentPoster().store(poster);
}

}  // namespace ui
