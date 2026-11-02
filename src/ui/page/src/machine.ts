// States of the page : loading, then in parallel the two workers, the menu and the panels.
// The data shown comes from the application (see state.ts); the regions only follow what the user asked for meanwhile,
// so that a click is not repeated while the application has not answered yet. Tested in tests/machine.test.ts
import { assign, fromPromise, type SnapshotFrom, setup } from 'xstate';
import type { CommandName, Native } from './native';
import {
  applyEvent,
  type Credentials,
  fromNativeState,
  type NativeEvent,
  type NativeState,
  type PageData,
  type PanelName,
  type Tab,
  type WarningsReport,
} from './state';

export interface PageInput {
  native: Native;
  texts: readonly string[]; // to translate
}

export interface PageContext extends PageInput {
  data: PageData | null; // once loaded
  pending: NativeEvent[]; // received while loading
  report: WarningsReport; // last warnings read
  error: string | null; // loading failed
}

export type PageEvent =
  | { type: 'native.event'; event: NativeEvent }
  | { type: 'ui.start'; tab: Tab }
  | { type: 'ui.stop'; tab: Tab }
  | { type: 'ui.command'; name: CommandName; argument?: unknown }
  | { type: 'ui.openMenu' }
  | { type: 'ui.closeMenu' }
  | { type: 'ui.openPanel'; panel: PanelName }
  | { type: 'ui.closePanel' }
  | { type: 'ui.savePreferences'; credentials: Credentials };

