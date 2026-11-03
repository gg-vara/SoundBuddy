// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// What components get of the page's state machine : its snapshot, as a signal, and a way to send it events
import { createSignal } from 'solid-js';
import type { Actor } from 'xstate';
import { setTranslations } from './i18n';
import type { PageEvent, PageSnapshot, pageMachine } from './machine';
import type { PageData } from './state';

export interface View {
  snapshot: () => PageSnapshot;
  // once loaded
  data: () => PageData | null;
  send: (event: PageEvent) => void;
}

export function createView(page: Actor<typeof pageMachine>): View {
  const [snapshot, setSnapshot] = createSignal(page.getSnapshot());
  page.subscribe(next => {
    // together, so that texts are never shown untranslated
    if (next.context.data) setTranslations(next.context.data.translations);
    setSnapshot(next);
  });

  return {
    snapshot,
    data: () => snapshot().context.data,
    send: event => page.send(event),
  };
}

// the data of a loaded page
export function loaded(view: View): PageData {
  const data = view.data();
  if (!data) throw new Error('The page is not loaded yet');
  return data;
}
