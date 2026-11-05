// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// What is running, and whether a newer version exists
import { Code, FolderOpen, RefreshCw } from 'lucide';
import { createMemo } from 'solid-js';
import { t } from '../i18n';
import { regionState } from '../machine';
import { loaded, type View } from '../view';
import { Icon } from './Icon';
import { Sheet } from './Sheet';

export function AboutSheet(props: { view: View }) {
  const data = () => loaded(props.view);
  const app = () => data().app;
  const open = createMemo(() => regionState(props.view.snapshot(), 'panel') === 'about');

  const updateStatus = () => {
    const { searchingUpdates, updateResult } = data();
    if (searchingUpdates) return t('Searching for updates...');

    switch (updateResult?.kind) {
      case undefined:
        return '';
      case 'ready':
        return t('Version %1 is ready : it will be installed at the next launch.', updateResult.version);
      case 'upToDate':
        return t('Up to date.');
      case 'notInstalled':
        return t('Updates are only available for installed versions of %1.', app().name);
      case 'failed':
        return `${t('Error while checking updates')} : ${updateResult.error}`;
    }
  };

  const close = () => props.view.send({ type: 'ui.closePanel' });

  return (
    <Sheet open={open()} onClose={close}>
      <h2>{t('About %1', app().name)}</h2>
      <dl class="about">
        <dt>{t('Version')}</dt>
        <dd>{app().version}</dd>
        <dt>{t('Build')}</dt>
        <dd>{app().debug ? t('Debug') : t('Production')}</dd>
        <dt>{t('System')}</dt>
        <dd>{app().system}</dd>
        <dt>{t('Data folder')}</dt>
        <dd>{app().dataFolder}</dd>
        <dt>{t('License')}</dt>
        <dd>GPL-3.0-or-later</dd>
      </dl>
      <p class="about-note">{app().copyright}</p>
      <fieldset>
        <legend>{t('Updates')}</legend>
        <div class="update-row">
          <span role="status">{updateStatus()}</span>
          <button
            type="button"
            class="button"
            disabled={data().searchingUpdates}
            onClick={() => props.view.send({ type: 'ui.command', name: 'checkUpdates' })}
          >
            <Icon icon={RefreshCw} />
            {t('Check for updates')}
          </button>
        </div>
      </fieldset>
      <div class="actions">
        <button type="button" class="button" onClick={() => props.view.send({ type: 'ui.command', name: 'openRepository' })}>
          <Icon icon={Code} />
          {t('Source code')}
        </button>
        <button type="button" class="button" onClick={() => props.view.send({ type: 'ui.command', name: 'openDataFolder' })}>
          <Icon icon={FolderOpen} />
          {t('Access upload data folder')}
        </button>
        <button type="button" class="button primary" autofocus onClick={close}>
          {t('Close')}
        </button>
      </div>
    </Sheet>
  );
}
