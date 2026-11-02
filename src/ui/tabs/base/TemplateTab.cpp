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

#include "TemplateTab.h"

#include "src/ui/WorkerBridge.hpp"

TemplateTab::TemplateTab(QWidget* parent) :
    QWidget(parent),
    _button(new QPushButton(this)),
    _logArea(new QScrollArea(this)) {
    //
    QObject::connect(this->_logArea->verticalScrollBar(), &QScrollBar::rangeChanged, this, &TemplateTab::_scrollUpdate);

    this->setLayout(new QBoxLayout(QBoxLayout::TopToBottom, this));
    this->_logArea->setWidgetResizable(true);
}

// the scroll area deletes the previous log
void TemplateTab::_createNewLog() {
    this->_lsv = new LogScrollView(nullptr);
    this->_logArea->setWidget(this->_lsv);
}

int TemplateTab::workerStarted() {
    this->_button->setEnabled(false);
    this->_createNewLog();
    return ++this->_run;
}

void TemplateTab::workerFinished(int run) {
    // a stopped run finishes after its replacement started
    if (run != this->_run) return;
    this->_button->setEnabled(true);
}

workers::MessageSink TemplateTab::messageSink() {
    return [this](const workers::Message& message) {
        ui::post(this, [this, text = QString::fromStdString(message.text), kind = message.kind, line = message.line] {
            this->_printLog(text, kind, line);
        });
    };
}

void TemplateTab::_printLog(const QString& text, workers::MessageKind kind, workers::LineMode line) {
    // no log until a worker started
    if (!this->_lsv) return;

    if (line == workers::LineMode::ReplaceLast) {
        this->_lsv->updateLatestMessage(text, kind);
    } else {
        this->_lsv->addMessage(text, kind);
    }
}

void TemplateTab::_scrollUpdate(int /*min*/, int /*max*/) {
    // to perform heavy CPU consuming action
    auto* tabScrollBar = this->_logArea->verticalScrollBar();
    tabScrollBar->setValue(tabScrollBar->maximum());
}
