# SoundBuddy

Desktop companion of [SoundVitrine](https://github.com/gg-vara/SoundVitrine) : publishes an iTunes / Music library and what is playing. C++20 with a native window hosting a web page (WebView2, WKWebView). See `README.md` for the details.

## Never

- **Launch the app (`SoundBuddy.exe`, an installed copy, or `build_*/out/`)** : it reads the user's real settings, and may send what is playing to their real SoundVitrine. Run the tests instead, which keep to their own identity (`tests/support/version.h`).
- Install software the build needs : ask the user.
- Push or publish without being asked.

## Commands

On Windows, from the repository, `scripts/msys2.sh` runs a command in the MSYS2 CLANG64 environment :

```bash
scripts/msys2.sh cmake --workflow --preset windows-ci
```

This is what CI runs : a Debug build with clang-tidy, whose warnings are errors, then the unit tests. Others :

- `scripts/msys2.sh ctest --preset windows-ci --output-on-failure -R <test>` : some tests again
- `scripts/msys2.sh clang-format -i <files>` : layout, before committing
- `cmake --workflow --preset windows-package` : unsigned packages in `build_release_unsigned/releases`, nothing published
- on macOS : `cmake --workflow --preset macos-test`, without the wrapper. The usual machine is Windows : CI is what checks macOS

The user interface page, in `src/ui/page` (Solid 2, XState, Vite, Biome) :

- `bun test` and `bun x biome check --write .`, from that folder
- `bun run dev` : the page in a browser at http://localhost:5173, against fake native data (`src/dev/fakeNative.ts`). Safe : it sends nothing anywhere

## Structure

- `src/ui/Controller` : the application state and its workers, independent of the platform. It talks to the window through the `ui::Shell` interface
- `src/ui/PageMessages` : every message between the page and the application. `tests/page_contract_test.cpp` checks them against the page's TypeScript types
- `src/ui/win`, `src/ui/mac` : the native windows
- `src/workers` : heartbeat, library feed, shouts. `*.win.cpp` and `*.mac.cpp` are the platform parts
- `tests/` : one program per test, run by ctest. The `network` label reaches public services, and is left out of default runs

## What SoundVitrine expects

Released versions must keep working : `POST /hb/creds`, `/u/{user}/uploadMusicLibrary` and `/u/{user}/uploadShout` are multipart forms answered in plain text, which the app shows to its users. To try them, SoundVitrine's `bun run dev:seed` gives a local server with a test account.

## Conventions

- Several small commits rather than one large one. Commit messages in lowercase, saying what changed
- LF line endings everywhere (`.gitattributes`)
- Comments say why, in the style of the surrounding code
- Version and release notes : `CMakeLists.txt` and `CHANGELOG.md`, which packaging reads
