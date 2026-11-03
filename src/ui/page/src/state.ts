// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// What the application tells the page (src/ui/Controller.cpp), and the data the page shows : pure functions, tested in tests/

export type Tab = 'shout' | 'feeder';
export type MessageKind = 'standard' | 'warning' | 'issue';
export type Connectivity = 'refused' | 'checking' | 'connected';
export type PanelName = 'preferences' | 'warnings';

export interface LogMessage {
  text: string;
  kind: MessageKind;
  time: number; // milliseconds since the epoch
}

export interface Status {
  text: string;
  connectivity: Connectivity;
}

// what the music app plays : only "date" when nothing is playing
export interface Shout {
  date?: string;
  name?: string;
  album?: string;
  artist?: string;
  genre?: string;
  duration?: number; // seconds
  playerPositionMS?: number;
  playerState?: boolean; // playing
  year?: number;
}

export interface Settings {
  platformHostUrl: string;
  platformUsername: string;
  platformPassword: string;
  autoRunShout: boolean;
  runAtStartup: boolean;
}

export type Credentials = Pick<Settings, 'platformHostUrl' | 'platformUsername' | 'platformPassword'>;

export interface AppInfo {
  name: string;
  version: string;
  platformName: string;
  musicApp: string;
  language: string;
  debug: boolean;
}

// tracks of the latest library export missing metadata : column => value, empty when missing
export type WarningsReport = Record<string, string>[];

// the whole state, answering the "ready" query
export interface NativeState {
  app: AppInfo;
  translations: Record<string, string>;
  status: Status;
  tabs: Record<Tab, { running: boolean; log: LogMessage[] }>;
  nowPlaying: Shout | null;
  settings: Settings;
  canOpenPlatform: boolean;
  hasWarnings: boolean;
  searchingUpdates: boolean;
  panel: PanelName | null;
}

// state changes, through sb.receive()
export type NativeEvent =
  | { type: 'status'; status: Status }
  | { type: 'log'; tab: Tab; message: LogMessage; replaceLast: boolean }
  | { type: 'run'; tab: Tab; running: boolean; newLog: boolean }
  | { type: 'nowPlaying'; shout: Shout | null }
  | { type: 'settings'; settings: Settings; canOpenPlatform: boolean }
  | { type: 'warnings'; hasWarnings: boolean }
  | { type: 'update'; searching: boolean }
  | { type: 'panel'; panel: PanelName };

// as many messages as a tab keeps, as the application
export const maxLogEntries = 100;

export interface PageData {
  app: AppInfo;
  translations: Record<string, string>;
  status: Status;
  logs: Record<Tab, LogMessage[]>;
  running: Record<Tab, boolean>;
  nowPlaying: Shout | null;
  settings: Settings;
  canOpenPlatform: boolean;
  hasWarnings: boolean;
  searchingUpdates: boolean;
  requestedPanel: PanelName | null; // by the application, until opened
}

export function fromNativeState(state: NativeState): PageData {
  return {
    app: state.app,
    translations: state.translations,
    status: state.status,
    logs: { shout: state.tabs.shout.log, feeder: state.tabs.feeder.log },
    running: { shout: state.tabs.shout.running, feeder: state.tabs.feeder.running },
    nowPlaying: state.nowPlaying,
    settings: state.settings,
    canOpenPlatform: state.canOpenPlatform,
    hasWarnings: state.hasWarnings,
    searchingUpdates: state.searchingUpdates,
    requestedPanel: state.panel,
  };
}

// "log" with "message" appended, or replacing the last message
export function appendLog(log: LogMessage[], message: LogMessage, replaceLast: boolean): LogMessage[] {
  if (replaceLast && log.length > 0) return [...log.slice(0, -1), message];
  return [...log, message].slice(-maxLogEntries);
}

export function applyEvent(data: PageData, event: NativeEvent): PageData {
  switch (event.type) {
    case 'status':
      return { ...data, status: event.status };
    case 'log':
      return { ...data, logs: { ...data.logs, [event.tab]: appendLog(data.logs[event.tab], event.message, event.replaceLast) } };
    case 'run':
      return {
        ...data,
        running: { ...data.running, [event.tab]: event.running },
        logs: event.newLog ? { ...data.logs, [event.tab]: [] } : data.logs,
      };
    case 'nowPlaying':
      return { ...data, nowPlaying: event.shout };
    case 'settings':
      return { ...data, settings: event.settings, canOpenPlatform: event.canOpenPlatform };
    case 'warnings':
      return { ...data, hasWarnings: event.hasWarnings };
    case 'update':
      return { ...data, searchingUpdates: event.searching };
    case 'panel':
      return { ...data, requestedPanel: event.panel };
  }
}

// a track is playing, or paused
export function isTrack(shout: Shout | null): shout is Shout & { name: string } {
  return !!shout?.name;
}

// position in the track, from 0 to 1, "elapsed" milliseconds after the shout while playing
export function trackProgress(shout: Shout | null, elapsed: number): number {
  if (!isTrack(shout) || !shout.duration) return 0;
  const position = (shout.playerPositionMS ?? 0) + (shout.playerState ? elapsed : 0);
  return Math.min(1, Math.max(0, position / 1000 / shout.duration));
}
