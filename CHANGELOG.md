# Changelog

Each section becomes the notes of its GitHub release, and of the update that installed copies download (see `cmake/velopack/vpk.cmake`).

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
