// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The real window on Windows, driven through WebView2's debugging port and Win32 messages, against a local server standing in for
// SoundVitrine. What it covers is what the headless tests cannot : the page calls and events across threads, the dialogs, the tray menu.
// Debug builds open their window when they start. SOUNDBUDDY_EXE and SOUNDBUDDY_LIBRARY are set by tests/CMakeLists.txt

import { afterAll, afterEach, beforeAll, expect, test } from 'bun:test';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const EXE = process.env.SOUNDBUDDY_EXE as string;
const LIBRARY = process.env.SOUNDBUDDY_LIBRARY as string;
const TIMEOUT_MS = 60_000;

// the thresholds leave room for a loaded machine : what they catch takes as long as the server below, whose tokens never come in time
const RIGHT_AWAY_MS = 1500;
const TOKENS_DELAY_MS = 30_000;

const WM_NULL = 0x0000;
const WM_CLOSE = 0x0010;
const WM_ENDSESSION = 0x0016;
const WM_COMMAND = 0x0111;
const WM_KEYDOWN = 0x0100;
const WM_CONTEXTMENU = 0x007b;
const MN_GETHMENU = 0x01e1;
const VK_RETURN = 0x0d;
const VK_DOWN = 0x28;
const IDYES = 6;
const MF_GRAYED = 0x1;
const TRAY_ICON_MESSAGE = 0x8000 + 2; // WM_APP + 2, in src/ui/win/MainWindow.win.cpp
const MENU_MY_PLATFORM = 3; // MenuCommand, in src/ui/NativeShell.h

const WINDOW_CLASS = 'SoundBuddyWindow';
const DIALOG_CLASS = '#32770';
const MENU_CLASS = '#32768';

// Win32 from PowerShell : what "body" outputs
const WIN32_API = `
Add-Type -Namespace W -Name U -UsingNamespace System.Text -MemberDefinition @'
public delegate bool EnumProc(System.IntPtr h, System.IntPtr p);
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, System.IntPtr p);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(System.IntPtr h, out uint p);
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(System.IntPtr h, StringBuilder s, int n);
[DllImport("user32.dll")] public static extern System.IntPtr SendMessageTimeout(System.IntPtr h, uint m, System.IntPtr w, System.IntPtr l, uint f, uint t, out System.IntPtr r);
[DllImport("user32.dll")] public static extern bool PostMessage(System.IntPtr h, uint m, System.IntPtr w, System.IntPtr l);
[DllImport("user32.dll")] public static extern uint GetMenuState(System.IntPtr m, uint id, uint flags);
'@
function Find($owner, $class) {
  $script:found = [IntPtr]::Zero
  [void][W.U]::EnumWindows({ param($h, $p)
    $o = 0; [void][W.U]::GetWindowThreadProcessId($h, [ref]$o)
    if ($o -eq $owner) { $s = New-Object System.Text.StringBuilder 256; [void][W.U]::GetClassName($h, $s, 256); if ($s.ToString() -eq $class) { $script:found = $h } }
    $true
  }, [IntPtr]::Zero)
  $script:found
}
`;

