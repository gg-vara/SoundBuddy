// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import { describe, expect, test } from 'bun:test';
import { appendLog, applyEvent, fromNativeState, type LogMessage, maxLogEntries, trackProgress } from '../src/state';
import { nativeState } from './fixtures';

const message = (text: string): LogMessage => ({ text, kind: 'standard', time: 0 });

describe('appendLog', () => {
  test('appends', () => {
    expect(appendLog([message('a')], message('b'), false).map(m => m.text)).toEqual(['a', 'b']);
  });

  test('replaces the last message, as a progress', () => {
    expect(appendLog([message('a'), message('1 %')], message('2 %'), true).map(m => m.text)).toEqual(['a', '2 %']);
  });

  test('replacing in an empty log appends', () => {
    expect(appendLog([], message('a'), true).map(m => m.text)).toEqual(['a']);
  });

  test('keeps the last messages only', () => {
    const full = Array.from({ length: maxLogEntries }, (_, i) => message(String(i)));
    const log = appendLog(full, message('new'), false);
    expect(log).toHaveLength(maxLogEntries);
    expect(log[0].text).toBe('1');
    expect(log.at(-1)?.text).toBe('new');
  });
});

describe('applyEvent', () => {
  const data = fromNativeState(nativeState());

  test('a new run clears its log only', () => {
    const before = applyEvent(data, { type: 'log', tab: 'shout', message: message('old'), replaceLast: false });
    const after = applyEvent(before, { type: 'run', tab: 'shout', running: true, newLog: true });
    expect(after.logs.shout).toEqual([]);
    expect(after.running.shout).toBe(true);
    expect(after.logs.feeder).toBe(before.logs.feeder);
  });

  test('the end of a run keeps its log', () => {
    const before = applyEvent(data, { type: 'log', tab: 'feeder', message: message('done'), replaceLast: false });
    const after = applyEvent(before, { type: 'run', tab: 'feeder', running: false, newLog: false });
    expect(after.logs.feeder.map(m => m.text)).toEqual(['done']);
    expect(after.running.feeder).toBe(false);
  });

  test('settings carry whether the platform can be opened', () => {
    const settings = { ...data.settings, platformHostUrl: '' };
    const after = applyEvent(data, { type: 'settings', settings, canOpenPlatform: false });
    expect(after.settings.platformHostUrl).toBe('');
    expect(after.canOpenPlatform).toBe(false);
  });

  test('a panel requested by the application waits to be opened', () => {
    expect(applyEvent(data, { type: 'panel', panel: 'warnings' }).requestedPanel).toBe('warnings');
  });

  test('does not change the data it is given', () => {
    const copy = structuredClone(data);
    applyEvent(data, { type: 'log', tab: 'shout', message: message('x'), replaceLast: true });
    expect(data).toEqual(copy);
  });
});

describe('trackProgress', () => {
  const shout = { name: 'Teardrop', duration: 100, playerPositionMS: 25_000, playerState: true };

  test('position over duration, going on while playing', () => {
    expect(trackProgress(shout, 0)).toBe(0.25);
    expect(trackProgress(shout, 10_000)).toBe(0.35);
  });

  test('stays still while paused, and within bounds', () => {
    expect(trackProgress({ ...shout, playerState: false }, 10_000)).toBe(0.25);
    expect(trackProgress(shout, 1_000_000)).toBe(1);
  });

  test('nothing without a track or a duration', () => {
    expect(trackProgress(null, 0)).toBe(0);
    expect(trackProgress({ date: 'x' }, 0)).toBe(0);
    expect(trackProgress({ ...shout, duration: 0 }, 0)).toBe(0);
  });
});
