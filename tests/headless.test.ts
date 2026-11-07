// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The real executable, without a window, against a local server standing in for SoundVitrine.
// SOUNDBUDDY_EXE and SOUNDBUDDY_LIBRARY are set by tests/CMakeLists.txt

import { afterAll, afterEach, beforeAll, describe, expect, test } from 'bun:test';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

const EXE = process.env.SOUNDBUDDY_EXE as string;
const LIBRARY = process.env.SOUNDBUDDY_LIBRARY as string;
const TIMEOUT_MS = 20_000;

type Message = Record<string, any>;

// a running app, and the messages it wrote
class App {
  readonly process: Bun.Subprocess<'pipe', 'pipe', 'ignore'>;
  readonly messages: Message[] = [];
  private lines = 0;
  private waiters: { matches: (message: Message) => boolean; resolve: (message: Message) => void }[] = [];

  constructor(dataDir: string, library = true) {
    const args = [EXE, '--headless', '--data-dir', dataDir, ...(library ? ['--library', LIBRARY] : [])];
    this.process = Bun.spawn(args, { stdin: 'pipe', stdout: 'pipe', stderr: 'ignore' });
    void this.read();
  }

  private async read() {
    let pending = '';
    const decoder = new TextDecoder();
    for await (const chunk of this.process.stdout) {
      pending += decoder.decode(chunk, { stream: true });
      for (let end = pending.indexOf('\n'); end >= 0; end = pending.indexOf('\n')) {
        const message = JSON.parse(pending.slice(0, end)) as Message;
        pending = pending.slice(end + 1);
        this.messages.push(message);
        this.waiters = this.waiters.filter(waiter => {
          if (!waiter.matches(message)) return true;
          waiter.resolve(message);
          return false;
        });
      }
    }
  }

  // the first message, already written or to come, which "matches"
  wait(matches: (message: Message) => boolean, what: string): Promise<Message> {
    const found = this.messages.find(matches);
    if (found) return Promise.resolve(found);

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error(`No ${what} in ${JSON.stringify(this.messages)}`)), TIMEOUT_MS);
      this.waiters.push({
        matches,
        resolve: message => {
          clearTimeout(timer);
          resolve(message);
        },
      });
    });
  }

  private send(line: string): number {
    this.process.stdin.write(`${line}\n`);
    void this.process.stdin.flush();
    this.lines += 1;
    return this.lines;
  }

  async query(name: string, argument: unknown = null): Promise<any> {
    const line = this.send(`query ${JSON.stringify([name, argument])}`);
    return (await this.wait(message => message.type === 'answer' && message.line === line, `answer of ${name}`)).value;
  }

  async command(name: string, argument: unknown = null) {
    const line = this.send(`command ${JSON.stringify([name, argument])}`);
    await this.wait(message => message.type === 'done' && message.line === line, `end of ${name}`);
  }

  raw(line: string): number {
    return this.send(line);
  }

  closeInput() {
    void this.process.stdin.end();
    return this.process.exited;
  }
}

// what SoundBuddy asked of the server
interface Received {
  path: string;
  headers: Headers;
  body: string;
}

let server: ReturnType<typeof Bun.serve>;
let received: Received[] = [];
const folders: string[] = [];
const apps: App[] = [];

const dataFolder = () => {
  const folder = mkdtempSync(join(tmpdir(), 'soundbuddy-headless-'));
  folders.push(folder);
  return folder;
};

const start = async (dataDir = dataFolder(), library = true) => {
  const app = new App(dataDir, library);
  apps.push(app);
  await app.wait(message => message.type === 'started', 'start');
  return app;
};

const PASSWORD = 'right&pass=word %é';

const preferences = (password = PASSWORD, username = 'tester') => ({
  platformHostUrl: `http://localhost:${server.port}`,
  platformUsername: username,
  platformPassword: password,
});

// an OAuth platform : refresh tokens work once, and using one again ends the login, as SoundVitrine does
let validAccess = new Set<string>();
let validRefresh = new Set<string>();
let usedRefresh = new Set<string>();
let tokenCount = 0;
let replays = 0;
let expiresIn = 3600;
let refreshDelayMs = 0;
let tokensDelayMs = 0;

const tokensAnswer = () => {
  tokenCount += 1;
  const access = `access-${tokenCount}`;
  const refresh = `refresh-${tokenCount}`;
  validAccess.add(access);
  validRefresh.add(refresh);
  return Response.json({ access_token: access, refresh_token: refresh, token_type: 'Bearer', expires_in: expiresIn, scope: 'all' });
};

