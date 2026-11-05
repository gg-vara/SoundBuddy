// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The real executable, without a window, against a local server standing in for SoundVitrine.
// SOUNDBUDDY_EXE and SOUNDBUDDY_LIBRARY are set by tests/CMakeLists.txt

import { afterAll, afterEach, beforeAll, expect, test } from 'bun:test';
import { existsSync, mkdtempSync, rmSync } from 'node:fs';
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

const preferences = (password = 'right-password') => ({
  platformHostUrl: `http://localhost:${server.port}`,
  platformUsername: 'tester',
  platformPassword: password,
});

beforeAll(() => {
  server = Bun.serve({
    port: 0,
    async fetch(request) {
      const body = Buffer.from(await request.arrayBuffer()).toString('latin1');
      const path = new URL(request.url).pathname;
      received.push({ path, headers: request.headers, body });
      if (path === '/hb/creds' && !body.includes('right-password')) return new Response('Refused by the server', { status: 403 });
      return new Response('Bon appétit!');
    },
  });
});

afterEach(async () => {
  for (const app of apps.splice(0)) app.process.kill();
  received = [];
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

test(
  'checks credentials as the server expects, and says when they are refused',
  async () => {
    const app = await start();
    await app.command('savePreferences', preferences());

    await app.wait(message => message.type === 'status' && message.status.connectivity === 'connected', 'connected status');
    const check = received.find(request => request.path === '/hb/creds') as Received;
    expect(check.headers.get('x-client-id')).toBe('SoundBuddy');
    expect(check.headers.get('x-client-version')).toBeTruthy();
    expect(check.body).toContain('name="username"');
    expect(check.body).toContain('tester');

    await app.command('savePreferences', preferences('wrong-password'));
    const refused = await app.wait(
      message => message.type === 'status' && message.status.connectivity === 'refused' && message.status.text.includes('Refused by the server'),
      'refused status',
    );
    expect(refused).toBeTruthy();
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

    const upload = received.find(request => request.path === '/u/tester/uploadMusicLibrary') as Received;
    expect(upload).toBeTruthy();
    expect(upload.body).toContain('name="SoundVitrine_file"');
    expect(upload.body).toContain('Content-Type: application/octet-stream');
    expect(upload.body).toContain('name="headless"');
    expect(upload.body).toContain('right-password');

    const logged = app.messages.filter(message => message.type === 'log' && message.tab === 'feeder').map(message => message.message.text);
    expect(logged.join('\n')).toContain('Bon appétit!');
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
    expect(received.some(request => request.path.includes('uploadMusicLibrary'))).toBe(false);
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