export const pageMachine = setup({
  types: {
    context: {} as PageContext,
    events: {} as PageEvent,
    input: {} as PageInput,
  },
  actors: {
    loadState: fromPromise<NativeState, PageInput>(({ input }) => input.native.ready(input.texts)),
    loadWarnings: fromPromise<WarningsReport, PageInput>(({ input }) => input.native.warnings()),
  },
  delays: {
    // the application answers a start or a stop right away : past this, the request is considered lost
    answerTimeout: 10_000,
  },
  guards: {
    isRunning: ({ context }, params: { tab: Tab }) => context.data?.running[params.tab] === true,
    isStopped: ({ context }, params: { tab: Tab }) => context.data?.running[params.tab] === false,
    isTab: ({ event }, params: { tab: Tab }) => (event.type === 'ui.start' || event.type === 'ui.stop') && event.tab === params.tab,
    isPanel: ({ event }, params: { panel: PanelName }) => event.type === 'ui.openPanel' && event.panel === params.panel,
    isRequested: ({ context }, params: { panel: PanelName }) => context.data?.requestedPanel === params.panel,
  },
  actions: {
    command: ({ context }, params: { name: CommandName }) => context.native.command(params.name),
    forwardCommand: ({ context, event }) => {
      if (event.type === 'ui.command') context.native.command(event.name, event.argument);
    },
    savePreferences: ({ context, event }) => {
      if (event.type === 'ui.savePreferences') context.native.command('savePreferences', event.credentials);
    },
    bufferEvent: assign({
      pending: ({ context, event }) => (event.type === 'native.event' ? [...context.pending, event.event] : context.pending),
    }),
    applyEvent: assign({
      data: ({ context, event }) => (event.type === 'native.event' && context.data ? applyEvent(context.data, event.event) : context.data),
    }),
    clearRequestedPanel: assign({
      data: ({ context }) => (context.data ? { ...context.data, requestedPanel: null } : null),
    }),
  },
}).createMachine({
  id: 'page',
  context: ({ input }) => ({ ...input, data: null, pending: [], report: [], error: null }),
  initial: 'loading',
  states: {
    loading: {
      invoke: {
        src: 'loadState',
        input: ({ context }) => ({ native: context.native, texts: context.texts }),
        onDone: {
          target: 'ready',
          // the events received meanwhile happened after the state was read
          actions: assign({
            data: ({ context, event }) => context.pending.reduce(applyEvent, fromNativeState(event.output)),
            pending: [],
          }),
        },
        onError: {
          target: 'failed',
          actions: assign({ error: ({ event }) => String(event.error) }),
        },
      },
      on: { 'native.event': { actions: 'bufferEvent' } },
    },

    failed: {},

    ready: {
      type: 'parallel',
      on: { 'native.event': { actions: 'applyEvent' } },
      states: {
        shout: {
          initial: 'idle',
          states: {
            idle: {
              always: { guard: { type: 'isRunning', params: { tab: 'shout' } }, target: 'running' },
              on: {
                'ui.start': {
                  guard: { type: 'isTab', params: { tab: 'shout' } },
                  target: 'starting',
                  actions: { type: 'command', params: { name: 'runShouts' } },
                },
              },
            },
            starting: {
              always: { guard: { type: 'isRunning', params: { tab: 'shout' } }, target: 'running' },
              after: { answerTimeout: 'idle' },
            },
            running: {
              always: { guard: { type: 'isStopped', params: { tab: 'shout' } }, target: 'idle' },
              on: {
                'ui.stop': {
                  guard: { type: 'isTab', params: { tab: 'shout' } },
                  target: 'stopping',
                  actions: { type: 'command', params: { name: 'stopShouts' } },
                },
              },
            },
            stopping: {
              always: { guard: { type: 'isStopped', params: { tab: 'shout' } }, target: 'idle' },
              after: { answerTimeout: 'running' },
            },
          },
        },

        // the feeder stops by itself
        feeder: {
          initial: 'idle',
          states: {
            idle: {
              always: { guard: { type: 'isRunning', params: { tab: 'feeder' } }, target: 'running' },
              on: {
                'ui.start': {
                  guard: { type: 'isTab', params: { tab: 'feeder' } },
                  target: 'starting',
                  actions: { type: 'command', params: { name: 'runFeeder' } },
                },
              },
            },
            starting: {
              always: { guard: { type: 'isRunning', params: { tab: 'feeder' } }, target: 'running' },
              after: { answerTimeout: 'idle' },
            },
            running: {
              always: { guard: { type: 'isStopped', params: { tab: 'feeder' } }, target: 'idle' },
            },
          },
        },

        // a popover, opened and closed by the page (clicks beside it, Escape) : commands close it, as they come from its items or elsewhere
        menu: {
          initial: 'closed',
          on: {
            'ui.command': { target: '.closed', actions: 'forwardCommand' },
            'ui.openPanel': '.closed',
            'ui.closeMenu': '.closed',
          },
          states: {
            closed: { on: { 'ui.openMenu': 'open' } },
            open: {},
          },
        },

        // one at a time, opened by the user or by the application (tray menu)
        panel: {
          initial: 'closed',
          always: [
            {
              guard: { type: 'isRequested', params: { panel: 'preferences' } },
              target: '.preferences',
              actions: 'clearRequestedPanel',
            },
            {
              guard: { type: 'isRequested', params: { panel: 'warnings' } },
              target: '.warnings',
              actions: 'clearRequestedPanel',
            },
          ],
          on: {
            'ui.openPanel': [
              { guard: { type: 'isPanel', params: { panel: 'preferences' } }, target: '.preferences' },
              { guard: { type: 'isPanel', params: { panel: 'warnings' } }, target: '.warnings' },
            ],
            'ui.closePanel': '.closed',
          },
          states: {
            closed: { id: 'panelClosed' },
            preferences: {
              on: { 'ui.savePreferences': { target: 'closed', actions: 'savePreferences' } },
            },
            warnings: {
              initial: 'loading',
              states: {
                loading: {
                  invoke: {
                    src: 'loadWarnings',
                    input: ({ context }) => ({ native: context.native, texts: context.texts }),
                    onDone: { target: 'shown', actions: assign({ report: ({ event }) => event.output }) },
                    onError: '#panelClosed',
                  },
                },
                shown: {},
              },
            },
          },
        },
      },
    },
  },
});

export type PageSnapshot = SnapshotFrom<typeof pageMachine>;

export type WorkerState = 'idle' | 'starting' | 'running' | 'stopping';

// state of a region of "ready", or null while not ready
export function regionState(snapshot: PageSnapshot, region: 'shout' | 'feeder'): WorkerState | null;
export function regionState(snapshot: PageSnapshot, region: 'menu'): 'closed' | 'open' | null;
export function regionState(snapshot: PageSnapshot, region: 'panel'): 'closed' | 'preferences' | 'warnings' | null;
export function regionState(snapshot: PageSnapshot, region: 'shout' | 'feeder' | 'menu' | 'panel'): string | null {
  const { value } = snapshot;
  if (typeof value !== 'object' || !('ready' in value)) return null;
  const regionValue = (value.ready as Record<string, unknown>)[region];
  return typeof regionValue === 'string' ? regionValue : Object.keys(regionValue as object)[0];
}