async function win32(body: string): Promise<string> {
  const shell = Bun.spawn(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', WIN32_API + body], { stdout: 'pipe', stderr: 'pipe' });
  const [output, errors] = await Promise.all([new Response(shell.stdout).text(), new Response(shell.stderr).text()]);
  await shell.exited;
  if (errors.trim()) throw new Error(errors);
  return output.trim();
}

// a window of "pid", waiting for it to come : "class" as a number, 0 if it never does
async function windowOf(pid: number, windowClass: string): Promise<string> {
  for (let attempt = 0; attempt < 40; attempt++) {
    const found = await win32(`Find ${pid} '${windowClass}'`);
    if (found !== '0') return found;
    await Bun.sleep(250);
  }
  return '0';
}

const post = (window: string, message: number, wParam = 0, lParam = 0) =>
  win32(`[void][W.U]::PostMessage([IntPtr]${window}, ${message}, [IntPtr]${wParam}, [IntPtr]${lParam})`);

// how long it takes the thread of "window" to handle "message" : -1 if it does not within 10 seconds
async function sendTime(window: string, message: number, wParam = 0): Promise<number> {
  const output = await win32(`
    $r = [IntPtr]::Zero
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $answered = [W.U]::SendMessageTimeout([IntPtr]${window}, ${message}, [IntPtr]${wParam}, [IntPtr]::Zero, 2, 10000, [ref]$r)
    if ($answered -eq [IntPtr]::Zero) { -1 } else { $watch.ElapsedMilliseconds }
  `);
  return Number(output);
}

// tokens answered after "tokensDelayMs", or by "tokensAnswer"
let server: ReturnType<typeof Bun.serve>;
let tokensDelayMs = 0;
let tokensAnswer: (() => Response) | null = null;

beforeAll(() => {
  server = Bun.serve({
    port: 0,
    idleTimeout: 120,
    async fetch(request) {
      if (new URL(request.url).pathname !== '/oauth/token') return new Response('', { status: 404 });
      await Bun.sleep(tokensDelayMs);
      return tokensAnswer?.() ?? new Response('error code: 522', { status: 522 });
    },
  });
});

// a port nothing listens to
function freePort(): number {
  const probe = Bun.listen({ hostname: '127.0.0.1', port: 0, socket: { data() {} } });
  const { port } = probe;
  probe.stop(true);
  return port;
}

class Page {
  private id = 0;
  private readonly pending = new Map<number, (message: any) => void>();

  private constructor(private readonly socket: WebSocket) {
    socket.onmessage = event => {
      const message = JSON.parse(event.data as string);
      this.pending.get(message.id)?.(message);
    };
  }

  static async of(port: number): Promise<Page> {
    let url: string | undefined;
    for (let attempt = 0; attempt < 200 && !url; attempt++) {
      try {
        const targets = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()) as { type: string; webSocketDebuggerUrl: string }[];
        url = targets.find(target => target.type === 'page')?.webSocketDebuggerUrl;
      } catch {}
      if (!url) await Bun.sleep(100);
    }
    if (!url) throw new Error('No page to debug');

    const socket = new WebSocket(url);
    await new Promise((resolve, reject) => {
      socket.onopen = resolve;
      socket.onerror = reject;
    });
    return new Page(socket);
  }

  // the value of "expression", awaited : what it throws fails the test
  evaluate(expression: string): Promise<any> {
    const id = ++this.id;
    return new Promise((resolve, reject) => {
      this.pending.set(id, message => {
        if (message.result?.exceptionDetails) reject(new Error(`${expression} : ${JSON.stringify(message.result.exceptionDetails)}`));
        else resolve(message.result?.result?.value);
      });
      this.socket.send(JSON.stringify({ id, method: 'Runtime.evaluate', params: { expression, awaitPromise: true, returnByValue: true } }));
    });
  }

  async until(expression: string, what: string): Promise<void> {
    for (let attempt = 0; attempt < 150; attempt++) {
      if (await this.evaluate(expression)) return;
      await Bun.sleep(100);
    }
    throw new Error(`Never ${what}`);
  }

  loaded() {
    return this.until(`typeof window.sbQuery === 'function' && document.querySelector('.pill') !== null`, 'loaded');
  }

  close() {
    this.socket.close();
  }
}

interface App {
  process: Bun.Subprocess;
  folder: string;
  port: number;
  page: Page;
}

const running: App[] = [];
const folders: string[] = [];

