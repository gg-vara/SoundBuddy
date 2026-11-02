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

#include <QtWidgets>

enum class ConnectivityIndicator { NOK = 0, ONGOING = 1, OK = 2 };

Q_DECLARE_METATYPE(ConnectivityIndicator)

class LightWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool on READ isOn WRITE setOn)

 public:
    explicit LightWidget(const QColor& color, QWidget* parent = nullptr);
    bool isOn() const;
    void setOn(bool on);

    void turnOff();
    void turnOn();

 protected:
    void paintEvent(QPaintEvent* /*event*/) override;

 private:
    QColor _color;
    bool _on = true;
};
