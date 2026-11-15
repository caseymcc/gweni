//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  hud_example: HudExample (examples/hud/hud.cpp), gweni drawn as a HUD over
//  the app's own GL scene, captured headless (--headless --stats-overlay
//  --screenshot) three times: twice over its default scene colour and once
//  over another (--clear). It fails unless
//    - the two default captures are byte for byte the same PNG;
//    - the scene probe (clear of the cube and of every control) is the scene's
//      colour exactly, in both scenes: the canvas draws no background;
//    - the Button probe (inside the Spin button's border) is the same pixel in
//      both scenes and neither scene's colour: the Button is drawn opaque over
//      the scene, clearDepth() having cleared the scene's depth;
//    - the tag probe (inside the Cube tag, a Button the app places over the
//      cube's centre) is the Faster button's probe pixel, the same look at
//      the same place in a Button of the same size: the cube's depth hides
//      no control (without clearDepth() the cube's face shows there);
//    - the panel probe (the panel's bottom padding, over the bare scene) is the
//      panel colour blended over each scene's colour at the panel's alpha,
//      each channel within 2 and between the two colours, and differs between
//      the scenes: the scene shows through the translucent panel.
//  The probes and colours are the line HudExample prints with its PNG.
//
//  node tests/hudExample.mjs <HudExample> <scratch dir>
//

import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, rmSync } from 'node:fs';
import { join } from 'node:path';

import { decodePng } from './web/png.mjs';

function fail(message)
{
    console.error(`hudExample.mjs: FAIL: ${message}`);
    process.exit(1);
}

const [exe, scratch] = process.argv.slice(2);
if(!exe || !scratch)
    fail('usage: node tests/hudExample.mjs <HudExample> <scratch dir>');

rmSync(scratch, { recursive: true, force: true });
mkdirSync(scratch, { recursive: true });

// one capture: its PNG decoded and the values of its "HudExample:" line
function capture(name, extra)
{
    const png = join(scratch, `${name}.png`);
    const r = spawnSync(exe, ['--headless', '--stats-overlay', '--screenshot', png, ...extra],
        { encoding: 'utf8' });
    process.stdout.write(r.stdout ?? '');
    process.stderr.write(r.stderr ?? '');
    if(r.status !== 0)
        fail(`${name}: HudExample exited ${r.status ?? r.signal}`);
    if(r.stderr)
        fail(`${name}: HudExample wrote to stderr`);

    const line = r.stdout.split('\n').find((l) => l.startsWith('HudExample: clear='));
    if(!line)
        fail(`${name}: no "HudExample: clear=" line`);
    const values = {};
    for(const pair of line.slice('HudExample: '.length).split(' '))
    {
        const [key, value] = pair.split('=');
        values[key] = value.split(',').map(Number);
    }
    const bytes = readFileSync(png);
    return { name, bytes, image: decodePng(bytes), values };
}

function pixel(shot, key)
{
    const [x, y] = shot.values[key];
    const { image } = shot;
    if(x < 0 || y < 0 || x >= image.width || y >= image.height)
        fail(`${shot.name}: ${key} ${x},${y} is outside the ${image.width}x${image.height} PNG`);
    const p = (y*image.width + x)*image.channels;
    return [image.pixels[p], image.pixels[p + 1], image.pixels[p + 2]];
}

const same = (a, b) => a.every((v, i) => v === b[i]);
const text = (c) => c.join(',');

const first = capture('default', []);
const second = capture('default_again', []);
const other = capture('other_scene', ['--clear', '200,180,40']);

if(!first.bytes.equals(second.bytes))
    fail('the two default captures differ');

for(const shot of [first, other])
{
    const clear = shot.values.clear;
    const scene = pixel(shot, 'probe_scene');
    if(!same(scene, clear))
        fail(`${shot.name}: the scene probe is ${text(scene)}, not the scene's ${text(clear)}`);

    const button = pixel(shot, 'probe_button');
    if(same(button, clear))
        fail(`${shot.name}: the Button probe is the scene's colour ${text(clear)}: the UI is hidden`);

    const faster = pixel(shot, 'probe_faster');
    const tag = pixel(shot, 'probe_tag');
    if(!same(tag, faster))
        fail(`${shot.name}: the Cube tag's probe is ${text(tag)}, not the Faster button's`
            + ` ${text(faster)}: the cube's depth hides the tag`);

    const [r, g, b, a] = shot.values.panel_color;
    const tint = [r, g, b];
    const panel = pixel(shot, 'probe_panel');
    for(let c = 0; c < 3; ++c)
    {
        const expected = tint[c]*a/255 + clear[c]*(1 - a/255);
        const low = Math.min(tint[c], clear[c]), high = Math.max(tint[c], clear[c]);
        if(Math.abs(panel[c] - expected) > 2 || panel[c] < low || panel[c] > high)
            fail(`${shot.name}: the panel probe is ${text(panel)}; channel ${c} should be`
                + ` ${expected.toFixed(1)}, ${text(tint)} at alpha ${a} over ${text(clear)}`);
    }
    console.log(`${shot.name}: scene ${text(scene)}, button ${text(button)}, tag ${text(tag)}, panel ${text(panel)}`);
}

if(!same(pixel(first, 'probe_button'), pixel(other, 'probe_button')))
    fail(`the Button probe differs between the scenes (${text(pixel(first, 'probe_button'))}`
        + ` and ${text(pixel(other, 'probe_button'))}): it is not drawn opaque over the scene`);
if(same(pixel(first, 'probe_panel'), pixel(other, 'probe_panel')))
    fail('the panel probe is the same over both scenes: nothing shows through it');

console.log('hudExample.mjs: ok');
