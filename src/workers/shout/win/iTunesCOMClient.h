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

#ifdef _WIN32

#pragma once

#include <windows.h>
#include <ocidl.h>
#include <wrl/client.h>

#include <functional>
#include <string>
#include <unordered_map>

// Client of the iTunes COM API, without any framework dependency.
// Properties are accessed by name through IDispatch (late binding), and events are received
// through the "_IiTunesEvents" connection point.
// iTunes runs in its own process : every call crosses processes, so property ids are resolved once.
// Must be created, used and destroyed on a single thread, initialized as a COM STA.
// Events are delivered while that thread dispatches its window messages, including during calls to iTunes.
class iTunesCOMClient {
 public:
    enum class Event {
        PlayerStateChanged,  // play or stop
        AboutToQuit
    };

    struct Track {
        bool exists = false;
        std::wstring name;
        std::wstring album;
        std::wstring artist;
        std::wstring genre;
        std::wstring location;
        std::wstring playedDate;   // ISO 8601 local time, empty if unknown
        std::wstring skippedDate;  // ISO 8601 local time, empty if unknown
        int duration = 0;          // seconds
        int year = 0;
    };

    // everything there is to know about what is played
    struct Snapshot {
        Track track;
        int positionMS = 0;
        bool playing = false;
    };

    // cheap subset of a snapshot, to detect jumps within the current track
    struct PlayingPosition {
        std::wstring location;  // empty if no current track
        int positionMS = 0;
    };

    // throws std::runtime_error if iTunes cannot be reached
    explicit iTunesCOMClient(std::function<void(Event)> onEvent);
    ~iTunesCOMClient();

    iTunesCOMClient(const iTunesCOMClient&) = delete;
    void operator=(const iTunesCOMClient&) = delete;

    Snapshot snapshot() const;
    PlayingPosition playingPosition() const;

 private:
    class EventSink;

    // DISPIDs of an interface, resolved on first use
    class PropertyIds {
     public:
        HRESULT get(IDispatch * object, const wchar_t * name, VARIANT * result);

     private:
        std::unordered_map<std::wstring, DISPID> _ids;
    };

    Microsoft::WRL::ComPtr<IDispatch> _app;
    Microsoft::WRL::ComPtr<IConnectionPoint> _connectionPoint;
    Microsoft::WRL::ComPtr<EventSink> _sink;
    DWORD _cookie = 0;

    mutable PropertyIds _appIds;
    mutable PropertyIds _trackIds;  // properties common to all kinds of tracks

    Microsoft::WRL::ComPtr<IDispatch> _currentTrack() const;
    static std::wstring _trackLocation(IDispatch* track);
    int _positionMS() const;
};

#endif
