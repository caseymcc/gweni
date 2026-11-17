//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  web_touch: ControlTest's interactive window driven as a phone drives it, in
//  the image's headless Chromium: a touch context (hasTouch, isMobile,
//  devicePixelRatio 2), fingers through CDP's Input.dispatchTouchEvent (the
//  browser's own touch events, which the page's listeners take,
//  platform::useBrowserInput) and an input method through CDP's
//  Input.imeSetComposition and Input.insertText (the events a real one gives
//  the page's hidden textarea).
//
//      node tests/web/touch.mjs [--out <dir>] <dir holding ControlTest.html>
//
//  At devicePixelRatio 2 the canvas's scale is 2, so a logical unit is a CSS
//  px: a finger moved N CSS px moves N units. The script reads the window's
//  state through ControlTest's gweni_web_probe export (Module.ccall, one JSON
//  object a query: the frames' layout passes, slots written and animations
//  run since a reset, the hovered and focused controls, the tooltip and the
//  context menu, a Button's presses, a control's rect, a TextBox's text,
//  scroll offsets, pan and value, the hit test at a point) and waits after each input for the
//  page to settle (frames.mjs). The checks, each one "touch.mjs: <check>: ok"
//  line or a "touch.mjs: FAIL: <check>: ..." one:
//    tap         ButtonDemo: a tap on "Toggle Me" presses it once and turns it
//                on, leaving it neither held nor hovered and the textarea
//                unfocused; a tap on "With Tooltip" shows no tooltip 1.5 s on;
//                a tap with a negative Touch.identifier presses it once more
//    drag        VirtualListDemo: a finger dragged 60 px up and held still
//                before its lift scrolls the list 60 units, and it stays there
//    drag_button ScrollControlDemo: a drag of 40 px up that starts on the
//                "Tall" button scrolls its ScrollControl 40 units and presses
//                nothing
//    fling       VirtualListDemo: a flick lifted while moving keeps scrolling
//                after the lift and stops on its own, then a settled frame
//                writes nothing and runs no animation; a second flick is
//                stopped by a finger put down, which scrolls it no further
//    keeps       the controls that keep a drag get it as a mouse drag: a
//                Slider's thumb (its value changes), a DragNumber (its
//                number changes), a Window's title bar (the window moves by
//                the finger), a splitter (its bar moves), a scroll bar's thumb
//                (the offset goes up with the finger going down, which a pan
//                would not), a node's title bar (the node moves by the finger
//                at the graph's zoom) and a pin (a drag from an output to an
//                input makes a link)
//    zoom_pan    ZoomPanelDemo: a drag of (40, -30) from a point the hit test
//                finds empty pans its ZoomPanel by exactly that, in frames
//                with no layout pass
//    graph_pan   NodeGraphDemo: a drag on the graph's empty space pans it by
//                the finger, with no marquee
//    long_press  ContextMenuDemo: a finger held still on the Document box opens
//                its menu at the finger, and the lift leaves it open
//    graph_long_press  NodeGraphDemo: a long press on empty space opens the
//                graph's menu with getContextMenuPosition() the point a right
//                click there gives
//    ime         TextBoxDemo: a tap on the first TextBox focuses the hidden
//                textarea inside the touchend's own task (focusin while it
//                runs: iOS opens a keyboard only from a user gesture's task);
//                a composition (imeSetComposition) shows inline in the box's
//                pixels and is not its text and fires no onTextChanged;
//                insertText commits "日本語", which the box then holds
//                exactly, with one onTextChanged; "hel" composing there and
//                a tap on the second box commits it to the first and ends
//                the textarea's composition too (one compositionend, the
//                textarea empty), so the next word is a composition of its
//                own in the second box alone; a Shift pressed before a
//                composition and let go during it reaches emscripten-glfw's
//                document listener and commits nothing; a tap elsewhere
//                blurs the textarea
//    password    TextBoxDemo: a tap on the PasswordTextBox focuses the page's
//                password field, not the textarea; "secret" composing for it
//                (which it shows none of) and a tap on the Copy button leave
//                the box holding exactly "secret" (the canvas types the word
//                as the focus leaves) and the password field composing
//                nothing, no stderr line
//    desktop     TextBoxDemo with no touch: a click on the box and "ab" and
//                Backspace typed with the keyboard leave "a" exactly (nothing
//                typed twice through the textarea), the textarea empty; a
//                second click on the box, moving no caret, gives the
//                textarea the focus back (emscripten-glfw focused the
//                canvas), so an input method's "ka" composes in the box
//  It also fails on any console error, uncaught exception or line on a page's
//  stderr (the page side's "no GLFW3.isAnyElementFocused" line included).
//  --out keeps a screenshot of the composition and of the long press's menu.
//

import { createServer } from 'node:http';
import { mkdirSync, readFileSync, statSync, writeFileSync } from 'node:fs';
import { extname, join, normalize, resolve, sep } from 'node:path';

import { firstFrame, settle } from './frames.mjs';
import { launchOptions, loadPlaywright } from './probe.mjs';

