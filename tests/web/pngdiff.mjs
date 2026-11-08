//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  Compares two sets of captures within a tolerance, one rule per PNG: they
//  must be the same size, no channel of any pixel may differ by more than T,
//  and at most F percent of the pixels may differ at all. It is the rule a
//  second backend is held to against the first where byte identity is not the
//  bar (web_pixels_native: the browser's SwiftShader against the native GLES
//  tree's llvmpipe; the Vulkan feature, #368, states its own the same way), so
//  any such test can share this script. It needs node alone (no Playwright).
//
//      node tests/web/pngdiff.mjs [--max-delta T] [--max-fraction F] [--count-over K]
//                                 [--diff-dir <dir>] [--quiet] <expected dir> <actual dir>
//
//  Every *.png in <expected dir> is compared with the one of the same name in
//  <actual dir>, and the two dirs must hold the same PNGs. F is a percentage
//  (0.5 means 0.5 %) of the pixels whose max |Δ| is over K (default 0: every
//  pixel that differs at all; 1 leaves out the 1-level differences that two
//  rasterisers' rounding of a blend or a colour conversion gives). Without --max-delta/--max-fraction it only measures: one
//  line per PNG with its max |Δ| and the share of pixels that differ, and the
//  worst of each over the set, and exits 0 if the sizes and the file sets match.
//
//  With a bound, it fails (exit 1, one "pngdiff.mjs: FAIL: ..." line per PNG
//  over it, naming the PNG and the measured values) on any PNG over either
//  bound, and writes <diff dir>/<name>.diff.png for each: the expected image,
//  the actual one and a map of the difference side by side (a pixel that
//  differs in red, brighter for a larger |Δ|, over a dimmed grey of the
//  expected one), printing its path. <diff dir> defaults to <actual dir>/diff.
//