const oauthError = (description: string, status = 400) => Response.json({ error: 'invalid_grant', error_description: description }, { status });

beforeAll(() => {
  server = Bun.serve({
    port: 0,
    async fetch(request) {
      const raw = await request.arrayBuffer();
      const body = Buffer.from(raw).toString('latin1');
      const path = new URL(request.url).pathname;
      received.push({ path, headers: request.headers, body });

      if (path === '/oauth/token') {
        await Bun.sleep(tokensDelayMs);
        const fields = new URLSearchParams(Buffer.from(raw).toString('utf8'));
        if (fields.get('client_id') !== 'soundbuddy') return oauthError('Unknown client', 401);

        if (fields.get('grant_type') === 'password') {
          return fields.get('username') === 'tester' && fields.get('password') === PASSWORD ? tokensAnswer() : oauthError('Refused by the server');
        }

        const token = fields.get('refresh_token') ?? '';

        // long enough for another worker to want one meanwhile
        await Bun.sleep(refreshDelayMs);
        if (usedRefresh.has(token)) {
          replays += 1;
          return oauthError('This refresh token is no longer valid');
        }
        if (!validRefresh.has(token)) return oauthError('This refresh token is no longer valid');
        validRefresh.delete(token);
        usedRefresh.add(token);
        return tokensAnswer();
      }

      const token = /^Bearer (.+)$/.exec(request.headers.get('authorization') ?? '')?.[1] ?? '';
      if (!validAccess.has(token)) {
        return Response.json({ type: 'about:blank', title: 'UNAUTHORIZED', status: 401, detail: 'Expired token' }, { status: 401 });
      }
      if (path === '/api/v1/me') return Response.json({ username: 'tester', scopes: [] });
      if (path === '/api/v1/me/library') return Response.json({ changed: true });
      return new Response('', { status: 404 });
    },
  });
});

afterEach(async () => {
  for (const app of apps.splice(0)) app.process.kill();
  received = [];
  validAccess = new Set();
  validRefresh = new Set();
  usedRefresh = new Set();
  replays = 0;
  expiresIn = 3600;
  refreshDelayMs = 0;
  tokensDelayMs = 0;
});

afterAll(() => {
  server.stop(true);
  for (const folder of folders) {
    // what a process just killed still holds is left to the temporary folder
    try {
      rmSync(folder, { recursive: true, force: true });
    } catch {}
  }
});

test('refuses to run against the real settings', async () => {
  const app = Bun.spawn([EXE, '--headless'], { stdin: 'ignore', stdout: 'ignore', stderr: 'pipe' });
  expect(await app.exited).toBe(2);
  expect(await new Response(app.stderr).text()).toContain('--data-dir');
});

test(
  'answers with the state, and keeps to its data folder',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);

    const state = await app.query('ready', ['Shout']);
    expect(state.settings.platformHostUrl).toBe('');
    expect(state.settings.runAtStartup).toBe(false);
    expect(state.tabs.shout.running).toBe(false);
    expect(state.translations).toHaveProperty('Shout');

    await app.command('setAutoRunShout', false);
    expect(existsSync(join(folder, 'settings.json'))).toBe(true);
    expect(existsSync(join(folder, 'logs', 'soundbuddy.log'))).toBe(true);
  },
  TIMEOUT_MS,
);

test(
  'tells what About shows, opens nothing, and looks for no update when isolated',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);

    const { app: info } = await app.query('ready', []);
    expect(info.version).toBeTruthy();
    expect(info.debug).toBe(process.env.SOUNDBUDDY_DEBUG_BUILD === '1');
    expect(info.system).toBeTruthy();
    expect(info.copyright).toContain('Guillaume Vara');
    expect(info.repositoryUrl).toContain('SoundBuddy');
    expect(resolve(info.dataFolder)).toBe(resolve(folder));

    await app.command('openRepository');
    await app.command('checkUpdates');
    const checked = await app.wait(message => message.type === 'update' && message.result?.kind === 'notInstalled', 'result of the check');
    expect(checked.searching).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'a line which is neither a query nor a command is reported',
  async () => {
    const app = await start();
    const line = app.raw('nonsense');
    const error = await app.wait(message => message.type === 'error', 'error');
    expect(error.line).toBe(line);
  },
  TIMEOUT_MS,
);

const settingsOf = (folder: string) => readFileSync(join(folder, 'settings.json'), 'utf8');

const connected = (app: App) => app.wait(message => message.type === 'status' && message.status.connectivity === 'connected', 'connected status');

