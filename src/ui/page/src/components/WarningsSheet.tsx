// Tracks of the latest library export missing metadata
import { createMemo, For, Show } from 'solid-js';
import { t } from '../i18n';
import type { WarningsReport } from '../state';
import type { View } from '../view';
import { Sheet } from './Sheet';

export function WarningsSheet(props: { view: View }) {
  // once read
  const open = createMemo(() => props.view.snapshot().matches({ ready: { panel: { warnings: 'shown' } } }));

  return (
    <Sheet open={open()} wide onClose={() => props.view.send({ type: 'ui.closePanel' })}>
      <h2>{t('Read latest upload warnings report')}</h2>
      <div class="table-scroll">
        <Report report={props.view.snapshot().context.report} />
      </div>
      <div class="actions">
        <button type="button" class="button primary" autofocus onClick={() => props.view.send({ type: 'ui.closePanel' })}>
          {t('Close')}
        </button>
      </div>
    </Sheet>
  );
}

function Report(props: { report: WarningsReport }) {
  // columns : the keys of the first track
  const columns = createMemo(() => Object.keys(props.report[0] ?? {}));

  return (
    <Show
      when={props.report.length > 0}
      fallback={
        <div class="congrats">
          <strong>{t('No warnings found')}</strong>
          <span>{t('Congratulations, your latest export was flawless !')}</span>
        </div>
      }
    >
      <table>
        <thead>
          <tr>
            <For each={columns()}>{column => <th>{column}</th>}</For>
          </tr>
        </thead>
        <tbody>
          <For each={props.report}>
            {track => (
              <tr>
                <For each={columns()}>
                  {column => (
                    <Show when={track[column]} fallback={<td class="missing">{`⚠ ${t('Missing !')}`}</td>}>
                      {value => <td title={value()}>{value()}</td>}
                    </Show>
                  )}
                </For>
              </tr>
            )}
          </For>
        </tbody>
      </table>
    </Show>
  );
}
