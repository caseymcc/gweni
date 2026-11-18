//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  web_site_smoke's browser half: loads the pages of a site staged by
//  tools/build_site.sh in the image's headless Chromium, as a visitor would.
//
//      node tests/web/site.mjs [--out <dir>] <staged site dir>
//      node tests/web/site.mjs [--out <dir>] --url <base URL>
//
//  It serves the directory on a loopback port, as a static host serves it (or,
//  with --url, takes the site from a server already serving it, e.g.
//  python3 -m http.server -d site), and opens, each in a fresh browser context
//  at 1280x800 CSS px:
//    index.html       the landing page: every image it shows loads
//    demo.html        the OpenGLCore demo's window, with the stats overlay:
//                     its argv starts with the page's default --stats-overlay
//                     (Module.gweniDefaultArgs, web/pre.js) and the app says
//                     on stdout that the overlay is open
//    demo.html?stats-overlay=off
//                     the same window without it: the default and then
//                     --stats-overlay off, and no such line
//    controltest.html?control=ButtonDemo, ?control=TextBoxDemo and
//    ?control=ShowcaseButtonsDemo&scale=2
//                     ControlTest's window; the picker shows the control the
//                     URL names
//    demo.html?depth-view
//                     the demo with the stats overlay and the depth view:
//                     --depth-view after the default --stats-overlay in its
//                     argv, the app saying on stdout that both are open, and
//                     its gweni_web_depth_view export reading the view open
//                     and the shell's Depth button on; then the header's "Z
//                     spacing" slider (checkSpacing, below), and Escape
//                     disabling it and the export's open enabling it again
//    controltest.html?control=ButtonDemo&depth-view
//                     the gallery with the view open (the export) and its
//                     "Depth view" checkbox checked; unchecking it closes the
//                     view and takes depth-view out of the URL, checking it
//                     again opens the view and puts it back, the "Z spacing"
//                     slider disabled while it is closed; the slider as on the
//                     demo (checkSpacing: enabled at x1 with the view open, x4
//                     set through it read back from the app's
//                     gweni_web_depth_view_spacing export, a + key press and
//                     one Alt+wheel notch over the canvas each moving it by
//                     exactly one ZSpacingStep with the page neither scrolled
//                     nor zoomed, R putting it back at x1); and picking
//                     SliderDemo then loads ?control=SliderDemo&depth-view,
//                     the view open and the checkbox checked again
//    controltest.html?control=UnicodeTextDemo
//                     the gallery showing CJK: it fetches the lazy package's
//                     CJK face (resources-lazy/) once and it arrives
//                     (ControlTest's gweni_web_glyph_fallback), and the face's
//                     license is under licenses/
//  Every other page shows no CJK and must ask for nothing under
//  resources-lazy/: the face is fetched only for text that shows it.
//  A page with a window must draw frames (gweniRun.frames, web/pre.js) on a
//  WebGL 2 context on ANGLE's SwiftShader with at least 24 depth bits
//  (run.mjs' conditions, through probe.mjs' launchOptions), write nothing to
//  stderr (the page shows it in its #output panel, under the canvas) and its
//  canvas must not be one colour once it has drawn and settled (frames.mjs:
//  the window's loop is idle, drawing only while it has work). On the scale=2 page it then
//  picks SliderDemo, which must load controltest.html?control=SliderDemo&scale=2
//  and draw it. Every link and image of every page that points into the site
//  (relative URLs: the site is served from a subdirectory on GitHub Pages)
//  must be there. Every page says which commit it was built from: one
//  "built from <short sha>[-dirty] (<YYYY-MM-DD>)" stamp (without the date
//  where git could not read it, or "built from unknown"), the sha a link to its
//  commit, in index.html's footer and in the app pages' header (build_site.sh
//  fills it in).
//
//  It fails, one "site.mjs: FAIL: <page>: ..." line each, on any console error,
//  uncaught exception, failed request or HTTP error status (a 404 for a file
//  the site lacks), a page that ends its run (the window exits) or draws no
//  frames, a missing or wrong WebGL context, a window that writes to stderr, a
//  blank canvas, a picker that does not show its control, a page without its
//  built-from stamp, and a link into the site that is not there. It ends with
//  "site.mjs: OK, every page loaded" when everything passed. --out keeps a
//  screenshot of each page there, as a visitor sees it.
//
//  The size budget is tools/build_site.sh's own check (tools/site/sizes.mjs),
//  which web_site_smoke runs before this (tests/web/site.cmake).
//

