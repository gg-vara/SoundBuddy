// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Calls to the application, bound by the native shell (src/ui/win/MainWindow.win.cpp) :
// sbQuery(name, argument) answers, sbCommand(name, argument) acts; the application calls sb.receive(event).
// The application handles these names (src/app/PageMessages.h), as tests/page_contract_test.cpp checks
import type { NativeEvent, NativeState, WarningsReport } from './state';

export type QueryName = 'ready' | 'warnings';

export type CommandName =
  | 'runShouts'
  | 'stopShouts'
  | 'runFeeder'
  | 'setAutoRunShout'
  | 'setRunAtStartup'
  | 'savePreferences'
  | 'openPlatform'
  | 'openDataFolder'
  | 'checkUpdates'
  | 'openRepository'
  | 'quit';

export interface Native {
  // the whole state, with the translations of "texts"
  ready(texts: readonly string[]): Promise<NativeState>;
  warnings(): Promise<WarningsReport>;
  command(name: CommandName, argument?: unknown): void;
}

type Binding<Name extends string> = (name: Name, argument: unknown) => Promise<unknown>;

declare global {
  interface Window {
    sbQuery?: Binding<QueryName>;
    sbCommand?: Binding<CommandName>;
    sb?: { receive(event: NativeEvent): void };
  }
}

// the application hosting the page
export function hostNative(): Native | null {
  const { sbQuery, sbCommand } = window;
  if (!sbQuery || !sbCommand) return null;

  return {
    ready: texts => sbQuery('ready', texts) as Promise<NativeState>,
    warnings: () => sbQuery('warnings', null) as Promise<WarningsReport>,
    command: (name, argument = null) => void sbCommand(name, argument),
  };
}

// "receive" gets the events of the application
export function listenToNative(receive: (event: NativeEvent) => void): void {
  window.sb = { receive };
}
