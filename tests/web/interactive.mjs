//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  web_input: the interactive pages, ControlTest's window and the OpenGLCore
//  demo, driven in the image's headless Chromium with real input events
//  (Playwright's mouse and keyboard, which the page receives as a person's).
//
//      node tests/web/interactive.mjs [--out <dir>] <dir holding ControlTest.html and OpenGLCore.html>
//
//  Each page runs its window (capture::initGlfwContext on emscripten-glfw, one
//  frame per animation frame through capture::runWindowLoop); the script
//  waits for frames to be drawn after each input (gweniRun.frames, web/pre.js),
//  reads the page's pixels as a screenshot of the canvas, and reads
//  ControlTest's state through what it exports (Module.ccall:
//  gweni_web_bounds_listing, the --dump-bounds listing of the window's canvas
//  as it is; gweni_web_focused_text) and OpenGLCore's (gweni_web_stats_overlay,
//  its stats overlay's state; gweni_web_depth_view, its depth view's). The
//  checks, each one "interactive.mjs: <check>: ok" line or a
//  "interactive.mjs: FAIL: <check>: ..." one:
//    button     ButtonDemo: a press on the "Text" button changes the pixels in
//               its bounds, and the release with the mouse moved off gives the
//               idle pixels back
//    typing     TextBoxDemo: a click on the first TextBox focuses it, and End,
//               "abc" and "é" (on no key of Playwright's US layout, so sent as
//               the browser's own key event, Input.dispatchKeyEvent, as a French
//               keyboard's "2" key sends it) leave its text as it was with
//               "abcé" after it
//    codeeditor CodeEditorDemo: a press on a line gives the editor the focus,
//               Tab puts spaces after the caret line's text and Ctrl+Z (after
//               "q" typed) undoes in the editor, the document's focus staying
//               where it was and both keydowns' defaults prevented by the
//               time they bubble to the window, so the browser moves no focus
//               and runs no undo of its own (read through
//               gweni_web_focused_text, a CodeEditor's caret line)
//    wheel      ScrollControlDemo: a wheel notch over the first ScrollControl
//               moves its inner panel up
//    cursor     CheckBoxDemo: the pointer over a LabelClickable makes the
//               canvas's CSS cursor "pointer", and off it "default"
//               (platform::setCursor on the web)
//    hidpi      ButtonDemo at devicePixelRatio 2 in a 640x480 CSS container: a
//               1280x960 canvas backing store, the canvas 640x480 logical and
//               its listing the one a capture at --size 640x480 --scale 2
//               lists in the same browser, and a press on "Text" changes its
//               pixels (the cursor mapped through the scale)
//    resize     the same page with the viewport grown so the container is
//               800x600 CSS: a 1600x1200 backing store and the listing of a
//               capture at --size 800x600 --scale 2
//    scale      ?scale=1.5 at devicePixelRatio 2 (an effective 3): a 1280x960
//               backing store, a 427x320 logical canvas (1280/3 rounded) and
//               the listing of a capture at --size 427x320 --scale 3
//    usage      ?bogus: the run ends with exit status 1 and ControlTest's
//               "unknown argument: --bogus" and usage on stderr, shown on the
//               page's #output panel
//    opengl     the OpenGLCore demo at 1024x768 CSS: a 1024x768 backing
//               store, and a click on the demo list's "Label" entry changes
//               the canvas
//    stats      OpenGLCore.html?stats-overlay at 1024x768 CSS: the overlay is
//               open and the shell's Stats button on; a click on the button
//               closes it and turns the button off, changing the canvas, and a
//               second click opens it and turns the button on again (read
//               through OpenGLCore's gweni_web_stats_overlay export)
//    depth      OpenGLCore.html at 1024x768 CSS: the depth view is closed and
//               the shell's Depth button off; a click on the button opens the
//               view and turns the button on, changing the canvas, and Escape
//               (the view has the mouse) closes it and turns the button off
//               (read through OpenGLCore's gweni_web_depth_view export)
//    clipboard  TextBoxDemo with clipboard-read and clipboard-write granted:
//               in the first box, Ctrl+A over "copy me" and "é" typed, then
//               Ctrl+C, puts that text on the browser's clipboard
//               (navigator.clipboard.readText() in the page), written in the
//               copy event (Module.gweniClipboard.writes); text the page
//               put on the clipboard (writeText) since, pasted with Ctrl+A
//               and Ctrl+V, is in the box in that keystroke, not the text
//               copied before it, and was answered by the page's paste event
//               (Module.gweniClipboard.answers: the headless shell fires a
//               native paste event for Playwright's Ctrl+V, so no synthetic
//               ClipboardEvent is dispatched; without one the check fails,
//               saying so); Ctrl+A and Ctrl+X empty the box and put its text
//               on the clipboard, written in the cut event; and a click on the demo's Copy button
//               (platform::setClipboardText from its press, written through
//               navigator.clipboard.writeText) puts the first box's text there.
//               No line on the page's stderr (the in-process clipboard's
//               "in-process only" line included)
//  It also fails on any console error or uncaught exception in any page, and
//  unless every page made a WebGL 2 context on ANGLE's SwiftShader with at
//  least 24 depth bits (run.mjs' conditions). The page's container is the
//  viewport less the shell's 96 CSS px #output panel (web/shell.html).
//  --out keeps the screenshots of each check there.
//

