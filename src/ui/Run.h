// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// User interface : a web page (src/ui/page) in the system webview, hosted by a native shell (src/ui/win, src/ui/mac)
namespace ui {

// shows the user interface (in the tray) until the user quits : returns the exit code
int run(int argc, char** argv);

}  // namespace ui
