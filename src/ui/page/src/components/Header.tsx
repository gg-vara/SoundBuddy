import { createEffect, createMemo, Show } from 'solid-js';
import { animate, bouncy } from '../animations';
import { t } from '../i18n';
import { regionState } from '../machine';
import { loaded, type View } from '../view';
import { Icon } from './Icon';

export function Header(props: { view: View }) {
  const data = () => loaded(props.view);
  const status = () => data().status;

  // the pill bounces on each change of connectivity
  let pill!: HTMLDivElement;
  const statusKey = createMemo(() => `${status().connectivity} ${status().text}`);
  createEffect(statusKey, () => animate(pill, [{ transform: 'scale(.92)' }, { transform: 'none' }], bouncy), { defer: true });

  return (
    <header>
      <div class="logo" aria-hidden="true">
        &#9835;
      </div>
      <h1>{data().app.name}</h1>
      <Show when={data().app.debug}>
        <span class="badge">DEBUG</span>
      </Show>
      <div class="spacer" />
      <div class="pill" data-state={status().connectivity} title={status().text} ref={pill}>
        <span class="dot" />
        <span class="pill-text">{status().text}</span>
      </div>
      <button
        class="icon-button"
        title={t('My %1', data().app.platformName)}
        disabled={!data().canOpenPlatform}
        onClick={() => props.view.send({ type: 'ui.command', name: 'openPlatform' })}
      >
        <Icon name="external" />
      </button>
      <button class="icon-button" title={t('Preferences')} onClick={() => props.view.send({ type: 'ui.openPanel', panel: 'preferences' })}>
        <Icon name="gear" />
      </button>
      <Menu view={props.view} />
    </header>
  );
}

// a popover : the page closes it on a click beside it or Escape, and tells the state machine
function Menu(props: { view: View }) {
  const data = () => loaded(props.view);
  const open = createMemo(() => regionState(props.view.snapshot(), 'menu') === 'open');

  // the state machine closes it too, after a command
  let menu!: HTMLDivElement;
  createEffect(open, isOpen => {
    if (!isOpen && menu.matches(':popover-open')) menu.hidePopover();
  });

  const version = () => {
    const { app, searchingUpdates } = data();
    return `${app.name} ${app.version}` + (searchingUpdates ? ` — ${t('Searching for updates...')}` : '');
  };

  return (
    <>
      <button class="icon-button menu-button" title={t('More')} popovertarget="menu" aria-haspopup="menu">
        <Icon name="more" />
      </button>
      <div
        id="menu"
        class="menu"
        role="menu"
        popover="auto"
        ref={menu}
        onToggle={event => props.view.send(event.newState === 'open' ? { type: 'ui.openMenu' } : { type: 'ui.closeMenu' })}
      >
        <button role="menuitem" disabled={!data().hasWarnings} onClick={() => props.view.send({ type: 'ui.openPanel', panel: 'warnings' })}>
          {t('Read latest upload warnings report')}
        </button>
        <button role="menuitem" onClick={() => props.view.send({ type: 'ui.command', name: 'openDataFolder' })}>
          {t('Access upload data folder')}
        </button>
        <hr />
        <button role="menuitem" disabled={data().searchingUpdates} onClick={() => props.view.send({ type: 'ui.command', name: 'checkUpdates' })}>
          {t('Check for updates')}
        </button>
        <div class="menu-note">{version()}</div>
        <hr />
        <button role="menuitem" onClick={() => props.view.send({ type: 'ui.command', name: 'quit' })}>
          {t('Quit')}
        </button>
      </div>
    </>
  );
}