import { createServer } from 'node:http';
import { mkdirSync, mkdtempSync, readFileSync, statSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { extname, join, normalize, resolve, sep } from 'node:path';

import { isSwiftShader, launchOptions, loadPlaywright } from './probe.mjs';
import { runPage } from './run.mjs';

const usage = 'usage: node interactive.mjs [--out <dir>] <dir holding ControlTest.html and OpenGLCore.html>';
const outputPanel = 96;     // the shell's #output height, CSS px

let failures = 0;
function fail(check, message)
{
    ++failures;
    console.error(`interactive.mjs: FAIL: ${check}: ${message}`);
}
function pass(check, detail)
{
    console.log(`interactive.mjs: ${check}: ok${detail ? `, ${detail}` : ''}`);
}

const mimeTypes = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
};

// The page directory on a loopback port, nothing outside it.
function serve(root)
{
    const server = createServer((request, response) =>
    {
        const path = normalize(join(root, decodeURIComponent(new URL(request.url, 'http://localhost').pathname)));
        let body = null;
        try
        {
            if((path === root || path.startsWith(root + sep)) && statSync(path).isFile())
                body = readFileSync(path);
        }
        catch(e)
        {
            body = null;
        }
        if(body === null)
        {
            response.writeHead(404);
            response.end();
            return;
        }
        response.writeHead(200, { 'Content-Type': mimeTypes[extname(path)] ?? 'application/octet-stream', 'Cache-Control': 'no-store' });
        response.end(body);
    });
    return new Promise((resolvePromise, reject) =>
    {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', () => resolvePromise(server));
    });
}

// The controls of a --dump-bounds listing with their bounds made absolute
// (each line's bounds are in its parent's coordinates), in the listing's order.
function parseListing(listing)
{
    const controls = [];
    const origins = [];
    for(const line of listing.split('\n'))
    {
        const match = /^( *)(\S+) bounds=\((-?\d+), (-?\d+), (-?\d+), (-?\d+)\).* visible=(yes|no)( inner)?$/.exec(line);
        if(!match)
            continue;
        const depth = match[1].length/2;
        const parent = depth > 0 ? origins[depth - 1] : { x: 0, y: 0 };
        const x = parent.x + Number(match[3]), y = parent.y + Number(match[4]);
        origins[depth] = { x, y };
        controls.push({ type: match[2], x, y, w: Number(match[5]), h: Number(match[6]), visible: match[7] === 'yes', inner: !!match[8], depth });
    }
    return controls;
}

function listingSize(listing)
{
    const match = /^# control=\S+ size=(\d+)x(\d+)/.exec(listing);
    return match ? `${match[1]}x${match[2]}` : null;
}

// The line where two listings first differ, for a failure.
function firstDifference(a, b)
{
    const x = a.split('\n'), y = b.split('\n');
    for(let i = 0; i < Math.max(x.length, y.length); ++i)
        if(x[i] !== y[i])
            return `line ${i + 1}: window "${x[i] ?? '(none)'}", capture "${y[i] ?? '(none)'}"`;
    return null;
}

class Session
{
    constructor(browser, base, out)
    {
        this.browser = browser;
        this.base = base;
        this.out = out;
        this.errors = [];
    }

