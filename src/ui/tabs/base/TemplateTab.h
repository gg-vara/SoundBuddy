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

#include <QWidget>
#include <QBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QWindow>
#include <QStyle>

#include "src/workers/base/Message.h"
#include "src/workers/base/MessengerThread.hpp"

#include "LogScrollView.hpp"

class TemplateTab : public QWidget {
  Q_OBJECT 

 public:
    explicit TemplateTab(QWidget* parent = nullptr);
    QPushButton* tButton = nullptr;

    void bindWithWorker(MessengerThread *bThread);
    void bindWithMessenger(IMessenger *bMessenger);

    // prints the messages of a worker in this tab's log, from any thread
    workers::MessageSink messageSink();

 protected:
    MessengerThread* bThread = nullptr;
    LogScrollView* lsv = nullptr;
    QScrollArea* scrollArea = nullptr;

    void scrollUpdate(int min, int max);

    void createNewLog();

 protected slots:
    void printLog(const QString &message, const MessageType &msgType, const bool replacePreviousLine);
    void onThreadEnd();
    void onThreadStart();
};
