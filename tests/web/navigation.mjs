//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  web_navigation: the OpenGLCore demo's window as a page with a URL of its
//  own (platform.h's URL, history, opener and storage on the web), driven in
//  the image's headless Chromium with real clicks and the browser's own back,
//  forward and reload.
//
//      node tests/web/navigation.mjs <dir holding OpenGLCore.html>
//
//  It serves the build dir on a loopback port, with two additions: demo.html
//  and site.css from tools/site/ (the hosted site's demo page, which runs the
//  same OpenGLCore.js with its default --stats-overlay), and, for every path
//  under /app/ (as the orchestrator serves its frontend), OpenGLCore.html with
//  <base href="/"> put in its <head>, so its relative script loads from the
//  root and not from the deep path (Emscripten finds the .wasm and .data next
//  to the script). The shell's state is read through OpenGLCore's exports
//  (Module.ccall): gweni_web_navigation ("demo=<shown> link=x,y,w,h
//  url=<platform::currentUrl()>") and gweni_web_demo_entry(name) (a demo list
//  entry's rect). The checks, each one "navigation.mjs: <check>: ok" line or a
//  "navigation.mjs: FAIL: <check>: ..." one, on OpenGLCore.html?stats-overlay
//  and on demo.html?stats-overlay=off, each in a context of its own (its own
//  localStorage):
//    <page> pick     a click on the list's Button entry, then on Label, shows
//                    that page with no reload, pushes ?<the query's own
//                    keys>&demo=<Name> (one history entry each) and titles
//                    the page "gweni — <Name>"; LabelDemo picked again
//                    pushes nothing
//    <page> history  page.goBack() shows ButtonDemo with its URL, goForward()
//                    LabelDemo again
//    <page> reload   page.reload() of that deep link shows LabelDemo, the URL
//                    and the history length as they were
//    <page> link     a click on the status bar's link opens a new tab
//                    (context 'page') at its https URL, which the context
//                    routes to a local answer, so nothing leaves the host,
//                    with no window.opener (noopener)
//    <page> storage  the bare page, loaded again in the same context, shows
//                    the page last shown (gweni-demo/last-demo in localStorage)
//                    and replaces its URL with ?demo=<that page>
//  and once:
//    fresh           OpenGLCore.html in a fresh context (no ?demo=, nothing
//                    stored) reports the page it is built showing as shown,
//                    titles the page with it and stores it, its URL left bare;
//                    a pick of that page pushes nothing (it replaces with
//                    ?demo=<it>), and Back from a pick of ButtonDemo shows it
//    hash            on OpenGLCore.html?demo=ButtonDemo#a|b{c}^d\e (characters
//                    a browser encodes in a path but leaves in a hash), a pick
//                    of LabelDemo pushes ?demo=LabelDemo with the hash kept
//    path            /app/x/y in a fresh context starts the window, its
//                    currentUrl() /app/x/y; a pick pushes /app/x/y?demo=ButtonDemo
//                    (the <base href> changes nothing), and /app/tasks/42?demo=LabelDemo
//                    opens LabelDemo with that URL
//  It also fails on any console error or uncaught exception in any page, a
//  line on a run's stderr, and unless every page made a WebGL 2 context on
//  ANGLE's SwiftShader with at least 24 depth bits.
//

import { createServer } from 'node:http';
import { readFileSync, statSync } from 'node:fs';
import { dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

import { isSwiftShader, launchOptions, loadPlaywright } from './probe.mjs';

const usage = 'usage: node navigation.mjs <dir holding OpenGLCore.html>';
const siteDir = resolve(dirname(fileURLToPath(import.meta.url)), '../../tools/site');
// what the shell's link opens (Demo::LinkUrl), answered by the context's route
const linkUrl = 'https://github.com/caseymcc/gweni';

let failures = 0;

const mimeTypes = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'text/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
};

function fail(check, message)
{
    ++failures;
    console.log(`navigation.mjs: FAIL: ${check}: ${message}`);
}

function pass(check, detail)
{
    console.log(`navigation.mjs: ${check}: ok (${detail})`);
}

