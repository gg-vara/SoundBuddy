// src/settings, and AppSettings : settings file, import of the QSettings values of previous versions

#include <fstream>
#include <iterator>

#include <QCoreApplication>
#include <QSettings>

#include "Check.h"
#include "TestData.h"
#include "src/helpers/AppSettings.h"

namespace {

std::string readAll(const std::filesystem::path& file) {
    std::ifstream stream { file, std::ios::binary };
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

}  // namespace

int main(int argc, char** argv) {
    const auto file = testdata::scratchDirectory("settings é") / "sub" / "settings.json";

    CHECK(!settings::load(file), "missing file : nothing");

    settings::Settings values { .platformHostUrl = "https://host.example", .platformUsername = "j\xC3\xA9r\xC3\xB4me \"q\"", .platformPassword = "p\\w\n", .autoRunShout = true, .runAtStartup = false };
    CHECK(settings::save(file, values), "save creates directories");
    CHECK(!std::filesystem::exists(file.parent_path() / "settings.json.tmp"), "no temporary file left");

    auto loaded = settings::load(file);
    CHECK(loaded && loaded->platformHostUrl == values.platformHostUrl && loaded->platformUsername == values.platformUsername
        && loaded->platformPassword == values.platformPassword && loaded->autoRunShout && !loaded->runAtStartup, "round trip : UTF-8, escapes");

    values.runAtStartup = true;
    values.platformHostUrl = "x";
    const auto overwritten = settings::save(file, values) ? settings::load(file) : std::nullopt;
    CHECK(overwritten && overwritten->runAtStartup && overwritten->platformHostUrl == "x", "overwrite");

    { std::ofstream { file, std::ios::binary } << R"({"autoRunShout": true, "platformUsername": 42, "unknown": []})"; }
    loaded = settings::load(file);
    CHECK(loaded && loaded->autoRunShout && loaded->platformUsername.empty() && !loaded->runAtStartup, "partial file : defaults for missing values and wrong types");

    { std::ofstream { file, std::ios::binary } << "{ not json"; }
    CHECK(!settings::load(file), "invalid JSON : nothing");
    { std::ofstream { file, std::ios::binary } << "[1,2]"; }
    CHECK(!settings::load(file), "not an object : nothing");

    // AppSettings, under the test identity
    QCoreApplication app(argc, argv);
    app.setOrganizationName(APP_PUBLISHER_ORG);
    app.setApplicationName(APP_NAME);

    testdata::removeDataDirectory();
    const auto dataFile = platform::dataDirectory() / "settings.json";
    {
        QSettings legacy;
        legacy.clear();
        legacy.setValue("PlatformHostUrl", "https://legacy.example");
        legacy.setValue("PlatformUsername", QString::fromUtf8("l\xC3\xA9gacy"));
        legacy.setValue("PlatformPassword", "secret");
        legacy.setValue("AutoRunShout", true);
        legacy.setValue("MustRunAtStartup", false);
        legacy.sync();
    }
    {
        const AppSettings appSettings;
        const auto& imported = appSettings.values();
        CHECK(imported.platformHostUrl == "https://legacy.example" && imported.platformUsername == "l\xC3\xA9gacy"
            && imported.platformPassword == "secret" && imported.autoRunShout && !imported.runAtStartup, "previous versions settings imported");
        CHECK(std::filesystem::exists(dataFile), "settings file created by the import");
        CHECK(appSettings.getConnectivityInfos().areOK && appSettings.getConnectivityInfos().getPlaformHomeUrl() == "https://legacy.example/u/l\xC3\xA9gacy", "connectivity infos");
    }
    {
        QSettings legacy;
        legacy.setValue("PlatformHostUrl", "https://changed.example");
        legacy.sync();

        AppSettings appSettings;
        CHECK(appSettings.values().platformHostUrl == "https://legacy.example", "the settings file wins once created");

        appSettings.values().autoRunShout = false;
        CHECK(appSettings.save() && !AppSettings().values().autoRunShout, "saved, then reloaded");
    }

    // previous versions settings, under the test identity
    {
        QSettings legacy;
        legacy.clear();
        legacy.sync();
    }
#ifdef _WIN32
    QSettings(R"(HKEY_CURRENT_USER\Software)", QSettings::NativeFormat).remove(APP_PUBLISHER_ORG);
#endif

    testdata::removeDataDirectory();
    testdata::removeScratchDirectories();
    return check::summary();
}