import { mkdirSync, readdirSync, readFileSync, realpathSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

import { decodePng, encodePng } from './png.mjs';

const usage = 'usage: node pngdiff.mjs [--max-delta T] [--max-fraction F] [--count-over K] [--diff-dir <dir>] [--quiet] <expected dir> <actual dir>';

// RGBA of pixel i of a decoded PNG (alpha 255 for an RGB one)
function rgba(image, i, out)
{
    const p = i*image.channels;
    out[0] = image.pixels[p];
    out[1] = image.pixels[p + 1];
    out[2] = image.pixels[p + 2];
    out[3] = image.channels === 4 ? image.pixels[p + 3] : 255;
}

// What two decoded PNGs differ by: null if their sizes differ, otherwise the
// largest per-channel |Δ|, the count of pixels whose largest per-channel |Δ| is
// over countOver (any difference at 0) and the per-pixel max |Δ| (for the diff
// image).
export function compareImages(expected, actual, countOver = 0)
{
    if(expected.width !== actual.width || expected.height !== actual.height)
        return null;
    const total = expected.width*expected.height;
    const deltas = new Uint8Array(total);
    const e = [0, 0, 0, 0], a = [0, 0, 0, 0];
    let maxDelta = 0, differing = 0;
    for(let i = 0; i < total; ++i)
    {
        rgba(expected, i, e);
        rgba(actual, i, a);
        let d = 0;
        for(let c = 0; c < 4; ++c)
            d = Math.max(d, Math.abs(e[c] - a[c]));
        deltas[i] = d;
        if(d > countOver)
            ++differing;
        if(d > maxDelta)
            maxDelta = d;
    }
    return { total, maxDelta, differing, fraction: 100*differing/total, deltas };
}

// expected | actual | difference, one image
function diffImage(expected, actual, deltas)
{
    const { width, height } = expected;
    const out = Buffer.alloc(width*3*height*4);
    const px = [0, 0, 0, 0];
    for(let y = 0; y < height; ++y)
    {
        for(let x = 0; x < width; ++x)
        {
            const i = y*width + x;
            const row = (y*width*3 + x)*4;
            rgba(expected, i, px);
            out.set([px[0], px[1], px[2], 255], row);
            const grey = Math.round((px[0] + px[1] + px[2])/3/4);
            rgba(actual, i, px);
            out.set([px[0], px[1], px[2], 255], row + width*4);
            const d = deltas[i];
            out.set(d ? [Math.min(255, 128 + 32*d), 0, 0, 255] : [grey, grey, grey, 255], row + width*8);
        }
    }
    return encodePng(width*3, height, out);
}

function pngsIn(dir)
{
    return readdirSync(dir).filter(name => name.endsWith('.png') && !name.endsWith('.diff.png')).sort();
}

// Compares the two dirs as the command line describes (see the header) and
// returns the exit status.
export function compareDirs({ expected, actual, maxDelta = null, maxFraction = null, countOver = 0, diffDir = null, quiet = false })
{
    const bounded = maxDelta !== null || maxFraction !== null;
    const differingKey = countOver ? `differing_over_${countOver}` : 'differing';
    const differingText = countOver ? `differ by more than ${countOver}` : 'differ';
    const names = pngsIn(expected);
    const actualNames = new Set(pngsIn(actual));
    const failures = [];
    let worstDelta = { value: 0, name: '-' }, worstFraction = { value: 0, name: '-' };
    if(names.length === 0)
        failures.push(`no PNG in ${expected}`);

    for(const name of names)
    {
        if(!actualNames.delete(name))
        {
            failures.push(`${name}: missing from ${actual}`);
            continue;
        }
        let e, a;
        try
        {
            e = decodePng(readFileSync(join(expected, name)));
            a = decodePng(readFileSync(join(actual, name)));
        }
        catch(err)
        {
            failures.push(`${name}: ${err.message}`);
            continue;
        }
        const result = compareImages(e, a, countOver);
        if(!result)
        {
            failures.push(`${name}: ${a.width}x${a.height}, expected ${e.width}x${e.height}`);
            continue;
        }
        const fraction = result.fraction.toFixed(3);
        if(!quiet)
            console.log(`pngdiff.mjs: ${name} max_delta=${result.maxDelta} ${differingKey}=${result.differing}/${result.total} (${fraction} %)`);
        if(result.maxDelta > worstDelta.value)
            worstDelta = { value: result.maxDelta, name };
        if(result.fraction > worstFraction.value)
            worstFraction = { value: result.fraction, name };

        const over = [];
        if(maxDelta !== null && result.maxDelta > maxDelta)
            over.push(`max |delta| ${result.maxDelta} > ${maxDelta}`);
        if(maxFraction !== null && result.fraction > maxFraction)
            over.push(`${fraction} % of pixels ${differingText} > ${maxFraction} %`);
        if(over.length)
        {
            const dir = diffDir ?? join(actual, 'diff');
            mkdirSync(dir, { recursive: true });
            const path = join(dir, name.replace(/\.png$/, '.diff.png'));
            writeFileSync(path, diffImage(e, a, result.deltas));
            failures.push(`${name}: ${over.join(', ')} (expected | actual | difference: ${path})`);
        }
    }
    for(const name of actualNames)
        failures.push(`${name}: in ${actual}, not in ${expected}`);

    console.log(`pngdiff.mjs: ${names.length} PNG(s): worst max |delta| ${worstDelta.value} (${worstDelta.name}), `
        + `worst share of pixels that ${differingText} ${worstFraction.value.toFixed(3)} % (${worstFraction.name})`
        + (bounded ? `; bound: max |delta| <= ${maxDelta ?? 'any'}, at most ${maxFraction ?? 'any'} % ${differingText}` : ''));
    for(const f of failures)
        console.error(`pngdiff.mjs: FAIL: ${f}`);
    if(failures.length)
        return 1;
    console.log(`pngdiff.mjs: OK`);
    return 0;
}

export function parseArgs(argv)
{
    const options = { maxDelta: null, maxFraction: null, countOver: 0, diffDir: null, quiet: false, expected: null, actual: null };
    const dirs = [];
    for(let i = 0; i < argv.length; ++i)
    {
        const arg = argv[i];
        if(arg === '--max-delta' && i + 1 < argv.length)
            options.maxDelta = Number(argv[++i]);
        else if(arg === '--max-fraction' && i + 1 < argv.length)
            options.maxFraction = Number(argv[++i]);
        else if(arg === '--count-over' && i + 1 < argv.length)
            options.countOver = Number(argv[++i]);
        else if(arg === '--diff-dir' && i + 1 < argv.length)
            options.diffDir = argv[++i];
        else if(arg === '--quiet')
            options.quiet = true;
        else if(!arg.startsWith('--'))
            dirs.push(arg);
        else
            return null;
    }
    if(dirs.length !== 2 || Number.isNaN(options.maxDelta) || Number.isNaN(options.maxFraction) || !(options.countOver >= 0))
        return null;
    [options.expected, options.actual] = dirs;
    return options;
}

if(process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href)
{
    const options = parseArgs(process.argv.slice(2));
    if(!options)
    {
        console.error(usage);
        process.exit(1);
    }
    process.exit(compareDirs(options));
}
