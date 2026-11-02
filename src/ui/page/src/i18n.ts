// Texts of the page, translated by the application (src/i18n/*.inc) : t() only takes the texts listed here
import { createSignal } from 'solid-js';

export const texts = [
  '%1 connectivity',
  'Access upload data folder',
  'Automation',
  'Cancel',
  'Check for updates',
  'Close',
  'Congratulations, your latest export was flawless !',
  'Connect to %1',
  'Generate Digest and Upload',
  'Hide password',
  'Host URL',
  'Launch %1 at system boot',
  'Missing !',
  'More',
  'My %1',
  'No messages yet',
  'No warnings found',
  'Nothing playing',
  'Password',
  'Preferences',
  'Quit',
  'Read latest upload warnings report',
  'Save',
  'Searching for updates...',
  'Show password',
  'Start shouting at app launch',
  'Stop',
  'Username',
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