    // A page of the build dir at a viewport (CSS px) and devicePixelRatio,
    // once it has drawn a few frames (or, \a untilExit, once its run has
    // ended); null after a failure of the check.
    async open(check, path, viewport, deviceScaleFactor = 1, untilExit = false, permissions = [])
    {
        const context = await this.browser.newContext({ viewport, deviceScaleFactor });
        if(permissions.length)
            await context.grantPermissions(permissions, { origin: this.base });
        const page = await context.newPage();
        page.on('console', msg => { if(msg.type() === 'error') this.errors.push(`${check}: console error: ${msg.text()}`); });
        page.on('pageerror', err => this.errors.push(`${check}: uncaught exception: ${err.stack || err.message}`));
        await page.goto(`${this.base}/${path}`, { waitUntil: 'commit' });
        try
        {
            await page.waitForFunction(exit => window.gweniRun && (window.gweniRun.done || (!exit && window.gweniRun.frames >= 3)),
                untilExit, { timeout: 300000, polling: 100 });
        }
        catch(e)
        {
            fail(check, untilExit ? `${path} did not end in 300 s` : `${path} drew no frames in 300 s`);
            await context.close();
            return null;
        }
        if(untilExit)
            return { page, context };
        const webgl = await page.evaluate(() => window.gweniRun.webgl);
        if(!webgl || !webgl.webgl2)
            fail(check, `${path} made no WebGL 2 context`);
        else if(!isSwiftShader(webgl.renderer))
            fail(check, `${path}: asked for ANGLE on SwiftShader, got ${webgl.renderer}`);
        else if(!(webgl.depthBits >= 24))
            fail(check, `${path}'s WebGL 2 context has DEPTH_BITS ${webgl.depthBits}`);
        return { page, context };
    }

    // Waits until the page has drawn \a count more frames.
    async frames(page, count = 3)
    {
        const now = await page.evaluate(() => window.gweniRun.frames);
        await page.waitForFunction(target => window.gweniRun.frames >= target, now + count, { timeout: 60000, polling: 16 });
    }

    // The canvas inside a rect (CSS px), as PNG bytes, kept under --out.
    async shot(page, rect, name)
    {
        const png = await page.screenshot({ clip: rect });
        if(this.out)
            writeFileSync(join(this.out, `${name}.png`), png);
        return png;
    }
}

const listingOf = page => page.evaluate(() => Module.ccall('gweni_web_bounds_listing', 'string', [], []));
const focusedText = page => page.evaluate(() => Module.ccall('gweni_web_focused_text', 'string', [], []));
const backingSize = page => page.evaluate(() => `${Module.canvas.width}x${Module.canvas.height}`);

// A control's rect in CSS px, from its logical bounds at \a unit CSS px per
// logical unit.
function cssRect(control, unit = 1)
{
    return { x: control.x*unit, y: control.y*unit, width: control.w*unit, height: control.h*unit };
}
const centre = rect => ({ x: rect.x + rect.width/2, y: rect.y + rect.height/2 });

// The --dump-bounds listing of a capture in the same browser (run.mjs).
async function captureListing(page, control, size, scale)
{
    const out = mkdtempSync(join(tmpdir(), 'gweni-web-input-'));
    const status = await runPage({ page, args: ['--headless', '--control', control, '--size', size, '--scale', String(scale), '--dump-bounds', '/out/bounds.txt'],
        out, mounts: [], timeout: 300, swiftShader: true, webgl2: true, quiet: true });
    if(status !== 0)
        return null;
    return readFileSync(join(out, 'bounds.txt'), 'utf8');
}

