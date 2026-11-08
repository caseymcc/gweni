//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The PNGs the captures write, in and out of node, with nothing but node's own
//  zlib: decodePng reads the 8-bit RGB/RGBA, non-interlaced PNGs that
//  capture::writeRgbaPng writes, encodePng writes an 8-bit RGBA one (pngdiff.mjs'
//  diff images). shot.mjs, site.mjs and pngdiff.mjs share them.
//

import { deflateSync, inflateSync } from 'node:zlib';

// width, height, bytes per pixel and the unfiltered rows of an 8-bit RGB or
// RGBA PNG (what capture::writeRgbaPng writes); throws on anything else
export function decodePng(bytes)
{
    if(bytes.length < 8 || bytes.readUInt32BE(0) !== 0x89504e47)
        throw new Error('not a PNG');
    let offset = 8, width = 0, height = 0, channels = 0;
    const idat = [];
    while(offset < bytes.length)
    {
        const length = bytes.readUInt32BE(offset);
        const type = bytes.toString('latin1', offset + 4, offset + 8);
        const chunk = bytes.subarray(offset + 8, offset + 8 + length);
        if(type === 'IHDR')
        {
            width = chunk.readUInt32BE(0);
            height = chunk.readUInt32BE(4);
            const depth = chunk[8], colourType = chunk[9];
            channels = { 2: 3, 6: 4 }[colourType];
            if(depth !== 8 || !channels || chunk[12] !== 0)
                throw new Error(`unsupported PNG: depth ${depth}, colour type ${colourType}, interlace ${chunk[12]}`);
        }
        else if(type === 'IDAT')
            idat.push(chunk);
        offset += 12 + length;
    }
    const raw = inflateSync(Buffer.concat(idat));
    const stride = width*channels;
    const pixels = Buffer.alloc(stride*height);
    for(let y = 0; y < height; ++y)
    {
        const filter = raw[y*(stride + 1)];
        const line = raw.subarray(y*(stride + 1) + 1, (y + 1)*(stride + 1));
        for(let x = 0; x < stride; ++x)
        {
            const a = x >= channels ? pixels[y*stride + x - channels] : 0;
            const b = y > 0 ? pixels[(y - 1)*stride + x] : 0;
            const c = (x >= channels && y > 0) ? pixels[(y - 1)*stride + x - channels] : 0;
            let predictor = 0;
            if(filter === 1) predictor = a;
            else if(filter === 2) predictor = b;
            else if(filter === 3) predictor = (a + b) >> 1;
            else if(filter === 4)
            {
                const pa = Math.abs(b - c), pb = Math.abs(a - c), pc = Math.abs(a + b - 2*c);
                predictor = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
            }
            pixels[y*stride + x] = (line[x] + predictor) & 255;
        }
    }
    return { width, height, channels, pixels };
}

const crcTable = (() =>
{
    const table = new Uint32Array(256);
    for(let n = 0; n < 256; ++n)
    {
        let c = n;
        for(let k = 0; k < 8; ++k)
            c = (c & 1) ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        table[n] = c >>> 0;
    }
    return table;
})();

function crc32(buffer)
{
    let c = 0xffffffff;
    for(const byte of buffer)
        c = crcTable[(c ^ byte) & 255] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
}

function chunk(type, data)
{
    const head = Buffer.alloc(8);
    head.writeUInt32BE(data.length, 0);
    head.write(type, 4, 'latin1');
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])), 0);
    return Buffer.concat([head, data, crc]);
}

// an 8-bit RGBA PNG of width x height pixels (4 bytes each, rows top-down),
// every row unfiltered
export function encodePng(width, height, rgba)
{
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header[8] = 8;      // bit depth
    header[9] = 6;      // RGBA
    const stride = width*4;
    const raw = Buffer.alloc((stride + 1)*height);
    for(let y = 0; y < height; ++y)
        rgba.copy(raw, y*(stride + 1) + 1, y*stride, (y + 1)*stride);
    return Buffer.concat([
        Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
        chunk('IHDR', header),
        chunk('IDAT', deflateSync(raw)),
        chunk('IEND', Buffer.alloc(0)),
    ]);
}
