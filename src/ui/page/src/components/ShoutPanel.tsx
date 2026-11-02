import { createMemo, onSettled } from 'solid-js';
import { animate } from '../animations';
import { t } from '../i18n';
import { regionState } from '../machine';
import { loaded, type View } from '../view';
import { Log } from './Log';
import { NowPlaying } from './NowPlaying';
import { Toggle } from './Toggle';

export function ShoutPanel(props: { view: View }) {
  const data = () => loaded(props.view);
  const worker = createMemo(() => regionState(props.view.snapshot(), 'shout'));
  const waiting = () => worker() === 'starting' || worker() === 'stopping';
  const stoppable = () => worker() === 'running' || worker() === 'stopping';

  let panel!: HTMLElement;
  onSettled(() =>
    animate(
      panel,
      [
        { opacity: 0, transform: 'translateY(6px)' },
        { opacity: 1, transform: 'none' },
      ],
      { duration: 220, easing: 'ease-out' },
    ),
  );

  return (
    <section class="panel" role="tabpanel" ref={panel}>
      <NowPlaying shout={data().nowPlaying} />
      <div class="toolbar">
        <button
          type="button"
          class={['button', { primary: !stoppable(), busy: waiting() }]}
          disabled={waiting()}
          onClick={() => props.view.send(worker() === 'running' ? { type: 'ui.stop', tab: 'shout' } : { type: 'ui.start', tab: 'shout' })}
        >
          <span class="spinner" />
          {stoppable() ? t('Stop') : t('Connect to %1', data().app.musicApp)}
        </button>
        <Toggle
          checked={data().settings.autoRunShout}
          onChange={checked => props.view.send({ type: 'ui.command', name: 'setAutoRunShout', argument: checked })}
        >
          {t('Start shouting at app launch')}
        </Toggle>
      </div>
      <Log messages={data().logs.shout} language={data().app.language} />
    </section>
  );
}
