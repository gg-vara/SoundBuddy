// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// An icon of Lucide, drawn with currentColor. Each is imported by name from "lucide", so that only those used are bundled :
// its plain data rather than "lucide-solid", which does not support Solid 2
import type { IconNode } from 'lucide';

// from the library's own data, which holds no user text
const markup = (icon: IconNode) =>
  icon
    .map(([tag, attributes]) => {
      const attributesText = Object.entries(attributes)
        .map(([name, value]) => `${name}="${value}"`)
        .join(' ');
      return `<${tag} ${attributesText} />`;
    })
    .join('');

export function Icon(props: { icon: IconNode }) {
  return <svg viewBox="0 0 24 24" aria-hidden="true" innerHTML={markup(props.icon)} />;
}