test(
  'exchanges the password for tokens, then forgets it',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    await app.command('savePreferences', preferences());

    await connected(app);
    await app.wait(message => message.type === 'tray' && message.dot === 'ok', 'green dot');
    expect(app.messages.some(message => message.type === 'tray' && message.shouting)).toBe(false);

    const login = received.find(request => request.path === '/oauth/token') as Received;
    expect(login.headers.get('content-type')).toBe('application/x-www-form-urlencoded');
    const check = received.find(request => request.path === '/api/v1/me') as Received;
    expect(check.headers.get('authorization')).toBe('Bearer access-' + tokenCount);
    expect(check.body).toBe('');

    await app.wait(message => message.type === 'settings' && message.settings.platformPassword === '', 'password forgotten');
    expect(settingsOf(folder)).not.toContain('right');
    expect(settingsOf(folder)).toContain('"platformPassword": ""');

    // nothing else carries it
    expect(received.filter(request => request.path !== '/oauth/token').some(request => request.body.includes('right'))).toBe(false);

    // the tokens are not readable on Windows, where the file holds them
    const stored = join(folder, 'session.dat');
    if (process.platform === 'win32') expect(readFileSync(stored, 'latin1')).not.toContain('refresh-');
  },
  TIMEOUT_MS,
);

test(
  'says when the password is refused, and does not keep it',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    await app.command('savePreferences', preferences('wrong-password'));

    await app.wait(
      message => message.type === 'status' && message.status.connectivity === 'refused' && message.status.text.includes('Refused by the server'),
      'refused status',
    );
    await app.wait(message => message.type === 'tray' && message.dot === 'issue', 'red dot');
    await app.wait(message => message.type === 'settings' && message.settings.platformPassword === '', 'password forgotten');
    expect(settingsOf(folder)).not.toContain('wrong-password');
  },
  TIMEOUT_MS,
);

test(
  'an older settings file gets rid of its password on the first run',
  async () => {
    const folder = dataFolder();
    mkdirSync(folder, { recursive: true });
    writeFileSync(join(folder, 'settings.json'), JSON.stringify({ ...preferences(), autoRunShout: false, runAtStartup: false }));

    const app = await start(folder);
    await connected(app);
    await app.wait(message => message.type === 'settings' && message.settings.platformPassword === '', 'password forgotten');
    expect(settingsOf(folder)).not.toContain('right');
  },
  TIMEOUT_MS,
);

// the window lives on the thread answering these calls : what a slow platform delays, it freezes
describe('with a platform slow to give tokens', () => {
  const slowStart = async () => {
    tokensDelayMs = 3000;
    const folder = dataFolder();
    mkdirSync(folder, { recursive: true });
    writeFileSync(join(folder, 'settings.json'), JSON.stringify({ ...preferences(), autoRunShout: false, runAtStartup: false }));
    const app = await start(folder);
    // the heartbeat is now asking for them
    await Bun.sleep(300);
    return app;
  };

  const timeOf = async (action: () => Promise<unknown>) => {
    const begin = performance.now();
    await action();
    return performance.now() - begin;
  };

  test(
    'the page gets its state right away',
    async () => {
      const app = await slowStart();
      expect(await timeOf(() => app.query('ready', []))).toBeLessThan(500);
    },
    TIMEOUT_MS,
  );

  test(
    'a worker waiting for them stops right away',
    async () => {
      const app = await slowStart();
      await app.command('runFeeder');
      // long enough to read the library, and wait for tokens
      await Bun.sleep(1000);
      expect(await timeOf(() => app.command('runFeeder'))).toBeLessThan(500);
    },
    TIMEOUT_MS,
  );
});

test(
  'keeps the password while the platform cannot answer',
  async () => {
    const folder = dataFolder();
    mkdirSync(folder, { recursive: true });
    const unreachable = { ...preferences(), platformHostUrl: 'http://127.0.0.1:9' };
    writeFileSync(join(folder, 'settings.json'), JSON.stringify({ ...unreachable, autoRunShout: false, runAtStartup: false }));

    const app = await start(folder);
    await app.wait(message => message.type === 'status' && message.status.connectivity === 'refused', 'refused status');
    await Bun.sleep(300);
    expect(settingsOf(folder)).toContain('right');
  },
  TIMEOUT_MS,
);

