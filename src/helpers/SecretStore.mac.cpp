// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef __APPLE__

#include "SecretStore.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "version.h"

namespace secrets {

namespace {

// a Core Foundation object, released once destroyed
template <typename Ref>
class Owned {
 public:
    explicit Owned(Ref ref) :
        _ref(ref) {}
    ~Owned() {
        if (_ref) CFRelease(_ref);
    }
    Owned(Owned&& other) noexcept :
        _ref(std::exchange(other._ref, nullptr)) {}
    Owned(const Owned&) = delete;
    void operator=(const Owned&) = delete;

    Ref get() const { return _ref; }

 private:
    Ref _ref;
};

Owned<CFStringRef> toCFString(const std::string& text) {
    return Owned<CFStringRef> { CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8) };
}

// what designates the item of "name"
Owned<CFMutableDictionaryRef> queryOf(const std::filesystem::path& name) {
    Owned<CFMutableDictionaryRef> query {
        CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)
    };
    const auto service = toCFString(APP_NAME);
    const auto account = toCFString(name.string());
    CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query.get(), kSecAttrService, service.get());
    CFDictionarySetValue(query.get(), kSecAttrAccount, account.get());
    return query;
}

}  // namespace

bool store(const std::filesystem::path& name, std::string_view secret) {
    const Owned<CFDataRef> data {
        CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(secret.data()), static_cast<CFIndex>(secret.size()))
    };
    const auto query = queryOf(name);

    Owned<CFMutableDictionaryRef> update {
        CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)
    };
    CFDictionarySetValue(update.get(), kSecValueData, data.get());

    const auto updated = SecItemUpdate(query.get(), update.get());
    if (updated == errSecSuccess) return true;
    if (updated != errSecItemNotFound) return false;

    CFDictionarySetValue(query.get(), kSecValueData, data.get());
    return SecItemAdd(query.get(), nullptr) == errSecSuccess;
}

std::optional<std::string> load(const std::filesystem::path& name) {
    const auto query = queryOf(name);
    CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    if (SecItemCopyMatching(query.get(), &result) != errSecSuccess) return std::nullopt;
    const Owned<CFDataRef> data { static_cast<CFDataRef>(result) };

    return std::string { reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())),
                         static_cast<std::size_t>(CFDataGetLength(data.get())) };
}

void remove(const std::filesystem::path& name) {
    SecItemDelete(queryOf(name).get());
}

}  // namespace secrets

#endif
