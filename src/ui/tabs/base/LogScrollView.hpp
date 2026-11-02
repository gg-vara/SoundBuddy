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

#include <QLabel>
#include <QVBoxLayout>
#include <QList>

#include "src/workers/base/Message.h"

class LogScrollView : public QWidget {
 private:
    static const int _maxLogMessages = 100;

    void limitLogSize() {
        if(this->layout()->count() > this->_maxLogMessages) {
            auto qli = this->layout()->takeAt(0);
            auto ltr = reinterpret_cast<QLabel*>(qli->widget());
            ltr->setParent(nullptr);
            delete ltr;
            delete qli;
        }
    }

 public:
    explicit LogScrollView(QWidget* parent) : QWidget(parent) {
        this->setLayout(new QVBoxLayout);
        this->layout()->setAlignment(Qt::AlignTop);

        // set background color
        QPalette palette = this->palette();
        palette.setColor(this->backgroundRole(), Qt::white);
        this->setPalette(palette);
    }

    void addMessage(const QString &newMessage, workers::MessageKind msgType) {
        //
        auto label = new QLabel(newMessage);
        label->setWordWrap(true);

        //
        Qt::GlobalColor color;
        switch(msgType) {
            case workers::MessageKind::Issue:
                color = Qt::red;
            break;

            case workers::MessageKind::Warning:
                color = Qt::darkYellow;
            break;

            default:
                color = Qt::black;
            break;
        }

        //
        QPalette palette = label->palette();
        palette.setColor(label->foregroundRole(), color);
        label->setPalette(palette);

        //
        this->limitLogSize();

        //
        this->layout()->addWidget(label);
    }

    void updateLatestMessage(const QString &newMessage, workers::MessageKind msgType) {
        //
        auto latestLabelIndex = this->layout()->count() - 1;

        // if no message, add message
        if(latestLabelIndex < 0) {
            return this->addMessage(newMessage, msgType);
        }

        // get label
        auto lbl = reinterpret_cast<QLabel*>(
            this->layout()->itemAt(latestLabelIndex)->widget()
        );

        // update its content
        lbl->setText(newMessage);
    }
};