test(
  'a new start keeps the login, without any password',
  async () => {
    const folder = dataFolder();
    const first = await start(folder);
    await first.command('savePreferences', preferences());
    await connected(first);
    await first.wait(message => message.type === 'settings' && message.settings.platformPassword === '', 'password forgotten');
    first.process.kill();
    await first.process.exited;
    received = [];

    const second = await start(folder);
    await connected(second);
    expect(received.some(request => request.path === '/oauth/token')).toBe(false);
    expect((await second.query('ready', [])).settings.platformPassword).toBe('');
  },
  TIMEOUT_MS,
);

test(
  'saving without a password keeps the login, but not for another user',
  async () => {
    const app = await start();
    await app.command('savePreferences', preferences());
    await connected(app);
    await app.wait(message => message.type === 'settings' && message.settings.platformPassword === '', 'password forgotten');

    app.messages.length = 0;
    received = [];
    await app.command('savePreferences', preferences(''));
    await connected(app);
    expect(received.some(request => request.path === '/oauth/token')).toBe(false);

    app.messages.length = 0;
    await app.command('savePreferences', preferences('', 'somebody'));
    await app.wait(message => message.type === 'status' && message.status.connectivity === 'refused', 'asks for credentials');
  },
  TIMEOUT_MS,
);

test(
  'an access token refused is renewed, once',
  async () => {
    const app = await start();
    await app.command('savePreferences', preferences());
    await connected(app);
    validAccess.clear();

    received = [];
    app.messages.length = 0;
    await app.command('savePreferences', preferences(''));
    await connected(app);
    expect(received.filter(request => request.path === '/oauth/token')).toHaveLength(1);
    expect(replays).toBe(0);
  },
  TIMEOUT_MS,
);

test(
  'workers needing a new token at once never use a refresh token twice',
  async () => {
    // always about to expire : each call asks for a new one
    expiresIn = 1;
    refreshDelayMs = 150;
    const app = await start();
    await app.command('savePreferences', preferences());
    await connected(app);

    for (let round = 0; round < 3; round++) {
      await app.command('runFeeder');
      await app.command('savePreferences', preferences(''));
      await app.wait(message => message.type === 'run' && message.tab === 'feeder' && message.running === false, 'end of the feed');
    }

    expect(received.filter(request => request.path === '/oauth/token').length).toBeGreaterThan(3);
    expect(replays).toBe(0);
  },
  TIMEOUT_MS,
);

test(
  'a login the platform ended asks for credentials again',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    await app.command('savePreferences', preferences());
    await connected(app);

    validAccess.clear();
    validRefresh.clear();
    app.messages.length = 0;
    await app.command('savePreferences', preferences(''));
    await app.wait(message => message.type === 'status' && message.status.connectivity === 'refused', 'refused status');
    expect((await app.query('ready', [])).canOpenPlatform).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'feeds the given library to the server',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    await app.command('savePreferences', preferences());
    await app.command('runFeeder');

    await app.wait(message => message.type === 'run' && message.tab === 'feeder' && message.running === false, 'end of the feed');

    const upload = received.find(request => request.path === '/api/v1/me/library') as Received;
    expect(upload).toBeTruthy();
    expect(upload.headers.get('authorization')).toStartWith('Bearer ');
    expect(upload.body).toContain('name="file"');
    expect(upload.body).toContain('Content-Type: application/octet-stream');
    expect(upload.body).not.toContain('right');

    const logged = app.messages.filter(message => message.type === 'log' && message.tab === 'feeder').map(message => message.message.text);
    expect(logged.join('\n')).toContain('bon appétit');
    expect(existsSync(join(folder, 'logs', 'soundbuddy.log'))).toBe(true);
  },
  TIMEOUT_MS,
);

test(
  'without --library, the feed fails rather than reading the real library',
  async () => {
    const app = await start(dataFolder(), false);
    await app.command('savePreferences', preferences());
    await app.command('runFeeder');

    const issue = await app.wait(message => message.type === 'log' && message.tab === 'feeder' && message.message.kind === 'issue', 'issue');
    expect(issue.message.text).toContain('--library');
    expect(received.some(request => request.path.includes('/library'))).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'runs beside another data folder, but not twice on the same one',
  async () => {
    const folder = dataFolder();
    await start(folder);
    await start();

    const second = Bun.spawn([EXE, '--headless', '--data-dir', folder], { stdin: 'ignore', stdout: 'ignore', stderr: 'ignore' });
    expect(await second.exited).toBe(1);
  },
  TIMEOUT_MS,
);

test(
  'ends at the end of its input, or when told to',
  async () => {
    const closed = await start();
    expect(await closed.closeInput()).toBe(0);

    const told = await start();
    told.raw('command ["quit",null]');
    expect(await told.process.exited).toBe(0);
  },
  TIMEOUT_MS,
);
