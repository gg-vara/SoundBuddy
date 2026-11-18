// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Preferences : logging in through the browser, on the platform's own pages, and launching at startup, applied right away
import { LogIn, LogOut, X } from 'lucide';
import { createEffect, createMemo, createSignal, untrack } from 'solid-js';
import { t } from '../i18n';
import { regionState } from '../machine';
import { loaded, type View } from '../view';
import { Icon } from './Icon';
import { Sheet } from './Sheet';
import { Toggle } from './Toggle';

export function PreferencesSheet(props: { view: View }) {
  const data = () => loaded(props.view);
  const open = createMemo(() => regionState(props.view.snapshot(), 'panel') === 'preferences');
  const login = () => data().login;
  const connect = () => props.view.send({ type: 'ui.command', name: 'connect', argument: host().trim() });

  // a draft, from the current settings on each opening
  const [host, setHost] = createSignal('');
  createEffect(open, isOpen => {
    if (isOpen) setHost(untrack(() => data().settings.platformHostUrl));
  });

  return (
    <Sheet open={open()} onClose={() => props.view.send({ type: 'ui.closePanel' })}>
      <form
        method="dialog"
        autocomplete="off"
        onSubmit={event => {
          event.preventDefault();
          connect();
        }}
      >
        <h2>{t('Preferences')}</h2>
        <fieldset>
          <legend>{t('%1 connectivity', data().app.platformName)}</legend>
          <input
            type="url"
            placeholder={t('Host URL')}
            spellcheck="false"
            autofocus
            disabled={data().canOpenPlatform || login().waiting}
            value={host()}
            onInput={event => setHost(event.currentTarget.value)}
          />
          {data().canOpenPlatform ? (
            <div class="update-row">
              <span role="status">{t('Logged in as "%1".', data().settings.platformUsername)}</span>
              <button type="button" class="button" onClick={() => props.view.send({ type: 'ui.command', name: 'disconnect' })}>
                <Icon icon={LogOut} />
                {t('Disconnect')}
              </button>
            </div>
          ) : login().waiting ? (
            <div class="update-row">
              <span role="status">{t('Finish logging in, in the browser.')}</span>
              <button type="button" class="button" onClick={connect}>
                {t('Open the browser again')}
              </button>
              <button
                type="button"
                class="icon-button"
                title={t('Cancel')}
                onClick={() => props.view.send({ type: 'ui.command', name: 'cancelLogin' })}
              >
                <Icon icon={X} />
              </button>
            </div>
          ) : (
            <div class="update-row">
              <span role="alert">{login().error}</span>
              <button type="submit" class="button primary" disabled={host().trim() === ''}>
                <Icon icon={LogIn} />
                {t('Connect through the browser')}
              </button>
            </div>
          )}
        </fieldset>
        <fieldset>
          <legend>{t('Automation')}</legend>
          <Toggle
            checked={data().settings.runAtStartup}
            onChange={checked => props.view.send({ type: 'ui.command', name: 'setRunAtStartup', argument: checked })}
          >
            {t('Launch %1 at system boot', data().app.name)}
          </Toggle>
        </fieldset>
        <div class="actions">
          <button type="button" class="button" onClick={() => props.view.send({ type: 'ui.closePanel' })}>
            {t('Close')}
          </button>
        </div>
      </form>
    </Sheet>
  );
}
