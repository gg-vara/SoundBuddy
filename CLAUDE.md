# SoundBuddy

Desktop companion of [SoundVitrine](https://github.com/gg-vara/SoundVitrine) : publishes an iTunes / Music library and what is playing. C++20 with a native window hosting a web page (WebView2, WKWebView). See `README.md` for the details.

## Never

- **Launch the app without `--data-dir`** : it would read the user's real settings, and may send what is playing to their real SoundVitrine. The user's installed copy may well be running : `--data-dir` runs beside it.
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

## Running the app

```bash
build_ci/out/SoundBuddy.exe --headless --data-dir <empty folder> --library "deps/ItunesLibraryParser/tests/samples/iTunes Music Library (S).xml"
```

No window, and nothing of the session's real data : see `platform::isolate()`. Without `--library`, the feed fails rather than reading the real library. `--data-dir` alone gives the real window, as isolated.

It is driven by lines on its standard input, and answers with JSON lines (`src/app/Headless.h`, `tests/headless.test.ts`) :

```
command ["savePreferences",{"platformHostUrl":"http://localhost:3000","platformUsername":"testcurator","platformPassword":"testpassword"}]
command ["runFeeder",null]
query ["ready",[]]
```

Those credentials are the ones of SoundVitrine's `bun run dev:seed`. Its log is `<data folder>/logs/soundbuddy.log`.

Shouts still need the real music app : `runShouts` watches iTunes, so nothing replaces it in these runs yet.

## Structure

- `src/app/Controller` : the application state and its workers, independent of the platform. It talks to the window through the `ui::Shell` interface. It runs on the application thread (`src/app/AppThread`), and may wait there : the windows only display, and exchange messages with it. A page call may wait for a busy application, so no lock is held during a request either
- `src/app/PageMessages` : every message between the page and the application. `tests/page_contract_test.cpp` checks them against the page's TypeScript types
- `src/ui/NativeShell` : what the native windows share, and how they exchange messages with the application thread. `src/ui/win`, `src/ui/mac` : what is theirs. `src/app/Headless` : the same application without one
- `src/settings` : what the user sets, and where it is kept. `src/net` : HTTP, the login, uploads
- `src/workers` : heartbeat, library feed, shouts. `*.win.cpp` and `*.mac.cpp` are the platform parts
- `tests/` : one program per test, run by ctest. The `network` label reaches public services, and is left out of default runs

## What SoundVitrine expects

SoundVitrine's `/oauth/token`, `/api/v1/me`, `/api/v1/me/library` and `/api/v1/me/shouts`, so a version with them on the server is required. `src/net/PlatformSession` holds the login : the password is exchanged once, then removed from `settings.json`, and the tokens are kept by `src/helpers/SecretStore` (DPAPI, Keychain), named after the data folder so that `--data-dir` never reaches the real ones. A refresh token works once and using one twice ends the login : all workers share one session, which must stay the same object for as long as the login does. To try them, SoundVitrine's `bun run dev:seed`.

## Conventions

- Several small commits rather than one large one. Commit messages in lowercase, saying what changed
- LF line endings everywhere (`.gitattributes`)
- Comments say why, in the style of the surrounding code
- Version and release notes : `CMakeLists.txt` and `CHANGELOG.md`, which packaging reads
