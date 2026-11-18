// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The page is built into a single HTML file (scripts and styles inlined), embedded in the executable by CMake
import solid from '@solidjs/vite-plugin';
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

export default defineConfig({
  plugins: [solid(), viteSingleFile()],
  build: {
    // WebView2 : a recent Chromium
    target: 'es2022',
    emptyOutDir: true,
  },
});
