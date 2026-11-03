// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
        HRESULT get(IDispatch* object, const wchar_t* name, VARIANT* result);

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
