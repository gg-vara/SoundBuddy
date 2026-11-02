// Animations : springs, sampled into CSS linear() easings, for the Web Animations API and CSS transitions

export interface Timing {
  easing: string;
  duration: number;
}

// a damped oscillator, from 0 to 1, as Motion's springs
export function spring({ stiffness, damping, mass = 1 }: { stiffness: number; damping: number; mass?: number }): Timing {
  const w0 = Math.sqrt(stiffness / mass);
  const zeta = damping / (2 * Math.sqrt(stiffness * mass));
  const wd = w0 * Math.sqrt(Math.max(1 - zeta * zeta, 1e-6));
  const at = (time: number) =>
    zeta < 1
      ? 1 - Math.exp(-zeta * w0 * time) * (Math.cos(wd * time) + ((zeta * w0) / wd) * Math.sin(wd * time))
      : 1 - Math.exp(-w0 * time) * (1 + w0 * time);

  // settled once close to 1 for a while
  let duration = 0;
  while (duration < 4 && !(Math.abs(1 - at(duration)) < 1e-3 && Math.abs(1 - at(duration + 0.1)) < 1e-3)) duration += 0.02;

  const points = Array.from({ length: 61 }, (_, i) => at((duration * i) / 60).toFixed(4));
  return { easing: `linear(${points.join(', ')})`, duration: duration * 1000 };
}

export const snappy = spring({ stiffness: 320, damping: 24 });
export const bouncy = spring({ stiffness: 220, damping: 14 });

const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');

// an animation leaving no style behind once finished (no fill)
export function animate(element: Element, keyframes: Keyframe[], options: KeyframeAnimationOptions): void {
  if (reducedMotion.matches) return;
  element.animate(keyframes, { ...options, fill: 'none' });
}
