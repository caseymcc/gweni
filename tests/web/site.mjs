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
//  A page with a window must draw frames (gweniRun.frames, web/pre.js) on a
//  WebGL 2 context on ANGLE's SwiftShader with at least 24 depth bits
//  (run.mjs' conditions, through probe.mjs' launchOptions), write nothing to
//  stderr (the page shows it in its #output panel, under the canvas) and its
//  canvas must not be one colour after 5 frames. On the scale=2 page it then
//  picks SliderDemo, which must load controltest.html?control=SliderDemo&scale=2
//  and draw it. Every link and image of every page that points into the site
//  (relative URLs: the site is served from a subdirectory on GitHub Pages)
//  must be there.
//
//  It fails, one "site.mjs: FAIL: <page>: ..." line each, on any console error,
//  uncaught exception, failed request or HTTP error status (a 404 for a file
//  the site lacks), a page that ends its run (the window exits) or draws no
//  frames, a missing or wrong WebGL context, a window that writes to stderr, a
//  blank canvas, a picker that does not show its control, and a link into the
//  site that is not there. It ends with "site.mjs: OK, every page loaded" when
//  everything passed. --out keeps a screenshot of each page there, as a
//  visitor sees it.
//
//  The size budget is tools/build_site.sh's own check (tools/site/sizes.mjs),
//  which web_site_smoke runs before this (tests/web/site.cmake).
//

import { createServer } from 'node:http';
import { mkdirSync, readFileSync, statSync } from 'node:fs';
import { extname, join, normalize, resolve, sep } from 'node:path';

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
        await page.goto(`${this.base}/${path}`, { waitUntil: 'commit' });
        return { page, context, errors, label };
    }

    // Fails for every error the page showed so far, and forgets them.
    report(visit)
    {
        for(const e of visit.errors)
            fail(visit.label, e);
        visit.errors.length = 0;
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
            if(state && (state.done || state.frames >= 5))
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
        let ok = await this.checkLinks(visit);
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

    async app(path, name, control, overlay)
    {
        const visit = await this.open(path, path);
        let ok = await this.checkWindow(visit, name);
        if(ok && overlay !== undefined)
            ok = await this.checkOverlay(visit, overlay);
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
        ok = (await this.checkLinks(visit)) && ok;
        const clean = visit.errors.length === 0;
        this.report(visit);
        if(ok && clean)
            pass(visit.label, detail);
        return visit;
    }

    // The picker on an open gallery page: choosing a control loads its page,
    // the other parameters kept.
    async pick(visit, control, expected)
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
