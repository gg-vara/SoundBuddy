# SoundBuddy

[![CodeFactor](https://www.codefactor.io/repository/github/amphaal/SoundBuddy/badge)](https://www.codefactor.io/repository/github/amphaal/SoundBuddy)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/b569eb64104e42589fd8825098562243)](https://www.codacy.com/gh/Amphaal/SoundBuddy/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Amphaal/SoundBuddy&amp;utm_campaign=Badge_Grade)

Small companion app that feeds SoundVitrine with ITunes / Music library metadata !

## How to build and debug

Available for MacOS (BigSur 11.0+) and Windows 10+. Usage of Visual Studio Code (https://code.visualstudio.com) as IDE is recommanded.
Requires a C++20 enabled compiler. We assume you have Git installed on your system.

- Run command in terminal to clone this repository: `git clone --recurse-submodules -j8 https://github.com/gg-vara/SoundBuddy`
- In VS Code, use the command palette to run `Tasks : Run Tests Task`, which will configure the build
- In VS Code, simply run `Launch` in the `Execute and Debug` tab to build the executable 

SoundBuddy have some dependencies that are external and thus need to be installed on your system. They are listed in `prerequisites/`, in 3 parts :
- build : the toolchain (compiler, linker, CMake, Ninja) and release tooling (GitHub CLI)
- runtime : the libraries the app links to and ships with (Qt, until the native user interface)
- devex : development comfort (clangd and clang-tidy, debugger), optional to build

The other libraries are declared in CMake (`FetchContent`, submodules), and Velopack's CLI in the .NET tools manifest (`.config/dotnet-tools.json`). CMake checks the toolchain when configuring, and tells what is missing.

Here is how to proceed on each handled platform :

### MacOS (LLVMClang)

#### Dependencies
All dependencies are fetched via Homebrew (https://brew.sh), make sure you have it properly installed on your build system.
- Run command in terminal to fetch depedencies, listed in `prerequisites/Brewfile` : `brew bundle --file prerequisites/Brewfile`
- `brew bundle check --file prerequisites/Brewfile` tells what is missing

#### Security
Now from OSX 13.2+, executables have to be signed with a developper identity to be even lauched (for Qt binary depedencies via `macdeployqt`, for the Velopack package...).
You need a locally registered XCode / Apple developper platform linked to your XCode app, and then set `APPLE_CODESIGN_ID` and `APPLE_CERT_APP_CODE_SIGN_IDENTITY` env variables:
They would look like:
- APPLE_CODESIGN_ID=U2T7J2AC52
- APPLE_CERT_APP_CODE_SIGN_IDENTITY="Apple Development: Guillaume VARA (U2T7J2AC52)"

### Windows (MSYS2 CLANG64)

#### Dependencies
We use MSYS2 build system (https://www.msys2.org) as our build platform, with its CLANG64 environment (LLVM Clang, libc++). Make sure to install it if it is not.
- Define `MINGW64_ROOT` as environment variable, as the path of the CLANG64 environment installed by MSYS2 (Ex: `C:\msys64\clang64`).
- From VS Code, use the `Msys2 Shell` terminal (which opens CLANG64) to run the following commands.
- Run command in terminal to update obsolete packages : `pacman -Syyu`
- Run command in terminal to fetch depedencies, listed in `prerequisites/msys2/{build,runtime,devex}.txt` : `pacman -S --needed $(sed 's/#.*//' ./prerequisites/msys2/*.txt)` (or only some of the lists, as `./prerequisites/msys2/{build,runtime}.txt`)
- Install the .NET SDK (https://dotnet.microsoft.com/download), which runs Velopack's CLI to package releases

#### Security
On Windows, if installer is not signed, Chrome + Windows Defender will ring alarms all over the place.
- How-to: https://learn.microsoft.com/en-US/windows/msix/package/create-certificate-package-signing
- Download required Windows App Certification App via Windows SDK Setup (`mingw-w64-clang-x86_64-nss` does not seems to work atm): https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

- First, generate `.pfx` file by running `toolchains/win_generate_pfx.sh`. Script will ask for publisher infos: give required infos if relevant.
  - Make sure that the Organisation Name is the same as the `APP_PUBLISHER_ORG` in `CMakeLists.txt` !
- Then, build and run the project as usual (Debug builds are not signed)

### Building from the command line

Builds are described by CMake presets (`CMakePresets.json`), also used by the VS Code tasks. On Windows, run them from the CLANG64 shell :
- `cmake --preset windows-debug`, then `cmake --build --preset windows-debug`
- `cmake --list-presets=all` lists the available presets (`macos-*` on macOS)

### Visual Studio Code

The workspace recommends its extensions (`.vscode/extensions.json`) :
- clangd, for C++ code intelligence : it reads `build_debug/compile_commands.json`, exported when configuring with the Debug preset. Configure once (task `1. Generate Project files (Debug)`) before opening C++ files, and again when adding source files
- CMake Tools, for CMake language support, presets, builds and the tests explorer
- CodeLLDB and C/C++, for debugging only (C/C++'s own IntelliSense is disabled, as it would compete with clangd)

On Windows, clangd and CMake are taken from `MINGW64_ROOT`. On macOS, set `clangd.path` and `cmake.cmakePath` in your user settings, to Homebrew's ones.

### Tests

Unit tests lie in `tests/`, one program per tested part, and are built with the Debug presets.
- `cmake --workflow --preset windows-test` (or `macos-test`) builds in Debug and runs them
- from an existing Debug build : `ctest --preset windows-debug`
- tests against public services (httpbin.org, stream.wikimedia.org) are left out by default : `ctest --test-dir build_debug -L network`

Tests run under their own identity (`tests/support/version.h`) : they never touch the app's data directory, settings or startup registration.

## Translations

Texts are written in English in the code (`tr()`, or `i18n::tr()` outside of Qt classes). Their translations are compiled into the app from `src/i18n/*.inc` tables (`{ "English text", "translation" },`), and the language follows the system's user interface one.

To translate a new text, add it to `src/i18n/fr.inc`. Debug builds log every untranslated text as `[i18n] missing translation : "..."`.

## Publishing a release

SoundBuddy is installed and updated through [Velopack](https://velopack.io) : installed copies check this repository's GitHub releases, then download and apply newer versions by themselves.

Requirements, in addition to the build dependencies :
- .NET SDK (https://dotnet.microsoft.com/download), to run Velopack's CLI
- Velopack's CLI, pinned by `.config/dotnet-tools.json` to the version of the library (`VELOPACK_VERSION` in `cmake/Velopack.cmake`, which checks they match) : run `dotnet tool restore` once from the repository. To upgrade Velopack, change both versions (and the library hash)
- GitHub CLI, logged in (`gh auth login`), or a `GITHUB_TOKEN` environment variable

Then, after bumping the version in `CMakeLists.txt` (`project(... VERSION X.Y.Z`) :
- `cmake --workflow --preset windows-package` builds in Release and packages the app into `build_release_unsigned/releases`, without signing nor publishing anything : useful to check a release locally
- `cmake --workflow --preset windows-publish` builds, signs, packages, and publishes the `vX.Y.Z` GitHub release. Set the `SOUNDBUDDY_RELEASE_DRAFT` environment variable to create it as a draft instead
- on macOS, `macos-package` / `macos-publish` add the macOS packages to the same release

Packaging fetches the previous release (`vpk download github`) to generate delta updates, then runs `vpk pack`; publishing runs `vpk upload github` (see `cmake/Velopack.cmake`).

## License
    SoundBuddy
    Small companion app that feeds SoundVitrine with ITunes / Music library metadata
    Copyright (C) 2018-2021 Guillaume Vara <guillaume.vara@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Any graphical resources available within the source code may
    use a different license and copyright : please refer to their metadata
    for further details. Graphical resources without explicit references to a
    different license and copyright still refer to this GPL.
