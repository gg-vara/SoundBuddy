// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// What the music app plays : a new title springs in, the position goes on while playing
import { Music } from 'lucide';
import { createEffect, createMemo, createSignal } from 'solid-js';
import { animate, bouncy } from '../animations';
import { t } from '../i18n';
import { isTrack, type Shout, trackProgress } from '../state';
import { Icon } from './Icon';

export function NowPlaying(props: { shout: Shout | null }) {
  const title = createMemo(() => (isTrack(props.shout) ? props.shout.name : t('Nothing playing')));
  const details = () => {
    const shout = props.shout;
    if (!isTrack(shout)) return '';
    return [shout.artist, shout.album, shout.year || null].filter(Boolean).join(' · ');
  };
  const playing = createMemo(() => isTrack(props.shout) && props.shout.playerState === true);

  // time since the last shout, ticking while playing
  const [elapsed, setElapsed] = createSignal(0);
  createEffect(
    () => [props.shout, playing()] as const,
    ([, isPlaying]) => {
      setElapsed(0);
      if (!isPlaying) return;
      const received = Date.now();
      const timer = setInterval(() => setElapsed(Date.now() - received), 1000);
      return () => clearInterval(timer);
    },
  );

  // only the arrival of a new title animates : the content is never left hidden
  let meta!: HTMLDivElement;
  let cover!: HTMLDivElement;
  createEffect(
    title,
    () => {
      animate(
        meta,
        [
          { opacity: 0, transform: 'translateY(10px)' },
          { opacity: 1, transform: 'none' },
        ],
        bouncy,
      );
      animate(cover, [{ transform: 'scale(.8) rotate(-8deg)' }, { transform: 'none' }], bouncy);
    },
    { defer: true },
  );

  return (
    <div class="now" title={isTrack(props.shout) ? `${title()} — ${t(playing() ? 'playing' : 'paused')}` : ''}>
      <div class="cover" ref={cover}>
        <Icon icon={Music} />
      </div>
      <div class="meta" ref={meta}>
        <div class="title">{title()}</div>
        <div class="sub">{details()}</div>
        <div class="progress">
          <div style={{ width: `${(trackProgress(props.shout, elapsed()) * 100).toFixed(2)}%` }} />
        </div>
      </div>
      <div class={{ bars: true, on: playing() }} aria-hidden="true">
        <i />
        <i />
        <i />
        <i />
      </div>
    </div>
  );
}
