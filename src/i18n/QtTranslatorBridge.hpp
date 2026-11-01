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

#include <QTranslator>
#include <QString>

#include <string_view>

#include "Translations.h"

// Serves Qt's tr() calls from the i18n tables, until the UI no longer relies on Qt
class QtTranslatorBridge : public QTranslator {
 public:
    using QTranslator::QTranslator;

    QString translate(const char *, const char * sourceText, const char *, int) const override {
        if (sourceText == nullptr || i18n::language() == i18n::Language::English) return {};

        const std::string_view source { sourceText };
        const auto translation = i18n::tr(source);

        // a null string lets Qt fall back to the source text
        if (translation.data() == source.data()) return {};
        return QString::fromUtf8(translation.data(), static_cast<qsizetype>(translation.size()));
    }

    bool isEmpty() const override {
        return false;
    }
};
