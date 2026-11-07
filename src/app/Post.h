// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

// Workers call back on their own threads : their callbacks reach the application thread through here
namespace ui {

// runs "task" later on the application thread (AppThread), unless it is stopping meanwhile
void post(std::function<void()> task);

}  // namespace ui