import { createServer } from 'node:http';
import { mkdirSync, readFileSync, statSync } from 'node:fs';
import { extname, join, normalize, resolve, sep } from 'node:path';

import { settle } from './frames.mjs';
import { isSwiftShader, launchOptions, loadPlaywright } from './probe.mjs';
import { decodePng } from './png.mjs';

const usage = 'usage: node site.mjs [--out <dir>] <staged site dir> | --url <where a server serves it>';
const viewport = { width: 1280, height: 800 };

let failures = 0;
function fail(page, message)
{
    ++failures;
    console.error(`site.mjs: FAIL: ${page}: ${message}`);
}
function pass(page, detail)
{
    console.log(`site.mjs: ${page}: ok${detail ? `, ${detail}` : ''}`);
}

const mimeTypes = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'text/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
    '.png': 'image/png',
    '.txt': 'text/plain',
};

// The site on a loopback port, nothing outside it, as a static host serves it:
// a directory's index.html for the directory, 404 for anything else missing.
function serve(root)
{
    const server = createServer((request, response) =>
    {
        let path = normalize(join(root, decodeURIComponent(new URL(request.url, 'http://localhost').pathname)));
        let body = null;
        try
        {
            if(path === root || path.startsWith(root + sep))
            {
                if(statSync(path).isDirectory())
                    path = join(path, 'index.html');
                body = readFileSync(path);
            }
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

// The number of distinct colours in a PNG, up to 2.
function colourCount(png)
{
    const { width, height, channels, pixels } = decodePng(png);
    for(let i = channels; i < width*height*channels; i += channels)
        for(let c = 0; c < channels; ++c)
            if(pixels[i + c] !== pixels[c])
                return 2;
    return 1;
}

class Visit
{
    constructor(browser, base, out)
    {
        this.browser = browser;
        this.base = base;
        this.out = out;
    }

    // Opens a page of the site and records every error it shows into
    // this.errors[label] (read by check()).
    async open(label, path)
    {
        const context = await this.browser.newContext({ viewport });
        const page = await context.newPage();
        const errors = [];
        page.on('console', msg => { if(msg.type() === 'error') errors.push(`console error: ${msg.text()}`); });
        page.on('pageerror', err => errors.push(`uncaught exception: ${err.stack || err.message}`));
        page.on('requestfailed', request => errors.push(`request failed: ${request.url()}: ${request.failure()?.errorText}`));
        page.on('response', response => { if(response.status() >= 400) errors.push(`HTTP ${response.status()} for ${response.url()}`); });
        const lazy = [];
        page.on('request', request => { if(request.url().includes('/resources-lazy/')) lazy.push(request.url()); });
        await page.goto(`${this.base}/${path}`, { waitUntil: 'commit' });
        return { page, context, errors, label, lazy, showsCjk: false };
    }

    // Fails for every error the page showed so far, and for a request for the
    // lazy package (resources-lazy/, the CJK face) from a page that shows no
    // CJK, and forgets them.
    report(visit)
    {
        for(const e of visit.errors)
            fail(visit.label, e);
        visit.errors.length = 0;
        if(!visit.showsCjk && visit.lazy.length)
            fail(visit.label, `shows no CJK but asked for the lazy package: ${visit.lazy.join(', ')}`);
        visit.lazy.length = 0;
    }

    // Every link and image of the page that points into the site is there.
    async checkLinks(visit)
    {
        const urls = await visit.page.evaluate(() =>
            [...document.querySelectorAll('a[href], img[src], link[href], script[src]')].map(e => e.href || e.src));
        let missing = 0;
        for(const url of new Set(urls))
        {
            if(!url.startsWith(this.base + '/'))
                continue;
            const response = await fetch(url.split('#')[0]);
            if(response.status !== 200)
            {
                fail(visit.label, `${url.slice(this.base.length + 1)} is not there (HTTP ${response.status})`);
                ++missing;
            }
        }
        return missing === 0;
    }

    // The page's one "built from ..." stamp (build_site.sh's), inside \a where
    // (the landing page's footer, an app page's header), its sha a link to the
    // commit; true if it is there.
    async checkStamp(visit, where)
    {
        const stamps = await visit.page.evaluate(where => [...document.querySelectorAll('.built-from')].map(e => ({
            text: e.textContent.trim(), inside: e.closest(where) !== null,
            links: [...e.querySelectorAll('a[href]')].map(a => ({ href: a.href, text: a.textContent }))})), where);
        if(stamps.length !== 1)
        {
            fail(visit.label, `has ${stamps.length} built-from stamps, not one (tools/build_site.sh fills the page's gweni:built-from comment)`);
            return false;
        }
        const [stamp] = stamps;
        const match = /^built from (?:([0-9a-f]{7})(?:-dirty)?(?: \(\d{4}-\d{2}-\d{2}\))?|unknown)$/.exec(stamp.text);
        if(!match)
        {
            fail(visit.label, `its built-from stamp reads "${stamp.text}", not "built from <short sha> (<YYYY-MM-DD>)"`);
            return false;
        }
        if(!stamp.inside)
        {
            fail(visit.label, `its built-from stamp is not in its ${where}`);
            return false;
        }
        const sha = match[1];
        const commit = 'https://github.com/caseymcc/gweni/commit/';
        if(sha !== undefined && !(stamp.links.length === 1 && stamp.links[0].href.startsWith(commit + sha)
            && /^[0-9a-f]{40,64}$/.test(stamp.links[0].href.slice(commit.length))))
        {
            fail(visit.label, `its built-from stamp's ${sha} is not one link to its commit: ${JSON.stringify(stamp.links)}`);
            return false;
        }
        return true;
    }

    // Waits for an app page's window to draw a few frames and checks its
    // context and canvas; true if it all held.
    async checkWindow(visit, name)
    {
        const { page, label } = visit;
        // an error (a missing .data, say) stops the wait: the run may never start
        const deadline = Date.now() + 300000;
        for(;;)
        {
            const state = await page.evaluate(() => window.gweniRun ? { done: gweniRun.done, frames: gweniRun.frames } : null);
            if(state && (state.done || state.frames >= 1))
                break;
            if(visit.errors.length)
            {
                fail(label, `drew ${state ? state.frames : 0} frames before the error(s) below`);
                return false;
            }
            if(Date.now() > deadline)
            {
                fail(label, 'drew no frames in 300 s');
                return false;
            }
            await new Promise(resolvePromise => setTimeout(resolvePromise, 100));
        }
        // the window's loop is idle: it draws until it has nothing to do
        await settle(page, 5, 300000);
        const run = await page.evaluate(() => ({ done: gweniRun.done, exitCode: gweniRun.exitCode, aborted: gweniRun.aborted,
            frames: gweniRun.frames, webgl: gweniRun.webgl, output: gweniRun.output.map(l => l.text),
            stderr: gweniRun.output.filter(l => l.stream === 'stderr').map(l => l.text) }));
        if(run.done)
        {
            fail(label, `the window's run ended (exit ${run.exitCode}${run.aborted ? `, aborted: ${run.aborted}` : ''}): ${run.output.join(' | ')}`);
            return false;
        }
        if(!run.webgl || !run.webgl.webgl2)
        {
            fail(label, 'made no WebGL 2 context');
            return false;
        }
        if(!isSwiftShader(run.webgl.renderer))
        {
            fail(label, `asked for ANGLE on SwiftShader, got ${run.webgl.renderer}`);
            return false;
        }
        if(!(run.webgl.depthBits >= 24))
        {
            fail(label, `its WebGL 2 context has DEPTH_BITS ${run.webgl.depthBits}, the z layering needs at least 24`);
            return false;
        }
        const png = await page.locator('#canvas').screenshot();
        if(this.out)
            await page.screenshot({ path: join(this.out, `${name}.png`) });
        // the page shows stderr in its #output panel, under the canvas: a
        // complete site has nothing to say there (checked after the
        // screenshot, so --out keeps what a visitor sees)
        if(run.stderr.length)
        {
            fail(label, `wrote ${run.stderr.length} line(s) to stderr, shown in #output: ${run.stderr.join(' | ')}`);
            return false;
        }
        if(colourCount(png) === 1)
        {
            fail(label, `its canvas is one colour after ${run.frames} frames`);
            return false;
        }
        const backing = await page.evaluate(() => `${Module.canvas.width}x${Module.canvas.height}`);
        this.detail = `${run.frames} frames on ${run.webgl.version}, backing store ${backing}`;
        return true;
    }

    async landing()
    {
        const visit = await this.open('index.html', 'index.html');
        await visit.page.waitForLoadState('load');
        const images = await visit.page.evaluate(() =>
            [...document.images].map(img => ({ src: img.getAttribute('src'), loaded: img.complete && img.naturalWidth > 0 })));
        let ok = await this.checkStamp(visit, 'footer');
        ok = (await this.checkLinks(visit)) && ok;
        for(const img of images)
        {
            if(!img.loaded)
            {
                fail(visit.label, `the image ${img.src} did not load`);
                ok = false;
            }
        }
        if(this.out)
            await visit.page.screenshot({ path: join(this.out, 'index.png'), fullPage: true });
        const clean = visit.errors.length === 0;
        this.report(visit);
        if(ok && clean)
            pass(visit.label, `${images.length} image(s) shown, every link into the site there`);
        await visit.context.close();
    }

    // The demo page's stats overlay: its argv and what the app said on stdout
    // (the stderr check is checkWindow's).
    async checkOverlay(visit, open)
    {
        const run = await visit.page.evaluate(() => ({ argv: gweniRun.argv,
            stdout: gweniRun.output.filter(l => l.stream === 'stdout').map(l => l.text) }));
        const said = run.stdout.some(line => line.startsWith('OpenGLCore: the stats overlay is open'));
        const argv = JSON.stringify(run.argv);
        if(run.argv[0] !== '--stats-overlay')
        {
            fail(visit.label, `its argv ${argv} does not start with the page's default --stats-overlay`);
            return false;
        }
        if(said !== open)
        {
            fail(visit.label, `argv ${argv}, and the stats overlay is ${said ? 'open' : 'not open'}`);
            return false;
        }
        this.detail += `, argv ${argv}, stats overlay ${open ? 'open' : 'closed'}`;
        return true;
    }

    // The depth view on a page opened with ?depth-view: open (the app's
    // gweni_web_depth_view export, -1 only reads), --depth-view in its argv
    // and the app saying so on stdout; on the demo the shell's Depth button
    // on. On the gallery the "Depth view" checkbox is checked, a click on it
    // closes the view, unchecks it and takes depth-view out of the URL, and a
    // second opens the view, checks it and puts depth-view back; nothing
    // written to stderr meanwhile.
    async checkDepthView(visit, gallery)
    {
        const { page } = visit;
        const state = () => page.evaluate(() => Module.ccall('gweni_web_depth_view', 'string', ['number'], [-1]));
        const inUrl = () => new URL(page.url()).searchParams.has('depth-view');
        // drawn and settled (frames.mjs: an idle page draws no more)
        const frames = () => settle(page, 3);
        const run = await page.evaluate(() => ({ argv: gweniRun.argv,
            stdout: gweniRun.output.filter(l => l.stream === 'stdout').map(l => l.text) }));
        const first = await state();
        if(!run.argv.includes('--depth-view'))
        {
            fail(visit.label, `its argv ${JSON.stringify(run.argv)} has no --depth-view`);
            return false;
        }
        if(!run.stdout.some(line => line.startsWith('the depth view is open')) || !first.startsWith('view=open')
            || (!gallery && !first.includes(' button=on')))
        {
            fail(visit.label, `the depth view did not open as ?depth-view asks: ${first}`);
            return false;
        }
        let detail = `depth view open (${first.split(' rect=')[0]})`;
        if(gallery)
        {
            const box = page.locator('#depth-view');
            const checkedAtLoad = await box.isChecked();
            await box.click();
            await frames();
            const off = { state: await state(), checked: await box.isChecked(), url: inUrl(),
                spacing: await page.locator('#z-spacing').isDisabled() };
            await box.click();
            await frames();
            const on = { state: await state(), checked: await box.isChecked(), url: inUrl(),
                spacing: await page.locator('#z-spacing').isDisabled() };
            if(!checkedAtLoad)
            {
                fail(visit.label, 'the "Depth view" checkbox is not checked with ?depth-view');
                return false;
            }
            if(off.state !== 'view=closed' || off.checked || off.url || !off.spacing)
            {
                fail(visit.label, `unchecking "Depth view" left ${off.state}, checked ${off.checked}, depth-view in the URL ${off.url}, Z spacing disabled ${off.spacing}`);
                return false;
            }
            if(on.state !== 'view=open' || !on.checked || !on.url || on.spacing)
            {
                fail(visit.label, `checking "Depth view" again left ${on.state}, checked ${on.checked}, depth-view in the URL ${on.url}, Z spacing disabled ${on.spacing}`);
                return false;
            }
            detail += ', the checkbox closes and opens it, the URL and the Z spacing slider following';
        }
        const spacing = await this.checkSpacing(visit, gallery);
        if(!spacing)
            return false;
        detail += `, ${spacing}`;
        const stderr = await page.evaluate(() => gweniRun.output.filter(l => l.stream === 'stderr').map(l => l.text));
        if(stderr.length)
        {
            fail(visit.label, `wrote ${stderr.length} line(s) to stderr, shown in #output: ${stderr.join(' | ')}`);
            return false;
        }
        this.detail += `, ${detail}`;
        return true;
    }

    // The header's "Z spacing" slider with the depth view open: enabled, at x1
    // (log10 0); set to x4 it sets the app's factor to 4, read back through
    // gweni_web_depth_view_spacing (0 only reads); a + key press on the canvas
    // moves it by exactly one step (log10 of DepthView::ZSpacingStep, 1.25)
    // and one Alt+wheel notch towards the user (mouse.wheel(0, 100): 100 px,
    // one step to emscripten-glfw) one step back, the page neither scrolling
    // nor zooming; R puts it back at x1. On the demo, which has no checkbox,
    // Escape then closes the view and disables it, and the export opens it
    // again and enables it. Returns what passed, or null after a fail().
    async checkSpacing(visit, gallery)
    {
        const { page } = visit;
        const step = Math.log10(1.25);
        const slider = () => page.evaluate(() => {
            const input = document.getElementById('z-spacing');
            return input ? { value: Number(input.value), disabled: input.disabled } : null;
        });
        const factor = async () => {
            const state = await page.evaluate(() => Module.ccall('gweni_web_depth_view_spacing', 'string', ['number'], [0]));
            const found = /^view=open factor=([0-9.e+-]+)$/.exec(state);
            return found ? Number(found[1]) : NaN;
        };
        const view = () => page.evaluate(() => ({ scrollY: window.scrollY, scale: window.visualViewport.scale }));
        const near = (a, b) => Math.abs(a - b) < 1e-4;
        const frames = () => settle(page, 3);

        const start = await slider();
        if(!start || start.disabled || !near(start.value, 0) || !near(await factor(), 1))
        {
            fail(visit.label, `with the view open the Z spacing slider is ${JSON.stringify(start)}, the factor ${await factor()}: expected enabled at x1`);
            return null;
        }
        // moved by the visitor (an input event, as a drag sends)
        await page.evaluate(value => {
            const input = document.getElementById('z-spacing');
            input.value = String(value);
            input.dispatchEvent(new Event('input', { bubbles: true }));
            input.dispatchEvent(new Event('change', { bubbles: true }));
        }, Math.log10(4));
        await frames();
        const moved = await factor();
        if(!near(moved, 4))
        {
            fail(visit.label, `the Z spacing slider at x4 gave the app a factor of ${moved}`);
            return null;
        }
        const before = await view();
        // + on the canvas, which has the keyboard after the slider's change
        const box = await page.locator('#canvas').boundingBox();
        await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
        await page.keyboard.press('+');
        await frames();
        const plus = await slider();
        await page.keyboard.down('Alt');
        await page.mouse.wheel(0, 100);
        await frames();
        await page.keyboard.up('Alt');
        await frames();
        const wheel = await slider();
        const after = await view();
        if(!near(plus.value - Math.log10(4), step))
        {
            fail(visit.label, `a + key press moved the Z spacing slider from ${Math.log10(4)} to ${plus.value}, not one step (${step})`);
            return null;
        }
        if(!near(plus.value - wheel.value, step) || !near(Math.pow(10, wheel.value), await factor()))
        {
            fail(visit.label, `an Alt+wheel notch moved the Z spacing slider from ${plus.value} to ${wheel.value}, not one step back, the factor ${await factor()}`);
            return null;
        }
        if(before.scrollY !== after.scrollY || before.scale !== after.scale)
        {
            fail(visit.label, `Alt+wheel scrolled or zoomed the page: scrollY ${before.scrollY} -> ${after.scrollY}, scale ${before.scale} -> ${after.scale}`);
            return null;
        }
        await page.keyboard.press('r');
        await frames();
        const reset = await slider();
        if(!near(reset.value, 0) || !near(await factor(), 1))
        {
            fail(visit.label, `R left the Z spacing slider at ${reset.value}, the factor ${await factor()}: expected x1`);
            return null;
        }
        if(!gallery)
        {
            await page.keyboard.press('Escape');
            await frames();
            const closed = await slider();
            await page.evaluate(() => Module.ccall('gweni_web_depth_view', 'string', ['number'], [1]));
            await frames();
            const open = await slider();
            if(!closed.disabled || open.disabled)
            {
                fail(visit.label, `the Z spacing slider is ${closed.disabled ? '' : 'not '}disabled once Escape closed the view, and ${open.disabled ? '' : 'not '}enabled once it opened again`);
                return null;
            }
        }
        return 'the Z spacing slider sets the factor, follows +, Alt+wheel and R'
            + (gallery ? '' : ', disabled while the view is closed');
    }

    // \a depth: undefined, or 'demo' or 'gallery' for a page opened with
    // ?depth-view (checkDepthView)
    async app(path, name, control, overlay, depth)
    {
        const visit = await this.open(path, path);
        let ok = await this.checkWindow(visit, name);
        if(ok && overlay !== undefined)
            ok = await this.checkOverlay(visit, overlay);
        if(ok && depth !== undefined)
            ok = await this.checkDepthView(visit, depth === 'gallery');
        const detail = this.detail;
        if(control !== undefined)
        {
            const picked = await visit.page.evaluate(() => document.getElementById('control').value);
            if(picked !== control)
            {
                fail(visit.label, `the picker shows "${picked}", not ${control}`);
                ok = false;
            }
        }
        ok = (await this.checkStamp(visit, 'header')) && ok;
        ok = (await this.checkLinks(visit)) && ok;
        const clean = visit.errors.length === 0;
        this.report(visit);
        if(ok && clean)
            pass(visit.label, detail);
        return visit;
    }

    // The gallery at UnicodeTextDemo, whose CJK line shows: it fetches the lazy
    // CJK face from the site once (resources-lazy/), which arrives and is laid
    // out (ControlTest's gweni_web_glyph_fallback), and the face's license is
    // under licenses/.
    async cjk()
    {
        const visit = await this.open('controltest.html?control=UnicodeTextDemo', 'controltest.html?control=UnicodeTextDemo');
        visit.showsCjk = true;
        let ok = await this.checkWindow(visit, 'UnicodeTextDemo');
        let state = '';
        if(ok)
        {
            await visit.page.waitForFunction(() => /arrived=1|failed=1/.test(Module.ccall('gweni_web_glyph_fallback', 'string', [], [])),
                null, { timeout: 120000, polling: 50 }).catch(() => {});
            await settle(visit.page, 5, 300000);
            state = await visit.page.evaluate(() => Module.ccall('gweni_web_glyph_fallback', 'string', [], []));
            if(!/^fetches=1 arrived=1 failed=0 /.test(state) || visit.lazy.length !== 1)
            {
                fail(visit.label, `the CJK face: ${visit.lazy.length} request(s) (${visit.lazy.join(', ')}), ${state}; expected one fetch that arrived`);
                ok = false;
            }
            const license = await fetch(`${this.base}/licenses/DroidSansFallbackFull-LICENSE.txt`);
            if(license.status !== 200)
            {
                fail(visit.label, `licenses/DroidSansFallbackFull-LICENSE.txt is not there (HTTP ${license.status})`);
                ok = false;
            }
        }
        const clean = visit.errors.length === 0;
        this.report(visit);
        if(ok && clean)
            pass(visit.label, `the CJK face fetched once and arrived (${state}), its license under licenses/`);
        await visit.context.close();
    }

    // The picker on an open gallery page: choosing a control loads its page,
    // the other parameters kept.
    async pick(visit, control, expected, depthView = false)
    {
        const label = `${visit.label} -> ${control}`;
        visit.label = label;
        await Promise.all([visit.page.waitForURL(url => url.search === expected, { timeout: 60000 }).catch(() => null),
            visit.page.selectOption('#control', control)]);
        const search = new URL(visit.page.url()).search;
        let ok = true;
        if(search !== expected)
        {
            fail(label, `the picker loaded ${search}, not ${expected}`);
            ok = false;
        }
        else
            ok = await this.checkWindow(visit, `pick-${control}`);
        // the view opened again from the URL, the checkbox showing it
        if(ok && depthView)
        {
            const state = await visit.page.evaluate(() => Module.ccall('gweni_web_depth_view', 'string', ['number'], [-1]));
            const checked = await visit.page.locator('#depth-view').isChecked();
            if(state !== 'view=open' || !checked)
            {
                fail(label, `after the reload the depth view is ${state}, the checkbox checked ${checked}`);
                ok = false;
            }
            else
                this.detail += ', depth view open, the checkbox checked';
        }
        const clean = visit.errors.length === 0;
        this.report(visit);
        if(ok && clean)
            pass(label, `loaded ${expected}, ${this.detail}`);
    }
}

async function main()
{
    const argv = process.argv.slice(2);
    let out = null, dir = null, url = null;
    for(let i = 0; i < argv.length; ++i)
    {
        if(argv[i] === '--out' && i + 1 < argv.length)
            out = argv[++i];
        else if(argv[i] === '--url' && i + 1 < argv.length)
            url = argv[++i].replace(/\/+$/, '');
        else if(!argv[i].startsWith('--') && dir === null)
            dir = argv[i];
        else
        {
            console.error(usage);
            return 1;
        }
    }
    if((dir === null) === (url === null))
    {
        console.error(usage);
        return 1;
    }
    if(out)
        mkdirSync(out, { recursive: true });

    const { chromium } = loadPlaywright();
    const server = dir === null ? null : await serve(resolve(dir));
    const browser = await chromium.launch(launchOptions());
    try
    {
        const s = new Visit(browser, url ?? `http://127.0.0.1:${server.address().port}`, out);
        await s.landing();
        await (await s.app('demo.html', 'demo', undefined, true)).context.close();
        await (await s.app('demo.html?stats-overlay=off', 'demo-overlay-off', undefined, false)).context.close();
        await (await s.app('controltest.html?control=ButtonDemo', 'ButtonDemo', 'ButtonDemo')).context.close();
        await (await s.app('controltest.html?control=TextBoxDemo', 'TextBoxDemo', 'TextBoxDemo')).context.close();
        const scaled = await s.app('controltest.html?control=ShowcaseButtonsDemo&scale=2', 'ShowcaseButtonsDemo-scale2', 'ShowcaseButtonsDemo');
        await s.pick(scaled, 'SliderDemo', '?control=SliderDemo&scale=2');
        await scaled.context.close();
        await (await s.app('demo.html?depth-view', 'demo-depth-view', undefined, true, 'demo')).context.close();
        const depth = await s.app('controltest.html?control=ButtonDemo&depth-view', 'ButtonDemo-depth-view', 'ButtonDemo', undefined, 'gallery');
        await s.pick(depth, 'SliderDemo', '?control=SliderDemo&depth-view', true);
        await depth.context.close();
        await s.cjk();
    }
    finally
    {
        await browser.close();
        if(server)
            server.close();
    }

    if(failures)
    {
        console.error(`site.mjs: ${failures} failure(s)`);
        return 1;
    }
    console.log('site.mjs: OK, every page loaded');
    return 0;
}

process.exit(await main());