async function checkButton(s)
{
    const opened = await s.open('button', 'ControlTest.html?control=ButtonDemo', { width: 640, height: 480 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const buttons = parseListing(await listingOf(page)).filter(c => c.type === 'Button' && c.visible);
    if(buttons.length < 2)
        return fail('button', `ButtonDemo lists ${buttons.length} visible Button(s)`);
    const rect = cssRect(buttons[1]);   // "Text"
    await page.mouse.move(600, 400);
    await s.frames(page);
    const idle = await s.shot(page, rect, 'button-idle');
    await page.mouse.move(centre(rect).x, centre(rect).y);
    await page.mouse.down();
    await s.frames(page);
    const pressed = await s.shot(page, rect, 'button-pressed');
    await page.mouse.up();
    await page.mouse.move(600, 400);
    await s.frames(page);
    const released = await s.shot(page, rect, 'button-released');
    if(idle.equals(pressed))
        fail('button', 'a press on the "Text" button left its pixels as they were');
    else if(!idle.equals(released))
        fail('button', 'the release with the mouse moved off did not give back the idle pixels');
    else
        pass('button', `"Text" at (${rect.x}, ${rect.y}, ${rect.width}, ${rect.height})`);
    await context.close();
}

async function checkTyping(s)
{
    const opened = await s.open('typing', 'ControlTest.html?control=TextBoxDemo', { width: 640, height: 480 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const box = parseListing(await listingOf(page)).find(c => c.type === 'TextBox' && c.visible);
    if(!box)
        return fail('typing', 'TextBoxDemo lists no visible TextBox');
    const at = centre(cssRect(box));
    await page.mouse.click(at.x, at.y);
    await s.frames(page);
    const before = await focusedText(page);
    await page.keyboard.press('End');
    await page.keyboard.type('abc');
    // "é" is on no key of Playwright's US layout, so it is sent as the browser's
    // own key event, as a French keyboard's "2" key sends it
    const cdp = await context.newCDPSession(page);
    for(const type of ['keyDown', 'keyUp'])
        await cdp.send('Input.dispatchKeyEvent', { type, key: 'é', code: 'Digit2', windowsVirtualKeyCode: 50,
            ...(type === 'keyDown' ? { text: 'é', unmodifiedText: 'é' } : {}) });
    await s.frames(page);
    const after = await focusedText(page);
    if(after !== before + 'abcé')
        fail('typing', `the focused TextBox holds ${JSON.stringify(after)}, not ${JSON.stringify(before + 'abcé')}`);
    else
        pass('typing', `${JSON.stringify(before)} became ${JSON.stringify(after)}`);
    await context.close();
}

async function checkWheel(s)
{
    const opened = await s.open('wheel', 'ControlTest.html?control=ScrollControlDemo', { width: 640, height: 480 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const innerOf = controls =>
    {
        const i = controls.findIndex(c => c.type === 'ScrollControl' && c.visible);
        return i < 0 ? null : { scroll: controls[i], inner: controls.slice(i + 1).find(c => c.inner && c.depth === controls[i].depth + 1) };
    };
    const before = innerOf(parseListing(await listingOf(page)));
    if(!before || !before.inner)
        return fail('wheel', 'ScrollControlDemo lists no visible ScrollControl with an inner panel');
    const at = centre(cssRect(before.scroll));
    await page.mouse.move(at.x, at.y);
    await page.mouse.wheel(0, 100);
    await s.frames(page);
    const after = innerOf(parseListing(await listingOf(page)));
    if(!(after && after.inner && after.inner.y < before.inner.y))
        fail('wheel', `the inner panel stayed at y=${before.inner.y}${after && after.inner ? ` (now ${after.inner.y})` : ''}`);
    else
        pass('wheel', `inner panel y ${before.inner.y} -> ${after.inner.y}`);
    await context.close();
}

async function checkCursor(s)
{
    const opened = await s.open('cursor', 'ControlTest.html?control=CheckBoxDemo', { width: 640, height: 480 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const label = parseListing(await listingOf(page)).find(c => c.type === 'LabelClickable' && c.visible);
    if(!label)
        return fail('cursor', 'CheckBoxDemo lists no visible LabelClickable');
    const at = centre(cssRect(label));
    await page.mouse.move(at.x, at.y);
    await s.frames(page);
    const over = await page.evaluate(() => Module.canvas.style.cursor);
    await page.mouse.move(630, 470);
    await s.frames(page);
    const off = await page.evaluate(() => Module.canvas.style.cursor);
    if(over !== 'pointer' || off !== 'default')
        fail('cursor', `the canvas's CSS cursor is "${over}" over a LabelClickable and "${off}" off it, not "pointer" and "default"`);
    else
        pass('cursor', '"pointer" over a LabelClickable, "default" off it');
    await context.close();
}

// The window's backing store and listing against a capture's.
async function checkWindowSize(s, check, page, backing, logical, captureSize, scale, controlPage)
{
    const size = await backingSize(page);
    const listing = await listingOf(page);
    const captured = await captureListing(controlPage, 'ButtonDemo', captureSize, scale);
    if(size !== backing)
        return fail(check, `the canvas's backing store is ${size}, not ${backing}`);
    if(listingSize(listing) !== logical)
        return fail(check, `the canvas is ${listingSize(listing)} logical, not ${logical}`);
    if(captured === null)
        return fail(check, `the capture at --size ${captureSize} --scale ${scale} failed`);
    const difference = firstDifference(listing, captured);
    if(difference)
        return fail(check, `the listing is not the capture's at --size ${captureSize} --scale ${scale}: ${difference}`);
    pass(check, `backing store ${size}, ${logical} logical, the listing of a capture at --size ${captureSize} --scale ${scale}`);
}

async function checkHiDpi(s, controlPage)
{
    const opened = await s.open('hidpi', 'ControlTest.html?control=ButtonDemo', { width: 640, height: 480 + outputPanel }, 2);
    if(!opened)
        return;
    const { page, context } = opened;
    await checkWindowSize(s, 'hidpi', page, '1280x960', '640x480', '640x480', 2, controlPage);

    // the cursor through the scale: a press on "Text" changes its pixels
    const rect = cssRect(parseListing(await listingOf(page)).filter(c => c.type === 'Button' && c.visible)[1]);
    await page.mouse.move(600, 400);
    await s.frames(page);
    const idle = await s.shot(page, rect, 'hidpi-idle');
    await page.mouse.move(centre(rect).x, centre(rect).y);
    await page.mouse.down();
    await s.frames(page);
    const pressed = await s.shot(page, rect, 'hidpi-pressed');
    await page.mouse.up();
    if(idle.equals(pressed))
        fail('hidpi', 'a press on the "Text" button at devicePixelRatio 2 left its pixels as they were');
    else
        pass('hidpi', 'a press on "Text" changes its pixels');

    await page.setViewportSize({ width: 800, height: 600 + outputPanel });
    await page.waitForFunction(() => Module.canvas.width === 1600, null, { timeout: 60000 }).catch(() => {});
    await s.frames(page);
    await checkWindowSize(s, 'resize', page, '1600x1200', '800x600', '800x600', 2, controlPage);
    await context.close();
}

async function checkScale(s, controlPage)
{
    const opened = await s.open('scale', 'ControlTest.html?control=ButtonDemo&scale=1.5', { width: 640, height: 480 + outputPanel }, 2);
    if(!opened)
        return;
    const { page, context } = opened;
    await checkWindowSize(s, 'scale', page, '1280x960', '427x320', '427x320', 3, controlPage);
    await context.close();
}

async function checkUsage(s)
{
    const opened = await s.open('usage', 'ControlTest.html?bogus', { width: 640, height: 480 + outputPanel }, 1, true);
    if(!opened)
        return;
    const { page, context } = opened;
    const run = await page.evaluate(() => ({ exitCode: gweniRun.exitCode, done: gweniRun.done,
        stderr: gweniRun.output.filter(l => l.stream === 'stderr').map(l => l.text).join('\n'),
        panel: document.getElementById('output').textContent }));
    if(!run.done || run.exitCode !== 1)
        fail('usage', `?bogus ended with exit status ${run.exitCode}, not 1`);
    else if(!run.stderr.includes('ControlTest: unknown argument: --bogus') || !/usage/i.test(run.stderr))
        fail('usage', `stderr was ${JSON.stringify(run.stderr.slice(0, 200))}`);
    else if(!run.panel.includes('unknown argument: --bogus'))
        fail('usage', 'the page\'s #output panel does not show the error');
    else
        pass('usage', 'exit status 1, the error and usage on stderr and the page');
    await context.close();
}

async function checkOpenGLCore(s)
{
    const opened = await s.open('opengl', 'OpenGLCore.html', { width: 1024, height: 768 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const size = await backingSize(page);
    const whole = { x: 0, y: 0, width: 1024, height: 768 };
    await page.mouse.move(1000, 750);
    await s.frames(page);
    const before = await s.shot(page, whole, 'opengl-before');
    // the demo list's "Label" entry, under "Basic" (the shell's layout at 1024x768)
    await page.mouse.click(40, 205);
    await s.frames(page, 5);
    const after = await s.shot(page, whole, 'opengl-after');
    if(size !== '1024x768')
        fail('opengl', `the canvas's backing store is ${size}, not 1024x768`);
    else if(before.equals(after))
        fail('opengl', 'a click on the demo list changed nothing');
    else
        pass('opengl', 'backing store 1024x768, a click on the demo list changes the canvas');
    await context.close();
}

const statsOverlayState = page => page.evaluate(() => Module.ccall('gweni_web_stats_overlay', 'string', [], []));

// "overlay=open button=on rect=x,y,w,h" -> { overlay, button, rect }
function parseStatsState(text)
{
    const match = /^overlay=(\w+) button=(\w+) rect=(-?\d+),(-?\d+),(\d+),(\d+)$/.exec(text);
    if(!match)
        return null;
    const [x, y, w, h] = match.slice(3).map(Number);
    return { overlay: match[1], button: match[2], rect: { x, y, width: w, height: h } };
}

async function checkStatsButton(s)
{
    const opened = await s.open('stats', 'OpenGLCore.html?stats-overlay', { width: 1024, height: 768 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const whole = { x: 0, y: 0, width: 1024, height: 768 };
    const first = parseStatsState(await statsOverlayState(page));
    if(!first)
    {
        fail('stats', `gweni_web_stats_overlay gave no state`);
        await context.close();
        return;
    }
    // the canvas is 1024x768 logical at devicePixelRatio 1: a unit is a CSS px
    const at = centre(first.rect);
    const open = await s.shot(page, whole, 'stats-open');
    await page.mouse.click(at.x, at.y);
    await s.frames(page, 5);
    const second = parseStatsState(await statsOverlayState(page));
    const closed = await s.shot(page, whole, 'stats-closed');
    await page.mouse.click(at.x, at.y);
    await s.frames(page, 5);
    const third = parseStatsState(await statsOverlayState(page));
    const describe = state => state ? `overlay ${state.overlay}, button ${state.button}` : 'no state';

    if(first.overlay !== 'open' || first.button !== 'on')
        fail('stats', `?stats-overlay started with ${describe(first)}, not overlay open, button on`);
    else if(!second || second.overlay !== 'closed' || second.button !== 'off')
        fail('stats', `a click on the Stats button left ${describe(second)}, not overlay closed, button off`);
    else if(open.equals(closed))
        fail('stats', 'closing the overlay with the Stats button changed nothing on the canvas');
    else if(!third || third.overlay !== 'open' || third.button !== 'on')
        fail('stats', `a second click left ${describe(third)}, not overlay open, button on`);
    else
        pass('stats', `the Stats button at (${first.rect.x}, ${first.rect.y}) closes the overlay and opens it again`);
    await context.close();
}

// gweni_web_depth_view(on): -1 only reads, "view=open button=on rect=x,y,w,h"
const depthViewState = page => page.evaluate(() => Module.ccall('gweni_web_depth_view', 'string', ['number'], [-1]));

function parseDepthState(text)
{
    const match = /^view=(\w+) button=(\w+) rect=(-?\d+),(-?\d+),(\d+),(\d+)$/.exec(text);
    if(!match)
        return null;
    const [x, y, w, h] = match.slice(3).map(Number);
    return { view: match[1], button: match[2], rect: { x, y, width: w, height: h } };
}

async function checkDepthButton(s)
{
    const opened = await s.open('depth', 'OpenGLCore.html', { width: 1024, height: 768 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const whole = { x: 0, y: 0, width: 1024, height: 768 };
    const first = parseDepthState(await depthViewState(page));
    if(!first)
    {
        fail('depth', `gweni_web_depth_view gave no state`);
        await context.close();
        return;
    }
    const at = centre(first.rect);
    const before = await s.shot(page, whole, 'depth-before');
    await page.mouse.click(at.x, at.y);
    await s.frames(page, 5);
    const second = parseDepthState(await depthViewState(page));
    const open = await s.shot(page, whole, 'depth-open');
    // the view has the mouse: the way out is Escape
    await page.keyboard.press('Escape');
    await s.frames(page, 5);
    const third = parseDepthState(await depthViewState(page));
    const describe = state => state ? `view ${state.view}, button ${state.button}` : 'no state';

    if(first.view !== 'closed' || first.button !== 'off')
        fail('depth', `the demo started with ${describe(first)}, not view closed, button off`);
    else if(!second || second.view !== 'open' || second.button !== 'on')
        fail('depth', `a click on the Depth button left ${describe(second)}, not view open, button on`);
    else if(before.equals(open))
        fail('depth', 'opening the depth view with the Depth button changed nothing on the canvas');
    else if(!third || third.view !== 'closed' || third.button !== 'off')
        fail('depth', `Escape left ${describe(third)}, not view closed, button off`);
    else
        pass('depth', `the Depth button at (${first.rect.x}, ${first.rect.y}) opens the view and Escape closes it, the button following`);
    await context.close();
}

// CodeEditorDemo: Tab and Ctrl+Z are the editor's, not the browser's. A
// keydown listener on the window records whether each key's default was
// prevented by the time it bubbled there (emscripten-glfw's handlers run
// first), so the browser neither moved its focus nor ran its own undo.
async function checkCodeEditor(s)
{
    const opened = await s.open('codeeditor', 'ControlTest.html?control=CodeEditorDemo', { width: 640, height: 480 + outputPanel });
    if(!opened)
        return;
    const { page, context } = opened;
    const listed = parseListing(await listingOf(page));
    const editor = listed.find(c => c.type === 'CodeEditor' && c.visible);
    if(!editor)
        return fail('codeeditor', 'CodeEditorDemo lists no visible CodeEditor');
    // the rows on screen, top down: the fourth is the fourth line's
    const rows = listed.filter(c => c.type === 'CodeLine' && c.visible && c.y >= editor.y).sort((a, b) => a.y - b.y);
    if(rows.length < 4)
        return fail('codeeditor', `CodeEditorDemo lists ${rows.length} visible CodeLine rows, not 4 or more`);
    await page.evaluate(() =>
    {
        window.gweniKeys = [];
        window.addEventListener('keydown', e => window.gweniKeys.push({ key: e.key, ctrl: e.ctrlKey, prevented: e.defaultPrevented }));
    });
    const activeElement = () => page.evaluate(() => document.activeElement ? `${document.activeElement.tagName}#${document.activeElement.id}` : '(none)');
    // a press on the fourth line, right of its end: the caret at that end
    const rect = cssRect(editor);
    await page.mouse.click(rect.x + rect.width*0.8, centre(cssRect(rows[3])).y);
    await s.frames(page);
    const before = await focusedText(page);
    const focusBefore = await activeElement();

    await page.keyboard.press('Tab');
    await s.frames(page);
    const tabbed = await focusedText(page);
    const focusTabbed = await activeElement();

    await page.keyboard.type('q');
    await s.frames(page);
    const typed = await focusedText(page);
    await page.keyboard.press('Control+z');
    await s.frames(page);
    const undone = await focusedText(page);
    const focusUndone = await activeElement();
    const keys = await page.evaluate(() => window.gweniKeys);
    const tabKey = keys.find(k => k.key === 'Tab');
    const undoKey = keys.find(k => k.ctrl && k.key.toLowerCase() === 'z');

    if(!before)
        fail('codeeditor', 'the press did not give the CodeEditor the focus (no caret line)');
    else if(!(tabbed.startsWith(before) && tabbed.length > before.length && /^ +$/.test(tabbed.slice(before.length))))
        fail('codeeditor', `Tab left the caret line ${JSON.stringify(tabbed)}, not ${JSON.stringify(before)} and spaces`);
    else if(focusTabbed !== focusBefore || focusUndone !== focusBefore)
        fail('codeeditor', `the browser's focus moved from ${focusBefore} to ${focusTabbed}, then ${focusUndone}`);
    else if(!tabKey || !tabKey.prevented)
        fail('codeeditor', `Tab's keydown ${tabKey ? 'was not prevented' : 'never reached the window'}: the browser would move its focus`);
    else if(typed !== tabbed + 'q')
        fail('codeeditor', `typing "q" left ${JSON.stringify(typed)}`);
    else if(undone !== tabbed && undone !== before)
        fail('codeeditor', `Ctrl+Z left ${JSON.stringify(undone)}, neither ${JSON.stringify(tabbed)} nor ${JSON.stringify(before)}`);
    else if(!undoKey || !undoKey.prevented)
        fail('codeeditor', `Ctrl+Z's keydown ${undoKey ? 'was not prevented' : 'never reached the window'}: the page's undo would run`);
    else
        pass('codeeditor', `${JSON.stringify(before)} -> Tab ${JSON.stringify(tabbed)} -> Ctrl+Z ${JSON.stringify(undone)}, the focus on ${focusBefore}`);
    await context.close();
}


// The browser's own key event for "é", on no key of Playwright's US layout
// (as checkTyping sends it).
async function typeEAcute(context, page)
{
    const cdp = await context.newCDPSession(page);
    for(const type of ['keyDown', 'keyUp'])
        await cdp.send('Input.dispatchKeyEvent', { type, key: 'é', code: 'Digit2', windowsVirtualKeyCode: 50,
            ...(type === 'keyDown' ? { text: 'é', unmodifiedText: 'é' } : {}) });
}

async function checkClipboard(s)
{
    const opened = await s.open('clipboard', 'ControlTest.html?control=TextBoxDemo', { width: 640, height: 480 + outputPanel },
        1, false, ['clipboard-read', 'clipboard-write']);
    if(!opened)
        return;
    const { page, context } = opened;
    const readClipboard = () => page.evaluate(() => navigator.clipboard.readText());
    const writeClipboard = text => page.evaluate(t => navigator.clipboard.writeText(t), text);
    const answers = () => page.evaluate(() => (Module.gweniClipboard ? Module.gweniClipboard.answers.slice() : null));
    const writes = () => page.evaluate(() => (Module.gweniClipboard ? Module.gweniClipboard.writes.slice() : null));

    const controls = parseListing(await listingOf(page));
    const box = controls.find(c => c.type === 'TextBox' && c.visible);
    const button = controls.find(c => c.type === 'Button' && c.visible);
    if(!box || !button)
        return fail('clipboard', `TextBoxDemo lists ${box ? 'no visible Button' : 'no visible TextBox'}`);
    const at = centre(cssRect(box));
    await page.mouse.click(at.x, at.y);
    await s.frames(page);

    // copy
    await page.keyboard.press('Control+A');
    await page.keyboard.type('copy me');
    await typeEAcute(context, page);
    await page.keyboard.press('Control+A');
    await page.keyboard.press('Control+C');
    await s.frames(page);
    const copied = await readClipboard();
    if(copied !== 'copy meé')
        return fail('clipboard', `Ctrl+C over "copy meé" left ${JSON.stringify(copied)} on the clipboard`);
    if(JSON.stringify(await writes()) !== '["copy"]')
        return fail('clipboard', `Ctrl+C was written in the events ${JSON.stringify(await writes())}, not one copy event`);

    // paste: the clipboard's text at the keystroke, not the one copied before
    const pasted = 'pasted ü ✓ 日';
    await writeClipboard(pasted);
    const before = (await answers()).length;
    await page.keyboard.press('Control+A');
    await page.keyboard.press('Control+V');
    await s.frames(page);
    const after = await focusedText(page);
    const how = (await answers()).slice(before);
    if(how.length !== 1 || how[0] !== 'paste')
        return fail('clipboard', `the paste was answered by ${JSON.stringify(how)}, not the page's paste event`
            + ' (none reached gweni\'s listener: the headless shell fired no native paste event for Ctrl+V,'
            + ' or the listener did not take it)');
    if(after !== pasted)
        return fail('clipboard', `Ctrl+V of ${JSON.stringify(pasted)} left ${JSON.stringify(after)} in the box`);

    // cut
    await writeClipboard('not cut');
    await page.keyboard.press('Control+A');
    await page.keyboard.press('Control+X');
    await s.frames(page);
    const cutBox = await focusedText(page);
    const cut = await readClipboard();
    if(cutBox !== '' || cut !== pasted)
        return fail('clipboard', `Ctrl+X left ${JSON.stringify(cutBox)} in the box and ${JSON.stringify(cut)} on the clipboard`);
    if(JSON.stringify(await writes()) !== '["copy","cut"]')
        return fail('clipboard', `Ctrl+X was written in the events ${JSON.stringify(await writes())}, not one cut event after the copy`);

    // a Copy button: setClipboardText from its press
    await page.keyboard.type('from the button');
    await writeClipboard('not the button');
    const press = centre(cssRect(button));
    await page.mouse.click(press.x, press.y);
    await s.frames(page);
    const fromButton = await readClipboard();
    if(fromButton !== 'from the button')
        return fail('clipboard', `a click on Copy left ${JSON.stringify(fromButton)} on the clipboard`);

    const output = await page.evaluate(() => window.gweniRun.output.filter(line => line.stream !== 'stdout').map(line => line.text));
    if(output.length)
        return fail('clipboard', `the page wrote to stderr: ${JSON.stringify(output)}`);
    pass('clipboard', `copied ${JSON.stringify(copied)}, pasted ${JSON.stringify(after)} from the paste event, cut, and the Copy button copied`);
    await context.close();
}

async function main()
{
    const argv = process.argv.slice(2);
    let out = null, dir = null;
    for(let i = 0; i < argv.length; ++i)
    {
        if(argv[i] === '--out' && i + 1 < argv.length)
            out = argv[++i];
        else if(!argv[i].startsWith('--') && dir === null)
            dir = argv[i];
        else
        {
            console.error(usage);
            return 1;
        }
    }
    if(dir === null)
    {
        console.error(usage);
        return 1;
    }
    dir = resolve(dir);
    if(out)
        mkdirSync(out, { recursive: true });

    const { chromium } = loadPlaywright();
    const server = await serve(dir);
    const browser = await chromium.launch(launchOptions(true));
    const s = new Session(browser, `http://127.0.0.1:${server.address().port}`, out);
    const controlPage = join(dir, 'ControlTest.html');
    try
    {
        await checkButton(s);
        await checkTyping(s);
        await checkClipboard(s);
        await checkCodeEditor(s);
        await checkWheel(s);
        await checkCursor(s);
        await checkHiDpi(s, controlPage);
        await checkScale(s, controlPage);
        await checkUsage(s);
        await checkOpenGLCore(s);
        await checkStatsButton(s);
        await checkDepthButton(s);
    }
    finally
    {
        await browser.close();
        server.close();
    }
    for(const error of s.errors)
        fail('page', error);
    if(failures)
        return 1;
    console.log('interactive.mjs: OK, every check passed');
    return 0;
}

process.exit(await main());