// The build dir, demo.html and site.css from tools/site, and OpenGLCore.html
// (with <base href="/">) at every path under /app/.
function serve(root)
{
    const send = (response, type, body) =>
    {
        response.writeHead(200, { 'Content-Type': type, 'Cache-Control': 'no-store' });
        response.end(body);
    };
    const server = createServer((request, response) =>
    {
        const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
        try
        {
            if(pathname.startsWith('/app/'))
            {
                const html = readFileSync(join(root, 'OpenGLCore.html'), 'utf8');
                send(response, 'text/html', html.replace(/<head>/i, '<head><base href="/">'));
                return;
            }
            if(pathname === '/demo.html' || pathname === '/site.css')
            {
                send(response, mimeTypes[extname(pathname)], readFileSync(join(siteDir, pathname)));
                return;
            }
            const path = normalize(join(root, pathname));
            if((path === root || path.startsWith(root + sep)) && statSync(path).isFile())
            {
                send(response, mimeTypes[extname(path)] ?? 'application/octet-stream', readFileSync(path));
                return;
            }
        }
        catch(e)
        {
        }
        response.writeHead(404);
        response.end();
    });
    return new Promise((resolvePromise, reject) =>
    {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', () => resolvePromise(server));
    });
}

const navigation = page => page.evaluate(() => Module.ccall('gweni_web_navigation', 'string', [], []));

// "demo=<Name> link=x,y,w,h url=<url>" -> { demo, link, url }
function parseNavigation(text)
{
    const match = /^demo=(\S*) link=(-?\d+),(-?\d+),(\d+),(\d+) url=(.*)$/.exec(text);
    if(!match)
        return null;
    const [x, y, width, height] = match.slice(2, 6).map(Number);
    return { demo: match[1], link: { x, y, width, height }, url: match[6] };
}

class Session
{
    constructor(browser, base)
    {
        this.browser = browser;
        this.base = base;
        this.errors = [];
    }

    async newContext()
    {
        const context = await this.browser.newContext({ viewport: { width: 1024, height: 900 } });
        // the link's tab, answered here: the test never leaves the host
        await context.route(`${linkUrl}**`, route => route.fulfill({ status: 200, contentType: 'text/html', body: '<title>link</title>' }));
        return context;
    }

    // Loads \a path in \a page and waits until its window has drawn a few
    // frames; false after a failure of the check.
    async load(check, page, path, reload = false)
    {
        if(!page.gweniWatched)
        {
            page.gweniWatched = true;
            page.on('console', msg => { if(msg.type() === 'error') this.errors.push(`${check}: console error: ${msg.text()}`); });
            page.on('pageerror', err => this.errors.push(`${check}: uncaught exception: ${err.stack || err.message}`));
        }
        if(reload)
            await page.reload({ waitUntil: 'commit' });
        else
            await page.goto(`${this.base}${path}`, { waitUntil: 'commit' });
        try
        {
            await page.waitForFunction(() => window.gweniRun && (window.gweniRun.done || window.gweniRun.frames >= 3),
                null, { timeout: 300000, polling: 100 });
        }
        catch(e)
        {
            fail(check, `${path} drew no frames in 300 s`);
            return false;
        }
        const run = await page.evaluate(() => ({ done: gweniRun.done, exitCode: gweniRun.exitCode, webgl: gweniRun.webgl }));
        if(run.done)
            fail(check, `${path} ended (exit status ${run.exitCode}) instead of running its window`);
        else if(!run.webgl || !run.webgl.webgl2)
            fail(check, `${path} made no WebGL 2 context`);
        else if(!isSwiftShader(run.webgl.renderer))
            fail(check, `${path}: asked for ANGLE on SwiftShader, got ${run.webgl.renderer}`);
        else if(!(run.webgl.depthBits >= 24))
            fail(check, `${path}'s WebGL 2 context has DEPTH_BITS ${run.webgl.depthBits}`);
        else
            return true;
        return false;
    }

    // Waits until the page has drawn \a count more frames.
    async frames(page, count = 3)
    {
        const now = await page.evaluate(() => window.gweniRun.frames);
        await page.waitForFunction(target => window.gweniRun.frames >= target, now + count, { timeout: 60000, polling: 16 });
    }

    // Waits until the shell shows \a name; its navigation state, or null.
    async shown(page, name)
    {
        try
        {
            await page.waitForFunction(n => window.gweniRun && window.gweniRun.frames > 0 &&
                Module.ccall('gweni_web_navigation', 'string', [], []).startsWith(`demo=${n} `),
                name, { timeout: 30000, polling: 50 });
        }
        catch(e)
        {
            return null;
        }
        await this.frames(page, 2);
        return parseNavigation(await navigation(page));
    }

