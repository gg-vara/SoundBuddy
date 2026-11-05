// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace ui {

// Runs the application without a window, to test it or drive it from a program, one line per message :
//
//   on the standard input, as the page calls the application (src/ui/PageMessages.h, src/ui/page/src/native.ts) :
//     query ["ready",[]]
//     command ["runFeeder",null]
//   on the standard output, JSON objects with a "type" :
//     {"type":"started","version":"..."}          once, when ready
//     the events the page would get (src/ui/page/src/state.ts), as {"type":"status",...}
//     {"type":"answer","line":1,"value":...}      of a query, "line" counting the input lines from 1
//     {"type":"done","line":2}                    of a command
//     {"type":"error","line":3,"message":"..."}   of a line which is neither
//     {"type":"dialog",...}                       what a window would ask or tell : questions are answered yes
//     {"type":"tray","shouting":false,"dot":"ok"} what a tray icon would show ("dot" : none, ok or issue)
//
// Ends when told to quit, or at the end of the input : returns the exit code
int runHeadless();

}  // namespace ui