async function start(settings: Record<string, unknown> | null = {}): Promise<App> {
  const folder = mkdtempSync(join(tmpdir(), 'soundbuddy-window-'));
  folders.push(folder);
  mkdirSync(folder, { recursive: true });
  if (settings) {
    writeFileSync(
      join(folder, 'settings.json'),
      JSON.stringify({
        platformHostUrl: `http://localhost:${server.port}`,
        platformUsername: 'tester',
        platformPassword: 'password1',
        autoRunShout: false,
        runAtStartup: false,
        ...settings,
      }),
    );
  }

  const port = freePort();
  const process_ = Bun.spawn([EXE, '--data-dir', folder, '--library', LIBRARY], {
    env: { ...process.env, WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${port}` },
    stdout: 'ignore',
    stderr: 'ignore',
  });
  const page = await Page.of(port);
  await page.loaded();

  const app = { process: process_, folder, port, page };
  running.push(app);
  return app;
}

async function timeOf(action: () => Promise<unknown>): Promise<number> {
  const begin = performance.now();
  await action();
  return performance.now() - begin;
}

// the application is then waiting for an answer
async function quitWithShoutsRunning(app: App) {
  await app.page.evaluate(`sbCommand('runShouts', null)`);
  await app.page.until(`sbQuery('ready', []).then(state => state.tabs.shout.running)`, 'shouting');
  await app.page.evaluate(`sbCommand('quit', null)`);
}

afterEach(async () => {
  for (const app of running.splice(0)) {
    app.page.close();
    app.process.kill();
    await app.process.exited;
  }
  tokensDelayMs = 0;
  tokensAnswer = null;
});

afterAll(() => {
  server.stop(true);
  for (const folder of folders) {
    // what WebView2 still holds is left to the temporary folder
    try {
      rmSync(folder, { recursive: true, force: true });
    } catch {}
  }
});

test(
  'the page shows and answers while tokens do not come, and quitting ends it all',
  async () => {
    tokensDelayMs = TOKENS_DELAY_MS;
    const app = await start();

    expect(await timeOf(() => app.page.evaluate(`sbQuery('ready', [])`))).toBeLessThan(RIGHT_AWAY_MS);
    expect(await timeOf(() => app.page.evaluate(`sbQuery('warnings', null)`))).toBeLessThan(RIGHT_AWAY_MS);

    // the second stops the first, which is waiting for tokens
    await app.page.evaluate(`sbCommand('runFeeder', null)`);
    await Bun.sleep(500);
    await app.page.evaluate(`sbCommand('runFeeder', null)`);
    expect(await timeOf(() => app.page.evaluate(`sbQuery('ready', [])`))).toBeLessThan(RIGHT_AWAY_MS);

    void app.page.evaluate(`sbCommand('quit', null)`).catch(() => {});
    expect(await app.process.exited).toBe(0);
    expect(readFileSync(join(app.folder, 'logs', 'soundbuddy.log'), 'utf8')).not.toContain('[ERROR]');
  },
  TIMEOUT_MS,
);

test(
  'what the application tells reaches the page',
  async () => {
    const app = await start();

    await app.page.until(`document.querySelector('.pill').dataset.state === 'unreachable'`, 'unreachable');
    await app.page.evaluate(`sbCommand('runFeeder', null)`);
    await app.page.until(`sbQuery('ready', []).then(state => state.tabs.feeder.log.length > 0)`, 'a log');
  },
  TIMEOUT_MS,
);

test(
  'a dialog leaves the window responsive, and its answer goes back',
  async () => {
    tokensDelayMs = TOKENS_DELAY_MS;
    const app = await start();
    await quitWithShoutsRunning(app);

    const dialog = await windowOf(app.process.pid, DIALOG_CLASS);
    expect(dialog).not.toBe('0');

    const window = await windowOf(app.process.pid, WINDOW_CLASS);
    const answered = await sendTime(window, WM_NULL);
    expect(answered).toBeGreaterThanOrEqual(0);
    expect(answered).toBeLessThan(RIGHT_AWAY_MS);

    await post(dialog, WM_COMMAND, IDYES);
    expect(await app.process.exited).toBe(0);
  },
  TIMEOUT_MS,
);

test(
  'the session ending while a dialog waits does not hang',
  async () => {
    const app = await start();
    await quitWithShoutsRunning(app);
    expect(await windowOf(app.process.pid, DIALOG_CLASS)).not.toBe('0');

    // both threads would otherwise wait for each other
    const window = await windowOf(app.process.pid, WINDOW_CLASS);
    const handled = await sendTime(window, WM_ENDSESSION, 1);
    expect(handled).toBeGreaterThanOrEqual(0);
    expect(handled).toBeLessThan(5000);
  },
  TIMEOUT_MS,
);

// opens the menu of the tray icon, and what it tells of "My platform" : then chooses its Preferences
async function menuStateAndPreferences(app: App): Promise<{ platformGrayed: boolean }> {
  const window = await windowOf(app.process.pid, WINDOW_CLASS);
  await post(window, TRAY_ICON_MESSAGE, 0, WM_CONTEXTMENU);

  const menu = await windowOf(app.process.pid, MENU_CLASS);
  expect(menu).not.toBe('0');

  const state = await win32(`
    $h = [IntPtr]::Zero
    [void][W.U]::SendMessageTimeout([IntPtr]${menu}, ${MN_GETHMENU}, [IntPtr]::Zero, [IntPtr]::Zero, 2, 5000, [ref]$h)
    [W.U]::GetMenuState($h, ${MENU_MY_PLATFORM}, 0)
  `);

  // About, then the separators are skipped : Open monitor, Preferences
  for (const key of [VK_DOWN, VK_DOWN, VK_DOWN, VK_RETURN]) await post(menu, WM_KEYDOWN, key);

  return { platformGrayed: (Number(state) & MF_GRAYED) !== 0 };
}

test(
  'the tray menu tells whether the platform can be opened',
  async () => {
    const withLogin = await start();
    await withLogin.page.until(`sbQuery('ready', []).then(state => state.canOpenPlatform)`, 'with a login');
    expect((await menuStateAndPreferences(withLogin)).platformGrayed).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'the tray menu tells when the platform cannot be opened',
  async () => {
    const withoutLogin = await start(null);
    expect((await menuStateAndPreferences(withoutLogin)).platformGrayed).toBe(true);
  },
  TIMEOUT_MS,
);

test(
  'the tray menu opens a panel, on the page shown or on the next one',
  async () => {
    const app = await start();

    await menuStateAndPreferences(app);
    await app.page.until(`document.querySelector('.panel') !== null`, 'the panel on the page shown');

    // hidden, the page goes away : it is told once a new one is loaded
    const window = await windowOf(app.process.pid, WINDOW_CLASS);
    await post(window, WM_CLOSE);
    app.page.close();
    await Bun.sleep(1000);

    await menuStateAndPreferences(app);
    app.page = await Page.of(app.port);
    await app.page.loaded();
    await app.page.until(`document.querySelector('.panel') !== null`, 'the panel on the next page');
  },
  TIMEOUT_MS,
);
