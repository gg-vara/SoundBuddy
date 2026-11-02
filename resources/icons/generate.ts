// Generates the icons of SoundBuddy from svg/ : each size is rendered from the drawing, rather than scaled from one image.
// Run "bun run icons" from this folder after changing a drawing, then commit what it wrote.
import { Resvg } from '@resvg/resvg-js';

const folder = import.meta.dir;

// "file" of svg/ rendered as a PNG of "size" pixels, its currentColor being "color"
async function render(file: string, size: number, color = '#000'): Promise<Buffer> {
  const svg = (await Bun.file(`${folder}/svg/${file}`).text()).replaceAll('currentColor', color);
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

async function icoOf(file: string, sizes: number[], color?: string): Promise<Buffer> {
  return ico(await Promise.all(sizes.map(async size => ({ size, png: await render(file, size, color) }))));
}

// the sizes Windows shows, at 100 to 400 % scaling : small icons (16 px), taskbar (24 px), large icons (32 to 256 px)
const appSizes = [16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 128, 256];
const traySizes = [16, 20, 24, 28, 32, 40, 48, 64];

const outputs: Record<string, () => Promise<Buffer>> = {
  // Windows : the executable and its window, the tray on dark and light taskbars, the installer
  'app.ico': () => icoOf('app.svg', appSizes),
  'tray_dark.ico': () => icoOf('note.svg', traySizes, '#fff'),
  'tray_light.ico': () => icoOf('note.svg', traySizes, '#000'),
  'package.ico': () => icoOf('package.svg', appSizes),
  // macOS : the bundle, the installer, the Qt window and menu bar
  'app.icns': () => icns('app.svg'),
  'package.icns': () => icns('package.svg'),
  'app.png': () => render('app.svg', 512),
  'app_reverse.png': () => render('note.svg', 512),
};

for (const [name, generate] of Object.entries(outputs)) {
  const data = await generate();
  await Bun.write(`${folder}/${name}`, data);
  console.log(`${name} : ${(data.length / 1024).toFixed(1)} KB`);
}
