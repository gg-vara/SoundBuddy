// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The real executable, without a window, against a local server standing in for SoundVitrine.
// SOUNDBUDDY_EXE and SOUNDBUDDY_LIBRARY are set by tests/CMakeLists.txt

import { afterAll, afterEach, beforeAll, describe, expect, test } from 'bun:test';
import { createHash } from 'node:crypto';
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

const platformUrl = () => `http://localhost:${server.port}`;

// the codes the platform gave, with what their exchange must prove (PKCE) and repeat
let codes = new Map<string, { challenge: string; redirectUri: string }>();
let codeCount = 0;
let revoked: string[] = [];

// an OAuth platform : refresh tokens work once, and using one again ends the login, as SoundVitrine does
let validAccess = new Set<string>();
let validRefresh = new Set<string>();
let usedRefresh = new Set<string>();
let tokenCount = 0;
let replays = 0;
let expiresIn = 3600;
let refreshDelayMs = 0;
let tokensDelayMs = 0;
let tokensAnswerOverride: (() => Response) | null = null;

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
    // longer than what is tested : Bun would otherwise end the connection first
    idleTimeout: 60,
    async fetch(request) {
      const raw = await request.arrayBuffer();
      const body = Buffer.from(raw).toString('latin1');
      const path = new URL(request.url).pathname;
      received.push({ path, headers: request.headers, body });

      if (path === '/api/auth/oauth2/token') {
        await Bun.sleep(tokensDelayMs);
        if (tokensAnswerOverride) return tokensAnswerOverride();
        const fields = new URLSearchParams(Buffer.from(raw).toString('utf8'));
        if (fields.get('client_id') !== 'soundbuddy') return oauthError('Unknown client', 401);
        // tokens for the API, as SoundVitrine signs them
        if (fields.get('resource') !== `${platformUrl()}/api/v1`) return oauthError('Unknown resource');

        if (fields.get('grant_type') === 'authorization_code') {
          const given = codes.get(fields.get('code') ?? '');
          codes.delete(fields.get('code') ?? '');
          const proof = createHash('sha256')
            .update(fields.get('code_verifier') ?? '')
            .digest('base64url');
          if (!given || given.challenge !== proof || given.redirectUri !== fields.get('redirect_uri')) return oauthError('Invalid code');
          return tokensAnswer();
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

      if (path === '/api/auth/oauth2/revoke') {
        const fields = new URLSearchParams(body);
        revoked.push(fields.get('token') ?? '');
        validRefresh.delete(fields.get('token') ?? '');
        return new Response(null, { status: 200 });
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
  tokensAnswerOverride = null;
  codes = new Map();
  revoked = [];
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

const feedEnd = (app: App) => app.wait(message => message.type === 'run' && message.tab === 'feeder' && message.running === false, 'end of the feed');

const tokenRequests = () => received.filter(request => request.path === '/api/auth/oauth2/token');

// where SoundBuddy sends the browser, once asked to connect
async function authorization(app: App, host = platformUrl()) {
  app.messages.length = 0;
  await app.command('connect', host);
  const { login } = await app.wait(message => message.type === 'login' && message.login.url !== '', 'where the browser goes');
  return new URL(login.url);
}

// the browser coming back to SoundBuddy, as SoundVitrine sends it : with a code, an error, or another state
async function comeBack(url: URL, answer: Record<string, string> = {}) {
  const back = new URL(url.searchParams.get('redirect_uri') ?? '');
  const code = `code-${++codeCount}`;
  codes.set(code, { challenge: url.searchParams.get('code_challenge') ?? '', redirectUri: back.href });
  const fields = { code, state: url.searchParams.get('state') ?? '', ...answer };
  for (const [name, value] of Object.entries(fields)) back.searchParams.set(name, value);
  return fetch(back);
}

// logged in through the browser, as "tester"
async function logIn(app: App) {
  await comeBack(await authorization(app));
  await app.wait(message => message.type === 'settings' && message.settings.platformUsername === 'tester', 'logged in');
}

test(
  'logs in through the browser : the API, with PKCE, back to this computer',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    const url = await authorization(app);

    // what SoundVitrine checks
    expect(`${url.origin}${url.pathname}`).toBe(`${platformUrl()}/api/auth/oauth2/authorize`);
    expect(url.searchParams.get('client_id')).toBe('soundbuddy');
    expect(url.searchParams.get('response_type')).toBe('code');
    expect(url.searchParams.get('code_challenge_method')).toBe('S256');
    expect(url.searchParams.get('resource')).toBe(`${platformUrl()}/api/v1`);
    expect(url.searchParams.get('scope')?.split(' ')).toEqual(['library:write', 'shout:write', 'offline_access']);
    const back = new URL(url.searchParams.get('redirect_uri') ?? '');
    expect(back.hostname).toBe('127.0.0.1');
    expect(back.pathname).toBe('/callback');
    expect((await app.query('ready', [])).login.waiting).toBe(true);

    const page = await comeBack(url);
    expect(page.status).toBe(200);
    expect(await page.text()).toContain('SoundBuddy');

    await app.wait(message => message.type === 'settings' && message.settings.platformUsername === 'tester', 'logged in');
    await connected(app);
    await app.wait(message => message.type === 'tray' && message.dot === 'ok', 'green dot');
    const state = await app.query('ready', []);
    expect(state.login).toEqual({ waiting: false, url: '', error: '' });
    expect(state.canOpenPlatform).toBe(true);

    expect(tokenRequests()[0]?.headers.get('content-type')).toBe('application/x-www-form-urlencoded');
    const check = received.find(request => request.path === '/api/v1/me') as Received;
    expect(check.headers.get('authorization')).toBe(`Bearer access-${tokenCount}`);

    // the tokens are kept apart, and not readable on Windows, where the file holds them
    expect(settingsOf(folder)).not.toContain('refresh-');
    if (process.platform === 'win32') expect(readFileSync(join(folder, 'session.dat'), 'latin1')).not.toContain('refresh-');
  },
  TIMEOUT_MS,
);

test(
  'says why a login did not work : denied, or from another login',
  async () => {
    const app = await start();

    await comeBack(await authorization(app), { code: '', error: 'access_denied' });
    const denied = await app.wait(message => message.type === 'login' && message.login.error !== '', 'denied');
    // in the language of the system
    expect(denied.login.error).not.toBe('');

    await comeBack(await authorization(app), { state: 'another' });
    const other = await app.wait(message => message.type === 'login' && message.login.error !== '', 'another login');
    expect(other.login.error).not.toBe(denied.login.error);

    expect(tokenRequests()).toHaveLength(0);
    expect((await app.query('ready', [])).canOpenPlatform).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'a login cancelled stops waiting for the browser',
  async () => {
    const app = await start();
    const url = await authorization(app);
    await app.command('cancelLogin');

    expect((await app.query('ready', [])).login.waiting).toBe(false);
    expect(await comeBack(url).catch(() => null)).toBeNull();
  },
  TIMEOUT_MS,
);

test(
  'an older settings file loses its password : the user logs in through the browser',
  async () => {
    const folder = dataFolder();
    mkdirSync(folder, { recursive: true });
    const older = { platformHostUrl: platformUrl(), platformUsername: 'tester', platformPassword: 'secret', autoRunShout: false, runAtStartup: false };
    writeFileSync(join(folder, 'settings.json'), JSON.stringify(older));

    const app = await start(folder);
    await app.wait(message => message.type === 'status' && message.status.connectivity === 'refused', 'asks to log in');
    expect(settingsOf(folder)).not.toContain('secret');
    expect(tokenRequests()).toHaveLength(0);
  },
  TIMEOUT_MS,
);

// logged in, then started again with the access token expired : the heartbeat renews it first
async function expiredLogin() {
  const folder = dataFolder();
  expiresIn = 1;
  const first = await start(folder);
  await logIn(first);
  first.process.kill();
  await first.process.exited;
  received = [];
  return folder;
}

// the window lives on the thread answering these calls : what a slow platform delays, it freezes
describe('with a platform slow to give tokens', () => {
  const slowStart = async () => {
    const folder = await expiredLogin();
    tokensDelayMs = 3000;
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
    'gives up on them, rather than saying it is checking for long',
    async () => {
      const folder = await expiredLogin();
      tokensDelayMs = 15_000;

      const begin = performance.now();
      const app = await start(folder);
      await app.wait(message => message.type === 'status' && message.status.connectivity !== 'checking', 'end of the check');
      const taken = performance.now() - begin;
      expect(taken).toBeGreaterThan(9000);
      expect(taken).toBeLessThan(14_000);
    },
    TIMEOUT_MS * 2,
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
  'a platform that fails is not a refusal : it says so in its own words, and keeps the login',
  async () => {
    const folder = await expiredLogin();
    // what Cloudflare answers when the server behind it is down
    tokensAnswerOverride = () => new Response('error code: 522', { status: 522 });

    const app = await start(folder);
    const { status } = await app.wait(message => message.type === 'status' && message.status.connectivity !== 'checking', 'end of the check');
    expect(status.connectivity).toBe('unreachable');
    expect(status.text).not.toContain('522');
    await app.wait(message => message.type === 'tray' && message.dot === 'issue', 'dot');
    expect((await app.query('ready', [])).canOpenPlatform).toBe(true);
  },
  TIMEOUT_MS,
);

test(
  'a new start keeps the login',
  async () => {
    const folder = dataFolder();
    const first = await start(folder);
    await logIn(first);
    await connected(first);
    first.process.kill();
    await first.process.exited;
    received = [];

    const second = await start(folder);
    await connected(second);
    expect(tokenRequests()).toHaveLength(0);
    expect((await second.query('ready', [])).settings.platformUsername).toBe('tester');
  },
  TIMEOUT_MS,
);

test(
  'disconnecting forgets the login, and tells the platform',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    await logIn(app);
    await connected(app);

    app.messages.length = 0;
    await app.command('disconnect');
    await app.wait(message => message.type === 'status' && message.status.connectivity === 'refused', 'asks to log in');
    const state = await app.query('ready', []);
    expect(state.canOpenPlatform).toBe(false);
    expect(state.settings.platformUsername).toBe('');

    // told meanwhile
    for (let tries = 0; tries < 50 && revoked.length === 0; tries++) await Bun.sleep(100);
    expect(revoked).toEqual([`refresh-${tokenCount}`]);
    expect(existsSync(join(folder, 'session.dat'))).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'an access token refused is renewed, once',
  async () => {
    const app = await start();
    await logIn(app);
    await connected(app);
    validAccess.clear();

    received = [];
    app.messages.length = 0;
    await app.command('runFeeder');
    await feedEnd(app);
    expect(tokenRequests()).toHaveLength(1);
    expect(received.filter(request => request.path === '/api/v1/me/library')).toHaveLength(2);
    expect(replays).toBe(0);
  },
  TIMEOUT_MS,
);

test(
  'workers needing a new token at once never use a refresh token twice',
  async () => {
    // always about to expire : each call asks for a new one
    const folder = await expiredLogin();
    refreshDelayMs = 150;

    // the heartbeat and the feed, together at each start
    for (let round = 0; round < 3; round++) {
      const app = await start(folder);
      await app.command('runFeeder');
      await feedEnd(app);
      app.process.kill();
      await app.process.exited;
    }

    expect(tokenRequests().length).toBeGreaterThan(3);
    expect(replays).toBe(0);
  },
  TIMEOUT_MS * 2,
);

test(
  'a login the platform ended is forgotten',
  async () => {
    const app = await start();
    await logIn(app);
    await connected(app);

    validAccess.clear();
    validRefresh.clear();
    app.messages.length = 0;
    await app.command('runFeeder');
    await feedEnd(app);
    expect((await app.query('ready', [])).canOpenPlatform).toBe(false);
  },
  TIMEOUT_MS,
);

test(
  'feeds the given library to the server',
  async () => {
    const folder = dataFolder();
    const app = await start(folder);
    await logIn(app);
    await app.command('runFeeder');

    await feedEnd(app);

    const upload = received.find(request => request.path === '/api/v1/me/library') as Received;
    expect(upload).toBeTruthy();
    expect(upload.headers.get('authorization')).toStartWith('Bearer ');
    expect(upload.body).toContain('name="file"');
    expect(upload.body).toContain('Content-Type: application/octet-stream');

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
    await logIn(app);
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
