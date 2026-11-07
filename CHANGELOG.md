# Changelog

Each section becomes the notes of its GitHub release, and of the update that installed copies download (see `cmake/velopack/vpk.cmake`).

## 0.14.2

> **Requires SoundVitrine 0.13 or newer** : this version logs in with OAuth, which older servers do not offer. It cannot connect to them.

> **Unsigned build** : this installer is not code-signed yet. Windows SmartScreen may block it ("Windows protected your PC") : choose **More info**, then **Run anyway**.

### Changed

- The window only displays, the application running apart : it stays responsive whatever the application is doing, as stopping shouts or a library upload
- When SoundVitrine cannot be reached, it is said in plain words, in amber rather than red as a refusal would, and it keeps trying
- Waiting for a login ends after about 10 seconds, rather than saying it is checking for much longer

### Fixed

- A failure of the application ends it with an error code again

## 0.14.1

> **Requires SoundVitrine 0.13 or newer** : this version logs in with OAuth, which older servers do not offer. It cannot connect to them.

> **Unsigned build** : this installer is not code-signed yet. Windows SmartScreen may block it ("Windows protected your PC") : choose **More info**, then **Run anyway**.

### Fixed

- The window no longer freezes while SoundVitrine is slow or unreachable, at startup or later
- Stopping shouts no longer freezes the window while the last one is being sent

## 0.14.0

> **Requires SoundVitrine 0.13 or newer** : this version logs in with OAuth, which older servers do not offer. It cannot connect to them.

> **Unsigned build** : this installer is not code-signed yet. Windows SmartScreen may block it ("Windows protected your PC") : choose **More info**, then **Run anyway**.

This version includes 0.13.0, which was never published : see below for what it brings.

### Changed

- Releases are published as soon as they are built
- The installer and the portable archive carry the version in their names, and say there that they are unsigned, rather than the title of the release

## 0.13.0

> **Requires SoundVitrine 0.13 or newer** : this version logs in with OAuth, which older servers do not offer. It cannot connect to them.

> **Unsigned build** : this installer is not code-signed yet. Windows SmartScreen may block it ("Windows protected your PC") : choose **More info**, then **Run anyway**.

### New

- Your password is no longer kept : it is exchanged once for tokens, then removed from the settings file, including one saved by a previous version. If it is refused, Preferences ask for it again
- The tokens are stored by the system : encrypted for your Windows user, in the Keychain on macOS
- Saving Preferences with an empty password keeps your login

### Changed

- Library and shouts are sent with SoundVitrine's new API, and the password no longer travels with each of them
- The feed tells whether your library was updated, or was already up to date

## 0.12.0

> **Requires SoundVitrine 0.10 or newer** : older servers refuse the libraries this version uploads.

> **Unsigned build** : this installer is not code-signed yet. Windows SmartScreen may block it ("Windows protected your PC") : choose **More info**, then **Run anyway**.

### New

- About panel, opened from the tray menu or the ⋯ menu : version, build type, system, data folder, license, source code
- Checking for updates moves to the About panel, which shows the result. The question to restart for a downloaded update stays a dialog
- The window title, the tray tooltip and the first tray menu item show the version, and "(Debug)" on a debug build
- The tray icon tells what the app is doing : an orange circle while connected to iTunes, a green dot when connected to SoundVitrine, a red one when refused or unreachable
- Icons across the interface
- `--data-dir <folder>` keeps a run away from your real settings and data, `--library <file>` reads that library, and `--headless` runs without a window, driven through its standard input and output : to try or test the app safely

### Changed

- Compressed libraries are sent as `application/octet-stream`

## 0.11.0

> **Unsigned build** : this installer is not code-signed yet. Windows SmartScreen may block it ("Windows protected your PC") : choose **More info**, then **Run anyway**.

SoundBuddy is rewritten without Qt : a smaller, native app, with a new interface.

### New

- New interface, shown by the system's web view (WebView2 on Windows), following the system's light or dark theme
- Installs and updates itself : new versions published on GitHub are downloaded and applied automatically. The WebView2 runtime is installed when missing
- Library feeding 3x faster, with properly escaped JSON and decoded XML entities
- Libraries in folders with non-ASCII names are read on Windows
- Settings of previous versions are imported at first launch
- Logs are written to a rotating file
- New icons, sharp at every size Windows shows

### Changed

- The experimental DASH streaming is removed
- Windows 10 or 11, 64-bit

### Upgrading from 0.10.0

0.10.0 used another installer, which does not update to this version : install 0.11.0 with its installer, launch it once so it imports your settings, then uninstall 0.10.0 from Windows' installed apps.
