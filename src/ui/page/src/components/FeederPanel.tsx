import { onSettled } from 'solid-js';
import { animate } from '../animations';
import { t } from '../i18n';
import { regionState } from '../machine';
import { loaded, type View } from '../view';
import { Log } from './Log';

export function FeederPanel(props: { view: View }) {
  const data = () => loaded(props.view);
  const busy = () => regionState(props.view.snapshot(), 'feeder') !== 'idle';

  let panel!: HTMLElement;
  onSettled(() => animate(panel, [{ opacity: 0, transform: 'translateY(6px)' }, { opacity: 1, transform: 'none' }], { duration: 220, easing: 'ease-out' }));

  return (
    <section class="panel" role="tabpanel" ref={panel}>
      <div class="toolbar">
        <button class={['button', 'primary', { busy: busy() }]} disabled={busy()} onClick={() => props.view.send({ type: 'ui.start', tab: 'feeder' })}>
          <span class="spinner" />
          {t('Generate Digest and Upload')}
        </button>
      </div>
      <Log messages={data().logs.feeder} language={data().app.language} />
    </section>
  );
}
