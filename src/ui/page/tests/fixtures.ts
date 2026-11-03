// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Stand-ins for the application, in the tests
import type { CommandName, Native } from '../src/native';
import type { NativeState, WarningsReport } from '../src/state';

export function nativeState(changes: Partial<NativeState> = {}): NativeState {
  return {
    app: { name: 'SoundBuddy', version: '0.0.0', platformName: 'SoundVitrine', musicApp: 'iTunes', language: 'en', debug: false },
    translations: {},
    status: { text: 'Waiting for appropriate credentials.', connectivity: 'refused' },
    tabs: { shout: { running: false, log: [] }, feeder: { running: false, log: [] } },
    nowPlaying: null,
    settings: {
      platformHostUrl: 'https://host.test',
      platformUsername: 'user',
      platformPassword: 'secret',
      autoRunShout: false,
      runAtStartup: false,
    },
    canOpenPlatform: true,
    hasWarnings: false,
    searchingUpdates: false,
    panel: null,
    ...changes,
  };
}

export interface FakeNative extends Native {
  commands: [CommandName, unknown][];
  // answers the "ready" query, once called
  answerReady(state?: NativeState): void;
}

export function fakeNative(report: WarningsReport = []): FakeNative {
  let answer: ((state: NativeState) => void) | null = null;
  const pendingReady = new Promise<NativeState>(resolve => {
    answer = resolve;
  });

  const commands: [CommandName, unknown][] = [];
  return {
    commands,
    ready: () => pendingReady,
    warnings: () => Promise.resolve(report),
    command: (name, argument = null) => void commands.push([name, argument]),
    answerReady: (state = nativeState()) => answer?.(state),
  };
}
