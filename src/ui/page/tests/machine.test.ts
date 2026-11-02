import { describe, expect, test } from 'bun:test';
import { createActor, waitFor } from 'xstate';
import { pageMachine, regionState } from '../src/machine';
import type { NativeEvent, NativeState } from '../src/state';
import { fakeNative, nativeState, type FakeNative } from './fixtures';

// the page, loaded with "state"
async function loadedPage(state: NativeState = nativeState(), native: FakeNative = fakeNative()) {
  const page = createActor(pageMachine.provide({ delays: { answerTimeout: 50 } }), { input: { native, texts: ['Stop'] } });
  page.start();
  native.answerReady(state);
  await waitFor(page, snapshot => snapshot.matches('ready'));
  const receive = (event: NativeEvent) => page.send({ type: 'native.event', event });
  return { page, native, receive };
}

const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

describe('loading', () => {
  test('events received meanwhile are applied after the state', async () => {
    const native = fakeNative();
    const page = createActor(pageMachine, { input: { native, texts: [] } }).start();
    page.send({ type: 'native.event', event: { type: 'run', tab: 'shout', running: true, newLog: true } });
    page.send({ type: 'native.event', event: { type: 'log', tab: 'shout', message: { text: 'Listening', kind: 'standard', time: 1 }, replaceLast: false } });
    expect(page.getSnapshot().matches('loading')).toBe(true);

    native.answerReady(nativeState());
    await waitFor(page, snapshot => snapshot.matches('ready'));
    const { data } = page.getSnapshot().context;
    expect(data?.running.shout).toBe(true);
    expect(data?.logs.shout.map(m => m.text)).toEqual(['Listening']);
    expect(regionState(page.getSnapshot(), 'shout')).toBe('running');
  });

  test('a failed load is reported', async () => {
    const native = { ...fakeNative(), ready: () => Promise.reject(new Error('no bridge')) };
    const page = createActor(pageMachine, { input: { native, texts: [] } }).start();
    await waitFor(page, snapshot => snapshot.matches('failed'));
    expect(page.getSnapshot().context.error).toContain('no bridge');
  });

  test('workers already running show as running', async () => {
    const state = nativeState({ tabs: { shout: { running: false, log: [] }, feeder: { running: true, log: [] } } });
    const { page } = await loadedPage(state);
    expect(regionState(page.getSnapshot(), 'shout')).toBe('idle');
    expect(regionState(page.getSnapshot(), 'feeder')).toBe('running');
  });
});

describe('workers', () => {
  test('a start is asked once, however many clicks', async () => {
    const { page, native } = await loadedPage();
    page.send({ type: 'ui.start', tab: 'shout' });
    page.send({ type: 'ui.start', tab: 'shout' });
    expect(native.commands).toEqual([['runShouts', null]]);
    expect(regionState(page.getSnapshot(), 'shout')).toBe('starting');
  });

  test('starts, then stops, as the application tells', async () => {
    const { page, native, receive } = await loadedPage();
    page.send({ type: 'ui.start', tab: 'shout' });
    receive({ type: 'run', tab: 'shout', running: true, newLog: true });
    expect(regionState(page.getSnapshot(), 'shout')).toBe('running');

    page.send({ type: 'ui.stop', tab: 'shout' });
    page.send({ type: 'ui.stop', tab: 'shout' });
    expect(regionState(page.getSnapshot(), 'shout')).toBe('stopping');
    receive({ type: 'run', tab: 'shout', running: false, newLog: false });
    expect(regionState(page.getSnapshot(), 'shout')).toBe('idle');
    expect(native.commands.map(([name]) => name)).toEqual(['runShouts', 'stopShouts']);
  });

  test('a start the application never answers is given up', async () => {
    const { page } = await loadedPage();
    page.send({ type: 'ui.start', tab: 'feeder' });
    await sleep(80);
    expect(regionState(page.getSnapshot(), 'feeder')).toBe('idle');
  });

  test('a worker started by the application shows as running', async () => {
    const { page, receive } = await loadedPage();
    receive({ type: 'run', tab: 'shout', running: true, newLog: true });
    expect(regionState(page.getSnapshot(), 'shout')).toBe('running');
  });

  test('a fast feeder run ends in idle', async () => {
    const { page, receive } = await loadedPage();
    page.send({ type: 'ui.start', tab: 'feeder' });
    receive({ type: 'run', tab: 'feeder', running: true, newLog: true });
    receive({ type: 'run', tab: 'feeder', running: false, newLog: false });
    expect(regionState(page.getSnapshot(), 'feeder')).toBe('idle');
  });

  test('tabs do not mix', async () => {
    const { page, native, receive } = await loadedPage();
    page.send({ type: 'ui.start', tab: 'feeder' });
    receive({ type: 'run', tab: 'shout', running: true, newLog: true });
    expect(regionState(page.getSnapshot(), 'feeder')).toBe('starting');
    expect(regionState(page.getSnapshot(), 'shout')).toBe('running');
    expect(native.commands).toEqual([['runFeeder', null]]);
  });
});

describe('menu and panels', () => {
  test('a command closes the menu, and reaches the application', async () => {
    const { page, native } = await loadedPage();
    page.send({ type: 'ui.openMenu' });
    expect(regionState(page.getSnapshot(), 'menu')).toBe('open');
    page.send({ type: 'ui.command', name: 'checkUpdates' });
    expect(regionState(page.getSnapshot(), 'menu')).toBe('closed');
    expect(native.commands).toEqual([['checkUpdates', null]]);
  });

  test('preferences are saved, then closed', async () => {
    const { page, native } = await loadedPage();
    page.send({ type: 'ui.openPanel', panel: 'preferences' });
    expect(regionState(page.getSnapshot(), 'panel')).toBe('preferences');

    const credentials = { platformHostUrl: 'https://new.test', platformUsername: 'me', platformPassword: 'pw' };
    page.send({ type: 'ui.savePreferences', credentials });
    expect(regionState(page.getSnapshot(), 'panel')).toBe('closed');
    expect(native.commands).toEqual([['savePreferences', credentials]]);
  });

  test('saving is only possible from the preferences', async () => {
    const { page, native } = await loadedPage();
    page.send({ type: 'ui.savePreferences', credentials: { platformHostUrl: '', platformUsername: '', platformPassword: '' } });
    expect(native.commands).toEqual([]);
  });

  test('warnings are read, then shown', async () => {
    const report = [{ Name: 'Song', Artist: '' }];
    const { page } = await loadedPage(nativeState(), fakeNative(report));
    page.send({ type: 'ui.openMenu' });
    page.send({ type: 'ui.openPanel', panel: 'warnings' });
    expect(regionState(page.getSnapshot(), 'menu')).toBe('closed');
    await waitFor(page, snapshot => snapshot.matches({ ready: { panel: { warnings: 'shown' } } }));
    expect(page.getSnapshot().context.report).toEqual(report);
  });

  test('a panel requested by the application opens once', async () => {
    const { page, receive } = await loadedPage();
    receive({ type: 'panel', panel: 'preferences' });
    expect(regionState(page.getSnapshot(), 'panel')).toBe('preferences');
    expect(page.getSnapshot().context.data?.requestedPanel).toBeNull();

    page.send({ type: 'ui.closePanel' });
    expect(regionState(page.getSnapshot(), 'panel')).toBe('closed');
  });

  test('a panel requested before loading opens once loaded', async () => {
    const { page } = await loadedPage(nativeState({ panel: 'preferences' }));
    expect(regionState(page.getSnapshot(), 'panel')).toBe('preferences');
  });
});
