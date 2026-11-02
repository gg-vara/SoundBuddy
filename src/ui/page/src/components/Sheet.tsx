// Modal sheet : a native <dialog>, shown by the state machine. The page closes it on Escape or a click on its
// backdrop, and tells the state machine; focus stays inside while it is open
import type { JSX } from '@solidjs/web';
import { createEffect } from 'solid-js';

export function Sheet(props: { open: boolean; wide?: boolean; onClose: () => void; children: JSX.Element }) {
  let dialog!: HTMLDialogElement;

  createEffect(
    () => props.open,
    open => {
      if (open && !dialog.open) dialog.showModal();
      if (!open && dialog.open) dialog.close();
    },
  );

  return (
    <dialog
      class={['sheet', { wide: props.wide }]}
      ref={dialog}
      // closed by the page : Escape, or a click on the backdrop (the dialog itself, outside its content)
      onClose={() => props.onClose()}
      onClick={event => {
        if (event.target === dialog) dialog.close();
      }}
    >
      <div class="sheet-content">{props.children}</div>
    </dialog>
  );
}