    // A click at the centre of a rect on the canvas (logical units, which are
    // CSS px at scale 1), wherever the page put the canvas.
    async clickCanvas(page, rect)
    {
        const origin = await page.evaluate(() => { const r = Module.canvas.getBoundingClientRect(); return { x: r.left, y: r.top }; });
        await page.mouse.click(origin.x + rect.x + rect.width / 2, origin.y + rect.y + rect.height / 2);
    }

    // A click on the demo list's entry for \a name.
    async pick(page, name)
    {
        const text = await page.evaluate(n => Module.ccall('gweni_web_demo_entry', 'string', ['string'], [n]), name);
        const [x, y, width, height] = text.split(',').map(Number);
        if(!(width > 0))
            return false;
        await this.clickCanvas(page, { x, y, width, height });
        return true;
    }
}

const pageState = page => page.evaluate(() => ({
    search: location.search, pathname: location.pathname, title: document.title,
    length: history.length, marker: window.gweniNavigationMarker === true,
    stored: localStorage.getItem('gweni-demo/last-demo'),
    stderr: gweniRun.output.filter(l => l.stream === 'stderr').map(l => l.text),
}));

// The five checks of one page: \a file with \a query (its own keys, kept by
// every push).
async function checkPage(s, file, query)
{
    const name = file.replace(/\.html$/, '');
    const context = await s.newContext();
    const page = await context.newPage();
    const stderrOf = async () => (await pageState(page)).stderr;

    try
    {
        // pick
        if(!await s.load(`${name} pick`, page, `/${file}?${query}`))
            return;
        await page.evaluate(() => { window.gweniNavigationMarker = true; });
        const start = await pageState(page);
        let ok = true;
        for(const [i, demo] of ['ButtonDemo', 'LabelDemo'].entries())
        {
            if(!await s.pick(page, demo))
            {
                fail(`${name} pick`, `the list's ${demo} entry is not shown`);
                return;
            }
            const nav = await s.shown(page, demo);
            const state = await pageState(page);
            const search = `?${query}&demo=${demo}`;
            if(!nav)
                fail(`${name} pick`, `a click on the ${demo} entry did not show it (${await navigation(page)})`);
            else if(!state.marker)
                fail(`${name} pick`, `picking ${demo} reloaded the page`);
            else if(state.search !== search || nav.url !== `/${file}${search}`)
                fail(`${name} pick`, `picking ${demo} left location.search ${state.search} and currentUrl() ${nav.url}, not ${search}`);
            else if(state.length !== start.length + i + 1)
                fail(`${name} pick`, `history.length went from ${start.length} to ${state.length} after ${i + 1} picks`);
            else if(state.title !== `gweni — ${demo}`)
                fail(`${name} pick`, `the title is "${state.title}"`);
            else
                continue;
            ok = false;
            break;
        }
        if(!ok)
            return;
        // the page already shown, picked again: no entry, or Back would show it again
        const before = await pageState(page);
        await s.pick(page, 'LabelDemo');
        await s.frames(page, 5);
        const again = await pageState(page);
        if(again.length !== before.length || again.search !== before.search)
        {
            fail(`${name} pick`, `picking the shown LabelDemo again made history.length ${again.length} (was ${before.length}), location.search ${again.search}`);
            return;
        }
        pass(`${name} pick`, `?${query}&demo=ButtonDemo, then LabelDemo, one entry each, titled; LabelDemo again none`);

        // history
        await page.goBack();
        const back = await s.shown(page, 'ButtonDemo');
        const backState = await pageState(page);
        await page.goForward();
        const forward = await s.shown(page, 'LabelDemo');
        const forwardState = await pageState(page);
        if(!back)
            fail(`${name} history`, `goBack() left ${await navigation(page)}, not ButtonDemo`);
        else if(backState.search !== `?${query}&demo=ButtonDemo` || backState.title !== 'gweni — ButtonDemo')
            fail(`${name} history`, `after goBack() the URL is ${backState.search}, the title "${backState.title}"`);
        else if(!forward)
            fail(`${name} history`, `goForward() left ${await navigation(page)}, not LabelDemo`);
        else if(forwardState.search !== `?${query}&demo=LabelDemo` || forwardState.length !== start.length + 2)
            fail(`${name} history`, `after goForward() the URL is ${forwardState.search}, history.length ${forwardState.length}`);
        else
            pass(`${name} history`, 'back shows ButtonDemo, forward LabelDemo');

        // reload
        if(!await s.load(`${name} reload`, page, '', true))
            return;
        const reloaded = await s.shown(page, 'LabelDemo');
        const reloadState = await pageState(page);
        if(!reloaded)
            fail(`${name} reload`, `the reloaded deep link shows ${await navigation(page)}, not LabelDemo`);
        else if(reloadState.search !== `?${query}&demo=LabelDemo` || reloadState.length !== forwardState.length)
            fail(`${name} reload`, `after the reload the URL is ${reloadState.search}, history.length ${reloadState.length} (was ${forwardState.length})`);
        else
            pass(`${name} reload`, `?${query}&demo=LabelDemo opens LabelDemo, the history as it was`);

        // link
        const nav = parseNavigation(await navigation(page));
        const opened = context.waitForEvent('page', { timeout: 30000 }).catch(() => null);
        await s.clickCanvas(page, nav.link);
        const tab = await opened;
        if(!tab)
            fail(`${name} link`, 'a click on the status bar\'s link opened no tab');
        else
        {
            await tab.waitForLoadState('domcontentloaded').catch(() => {});
            const opener = await tab.evaluate(() => window.opener === null).catch(() => false);
            if(!tab.url().startsWith(linkUrl))
                fail(`${name} link`, `the new tab is at ${tab.url()}, not ${linkUrl}`);
            else if(!opener)
                fail(`${name} link`, 'the new tab has a window.opener (no noopener)');
            else
                pass(`${name} link`, `a new tab at ${tab.url()}, no opener`);
            await tab.close();
        }

        // storage
        const stderr = await stderrOf();
        if(!await s.load(`${name} storage`, page, `/${file}`))
            return;
        const restored = await s.shown(page, 'LabelDemo');
        const storedState = await pageState(page);
        if(!restored)
            fail(`${name} storage`, `the bare page shows ${await navigation(page)}, not the stored LabelDemo`);
        else if(storedState.stored !== 'LabelDemo')
            fail(`${name} storage`, `localStorage holds ${storedState.stored} under gweni-demo/last-demo`);
        else if(storedState.search !== '?demo=LabelDemo')
            fail(`${name} storage`, `the bare page's URL became ${storedState.search}, not ?demo=LabelDemo`);
        else
            pass(`${name} storage`, 'the bare page opens the stored LabelDemo, ?demo=LabelDemo');

        for(const line of stderr.concat((await pageState(page)).stderr))
            fail(name, `the run wrote to stderr: ${line}`);
    }
    finally
    {
        await context.close();
    }
}

