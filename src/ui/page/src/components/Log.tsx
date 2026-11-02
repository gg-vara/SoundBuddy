// Messages of a worker : new ones spring in, an issue shakes; a progress replaces the last one in place
import { createMemo, For, onSettled, Show, untrack } from 'solid-js';
import { animate, bouncy } from '../animations';
import { t } from '../i18n';
import type { LogMessage } from '../state';

export function Log(props: { messages: LogMessage[]; language: string }) {
  let container!: HTMLDivElement;

  // the messages already there when shown do not animate
  let shown = false;
  onSettled(() => {
    container.scrollTop = container.scrollHeight;
    shown = true;
  });

  const timeFormat = createMemo(() => new Intl.DateTimeFormat(props.language, { hour: '2-digit', minute: '2-digit', second: '2-digit' }));

  return (
    <div class="log" ref={container}>
      <Show when={props.messages.length > 0} fallback={<div class="empty">{t('No messages yet')}</div>}>
        <For each={props.messages} keyed={false}>
          {message => <Entry message={message()} time={timeFormat().format(message().time)} container={container} animated={shown} />}
        </For>
      </Show>
    </div>
  );
}

function Entry(props: { message: LogMessage; time: string; container: HTMLDivElement; animated: boolean }) {
  let entry!: HTMLDivElement;

  onSettled(() => {
    const { animated, container, kind } = untrack(() => ({
      animated: props.animated,
      container: props.container,
      kind: props.message.kind,
    }));
    if (!animated) return;

    // follows the end of the log, unless scrolled up
    const following = container.scrollHeight - entry.offsetHeight - container.scrollTop - container.clientHeight < 80;
    if (following) container.scrollTo({ top: container.scrollHeight, behavior: 'smooth' });

    animate(
      entry,
      [
        { opacity: 0, transform: 'translateY(14px) scale(.96)' },
        { opacity: 1, transform: 'none' },
      ],
      bouncy,
    );
    if (kind === 'issue') {
      animate(
        entry,
        [
          { transform: 'translateX(0)' },
          { transform: 'translateX(-6px)' },
          { transform: 'translateX(5px)' },
          { transform: 'translateX(-3px)' },
          { transform: 'translateX(0)' },
        ],
        { duration: 380, delay: 120, easing: 'ease-in-out', composite: 'add' },
      );
    }
  });

  return (
    <div class="msg" data-kind={props.message.kind} ref={entry}>
      <span class="time">{props.time}</span>
      <span class="text">{props.message.text}</span>
    </div>
  );
}
