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

#pragma once

#include <QMetaObject>
#include <QObject>

#include <utility>

// Workers call back on their own threads : their callbacks reach the Qt user interface through here
namespace ui {

// runs "function" later on the thread of "receiver" (the user interface thread), unless "receiver" is destroyed meanwhile.
// "receiver" must still exist when calling this : workers are stopped before the windows are destroyed
template <typename Function>
void post(QObject* receiver, Function&& function) {
    QMetaObject::invokeMethod(receiver, std::forward<Function>(function), Qt::QueuedConnection);
}

}  // namespace ui