// A hash the browser leaves as it is (| { } ^ \\ are encoded in a path, not in
// a hash) is kept by a pick, which pushes it back as currentUrl() gave it.
async function checkHash(s)
{
    const context = await s.newContext();
    const page = await context.newPage();
    try
    {
        if(!await s.load('hash', page, '/OpenGLCore.html?demo=ButtonDemo#a|b{c}^d\\e'))
            return;
        if(!await s.shown(page, 'ButtonDemo') || !await s.pick(page, 'LabelDemo'))
        {
            fail('hash', `the page shows ${await navigation(page)}, not ButtonDemo with a LabelDemo entry`);
            return;
        }
        const picked = await s.shown(page, 'LabelDemo');
        const state = await page.evaluate(() => ({ search: location.search, hash: location.hash,
            stderr: gweniRun.output.filter(l => l.stream === 'stderr').map(l => l.text) }));
        if(!picked || state.search !== '?demo=LabelDemo' || state.hash !== '#a|b{c}^d\\e')
            fail('hash', `a pick of LabelDemo left location.search ${state.search}, location.hash ${state.hash}`);
        else
            pass('hash', `#a|b{c}^d\\e kept, ?demo=LabelDemo pushed`);
        for(const line of state.stderr)
            fail('hash', `the run wrote to stderr: ${line}`);
    }
    finally
    {
        await context.close();
    }
}

