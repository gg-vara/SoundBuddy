# SoundBuddy

[![CodeFactor](https://www.codefactor.io/repository/github/amphaal/SoundBuddy/badge)](https://www.codefactor.io/repository/github/amphaal/SoundBuddy)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/b569eb64104e42589fd8825098562243)](https://www.codacy.com/gh/Amphaal/SoundBuddy/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Amphaal/SoundBuddy&amp;utm_campaign=Badge_Grade)

Small companion app that feeds SoundVitrine with ITunes / Music library metadata !

## How to build and debug

Available for MacOS (BigSur 11.0+) and Windows 10+. Usage of Visual Studio Code (https://code.visualstudio.com) as IDE is recommanded.
Requires a C++20 enabled compiler. We assume you have Git installed on your system.

- Run command in terminal to clone this repository: `git clone --recurse-submodules -j8 https://github.com/Amphaal/SoundBuddy`
- In VS Code, use the command palette to run `Tasks : Run Tests Task`, which will configure the build
- In VS Code, simply run `Launch` in the `Execute and Debug` tab to build the executable 

SoundBuddy have some dependencies that are external and thus need to be installed on your system. Here is how to proceed on each handled platform :

### MacOS (LLVMClang)

#### Dependencies
All dependencies are fetched via Homebrew (https://brew.sh), make sure you have it properly installed on your build system.
- Run command in terminal to fetch depedencies : `brew install $(cat ./prerequisites/brew.txt)`

#### Security
Now from OSX 13.2+, executables have to be signed with a developper identity to be even lauched (for Qt binary depedencies via `macdeployqt`, for IFW OSX Bundle via `CPack`...).
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
- Run command in terminal to fetch depedencies : `pacman -S --needed - < ./prerequisites/msys.txt`

#### Security
On Windows, if installer is not signed, Chrome + Windows Defender will ring alarms all over the place.
- How-to: https://learn.microsoft.com/en-US/windows/msix/package/create-certificate-package-signing
- Download required Windows App Certification App via Windows SDK Setup (`mingw-w64-clang-x86_64-nss` does not seems to work atm): https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

- First, generate `.pfx` file by running `toolchains/win_generate_pfx.sh`. Script will ask for publisher infos: give required infos if relevant.
  - Make sure that the Organisation Name is the same as the `APP_PUBLISHER_ORG` in `CMakeLists.txt` !
- Then, build and run the project as usual

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
