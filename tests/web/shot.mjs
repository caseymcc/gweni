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

import { mkdtempSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { decodePng } from './png.mjs';
import { parseArgs, runPage } from './run.mjs';

function fail(message)
{
    console.error(`shot.mjs: FAIL: ${message}`);
    return 1;
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

process.exit(await main());
