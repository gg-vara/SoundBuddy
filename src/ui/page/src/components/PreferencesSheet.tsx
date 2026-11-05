// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Preferences : the credentials are saved together; launching at startup applies right away
import { Eye, EyeOff, Save } from 'lucide';
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

  // drafts, from the current settings on each opening
  const [host, setHost] = createSignal('');
  const [username, setUsername] = createSignal('');
  const [password, setPassword] = createSignal('');
  const [revealed, setRevealed] = createSignal(false);
  createEffect(open, isOpen => {
    if (!isOpen) return;
    const settings = untrack(() => data().settings);
    setHost(settings.platformHostUrl);
    setUsername(settings.platformUsername);
    setPassword(settings.platformPassword);
    setRevealed(false);
  });

  return (
    <Sheet open={open()} onClose={() => props.view.send({ type: 'ui.closePanel' })}>
      <form
        method="dialog"
        autocomplete="off"
        onSubmit={event => {
          event.preventDefault();
          props.view.send({
            type: 'ui.savePreferences',
            credentials: { platformHostUrl: host().trim(), platformUsername: username().trim(), platformPassword: password() },
          });
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
            value={host()}
            onInput={event => setHost(event.currentTarget.value)}
          />
          <input
            type="text"
            placeholder={t('Username')}
            spellcheck="false"
            value={username()}
            onInput={event => setUsername(event.currentTarget.value)}
          />
          <div class="password">
            <input
              type={revealed() ? 'text' : 'password'}
              placeholder={t('Password')}
              value={password()}
              onInput={event => setPassword(event.currentTarget.value)}
            />
            <button
              type="button"
              class="icon-button"
              title={t(revealed() ? 'Hide password' : 'Show password')}
              onClick={() => setRevealed(!revealed())}
            >
              <Icon icon={revealed() ? EyeOff : Eye} />
            </button>
          </div>
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
            {t('Cancel')}
          </button>
          <button type="submit" class="button primary">
            <Icon icon={Save} />
            {t('Save')}
          </button>
        </div>
      </form>
    </Sheet>
  );
}
