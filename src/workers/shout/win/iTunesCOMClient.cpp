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

#include "iTunesCOMClient.h"

#include <oleauto.h>
#include <stdio.h>

#include <stdexcept>
#include <string>
#include <utility>

// identifiers of the iTunes COM SDK, defined once here
#include "iTunesCOMInterface_i.c"  // NOLINT(bugprone-suspicious-include)

using Microsoft::WRL::ComPtr;

namespace {

std::runtime_error comError(const char * what, const HRESULT hr) {
    char message[160];
    snprintf(message, sizeof(message), "%s (HRESULT 0x%08lX)", what, static_cast<unsigned long>(hr));
    return std::runtime_error(message);
}

struct Variant : public VARIANT {
    Variant() { VariantInit(this); }
    ~Variant() { VariantClear(this); }
    Variant(const Variant&) = delete;
    void operator=(const Variant&) = delete;
};

HRESULT propertyByName(IDispatch * object, const wchar_t * name, VARIANT * result) {
    DISPID id;
    auto nameParam = const_cast<LPOLESTR>(name);
    const auto hr = object->GetIDsOfNames(IID_NULL, &nameParam, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;

    DISPPARAMS noParams {};
    return object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &noParams, result, nullptr, nullptr);
}

std::wstring toString(VARIANT * value) {
    if (FAILED(VariantChangeType(value, value, 0, VT_BSTR)) || value->bstrVal == nullptr) return {};
    return { value->bstrVal, SysStringLen(value->bstrVal) };
}

int toInt(VARIANT * value) {
    if (FAILED(VariantChangeType(value, value, 0, VT_I4))) return 0;
    return value->lVal;
}

// ISO 8601 local date time, like "2019-08-12T21:29:55"
std::wstring toISODate(VARIANT * value) {
    if (FAILED(VariantChangeType(value, value, 0, VT_DATE))) return {};

    SYSTEMTIME time;
    if (!VariantTimeToSystemTime(value->date, &time)) return {};

    wchar_t formatted[32];
    swprintf(formatted, 32, L"%04u-%02u-%02uT%02u:%02u:%02u",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    return formatted;
}

}  // namespace

//
// Events
//

class iTunesCOMClient::EventSink final : public IDispatch {
 public:
    EventSink(std::function<void(Event)> onEvent, ITypeInfo * eventsInfo) : _onEvent(std::move(onEvent)) {
        // DISPIDs are read from iTunes type library rather than hardcoded
        _playId = _idOf(eventsInfo, L"OnPlayerPlayEvent");
        _stopId = _idOf(eventsInfo, L"OnPlayerStopEvent");
        _quitId = _idOf(eventsInfo, L"OnAboutToPromptUserToQuitEvent");
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** object) override {
        if (object == nullptr) return E_POINTER;

        if (riid == IID_IUnknown || riid == IID_IDispatch || riid == DIID__IiTunesEvents) {
            *object = static_cast<IDispatch *>(this);
            AddRef();
            return S_OK;
        }

        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = InterlockedDecrement(&_refCount);
        if (count == 0) delete this;
        return count;
    }

    // IDispatch, only Invoke is called by event sources
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT * count) override {
        *count = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo **) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR *, UINT, LCID, DISPID *) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *) override {
        if (id == _playId || id == _stopId) {
            _onEvent(Event::PlayerStateChanged);
        } else if (id == _quitId) {
            _onEvent(Event::AboutToQuit);
        }

        return S_OK;
    }

 private:
    LONG _refCount = 1;
    std::function<void(Event)> _onEvent;
    DISPID _playId;
    DISPID _stopId;
    DISPID _quitId;

    // the type info is a proxy to iTunes process, which cannot remote GetIDsOfNames : lookup through the members descriptions
    static DISPID _idOf(ITypeInfo * eventsInfo, const wchar_t * name) {
        TYPEATTR * attributes;
        auto hr = eventsInfo->GetTypeAttr(&attributes);
        if (FAILED(hr)) throw comError("Cannot read iTunes events description", hr);

        const auto functionsCount = attributes->cFuncs;
        eventsInfo->ReleaseTypeAttr(attributes);

        for (WORD i = 0; i < functionsCount; ++i) {
            FUNCDESC * function;
            if (FAILED(eventsInfo->GetFuncDesc(i, &function))) continue;

            const auto id = function->memid;
            eventsInfo->ReleaseFuncDesc(function);

            BSTR functionName = nullptr;
            if (FAILED(eventsInfo->GetDocumentation(id, &functionName, nullptr, nullptr, nullptr))) continue;

            const bool matches = functionName != nullptr && wcscmp(functionName, name) == 0;
            SysFreeString(functionName);

            if (matches) return id;
        }

        throw std::runtime_error("Unknown iTunes event");
    }
};


//
// Properties
//

