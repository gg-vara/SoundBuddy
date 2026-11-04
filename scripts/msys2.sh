#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Runs a command in MSYS2's CLANG64 environment, from the current folder, from any shell of Git Bash or MSYS2 :
#   scripts/msys2.sh cmake --workflow --preset windows-ci
#
# MSYS2's login shell resets the environment : what the build needs from Windows is handed over explicitly
# (APPDATA : gh finds its login there, LOCALAPPDATA and USERPROFILE : CMake finds Bun there)

set -euo pipefail

root="${MSYS2_ROOT:-C:\\msys64}"
msys="$(cygpath -u "$root")"

if [ ! -x "$msys/usr/bin/bash.exe" ]; then
    echo "No MSYS2 installation in $root : set the MSYS2_ROOT environment variable" >&2
    exit 1
fi

exec "$msys/usr/bin/env.exe" -u MINGW64_ROOT \
    MSYSTEM=CLANG64 CHERE_INVOKING=1 \
    MSYS2_ROOT="$root" \
    APPDATA="$APPDATA" LOCALAPPDATA="$LOCALAPPDATA" USERPROFILE="$USERPROFILE" \
    "$msys/usr/bin/bash.exe" -lc 'exec "$@"' bash "$@"
