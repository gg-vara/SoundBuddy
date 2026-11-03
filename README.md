# SoundBuddy

[![CodeFactor](https://www.codefactor.io/repository/github/gg-vara/soundbuddy/badge)](https://www.codefactor.io/repository/github/gg-vara/soundbuddy)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/b569eb64104e42589fd8825098562243)](https://www.codacy.com/gh/Amphaal/SoundBuddy/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Amphaal/SoundBuddy&amp;utm_campaign=Badge_Grade)

Small companion app that feeds SoundVitrine with ITunes / Music library metadata !

## How to build and debug

Available for macOS 13.3 (Ventura) or newer, and Windows 10 or newer. Usage of Visual Studio Code (https://code.visualstudio.com) as IDE is recommanded.
Requires a C++20 enabled compiler. We assume you have Git installed on your system.

- Run command in terminal to clone this repository: `git clone --recurse-submodules -j8 https://github.com/gg-vara/SoundBuddy`
- In VS Code, use the command palette to run `Tasks : Run Tests Task`, which will configure the build
- In VS Code, simply run `Launch` in the `Execute and Debug` tab to build the executable 

SoundBuddy have some dependencies that are external and thus need to be installed on your system. They are listed in `prerequisites/`, in 3 parts :
- build : the toolchain (compiler, linker, CMake, Ninja) and release tooling (GitHub CLI)
- runtime : the libraries the app links to and ships with (none : the system provides the webview, libc++ comes with the compiler)
- devex : development comfort (clangd and clang-tidy, debugger), optional to build

The other libraries are declared in CMake (`FetchContent`, submodules), and Velopack's CLI in the .NET tools manifest (`.config/dotnet-tools.json`). CMake checks the toolchain when configuring, and tells what is missing.

Here is how to proceed on each handled platform :

### MacOS (LLVMClang)

#### Dependencies
All dependencies are fetched via Homebrew (https://brew.sh), make sure you have it properly installed on your build system.
- Run command in terminal to fetch depedencies, listed in `prerequisites/Brewfile` : `brew bundle --file prerequisites/Brewfile`
- `brew bundle check --file prerequisites/Brewfile` tells what is missing

#### Security
Local builds are signed ad hoc, which lets them run on the Mac that built them. Distributed packages have to be signed with a Developer ID and notarized, or Gatekeeper refuses them : Velopack does both when packaging (see [Velopack's signing](https://docs.velopack.io/packaging/signing)), as these environment variables tell :
- `APPLE_CERT_APP_CODE_SIGN_IDENTITY` : the Developer ID Application certificate, as `"Developer ID Application: Guillaume VARA"`
- `APPLE_CERT_INSTALL_SIGN_IDENTITY` : the Developer ID Installer certificate, as `"Developer ID Installer: Guillaume VARA"`
- `APPLE_NOTARY_PROFILE` : the notarization credentials, stored once with `xcrun notarytool store-credentials <profile>`

Without them, packages are made unsigned.

### Windows (MSYS2 CLANG64)

#### Dependencies
We use MSYS2 build system (https://www.msys2.org) as our build platform, with its CLANG64 environment (LLVM Clang, libc++). Make sure to install it if it is not.
- Define `MSYS2_ROOT` as environment variable, as the path of the MSYS2 installation (Ex: `C:\msys64`, its default location, assumed by CMake when the variable is not set).
- From VS Code, use the `Msys2 Shell` terminal (which opens CLANG64) to run the following commands.
- Run command in terminal to update obsolete packages : `pacman -Syyu`
- Run command in terminal to fetch depedencies, listed in `prerequisites/msys2/{build,runtime,devex}.txt` : `pacman -S --needed $(sed 's/#.*//' ./prerequisites/msys2/*.txt)` (or only some of the lists, as `./prerequisites/msys2/{build,runtime}.txt`)
- Install the .NET SDK (https://dotnet.microsoft.com/download), which runs Velopack's CLI to package releases
- Install Bun (https://bun.sh), which builds the user interface page : `winget install Oven-sh.Bun`

#### Security
On Windows, if installer is not signed, Chrome + Windows Defender will ring alarms all over the place.

- First, generate `.pfx` file by running `toolchains/win_generate_pfx.sh` from its folder (with OpenSSL, as MSYS2 provides it). Script will ask for publisher infos: give required infos if relevant.
  - Make sure that the Organisation Name is the same as the `APP_PUBLISHER_ORG` in `CMakeLists.txt` !
- Then, package the app as usual (see Publishing a release) : Velopack signs the executable, the updater and the installer with it. Builds themselves, and packages of the `windows-release-unsigned` preset, are not signed

### Building from the command line

Builds are described by CMake presets (`CMakePresets.json`), also used by the VS Code tasks. On Windows, run them from the CLANG64 shell :
- `cmake --preset windows-debug`, then `cmake --build --preset windows-debug`
- `cmake --list-presets=all` lists the available presets (`macos-*` on macOS)
- `cmake --workflow --preset windows-ci` builds and tests as the CI does : clang-tidy runs while compiling (`SOUNDBUDDY_LINT`), any warning failing the build

### Continuous integration

GitHub Actions (`.github/workflows/ci.yml`) builds and tests every push to `main` and every pull request, with the prerequisites and presets above : on Windows in MSYS2 CLANG64 (`msys2/setup-msys2`, then `prerequisites/msys2/*.txt`) through `windows-ci`, on macOS with Homebrew (`prerequisites/Brewfile`) through `macos-test`.

### Visual Studio Code

The workspace recommends its extensions (`.vscode/extensions.json`) :
- clangd, for C++ code intelligence : it reads `build_debug/compile_commands.json`, exported when configuring with the Debug preset. Configure once (task `1. Generate Project files (Debug)`) before opening C++ files, and again when adding source files
- CMake Tools, for CMake language support, presets, builds and the tests explorer
- CodeLLDB and C/C++, for debugging only (C/C++'s own IntelliSense is disabled, as it would compete with clangd)

On Windows, Debug builds keep their debug info beside the executable, in `build_debug/out/SoundBuddy.exe.debug` : both debuggers (lldb, gdb) load it from there.

On Windows, clangd and CMake are taken from the CLANG64 environment of `MSYS2_ROOT`. On macOS, set `clangd.path` and `cmake.cmakePath` in your user settings, to Homebrew's ones.

### Static analysis and layout

- `.clang-tidy` lists the static analysis checks, which clangd shows in the editor as you type (with quick fixes). From the command line : `clang-tidy -p build_debug <file>`
- `.clang-format` describes the code layout, applied by clangd when formatting a file, or with `clang-format -i <file>`

Both come with `clang-tools-extra` (see `prerequisites/msys2/devex.txt`), or Homebrew's `llvm`.

### Tests

Unit tests lie in `tests/`, one program per tested part, and are built with the Debug presets.
- `cmake --workflow --preset windows-test` (or `macos-test`) builds in Debug and runs them
- from an existing Debug build : `ctest --preset windows-debug`
- tests against public services (httpbin.org, stream.wikimedia.org) are left out by default : `ctest --test-dir build_debug -L network`

Tests run under their own identity (`tests/support/version.h`) : they never touch the app's data directory, settings or startup registration.

## User interface

The user interface is a web page shown in the system webview, hosted by a native shell : a window and a tray icon on Windows (WebView2, `src/ui/win`), a window and a menu bar item on macOS (WKWebView, `src/ui/mac`). The application state and its workers live in C++ (`src/ui/Controller.cpp`); the page shows it, and asks for changes :

- `src/ui/page` : the page, in TypeScript with [Solid](https://www.solidjs.com), its states in an [XState](https://stately.ai/docs/xstate) machine (`src/machine.ts`), built by [Vite](https://vite.dev) into a single HTML file embedded in the executable
- `bun run dev` (from `src/ui/page`) : the page in a browser, with live reload, against a stand-in of the application (`src/dev/fakeNative.ts`)
- `bun test` : the page's tests (state machine, state changes), also run by `ctest` as the `page` test
- `bun run lint`, `bun run format` : lint and formatting by [Biome](https://biomejs.dev) (`biome.json`), also checked by `ctest` as the `page-lint` test; VS Code formats on save with its extension

Its dependencies are locked in `src/ui/page/bun.lock`; CMake installs them and builds the page with the app.

## Icons

Icons are drawn in SVG (`resources/icons/svg`) : `bun run icons` (from `resources/icons`) renders each size from the drawings into the Windows (`.ico`) and macOS (`.icns`) icons, committed with them. Windows picks the image fitting its scaling, and the tray icon follows the taskbar theme.

## Translations

Texts are written in English in the code (`i18n::tr()`, or `t()` in the page). Their translations are compiled into the app from `src/i18n/*.inc` tables (`{ "English text", "translation" },`), and the language follows the system's user interface one.

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
