/* Render Apache-2.0 Material icons into small, antialiased LVGL assets.
   Requires Node.js and sharp; sources and LICENSE stay with the assets. */
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
const dir = path.join(root, 'assets/images/excuse_call');
(async () => {
  let c = '/* Generated Material icons. See LICENSE and README.md in this directory. */\n#include "lvgl.h"\n';
  for (const [name, size, color] of [['call', 32, '#FFFFFF'], ['person', 56, '#7893A7']]) {
    const svg = fs.readFileSync(path.join(dir, name + '.svg'), 'utf8').replace('<svg ', '<svg fill="' + color + '" ');
    const large = await sharp(Buffer.from(svg)).resize(size * 4, size * 4).png().toBuffer();
    const icon = sharp(large).resize(size, size).ensureAlpha();
    await icon.clone().png().toFile(path.join(dir, name + '.png'));
    const rgba = await icon.raw().toBuffer();
    const bgra = Buffer.alloc(rgba.length);
    for (let i = 0; i < rgba.length; i += 4) {
      bgra[i] = rgba[i + 2]; bgra[i + 1] = rgba[i + 1]; bgra[i + 2] = rgba[i]; bgra[i + 3] = rgba[i + 3];
    }
    c += `static const uint8_t ${name}_pixels[] = {\n`;
    for (let i = 0; i < bgra.length; i += 24) c += [...bgra.subarray(i, i + 24)].join(',') + ',\n';
    c += `};\nconst lv_image_dsc_t ec_${name}_icon = {\n .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888, .w = ${size}, .h = ${size}, .stride = ${size * 4}},\n .data_size = sizeof(${name}_pixels), .data = ${name}_pixels\n};\n`;
  }
  fs.writeFileSync(path.join(dir, 'excuse_call_icons.c'), c);
})();