const usage = 'usage: node touch.mjs [--out <dir>] <dir holding ControlTest.html>';
const outputPanel = 96;     // the shell's #output height, CSS px
const viewport = { width: 640, height: 480 + outputPanel };

let failures = 0;
function fail(check, message)
{
    ++failures;
    console.error(`touch.mjs: FAIL: ${check}: ${message}`);
}
function pass(check, detail)
{
    console.log(`touch.mjs: ${check}: ok${detail ? `, ${detail}` : ''}`);
}
function expect(check, ok, message, detail)
{
    if(ok)
        pass(check, detail);
    else
        fail(check, message);
    return ok;
}

const mimeTypes = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
};

function serve(root)
{
    const server = createServer((request, response) =>
    {
        const name = decodeURIComponent(new URL(request.url, 'http://localhost').pathname).replace(/^\//, '');
        const path = normalize(join(root, name));
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

const sleep = ms => new Promise(done => setTimeout(done, ms));
const centre = rect => ({ x: rect.x + rect.w/2, y: rect.y + rect.h/2 });

// One page of ControlTest's window and what drives it.
class Page
{
    static async open(browser, base, control, touch = true)
    {
        const context = await browser.newContext(touch ?
            { viewport, deviceScaleFactor: 2, hasTouch: true, isMobile: true } : { viewport, deviceScaleFactor: 2 });
        const page = await context.newPage();
        const p = new Page(context, page, control);

        page.on('console', msg => { if(msg.type() === 'error') p.errors.push(`console error: ${msg.text()}`); });
        page.on('pageerror', err => p.errors.push(`uncaught exception: ${err.stack || err.message}`));
        // whether the textarea took the focus inside a touchend's task: set by
        // a listener added before the page's own, cleared by the next task
        await page.addInitScript(() =>
        {
            window.gweniTouchProbe = { inTouchEnd: false, focusInTouchEnd: 0, focusOutside: 0 };
            window.addEventListener('touchend', () =>
            {
                gweniTouchProbe.inTouchEnd = true;
                setTimeout(() => { gweniTouchProbe.inTouchEnd = false; }, 0);
            }, { capture: true });
            window.addEventListener('focusin', e =>
            {
                if(!window.Module || !Module.gweniInput || e.target !== Module.gweniInput.field)
                    return;
                if(gweniTouchProbe.inTouchEnd)
                    ++gweniTouchProbe.focusInTouchEnd;
                else
                    ++gweniTouchProbe.focusOutside;
            }, { capture: true });
        });
        await page.goto(`${base}/ControlTest.html?control=${control}`, { waitUntil: 'commit' });
        await firstFrame(page);
        p.cdp = await context.newCDPSession(page);
        return p;
    }

    constructor(context, page, control)
    {
        this.context = context;
        this.page = page;
        this.control = control;
        this.errors = [];
    }

    async probe(query)
    {
        const text = await this.page.evaluate(q => Module.ccall('gweni_web_probe', 'string', ['string'], [q]), query);
        return JSON.parse(text);
    }

    async settle()
    {
        await settle(this.page);
    }

    async touch(type, at, id = 1)
    {
        await this.cdp.send('Input.dispatchTouchEvent',
            { type, touchPoints: type === 'touchEnd' || type === 'touchCancel' ? [] : [{ x: at.x, y: at.y, id }] });
    }

    async tap(at, id = 1)
    {
        await this.touch('touchStart', at, id);
        await this.touch('touchEnd', at, id);
        await this.settle();
    }

    // A finger from \a from by \a by in \a steps moves a frame apart, held
    // still \a hold ms before its lift (so it does not fling), or lifted at
    // once (\a hold 0, a flick); \a lift false leaves it down.
    async drag(from, by, { steps = 6, hold = 250, lift = true } = {})
    {
        await this.touch('touchStart', from);
        for(let i = 1; i <= steps; ++i)
        {
            await this.touch('touchMove', { x: from.x + by.x*i/steps, y: from.y + by.y*i/steps });
            await sleep(16);
        }
        if(hold)
            await sleep(hold);
        if(lift)
            await this.touch('touchEnd', { x: from.x + by.x, y: from.y + by.y });
        await this.settle();
    }

    // The page's stderr lines, console errors and exceptions so far
    async problems()
    {
        const stderr = await this.page.evaluate(() => gweniRun.output.filter(line => line.stream === 'stderr').map(line => line.text));
        return this.errors.concat(stderr.map(line => `stderr: ${line}`));
    }

    async close(check)
    {
        for(const problem of await this.problems())
            fail(check, `${this.control}: ${problem}`);
        await this.context.close();
    }
}

async function checkTap(browser, base)
{
    const p = await Page.open(browser, base, 'ButtonDemo');
    await p.probe('watch');
    const toggle = await p.probe('button Toggle Me');
    await p.tap(centre(toggle.rect));
    const after = await p.probe('button Toggle Me');
    const state = await p.probe('state');
    const input = await p.page.evaluate(() => ({ handed: Module.gweniInput.touchesHanded,
        focused: document.activeElement === Module.gweniInput.field }));
    expect('tap', after.presses === 1 && after.toggle && !after.depressed && state.hovered === null && !input.focused
        && input.handed === 2,
        `a tap on "Toggle Me" pressed it ${after.presses} time(s) (1 wanted), left it ${after.toggle ? 'on' : 'off'}`
        + `${after.depressed ? ', held' : ''}, hovered ${state.hovered}, the textarea ${input.focused ? '' : 'not '}focused, `
        + `${input.handed} touch event(s) handed on (2 wanted)`,
        'pressed once, on, nothing hovered');

    const tipped = await p.probe('button With Tooltip');
    await p.tap(centre(tipped.rect));
    await sleep(1500);
    await p.settle();
    const later = await p.probe('state');
    expect('tap', !later.tooltip && later.hovered === null && (await p.probe('button With Tooltip')).presses === 1,
        `a tap on "With Tooltip" showed its tooltip (${later.tooltip}) or left ${later.hovered} hovered 1.5 s on`,
        'no tooltip 1.5 s after a tap on "With Tooltip"');

    // a Touch.identifier is any long: the page hands gweni a small slot of
    // its own, so a negative one (which inputTouch ignores) still taps
    await p.page.evaluate(() => window.addEventListener('touchstart',
        e => { window.lastTouchId = e.changedTouches[0].identifier; }, { capture: true }));
    await p.tap(centre(toggle.rect), -7);
    const negative = await p.probe('button Toggle Me');
    const seen = await p.page.evaluate(() => window.lastTouchId);
    expect('tap', seen < 0 && negative.presses === 2 && !negative.toggle,
        `a tap with Touch.identifier ${seen} (negative wanted) pressed "Toggle Me" `
        + `${negative.presses - 1} time(s) (1 wanted), leaving it ${negative.toggle ? 'on' : 'off'}`,
        'a tap with a negative identifier presses once');
    await p.close('tap');
}

async function checkDrag(browser, base)
{
    const p = await Page.open(browser, base, 'VirtualListDemo');
    const list = await p.probe('control VirtualList 0');
    await p.drag(centre(list.rect), { x: 0, y: -60 });
    await sleep(200);
    await p.settle();
    const after = await p.probe('control VirtualList 0');
    expect('drag', after.scroll.y === list.scroll.y + 60 && after.anims === 0,
        `a 60 px drag up scrolled the list from ${list.scroll.y} to ${after.scroll.y}, ${after.anims} animation(s) left`,
        `scrolled 60 units`);
    await p.close('drag');

    const q = await Page.open(browser, base, 'ScrollControlDemo');
    await q.probe('watch');
    const tall = await q.probe('button Tall');
    const scroll = await q.probe('control ScrollControl 3');
    // the button is taller than its viewport: a point of it inside that
    const from = { x: centre(tall.rect).x, y: centre(scroll.rect).y };
    const hit = await q.probe(`at ${from.x} ${from.y}`);
    if(hit.chain[0] !== 'Button')
        fail('drag_button', `the hit test at (${from.x}, ${from.y}) finds ${hit.chain.join(' < ')}, not "Tall"`);
    await q.drag(from, { x: 0, y: -40 });
    const moved = await q.probe('control ScrollControl 3');
    const pressed = await q.probe('button Tall');
    expect('drag_button', moved.scroll.y === scroll.scroll.y + 40 && pressed.presses === 0 && !pressed.depressed,
        `a 40 px drag up from "Tall" scrolled its ScrollControl from ${scroll.scroll.y} to ${moved.scroll.y} and pressed it `
        + `${pressed.presses} time(s)`, 'scrolled 40 units, pressed nothing');
    await q.close('drag_button');
}

async function checkFling(browser, base)
{
    const p = await Page.open(browser, base, 'VirtualListDemo');
    const list = await p.probe('control VirtualList 0');
    const from = centre(list.rect);

    await p.drag(from, { x: 0, y: -120 }, { steps: 6, hold: 0 });
    const lifted = await p.probe('control VirtualList 0');
    const flinging = lifted.anims;
    let stopped = null;
    const start = Date.now();
    while(Date.now() - start < 10000)
    {
        await sleep(100);
        const now = await p.probe('control VirtualList 0');
        if(now.anims === 0)
        {
            stopped = now;
            break;
        }
    }
    await p.settle();
    await p.probe('reset');
    await sleep(300);
    await p.settle();
    const settled = await p.probe('frames');
    const ok = expect('fling', flinging === 1 && stopped !== null && stopped.scroll.y > lifted.scroll.y
        && settled.total.slots === 0 && settled.total.animsRun === 0,
        `a flick registered ${flinging} animation(s) (1 wanted), went from ${lifted.scroll.y} to `
        + `${stopped ? stopped.scroll.y : '(still flinging after 10 s)'}, then settled frames wrote `
        + `${settled.total.slots} slot(s) and ran ${settled.total.animsRun} animation(s)`,
        `flung ${stopped ? stopped.scroll.y - lifted.scroll.y : 0} units past the lift, then stopped`);

    if(ok)
    {
        await p.drag(from, { x: 0, y: -120 }, { steps: 6, hold: 0 });
        await sleep(50);
        await p.touch('touchStart', from);
        await p.settle();
        const held = await p.probe('control VirtualList 0');
        await sleep(300);
        await p.settle();
        const still = await p.probe('control VirtualList 0');
        await p.touch('touchEnd', from);
        await p.settle();
        expect('fling', held.anims === 0 && still.scroll.y === held.scroll.y,
            `a finger put down on a flinging list left ${held.anims} animation(s) or it scrolled on from `
            + `${held.scroll.y} to ${still.scroll.y}`, 'a finger put down stops a fling');
    }
    await p.close('fling');
}

async function checkKeeps(browser, base)
{
    // SliderDemo: the first horizontal slider's thumb
    {
        const p = await Page.open(browser, base, 'SliderDemo');
        const slider = await p.probe('control HorizontalSlider 0');
        const thumb = await p.probe('control SliderBar 0');
        await p.drag(centre(thumb.rect), { x: 40, y: 0 });
        const after = await p.probe('control HorizontalSlider 0');
        const moved = await p.probe('control SliderBar 0');
        expect('keeps', after.value !== slider.value && moved.rect.x > thumb.rect.x,
            `a 40 px drag of the Slider's thumb left its value ${after.value} (was ${slider.value}) or the thumb at `
            + `${moved.rect.x} (was ${thumb.rect.x})`, `Slider ${slider.value} -> ${after.value}`);
        await p.close('keeps');
    }
    // DragNumberDemo: the first field
    {
        const p = await Page.open(browser, base, 'DragNumberDemo');
        const field = await p.probe('control DragNumber 0');
        await p.drag(centre(field.rect), { x: 40, y: 0 });
        const after = await p.probe('control DragNumber 0');
        expect('keeps', after.number !== field.number,
            `a 40 px drag of a DragNumber left its number ${after.number}`, `DragNumber ${field.number} -> ${after.number}`);
        await p.close('keeps');
    }
    // WindowDemo: a tap opens a window, a drag of its title bar moves it
    {
        const p = await Page.open(browser, base, 'WindowDemo');
        await p.tap(centre((await p.probe('button Normal Window')).rect));
        const window = await p.probe('control WindowControl 0');
        if(window.error)
            fail('keeps', 'a tap on "Normal Window" opened no window');
        else
        {
            // away from the canvas's nearer edges, which the window is kept inside
            const by = { x: window.rect.x > 100 ? -40 : 40, y: window.rect.y > 100 ? -30 : 30 };
            await p.drag({ x: window.rect.x + window.rect.w/3, y: window.rect.y + 10 }, by);
            const moved = await p.probe('control WindowControl 0');
            expect('keeps', moved.rect.x === window.rect.x + by.x && moved.rect.y === window.rect.y + by.y,
                `a (${by.x}, ${by.y}) drag of the Window's title bar at (${window.rect.x}, ${window.rect.y}) moved it by `
                + `(${moved.rect.x - window.rect.x}, `
                + `${moved.rect.y - window.rect.y})`, 'the Window moved by the finger');
        }
        await p.close('keeps');
    }
    // CrossSplitterDemo: the first splitter bar
    {
        const p = await Page.open(browser, base, 'CrossSplitterDemo');
        const bar = await p.probe('control SplitterBar 0');
        await p.drag(centre(bar.rect), { x: 30, y: 30 });
        const moved = await p.probe('control SplitterBar 0');
        expect('keeps', moved.rect.x !== bar.rect.x || moved.rect.y !== bar.rect.y,
            `a (30, 30) drag of a splitter bar left it at (${moved.rect.x}, ${moved.rect.y})`, 'the splitter moved');
        await p.close('keeps');
    }
    // ScrollControlDemo: the first ScrollControl's vertical thumb
    {
        const p = await Page.open(browser, base, 'ScrollControlDemo');
        const scroll = await p.probe('control ScrollControl 0');
        let thumb = null;
        for(let i = 0; i < 40 && !thumb; ++i)
        {
            const bar = await p.probe(`control ScrollBarBar ${i}`);
            if(bar.error)
                break;
            const r = bar.rect, s = scroll.rect;
            if(r.h > r.w && r.x >= s.x && r.x + r.w <= s.x + s.w && r.y >= s.y && r.y + r.h <= s.y + s.h)
                thumb = bar;
        }
        if(!thumb)
            fail('keeps', 'the first ScrollControl has no vertical thumb');
        else
        {
            await p.drag(centre(thumb.rect), { x: 0, y: 20 });
            const after = await p.probe('control ScrollControl 0');
            expect('keeps', after.scroll.y > scroll.scroll.y,
                `a 20 px drag down of the scroll bar's thumb left the offset at ${after.scroll.y} (was ${scroll.scroll.y})`,
                `the thumb's drag scrolled to ${after.scroll.y}`);
        }
        await p.close('keeps');
    }
    // NodeGraphDemo: a node's title bar, and a pin's link drag
    {
        const p = await Page.open(browser, base, 'NodeGraphDemo');
        const graph = await p.probe('control NodeGraph 0');
        const node = await p.probe('control Node 0');
        const title = await p.probe('control NodeTitleBar 0');
        await p.drag(centre(title.rect), { x: 24, y: 16 });
        // the node moves 24/zoom graph units, which are zoom canvas units
        // each; it is brought to the front, so it is another index now
        let moved = null;
        for(let i = 0; !moved; ++i)
        {
            const candidate = await p.probe(`control Node ${i}`);
            if(candidate.error)
                break;
            if(candidate.rect.w === node.rect.w && candidate.rect.h === node.rect.h
                && Math.abs(candidate.rect.x - node.rect.x - 24) <= 1 && Math.abs(candidate.rect.y - node.rect.y - 16) <= 1)
                moved = candidate;
        }
        expect('keeps', moved !== null,
            `a (24, 16) drag of a node's title bar at (${node.rect.x}, ${node.rect.y}) left no node of its size 24, 16 on `
            + `at zoom ${graph.zoom}`, 'the node moved by the finger');

        const from = await p.probe('pin Normal 0');
        const to = await p.probe('pin Scale 0');
        if(from.error || to.error)
            fail('keeps', 'NodeGraphDemo has no "Normal" or "Scale" pin');
        else
        {
            const links = (await p.probe('control NodeGraph 0')).links;
            await p.drag(from.marker, { x: to.marker.x - from.marker.x, y: to.marker.y - from.marker.y }, { steps: 10 });
            const after = await p.probe('control NodeGraph 0');
            const scale = await p.probe('pin Scale 0');
            expect('keeps', after.links === links + 1 && scale.links === 1 && after.pan.x === graph.pan.x
                && after.pan.y === graph.pan.y,
                `a drag from the "Normal" pin to "Scale" left ${after.links} link(s) (${links + 1} wanted), "Scale" with `
                + `${scale.links}, or panned the graph to (${after.pan.x}, ${after.pan.y})`, 'a pin drag made a link');
        }
        await p.close('keeps');
    }
}

// A point of \a rect whose hit test \a bare takes for empty space, on a 7 px grid
async function emptyPoint(p, rect, bare)
{
    for(let y = rect.y + Math.floor(rect.h/2); y < rect.y + rect.h - 4; y += 7)
        for(let x = rect.x + Math.floor(rect.w/3); x < rect.x + rect.w - 4; x += 7)
        {
            const at = await p.probe(`at ${x} ${y}`);
            if(bare(at.chain))
                return { x, y };
        }
    return null;
}

const bareGraph = chain => chain.includes('NodeGraph') && !chain.some(type => ['Node', 'Link', 'Pin', 'NodeTitleBar'].includes(type));

async function checkPan(browser, base)
{
    {
        const p = await Page.open(browser, base, 'ZoomPanelDemo');
        const panel = await p.probe('control ZoomPanel 0');
        // nothing between the hit control and the panel but plain containers
        const at = await emptyPoint(p, panel.rect, chain =>
        {
            const i = chain.indexOf('ZoomPanel');
            return i >= 0 && chain.slice(0, i).every(type => type === 'Base');
        });
        if(!at)
            fail('zoom_pan', 'no empty point in the ZoomPanel');
        else
        {
            await p.probe('reset');
            await p.drag(at, { x: 40, y: -30 });
            const after = await p.probe('control ZoomPanel 0');
            const frames = await p.probe('frames');
            expect('zoom_pan', after.pan.x === panel.pan.x + 40 && after.pan.y === panel.pan.y - 30
                && frames.total.passes === 0 && frames.total.frames > 0,
                `a (40, -30) drag from (${at.x}, ${at.y}) panned by (${after.pan.x - panel.pan.x}, `
                + `${after.pan.y - panel.pan.y}) in ${frames.total.frames} frame(s) with ${frames.total.passes} layout pass(es)`,
                `panned by the finger in ${frames.total.frames} frames, no layout pass`);
        }
        await p.close('zoom_pan');
    }
    {
        const p = await Page.open(browser, base, 'NodeGraphDemo');
        const graph = await p.probe('control NodeGraph 0');
        const at = await emptyPoint(p, graph.rect, bareGraph);
        if(!at)
            fail('graph_pan', 'no empty point in the NodeGraph');
        else
        {
            await p.touch('touchStart', at);
            for(let i = 1; i <= 4; ++i)
            {
                await p.touch('touchMove', { x: at.x + 10*i, y: at.y - 7.5*i });
                await sleep(16);
            }
            await p.settle();
            const marquee = await p.probe('control NodeGraphMarquee 0');
            await sleep(250);
            await p.touch('touchEnd', { x: at.x + 40, y: at.y - 30 });
            await p.settle();
            const after = await p.probe('control NodeGraph 0');
            expect('graph_pan', after.pan.x === graph.pan.x + 40 && after.pan.y === graph.pan.y - 30 && marquee.error,
                `a (40, -30) drag on empty space panned by (${after.pan.x - graph.pan.x}, ${after.pan.y - graph.pan.y})`
                + `${marquee.error ? '' : ' with a marquee shown'}`, 'panned by the finger, no marquee');
        }
        await p.close('graph_pan');
    }
}

async function checkLongPress(browser, base, out)
{
    {
        const p = await Page.open(browser, base, 'ContextMenuDemo');
        const box = await p.probe('control GroupBox 0');
        const at = centre(box.rect);
        await p.touch('touchStart', at);
        await sleep(800);
        await p.settle();
        const held = await p.probe('state');
        await p.touch('touchEnd', at);
        await p.settle();
        const lifted = await p.probe('state');
        if(out)
            await p.page.screenshot({ path: join(out, 'long-press.png') });
        expect('long_press', held.menu !== null && held.menu.x === Math.floor(at.x) && held.menu.y === Math.floor(at.y)
            && lifted.menu !== null && lifted.hovered === null,
            `a finger held 0.8 s at (${at.x}, ${at.y}) on the Document box opened ${held.menu ? `a menu at (${held.menu.x}, `
            + `${held.menu.y})` : 'no menu'}, which the lift ${lifted.menu ? 'left open' : 'closed'}, ${lifted.hovered} hovered`,
            'the menu opened at the finger and stayed open');
        await p.close('long_press');
    }
    {
        const p = await Page.open(browser, base, 'NodeGraphDemo');
        const graph = await p.probe('control NodeGraph 0');
        const at = await emptyPoint(p, graph.rect, bareGraph);
        if(!at)
            fail('graph_long_press', 'no empty point in the NodeGraph');
        else
        {
            await p.page.mouse.click(at.x, at.y, { button: 'right' });
            await p.settle();
            const right = await p.probe('control NodeGraph 0');
            const rightMenu = (await p.probe('state')).menu;
            await p.page.keyboard.press('Escape');
            await p.page.mouse.move(at.x + 200, at.y + 100);
            await p.settle();
            await p.touch('touchStart', at);
            await sleep(800);
            await p.touch('touchEnd', at);
            await p.settle();
            const long = await p.probe('control NodeGraph 0');
            const longMenu = (await p.probe('state')).menu;
            expect('graph_long_press', rightMenu !== null && longMenu !== null && long.context.x === right.context.x
                && long.context.y === right.context.y,
                `a right click opened ${rightMenu ? 'a' : 'no'} menu at graph point (${right.context.x}, ${right.context.y}), `
                + `a long press ${longMenu ? 'a' : 'no'} menu at (${long.context.x}, ${long.context.y})`,
                `the graph point (${long.context.x}, ${long.context.y}), as a right click's`);
        }
        await p.close('graph_long_press');
    }
}

async function checkIme(browser, base, out)
{
    const p = await Page.open(browser, base, 'TextBoxDemo');
    await p.probe('watch');
    const box = await p.probe('control TextBox 0');
    const clip = { x: box.rect.x, y: box.rect.y, width: box.rect.w, height: box.rect.h };
    await p.tap({ x: box.rect.x + box.rect.w - 5, y: centre(box.rect).y });
    const focused = await p.probe('state');
    const focus = await p.page.evaluate(() => ({ field: document.activeElement === Module.gweniInput.field,
        inTouchEnd: gweniTouchProbe.focusInTouchEnd, outside: gweniTouchProbe.focusOutside }));
    expect('ime', focused.focus === 'TextBox' && focus.field && focus.inTouchEnd === 1 && focus.outside === 0,
        `a tap on the box focused ${focused.focus}, the textarea ${focus.field ? '' : 'not '}focused, `
        + `${focus.inTouchEnd} time(s) inside the touchend's task and ${focus.outside} outside it (1 and 0 wanted)`,
        'the tap focused the textarea inside its touchend');
    const text = focused.text ?? '';
    const before = await p.page.screenshot({ clip });

    await p.cdp.send('Input.imeSetComposition', { text: '日本', selectionStart: 2, selectionEnd: 2 });
    await p.settle();
    const composing = await p.probe('state');
    const shown = await p.page.screenshot({ clip });
    if(out)
        writeFileSync(join(out, 'composing.png'), shown);
    expect('ime', composing.composing === '日本' && composing.text === text && composing.changes === 0 && !shown.equals(before),
        `the composition "日本" left the box composing "${composing.composing}" with text "${composing.text}" `
        + `(${JSON.stringify(text)} wanted), ${composing.changes} onTextChanged, pixels ${shown.equals(before) ? 'unchanged' : 'changed'}`,
        'composing inline, not the text, no event');

    await p.cdp.send('Input.insertText', { text: '日本語' });
    await p.settle();
    const committed = await p.probe('state');
    const value = await p.page.evaluate(() => Module.gweniInput.field.value);
    expect('ime', committed.text === text + '日本語' && committed.composing === '' && committed.changes === 1 && value === '',
        `the commit left "${committed.text}" (${JSON.stringify(text + '日本語')} wanted), composing "${committed.composing}", `
        + `${committed.changes} onTextChanged (1 wanted), the textarea holding "${value}"`,
        'committed exactly, one onTextChanged');

    // gweni ending a composition itself ends the textarea's: "hel" composing
    // in this box, a tap on the second one commits it here, and the browser's
    // composition is over too, so the next word starts afresh in the other
    // box instead of carrying "hel" on into it (a phone's keyboard composes
    // every word, so that would type it twice)
    const other = await p.probe('control TextBox 1');
    const otherText = other.text ?? '';
    await p.page.evaluate(() =>
    {
        window.gweniCompositionEnds = 0;
        window.gweniCompositionStarts = 0;
        Module.gweniInput.field.addEventListener('compositionend', () => { ++window.gweniCompositionEnds; });
        Module.gweniInput.field.addEventListener('compositionstart', () => { ++window.gweniCompositionStarts; });
    });
    await p.cdp.send('Input.imeSetComposition', { text: 'hel', selectionStart: 3, selectionEnd: 3 });
    await p.settle();
    await p.tap({ x: other.rect.x + other.rect.w - 5, y: centre(other.rect).y });
    const across = await p.page.evaluate(() => ({ field: document.activeElement === Module.gweniInput.field,
        composing: Module.gweniInput.composing, value: Module.gweniInput.field.value, ends: window.gweniCompositionEnds }));
    const firstBox = await p.probe('control TextBox 0');
    expect('ime', firstBox.text === text + '日本語hel' && across.ends === 1 && !across.composing && across.value === '' && across.field,
        `a tap on another box mid-composition left the first box "${firstBox.text}" (${JSON.stringify(text + '日本語hel')} wanted), `
        + `the textarea ${across.field ? '' : 'not '}focused, composing ${across.composing}, holding "${across.value}", `
        + `${across.ends} compositionend(s) (1 wanted)`, 'a tap on another box ended the browser\'s composition');
    const starts = await p.page.evaluate(() => window.gweniCompositionStarts);
    await p.cdp.send('Input.imeSetComposition', { text: 'wo', selectionStart: 2, selectionEnd: 2 });
    await p.settle();
    await p.cdp.send('Input.insertText', { text: 'word' });
    await p.settle();
    const second = await p.probe('control TextBox 1');
    const firstAfter = await p.probe('control TextBox 0');
    const started = await p.page.evaluate(() => window.gweniCompositionStarts) - starts;
    expect('ime', second.text === otherText + 'word' && firstAfter.text === firstBox.text && started === 1,
        `the next word left the other box "${second.text}" (${JSON.stringify(otherText + 'word')} wanted) and the first `
        + `"${firstAfter.text}" (${JSON.stringify(firstBox.text)} wanted), in ${started} new composition(s) (1 wanted)`,
        'the next word went into the other box alone, once, a composition of its own');

    // a Shift pressed before a composition and let go during it reaches
    // emscripten-glfw's listener (on document), so it is not left held; and
    // neither its press nor its release commits the composition
    await p.page.evaluate(() =>
    {
        window.gweniShiftUps = 0;
        document.addEventListener('keyup', e => { if(e.key === 'Shift') ++window.gweniShiftUps; });
    });
    await p.cdp.send('Input.dispatchKeyEvent', { type: 'rawKeyDown', key: 'Shift', code: 'ShiftLeft',
        windowsVirtualKeyCode: 16, modifiers: 8 });
    await p.settle();
    await p.cdp.send('Input.imeSetComposition', { text: 'ka', selectionStart: 2, selectionEnd: 2 });
    await p.settle();
    await p.cdp.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Shift', code: 'ShiftLeft', windowsVirtualKeyCode: 16 });
    await p.settle();
    const shifted = await p.probe('state');
    const shiftUps = await p.page.evaluate(() => window.gweniShiftUps);
    expect('ime', shiftUps === 1 && shifted.composing === 'ka',
        `a Shift let go during a composition reached the document ${shiftUps} time(s) (1 wanted), the box composing `
        + `"${shifted.composing}" ("ka" wanted)`, 'a Shift held across a composition went up, committing nothing');
    await p.cdp.send('Input.insertText', { text: '' });
    await p.settle();

    await p.tap({ x: 600, y: 470 });
    await sleep(50);
    const left = await p.page.evaluate(() => ({ field: document.activeElement === Module.gweniInput.field,
        blurs: Module.gweniInput.blurs }));
    const state = await p.probe('state');
    expect('ime', !left.field && left.blurs === 1 && state.focus !== 'TextBox',
        `a tap off the box left ${state.focus} focused, the textarea ${left.field ? 'focused' : 'blurred'} `
        + `(${left.blurs} blur(s), 1 wanted)`, 'the focus taken away blurred the textarea');
    await p.close('ime');

    // a phone's keyboard composes every word, a password box shows none of
    // it: the word must still be the box's when a tap on a button takes the
    // focus (Log in, Send), not dropped with the browser's late compositionend
    const w = await Page.open(browser, base, 'TextBoxDemo');
    const secret = await w.probe('control PasswordTextBox 0');
    await w.tap({ x: secret.rect.x + secret.rect.w - 5, y: centre(secret.rect).y });
    const inPassword = await w.page.evaluate(() => ({ password: document.activeElement === Module.gweniInput.password,
        type: document.activeElement && document.activeElement.type }));
    expect('password', inPassword.password,
        `a tap on the PasswordTextBox focused a ${inPassword.type} field, not the page's password field`,
        'the password box focused the page\'s password field');
    for(const word of ['s', 'se', 'sec', 'secr', 'secre', 'secret'])
    {
        await w.cdp.send('Input.imeSetComposition', { text: word, selectionStart: word.length, selectionEnd: word.length });
        await w.settle();
    }
    const copy = await w.probe('button Copy');
    await w.tap(centre(copy.rect));
    await sleep(50);
    await w.settle();
    const held = await w.probe('control PasswordTextBox 0');
    const field = await w.page.evaluate(() => ({ composing: Module.gweniInput.composing,
        value: Module.gweniInput.password.value }));
    expect('password', held.text === 'secret' && !field.composing && field.value === '',
        `"secret" composing and a tap on Copy left the PasswordTextBox "${held.text}" ("secret" wanted), the `
        + `password field composing ${field.composing}, holding "${field.value}"`,
        'the word composing went into the password box as the focus left');
    await w.close('password');

    const d = await Page.open(browser, base, 'TextBoxDemo', false);
    const first = await d.probe('control TextBox 0');
    await d.page.mouse.click(first.rect.x + first.rect.w - 5, centre(first.rect).y);
    await d.settle();
    const start = (await d.probe('state')).text ?? '';
    await d.page.keyboard.press('End');
    await d.page.keyboard.type('ab');
    await d.page.keyboard.press('Backspace');
    await d.settle();
    const typed = await d.probe('state');
    const desk = await d.page.evaluate(() => ({ field: document.activeElement === Module.gweniInput.field,
        value: Module.gweniInput.field.value }));
    expect('desktop', typed.text === start + 'a' && desk.field && desk.value === '',
        `"ab" and Backspace typed into the box left "${typed.text}" (${JSON.stringify(start + 'a')} wanted), the textarea `
        + `${desk.field ? '' : 'not '}focused holding "${desk.value}"`, 'typed once, through keydown');

    // a second click on the focused box that moves no caret: emscripten-glfw
    // focuses the canvas on its mousedown, and the field gets it back, so a
    // desktop input method still composes in it
    await sleep(800);
    await d.page.mouse.click(first.rect.x + first.rect.w - 5, centre(first.rect).y);
    await d.settle();
    const refocused = await d.page.evaluate(() => document.activeElement === Module.gweniInput.field);
    await d.cdp.send('Input.imeSetComposition', { text: 'ka', selectionStart: 2, selectionEnd: 2 });
    await d.settle();
    const composed = await d.probe('state');
    expect('desktop', refocused && composed.composing === 'ka',
        `a second click on the focused box left the textarea ${refocused ? '' : 'not '}focused, the box composing `
        + `"${composed.composing}" ("ka" wanted)`, 'a click that moves no caret gave the textarea the focus back');
    await d.cdp.send('Input.insertText', { text: '' });
    await d.settle();
    await d.close('desktop');
}

async function main()
{
    const args = process.argv.slice(2);
    let out = null;
    let dir = null;
    for(let i = 0; i < args.length; ++i)
    {
        if(args[i] === '--out' && i + 1 < args.length)
            out = resolve(args[++i]);
        else if(!dir && !args[i].startsWith('--'))
            dir = resolve(args[i]);
        else
        {
            console.error(usage);
            process.exit(2);
        }
    }
    if(!dir)
    {
        console.error(usage);
        process.exit(2);
    }
    if(out)
        mkdirSync(out, { recursive: true });

    const server = await serve(dir);
    const base = `http://127.0.0.1:${server.address().port}`;
    const { chromium } = loadPlaywright();
    const browser = await chromium.launch(launchOptions(true));
    try
    {
        await checkTap(browser, base);
        await checkDrag(browser, base);
        await checkFling(browser, base);
        await checkKeeps(browser, base);
        await checkPan(browser, base);
        await checkLongPress(browser, base, out);
        await checkIme(browser, base, out);
    }
    catch(e)
    {
        fail('run', e.stack || String(e));
    }
    finally
    {
        await browser.close();
        server.close();
    }
    if(failures)
    {
        console.error(`touch.mjs: ${failures} check(s) failed`);
        process.exit(1);
    }
    console.log('touch.mjs: OK, every check passed');
}

main();
