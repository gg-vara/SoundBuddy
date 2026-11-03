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

#include <wrl/client.h>
#include <cwchar>
#include <functional>
#ifdef _WIN32

#include "iTunesCOMClient.h"

#include <oleauto.h>

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

// identifiers of the iTunes COM interfaces, as iTunes registers them : the rest is late-bound, through IDispatch
// iTunesApp class
constexpr CLSID appClass {
    .Data1 = 0xDC0C2640, .Data2 = 0x1415, .Data3 = 0x4644, .Data4 = { 0x87, 0x5C, 0x6F, 0x4D, 0x76, 0x98, 0x39, 0xBA }
};
// _IiTunesEvents dispatch interface
constexpr IID eventsInterface {
    .Data1 = 0x5846EB78, .Data2 = 0x317E, .Data3 = 0x4B6F, .Data4 = { 0xB0, 0xC3, 0x11, 0xEE, 0x8C, 0x8F, 0xEE, 0xF2 }
};
// IITFileOrCDTrack interface
constexpr IID fileTrackInterface {
    .Data1 = 0x00D7FE99, .Data2 = 0x7868, .Data3 = 0x4CC7, .Data4 = { 0xAD, 0x9E, 0xAC, 0xFD, 0x70, 0xD0, 0x95, 0x66 }
};

std::runtime_error comError(const char* what, const HRESULT hr) {
    return std::runtime_error(std::format("{} (HRESULT 0x{:08X})", what, static_cast<unsigned long>(hr)));
}

struct Variant : public VARIANT {
    // VariantInit() initializes the base
    Variant() { VariantInit(this); }  // NOLINT(cppcoreguidelines-pro-type-member-init)
    ~Variant() { VariantClear(this); }
    Variant(const Variant&) = delete;
    void operator=(const Variant&) = delete;
};

HRESULT propertyByName(IDispatch* object, const wchar_t* name, VARIANT* result) {
    DISPID id = DISPID_UNKNOWN;
    auto* nameParam = const_cast<LPOLESTR>(name);
    const auto hr = object->GetIDsOfNames(IID_NULL, &nameParam, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;

    DISPPARAMS noParams {};
    return object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &noParams, result, nullptr, nullptr);
}

std::wstring toString(VARIANT* value) {
    if (FAILED(VariantChangeType(value, value, 0, VT_BSTR)) || value->bstrVal == nullptr) return {};
    return { value->bstrVal, SysStringLen(value->bstrVal) };
}

int toInt(VARIANT* value) {
    if (FAILED(VariantChangeType(value, value, 0, VT_I4))) return 0;
    return value->lVal;
}

// ISO 8601 local date time, like "2019-08-12T21:29:55"
std::wstring toISODate(VARIANT* value) {
    if (FAILED(VariantChangeType(value, value, 0, VT_DATE))) return {};

    SYSTEMTIME time;
    if (VariantTimeToSystemTime(value->date, &time) == 0) return {};

    return std::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
}

}  // namespace

//
// Events
//

class iTunesCOMClient::EventSink final : public IDispatch {
 public:
    // deleted by Release()
    virtual ~EventSink() = default;

    // DISPIDs are read from iTunes type library rather than hardcoded
    EventSink(std::function<void(Event)> onEvent, ITypeInfo* eventsInfo) :
        _onEvent(std::move(onEvent)),
        _playId(_idOf(eventsInfo, L"OnPlayerPlayEvent")),
        _stopId(_idOf(eventsInfo, L"OnPlayerStopEvent")),
        _quitId(_idOf(eventsInfo, L"OnAboutToPromptUserToQuitEvent")) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (object == nullptr) return E_POINTER;

        if (riid == IID_IUnknown || riid == IID_IDispatch || riid == eventsInterface) {
            *object = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }

        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }

    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = InterlockedDecrement(&_refCount);
        if (count == 0) delete this;
        return count;
    }

    // IDispatch, only Invoke is called by event sources
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {
        *count = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT /*iTInfo*/, LCID /*lcid*/, ITypeInfo** /*ppTInfo*/) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE
    GetIDsOfNames(REFIID, LPOLESTR* /*rgszNames*/, UINT /*cNames*/, LCID /*lcid*/, DISPID* /*rgDispId*/) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE Invoke(
        DISPID id, REFIID, LCID /*lcid*/, WORD /*wFlags*/, DISPPARAMS* /*pDispParams*/, VARIANT* /*pVarResult*/, EXCEPINFO* /*pExcepInfo*/,
        UINT* /*puArgErr*/
    ) override {
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
    static DISPID _idOf(ITypeInfo* eventsInfo, const wchar_t* name) {
        TYPEATTR* attributes = nullptr;
        const auto hr = eventsInfo->GetTypeAttr(&attributes);
        if (FAILED(hr)) throw comError("Cannot read iTunes events description", hr);

        const auto functionsCount = attributes->cFuncs;
        eventsInfo->ReleaseTypeAttr(attributes);

        for (WORD i = 0; i < functionsCount; ++i) {
            FUNCDESC* function = nullptr;
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

HRESULT iTunesCOMClient::PropertyIds::get(IDispatch* object, const wchar_t* name, VARIANT* result) {
    auto found = _ids.find(name);

    if (found == _ids.end()) {
        DISPID id = DISPID_UNKNOWN;
        auto* nameParam = const_cast<LPOLESTR>(name);
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
    auto hr = CoCreateInstance(appClass, nullptr, CLSCTX_ALL, IID_IDispatch, reinterpret_cast<void**>(_app.GetAddressOf()));
    if (FAILED(hr)) throw comError("Cannot connect to iTunes", hr);

    // events description, from the type library containing the application interface
    ComPtr<ITypeInfo> appInfo;
    ComPtr<ITypeLib> typeLib;
    ComPtr<ITypeInfo> eventsInfo;
    UINT index = 0;

    hr = _app->GetTypeInfo(0, LOCALE_USER_DEFAULT, &appInfo);
    if (SUCCEEDED(hr)) hr = appInfo->GetContainingTypeLib(&typeLib, &index);
    if (SUCCEEDED(hr)) hr = typeLib->GetTypeInfoOfGuid(eventsInterface, &eventsInfo);
    if (FAILED(hr)) throw comError("Cannot read iTunes events description", hr);

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) : COM reference counting, the ComPtr owns it
    _sink.Attach(new EventSink(std::move(onEvent), eventsInfo.Get()));

    // subscribe
    ComPtr<IConnectionPointContainer> container;
    hr = _app.As(&container);
    if (SUCCEEDED(hr)) hr = container->FindConnectionPoint(eventsInterface, &_connectionPoint);
    if (SUCCEEDED(hr)) hr = _connectionPoint->Advise(_sink.Get(), &_cookie);
    if (FAILED(hr)) throw comError("Cannot subscribe to iTunes events", hr);
}

iTunesCOMClient::~iTunesCOMClient() {
    if (_connectionPoint && (_cookie != 0U)) {
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
std::wstring iTunesCOMClient::_trackLocation(IDispatch* track) {
    Variant value;
    if (SUCCEEDED(propertyByName(track, L"Location", &value))) return toString(&value);

    // fallback on the IITFileOrCDTrack interface, much costlier since it is a new remote interface
    ComPtr<IDispatch> fileTrack;
    if (FAILED(track->QueryInterface(fileTrackInterface, reinterpret_cast<void**>(fileTrack.GetAddressOf())))) {
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
    auto& t = result.track;
    t.exists = true;

    const auto read = [this, &track](const wchar_t* name, auto convert, auto& field) {
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
