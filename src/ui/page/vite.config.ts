// The page is built into a single HTML file (scripts and styles inlined), embedded in the executable by CMake
import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';
import solid from 'vite-plugin-solid';

export default defineConfig({
  plugins: [solid(), viteSingleFile()],
  build: {
    // WebView2 : a recent Chromium
    target: 'es2022',
    emptyOutDir: true,
  },
});