// A fresh start with no page named (no ?demo=, nothing stored): the page the
// shell is built showing is the page shown, titled and stored with the URL
// left bare; picking it pushes nothing, so after a pick of ButtonDemo Back
// shows it again.
async function checkFresh(s)
{
    const context = await s.newContext();
    const page = await context.newPage();
    try
    {
        if(!await s.load('fresh', page, '/OpenGLCore.html'))
            return;
        await s.frames(page, 2);
        const nav = parseNavigation(await navigation(page));
        const start = await pageState(page);
        if(!nav || !nav.demo)
        {
            fail('fresh', `the bare page reports ${await navigation(page)}, no page shown`);
            return;
        }
        const first = nav.demo;
        if(start.search !== '' || nav.url !== '/OpenGLCore.html')
            fail('fresh', `the bare page's URL became ${nav.url} (location.search ${start.search})`);
        else if(start.title !== `gweni — ${first}`)
            fail('fresh', `the title is "${start.title}", not gweni — ${first}`);
        else if(start.stored !== first)
            fail('fresh', `localStorage holds ${start.stored} under gweni-demo/last-demo, not ${first}`);
        else if(!await s.pick(page, first))
            fail('fresh', `the list's ${first} entry is not shown`);
        else
        {
            await s.frames(page, 5);
            const again = await pageState(page);
            if(again.length !== start.length || again.search !== `?demo=${first}`)
            {
                fail('fresh', `picking the shown ${first} made history.length ${again.length} (was ${start.length}), location.search ${again.search}`);
                return;
            }
            await s.pick(page, 'ButtonDemo');
            const picked = await s.shown(page, 'ButtonDemo');
            const pickedState = await pageState(page);
            await page.goBack();
            const back = await s.shown(page, first);
            if(!picked || pickedState.length !== start.length + 1)
                fail('fresh', `a pick of ButtonDemo gave ${await navigation(page)}, history.length ${pickedState.length} (was ${start.length})`);
            else if(!back)
                fail('fresh', `goBack() after ButtonDemo shows ${await navigation(page)}, not ${first}`);
            else
                pass('fresh', `${first} shown, titled and stored, the URL bare; picking it again no entry; Back from ButtonDemo shows it`);
        }
        for(const line of (await pageState(page)).stderr)
            fail('fresh', `the run wrote to stderr: ${line}`);
    }
    finally
    {
        await context.close();
    }
}

// The page served at every path under /app/.
async function checkPath(s)
{
    const context = await s.newContext();
    const page = await context.newPage();
    try
    {
        if(!await s.load('path', page, '/app/x/y'))
            return;
        const start = parseNavigation(await navigation(page));
        if(!start || start.url !== '/app/x/y')
        {
            fail('path', `/app/x/y's currentUrl() is ${start && start.url}`);
            return;
        }
        if(!await s.pick(page, 'ButtonDemo'))
        {
            fail('path', 'the list\'s ButtonDemo entry is not shown');
            return;
        }
        const picked = await s.shown(page, 'ButtonDemo');
        const state = await pageState(page);
        if(!picked || picked.url !== '/app/x/y?demo=ButtonDemo' || state.pathname !== '/app/x/y')
        {
            fail('path', `a pick at /app/x/y gave ${picked && picked.url} (location.pathname ${state.pathname})`);
            return;
        }
        if(!await s.load('path', page, '/app/tasks/42?demo=LabelDemo'))
            return;
        const deep = await s.shown(page, 'LabelDemo');
        if(!deep || deep.url !== '/app/tasks/42?demo=LabelDemo')
            fail('path', `/app/tasks/42?demo=LabelDemo shows ${await navigation(page)}`);
        else
            pass('path', 'currentUrl() /app/x/y, a pick /app/x/y?demo=ButtonDemo, /app/tasks/42?demo=LabelDemo opens it');
        for(const line of state.stderr.concat((await pageState(page)).stderr))
            fail('path', `the run wrote to stderr: ${line}`);
    }
    finally
    {
        await context.close();
    }
}

async function main()
{
    const argv = process.argv.slice(2);
    if(argv.length !== 1 || argv[0].startsWith('--'))
    {
        console.error(usage);
        return 1;
    }
    const dir = resolve(argv[0]);

    const { chromium } = loadPlaywright();
    const server = await serve(dir);
    const browser = await chromium.launch(launchOptions(true));
    const s = new Session(browser, `http://127.0.0.1:${server.address().port}`);
    try
    {
        await checkPage(s, 'OpenGLCore.html', 'stats-overlay');
        await checkPage(s, 'demo.html', 'stats-overlay=off');
        await checkFresh(s);
        await checkHash(s);
        await checkPath(s);
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
    console.log('navigation.mjs: OK, every check passed');
    return 0;
}

process.exit(await main());
