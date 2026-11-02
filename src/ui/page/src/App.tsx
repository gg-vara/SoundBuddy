// The page : a view of the state machine's snapshot. Components only read it, and send it events
import { createSignal, Match, onSettled, Show, Switch } from 'solid-js';
import { animate } from './animations';
import { FeederPanel } from './components/FeederPanel';
import { Header } from './components/Header';
import { PreferencesSheet } from './components/PreferencesSheet';
import { ShoutPanel } from './components/ShoutPanel';
import { Tabs } from './components/Tabs';
import { WarningsSheet } from './components/WarningsSheet';
import type { Tab } from './state';
import type { View } from './view';

export function App(props: { view: View }) {
  return (
    <Switch>
      <Match when={props.view.snapshot().matches('failed')}>
        <div class="empty">{props.view.snapshot().context.error}</div>
      </Match>
      <Match when={props.view.data()}>
        <Loaded view={props.view} />
      </Match>
    </Switch>
  );
}

function Loaded(props: { view: View }) {
  const [tab, setTab] = createSignal<Tab>('shout');

  let root!: HTMLDivElement;
  onSettled(() => animate(root, [{ opacity: 0 }, { opacity: 1 }], { duration: 180, easing: 'ease-out' }));

  return (
    <div class="app" ref={root}>
      <Header view={props.view} />
      <Tabs view={props.view} selected={tab()} onSelect={setTab} />
      <main>
        <Show when={tab() === 'shout'} fallback={<FeederPanel view={props.view} />}>
          <ShoutPanel view={props.view} />
        </Show>
      </main>
      <PreferencesSheet view={props.view} />
      <WarningsSheet view={props.view} />
    </div>
  );
}