HRESULT iTunesCOMClient::PropertyIds::get(IDispatch * object, const wchar_t * name, VARIANT * result) {
    auto found = _ids.find(name);

    if (found == _ids.end()) {
        DISPID id;
        auto nameParam = const_cast<LPOLESTR>(name);
        const auto hr = object->GetIDsOfNames(IID_NULL, &nameParam, 1, LOCALE_USER_DEFAULT, &id);

        // unknown names are remembered too, so they are not looked up again
        found = _ids.emplace(name, SUCCEEDED(hr) ? id : DISPID_UNKNOWN).first;
    }

    if (found->second == DISPID_UNKNOWN) return DISP_E_UNKNOWNNAME;

    DISPPARAMS noParams {};
    return object->Invoke(found->second, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &noParams, result, nullptr, nullptr);
}

//
// Client
//

iTunesCOMClient::iTunesCOMClient(std::function<void(Event)> onEvent) {
    auto hr = CoCreateInstance(CLSID_iTunesApp, nullptr, CLSCTX_ALL, IID_IDispatch,
        reinterpret_cast<void **>(_app.GetAddressOf()));
    if (FAILED(hr)) throw comError("Cannot connect to iTunes", hr);

    // events description, from the type library containing the application interface
    ComPtr<ITypeInfo> appInfo;
    ComPtr<ITypeLib> typeLib;
    ComPtr<ITypeInfo> eventsInfo;
    UINT index;

    hr = _app->GetTypeInfo(0, LOCALE_USER_DEFAULT, &appInfo);
    if (SUCCEEDED(hr)) hr = appInfo->GetContainingTypeLib(&typeLib, &index);
    if (SUCCEEDED(hr)) hr = typeLib->GetTypeInfoOfGuid(DIID__IiTunesEvents, &eventsInfo);
    if (FAILED(hr)) throw comError("Cannot read iTunes events description", hr);

    _sink.Attach(new EventSink(std::move(onEvent), eventsInfo.Get()));

    // subscribe
    ComPtr<IConnectionPointContainer> container;
    hr = _app.As(&container);
    if (SUCCEEDED(hr)) hr = container->FindConnectionPoint(DIID__IiTunesEvents, &_connectionPoint);
    if (SUCCEEDED(hr)) hr = _connectionPoint->Advise(_sink.Get(), &_cookie);
    if (FAILED(hr)) throw comError("Cannot subscribe to iTunes events", hr);
}

iTunesCOMClient::~iTunesCOMClient() {
    if (_connectionPoint && _cookie) {
        _connectionPoint->Unadvise(_cookie);
    }
}

ComPtr<IDispatch> iTunesCOMClient::_currentTrack() const {
    Variant value;
    if (FAILED(_appIds.get(_app.Get(), L"CurrentTrack", &value))) return {};
    if (value.vt != VT_DISPATCH || value.pdispVal == nullptr) return {};
    return ComPtr<IDispatch> { value.pdispVal };
}

// "Location" is only exposed by file tracks : its id is looked up on each track rather than remembered,
// since other kinds of tracks (URL...) might use the same id for another property
std::wstring iTunesCOMClient::_trackLocation(IDispatch * track) const {
    Variant value;
    if (SUCCEEDED(propertyByName(track, L"Location", &value))) return toString(&value);

    // fallback on the IITFileOrCDTrack interface, much costlier since it is a new remote interface
    ComPtr<IDispatch> fileTrack;
    if (FAILED(track->QueryInterface(IID_IITFileOrCDTrack, reinterpret_cast<void **>(fileTrack.GetAddressOf())))) {
        return {};
    }

    if (FAILED(propertyByName(fileTrack.Get(), L"Location", &value))) return {};
    return toString(&value);
}

int iTunesCOMClient::_positionMS() const {
    Variant value;
    if (FAILED(_appIds.get(_app.Get(), L"PlayerPositionMS", &value))) return 0;
    return toInt(&value);
}

iTunesCOMClient::Snapshot iTunesCOMClient::snapshot() const {
    Snapshot result;

    const auto track = _currentTrack();
    if (!track) return result;

    // track
    auto &t = result.track;
    t.exists = true;

    const auto read = [this, &track](const wchar_t * name, auto convert, auto &field) {
        Variant value;
        if (SUCCEEDED(_trackIds.get(track.Get(), name, &value))) field = convert(&value);
    };

    read(L"Name", toString, t.name);
    read(L"Album", toString, t.album);
    read(L"Artist", toString, t.artist);
    read(L"Genre", toString, t.genre);
    read(L"Duration", toInt, t.duration);
    read(L"PlayedDate", toISODate, t.playedDate);
    read(L"SkippedDate", toISODate, t.skippedDate);
    read(L"Year", toInt, t.year);
    t.location = _trackLocation(track.Get());

    // player
    result.positionMS = _positionMS();

    // ITPlayerState : stopped (0), playing (1), fast forward (2), rewind (3)
    Variant state;
    if (SUCCEEDED(_appIds.get(_app.Get(), L"PlayerState", &state))) {
        result.playing = toInt(&state) > 0;
    }

    return result;
}

iTunesCOMClient::PlayingPosition iTunesCOMClient::playingPosition() const {
    PlayingPosition result;

    const auto track = _currentTrack();
    if (!track) return result;

    // position must be read before the location, otherwise both iTunes and the caller might lock
    result.positionMS = _positionMS();
    result.location = _trackLocation(track.Get());

    return result;
}

#endif
