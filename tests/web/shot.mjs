//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  web_controltest_shot, web_openglcore_screenshot: a capture in the browser
//  writes a PNG of the size it was asked for that is not one flat colour.
//
//      node tests/web/shot.mjs --size <W>x<H> --png <name> [run.mjs options] <page.html> -- <args>
//
//  Runs the page through run.mjs (runPage, with --out a scratch directory if
//  none is given), then fails, with one "shot.mjs: FAIL: ..." line, unless the
//  run exited 0 and wrote /out/<name>, a PNG W x H pixels, with more than one
//  colour in it: a context that cleared and drew nothing, or a readback of the
//  wrong buffer, is a single colour.
//

import { mkdtempSync, readFileSync, realpathSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { inflateSync } from 'node:zlib';

import { parseArgs, runPage } from './run.mjs';

function fail(message)
{
    console.error(`shot.mjs: FAIL: ${message}`);
    return 1;
}

// width, height, bytes per pixel and the unfiltered rows of an 8-bit RGB or
// RGBA PNG (what capture::writeRgbaPng writes); throws on anything else
export function decodePng(bytes)
{
    if(bytes.readUInt32BE(0) !== 0x89504e47)
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

async function main()
{
    const argv = process.argv.slice(2);
    let size = null, png = null;
    for(;;)
    {
        if(argv[0] === '--size' && argv.length > 1)
        {
            size = /^(\d+)x(\d+)$/.exec(argv[1]);
            argv.splice(0, 2);
        }
        else if(argv[0] === '--png' && argv.length > 1)
        {
            png = argv[1];
            argv.splice(0, 2);
        }
        else
            break;
    }
    const options = parseArgs(argv);
    if(!size || !png || !options)
    {
        console.error('usage: node shot.mjs --size <W>x<H> --png <name> [run.mjs options] <page.html> -- <args>');
        return 1;
    }
    if(!options.out)
        options.out = mkdtempSync(join(tmpdir(), 'gweni-web-shot-'));

    const status = await runPage(options);
    if(status !== 0)
        return fail(`the run exited ${status}`);

    let image;
    try
    {
        image = decodePng(readFileSync(join(options.out, png)));
    }
    catch(e)
    {
        return fail(`${png}: ${e.message}`);
    }
    const [width, height] = [Number(size[1]), Number(size[2])];
    if(image.width !== width || image.height !== height)
        return fail(`${png} is ${image.width}x${image.height}, not ${width}x${height}`);

    const first = image.pixels.subarray(0, image.channels);
    let colours = 1;
    for(let i = image.channels; i < image.pixels.length && colours === 1; i += image.channels)
    {
        if(image.pixels.compare(first, 0, image.channels, i, i + image.channels) !== 0)
            colours = 2;
    }
    if(colours === 1)
        return fail(`${png} is one flat colour (${[...first].join(',')})`);

    console.log(`shot.mjs: OK, ${png} ${width}x${height}, not one colour`);
    return 0;
}

// Only when run as a script (see run.mjs), so site.mjs can import decodePng.
if(process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href)
    process.exit(await main());
