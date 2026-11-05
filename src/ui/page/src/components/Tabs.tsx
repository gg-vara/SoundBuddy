// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import { type IconNode, Library, Radio } from 'lucide';
import { createEffect, For } from 'solid-js';
import { animate, snappy } from '../animations';
import { regionState } from '../machine';
import type { Tab } from '../state';
import type { View } from '../view';
import { Icon } from './Icon';

const tabs: { tab: Tab; label: string; icon: IconNode }[] = [
  { tab: 'shout', label: 'Shout!', icon: Radio },
  { tab: 'feeder', label: 'Feeder', icon: Library },
];

export function Tabs(props: { view: View; selected: Tab; onSelect: (tab: Tab) => void }) {
  const buttons: Partial<Record<Tab, HTMLButtonElement>> = {};
  let indicator!: HTMLDivElement;

  // the indicator slides under the selected tab, from the second selection on
  let placed = false;
  createEffect(
    () => props.selected,
    selected => {
      const button = buttons[selected];
      if (!button) return;
      const to = { transform: `translateX(${button.offsetLeft}px)`, width: `${button.offsetWidth}px` };
      if (placed) animate(indicator, [{ transform: indicator.style.transform, width: indicator.style.width }, to], snappy);
      Object.assign(indicator.style, to);
      placed = true;
    },
  );

  const isBusy = (tab: Tab) => regionState(props.view.snapshot(), tab) !== 'idle';

  return (
    <div class="tabs" role="tablist">
      <div class="indicator" ref={indicator} />
      <For each={tabs}>
        {entry => (
          <button
            type="button"
            role="tab"
            ref={element => (buttons[entry.tab] = element)}
            aria-selected={props.selected === entry.tab ? 'true' : 'false'}
            class={{ running: isBusy(entry.tab) }}
            onClick={() => props.onSelect(entry.tab)}
          >
            <Icon icon={entry.icon} />
            {entry.label}
            <span class="running-dot" />
          </button>
        )}
      </For>
    </div>
  );
}
