// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Texts of the page, translated by the application (src/i18n/*.inc) : t() only takes the texts listed here
import { createSignal } from 'solid-js';

export const texts = [
  '%1 connectivity',
  'About %1',
  'Access upload data folder',
  'Automation',
  'Build',
  'Cancel',
  'Check for updates',
  'Close',
  'Congratulations, your latest export was flawless !',
  'Connect to %1',
  'Data folder',
  'Debug',
  'Error while checking updates',
  'Generate Digest and Upload',
  'Hide password',
  'Host URL',
  'Launch %1 at system boot',
  'License',
  'Missing !',
  'More',
  'My %1',
  'No messages yet',
  'No warnings found',
  'Nothing playing',
  'Password',
  'Password (empty keeps your login)',
  'Preferences',
  'Production',
  'Quit',
  'Read latest upload warnings report',
  'Save',
  'Searching for updates...',
  'Show password',
  'Source code',
  'Start shouting at app launch',
  'Stop',
  'System',
  'Up to date.',
  'Updates',
  'Updates are only available for installed versions of %1.',
  'Username',
  'Version',
  'Version %1 is ready : it will be installed at the next launch.',
  'paused',
  'playing',
] as const;

export type Text = (typeof texts)[number];

const [translations, setTranslations] = createSignal<Record<string, string>>({});

export { setTranslations };

// "text" in the user interface language, with %1, %2... replaced by "args"
export function t(text: Text, ...args: (string | number)[]): string {
  return format(translations()[text] ?? text, args);
}

export function format(text: string, args: (string | number)[]): string {
  return text.replace(/%(\d)/g, (match, index: string) => String(args[Number(index) - 1] ?? match));
}
