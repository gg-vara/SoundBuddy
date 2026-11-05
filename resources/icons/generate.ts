// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Generates the icons of SoundBuddy from svg/ into build/, not versioned : each size is rendered from the drawing, rather than
// scaled from one image. The build runs it when needed (cmake/Icons.cmake, which lists these outputs).
import { Resvg } from '@resvg/resvg-js';

const folder = import.meta.dir;

// "svg" with a dot of "color" in its corner, a gap around it keeping it apart from the drawing on any background
function withDot(svg: string, color: string): string {
  const content = svg.replace(/^[\s\S]*?<svg[^>]*>/, '').replace(/<\/svg>\s*$/, '');
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">
    <mask id="gap"><rect width="128" height="128" fill="#fff" /><circle cx="110" cy="110" r="22" fill="#000" /></mask>
    <g mask="url(#gap)">${content}</g>
    <circle cx="110" cy="110" r="16" fill="${color}" />
  </svg>`;
}

// "file" of svg/ rendered as a PNG of "size" pixels, its currentColor being "color"
async function render(file: string, size: number, color = '#000', dot?: string): Promise<Buffer> {
  const drawing = (await Bun.file(`${folder}/svg/${file}`).text()).replaceAll('currentColor', color);
  const svg = dot ? withDot(drawing, dot) : drawing;
  const rendered = new Resvg(svg, { fitTo: { mode: 'width', value: size }, background: 'rgba(0, 0, 0, 0)' }).render();
  return Buffer.from(rendered.asPng());
}

// Windows icon : one PNG per size (Windows Vista and later)
function ico(images: { size: number; png: Buffer }[]): Buffer {
  const header = Buffer.alloc(6);
  header.writeUInt16LE(1, 2); // icon
  header.writeUInt16LE(images.length, 4);

  let offset = 6 + 16 * images.length;
  const entries = images.map(({ size, png }) => {
    const entry = Buffer.alloc(16);
    entry.writeUInt8(size % 256, 0); // 0 means 256
    entry.writeUInt8(size % 256, 1);
    entry.writeUInt16LE(1, 4); // color planes
    entry.writeUInt16LE(32, 6); // bits per pixel
    entry.writeUInt32LE(png.length, 8);
    entry.writeUInt32LE(offset, 12);
    offset += png.length;
    return entry;
  });

  return Buffer.concat([header, ...entries, ...images.map(image => image.png)]);
}

// macOS icon : PNG images, typed by size (the @2x ones for Retina screens)
const icnsTypes: [type: string, size: number][] = [
  ['icp4', 16],
  ['icp5', 32],
  ['icp6', 64],
  ['ic07', 128],
  ['ic08', 256],
  ['ic09', 512],
  ['ic10', 1024],
  ['ic11', 32],
  ['ic12', 64],
  ['ic13', 256],
  ['ic14', 512],
];

async function icns(file: string): Promise<Buffer> {
  const chunks = await Promise.all(
    icnsTypes.map(async ([type, size]) => {
      const png = await render(file, size);
      const chunk = Buffer.alloc(8);
      chunk.write(type, 0, 'ascii');
      chunk.writeUInt32BE(8 + png.length, 4);
      return Buffer.concat([chunk, png]);
    }),
  );

  const header = Buffer.alloc(8);
  header.write('icns', 0, 'ascii');
  header.writeUInt32BE(8 + chunks.reduce((total, chunk) => total + chunk.length, 0), 4);
  return Buffer.concat([header, ...chunks]);
}

async function icoOf(file: string, sizes: number[], color?: string, dot?: string): Promise<Buffer> {
  return ico(await Promise.all(sizes.map(async size => ({ size, png: await render(file, size, color, dot) }))));
}

// the sizes Windows shows, at 100 to 400 % scaling : small icons (16 px), taskbar (24 px), large icons (32 to 256 px)
const appSizes = [16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 128, 256];
const traySizes = [16, 20, 24, 28, 32, 40, 48, 64];

// the colors of the connectivity in the user interface (--ok and --issue, src/ui/page/src/style.css)
const dots = { '': undefined, _ok: '#2f9e62', _issue: '#d23f3f' };
const trayBases = { dark: ['note.svg', '#fff'], light: ['note.svg', '#000'], active: ['note-active.svg', undefined] } as const;

const trayIcons = Object.entries(trayBases).flatMap(([name, [file, color]]) =>
  Object.entries(dots).map(([suffix, dot]) => [`tray_${name}${suffix}.ico`, () => icoOf(file, traySizes, color, dot)] as const),
);

const outputs: Record<string, () => Promise<Buffer>> = {
  // Windows : the executable and its window, the tray on dark and light taskbars, the installer
  'app.ico': () => icoOf('app.svg', appSizes),
  ...Object.fromEntries(trayIcons),
  'package.ico': () => icoOf('package.svg', appSizes),
  // macOS : the bundle, the installer, the menu bar item (a template image)
  'app.icns': () => icns('app-macos.svg'),
  'package.icns': () => icns('package.svg'),
  'app_reverse.png': () => render('note.svg', 512),
  'app_reverse_active.png': () => render('note-active.svg', 512),
};

for (const [name, generate] of Object.entries(outputs)) {
  const data = await generate();
  await Bun.write(`${folder}/build/${name}`, data);
  console.log(`${name} : ${(data.length / 1024).toFixed(1)} KB`);
}
