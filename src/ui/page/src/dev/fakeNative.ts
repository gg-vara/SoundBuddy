// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Stand-in for the application, to work on the page in a browser (bun run dev) : plays the workers' messages
import type { Native } from '../native';
import type { LogMessage, NativeEvent, NativeState, Shout, Tab } from '../state';

const tracks: Shout[] = [
  {
    name: 'Teardrop',
    artist: 'Massive Attack',
    album: 'Mezzanine',
    year: 1998,
    duration: 330,
    playerPositionMS: 42_000,
    playerState: true,
  },
  { name: 'Roads', artist: 'Portishead', album: 'Dummy', year: 1994, duration: 305, playerPositionMS: 0, playerState: true },
  {
    name: 'Unfinished Sympathy',
    artist: 'Massive Attack',
    album: 'Blue Lines',
    year: 1991,
    duration: 308,
    playerPositionMS: 120_000,
    playerState: false,
  },
];

export function fakeNative(): Native {
  const send = (event: NativeEvent) => setTimeout(() => window.sb?.receive(event), 0);
  const log = (tab: Tab, text: string, kind: LogMessage['kind'] = 'standard', replaceLast = false) =>
    send({ type: 'log', tab, message: { text, kind, time: Date.now() }, replaceLast });

  const state: NativeState = {
    app: {
      name: 'SoundBuddy',
      version: 'dev',
      platformName: 'SoundVitrine',
      musicApp: 'iTunes',
      language: navigator.language,
      debug: true,
      system: 'Browser',
      copyright: 'Copyright (C) 2018-2026 Guillaume Vara',
      repositoryUrl: 'https://github.com/gg-vara/SoundBuddy',
      dataFolder: 'C:/Users/dev/AppData/Local/LVNWL/SoundBuddy',
    },
    translations: {},
    status: { text: 'Waiting for appropriate credentials.', connectivity: 'refused' },
    tabs: { shout: { running: false, log: [] }, feeder: { running: false, log: [] } },
    nowPlaying: null,
    settings: {
      platformHostUrl: 'https://soundvitrine.test',
      platformUsername: 'dev',
      platformPassword: 'dev',
      autoRunShout: false,
      runAtStartup: false,
    },
    canOpenPlatform: true,
    hasWarnings: true,
    searchingUpdates: false,
    updateResult: null,
  };

  let shoutTimer: ReturnType<typeof setInterval> | undefined;

  return {
    ready: async () => state,
    warnings: async () => [
      { Name: 'Song A', Artist: '', Album: 'Album A' },
      { Name: 'Song B', Artist: 'Someone', Album: '' },
    ],
    command(name, argument) {
      console.info('command', name, argument);
      switch (name) {
        case 'runShouts': {
          send({ type: 'run', tab: 'shout', running: true, newLog: true });
          log('shout', 'Waiting for iTunes to launch...');
          let index = 0;
          shoutTimer = setInterval(() => {
            const shout = tracks[index++ % tracks.length];
            send({ type: 'nowPlaying', shout });
            log('shout', `Shouting -> ${shout.name} - ${shout.album} - ${shout.artist}`);
          }, 3000);
          break;
        }
        case 'stopShouts':
          clearInterval(shoutTimer);
          log('shout', 'Stopped watching iTunes, ending session.');
          setTimeout(() => send({ type: 'run', tab: 'shout', running: false, newLog: false }), 300);
          break;
        case 'runFeeder': {
          send({ type: 'run', tab: 'feeder', running: true, newLog: true });
          log('feeder', 'Collecting tracks infos...');
          let percent = 0;
          const timer = setInterval(() => {
            percent += 10;
            log('feeder', `${percent} %`, 'standard', percent > 10);
            if (percent < 100) return;
            clearInterval(timer);
            log('feeder', '3 tracks without an album', 'warning');
            log('feeder', 'An error occured while sending tracks infos to SoundVitrine platform : refused', 'issue');
            send({ type: 'run', tab: 'feeder', running: false, newLog: false });
          }, 150);
          break;
        }
        case 'setAutoRunShout':
          state.settings = { ...state.settings, autoRunShout: argument === true };
          send({ type: 'settings', settings: state.settings, canOpenPlatform: true });
          break;
        case 'setRunAtStartup':
          state.settings = { ...state.settings, runAtStartup: argument === true };
          send({ type: 'settings', settings: state.settings, canOpenPlatform: true });
          break;
        case 'savePreferences':
          state.settings = { ...state.settings, ...(argument as object) };
          send({ type: 'settings', settings: state.settings, canOpenPlatform: state.settings.platformHostUrl !== '' });
          send({ type: 'status', status: { text: 'Asking for credentials validation...', connectivity: 'checking' } });
          setTimeout(
            () => send({ type: 'status', status: { text: `Logged as "${state.settings.platformUsername}"`, connectivity: 'connected' } }),
            1200,
          );
          break;
        case 'checkUpdates':
          send({ type: 'update', searching: true, result: null });
          setTimeout(() => send({ type: 'update', searching: false, result: { kind: 'upToDate', version: '', error: '' } }), 1500);
          break;
        default:
          break;
      }
    },
  };
}
