//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The browser runner: runs one web page of the web tree (an Emscripten
//  executable built by gweni_web_page, examples/openglcore/web/page.cmake) in
//  the image's headless Chromium, as a native test runs the executable.
//
//      node tests/web/run.mjs [options] <page.html> [-- <args>...]
//
//  It serves the page's directory on a local port, opens <page.html> with
//  <args> as its argv (each one an argv= of the URL query, which
//  web/pre.js hands to main verbatim), waits for main to return (or exit(), or
//  an abort), prints the run's stdout and stderr in the order they were written,
//  copies the files the run wrote under /out (its in-memory file system) to the
//  host, and exits with the run's exit status. A path given in <args> should
//  therefore be under /out, e.g. --shot /out/ButtonDemo.png.
//
//  It fails, exit 1 whatever the run's status, with one "run.mjs: FAIL: ..."
//  line, if the page logs a console error or throws an uncaught exception, if
//  the run aborts, if it times out, or unless the run made a WebGL 2 context
//  (capture::initEglContext reports it to the page) on ANGLE's SwiftShader with
//  a depth buffer of at least 24 bits, as web_probe asks of a fresh canvas: in a
//  browser with no WebGL 2 a run must not pass because it drew nothing.
//
//  Options:
//    --out <dir>       copy the run's /out files there (default: none copied)
//    --mount <dir>:<path>  copy the host directory <dir> into the page's file
//                      system at <path> before main runs (repeatable), e.g. a
//                      resources/ copy missing one file for --resources <path>
//    --timeout <s>     how long the run may take (default 600)
//    --no-swiftshader  leave out the flags that ask for ANGLE on SwiftShader
//                      (probe.mjs' launchOptions, which also keeps the image's
//                      Vulkan driver pin, lavapipe, away from Chromium)
//    --no-webgl2       launch with --disable-webgl2, so the run gets no WebGL 2
//                      context and must fail (web_run_fails_without_webgl2)
//    --quiet           do not print the run's output (it is still checked)
//
//  runPage(options) is the same, for a script that checks what a run wrote
//  (shot.mjs).
//

import { createServer } from 'node:http';
import { mkdirSync, readdirSync, readFileSync, realpathSync, statSync, writeFileSync } from 'node:fs';
import { basename, dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { pathToFileURL } from 'node:url';

import { isSwiftShader, launchOptions, loadPlaywright } from './probe.mjs';

const usage = 'usage: node run.mjs [--out <dir>] [--mount <dir>:<path>]... [--timeout <s>] [--no-swiftshader] [--no-webgl2] [--quiet] <page.html> [-- <args>...]';

function fail(message)
{
    console.error(`run.mjs: FAIL: ${message}`);
    return 1;
}

// The runner's options from argv, as runPage() takes them; null if they are
// not usable. Everything after "--" is the page's argv.
export function parseArgs(argv)
{
    const options = { out: null, mounts: [], timeout: 600, swiftShader: true, webgl2: true, quiet: false, page: null, args: [] };
    for(let i = 0; i < argv.length; ++i)
    {
        const arg = argv[i];
        if(arg === '--')
        {
            options.args = argv.slice(i + 1);
            break;
        }
        else if(arg === '--out' && i + 1 < argv.length)
            options.out = argv[++i];
        else if(arg === '--mount' && i + 1 < argv.length)
        {
            const mount = argv[++i];
            const colon = mount.lastIndexOf(':');
            if(colon <= 0 || !mount.slice(colon + 1).startsWith('/'))
                return null;
            options.mounts.push({ dir: mount.slice(0, colon), path: mount.slice(colon + 1) });
        }
        else if(arg === '--timeout' && i + 1 < argv.length)
            options.timeout = Number(argv[++i]);
        else if(arg === '--no-swiftshader')
            options.swiftShader = false;
        else if(arg === '--no-webgl2')
            options.webgl2 = false;
        else if(arg === '--quiet')
            options.quiet = true;
        else if(!arg.startsWith('--') && options.page === null)
            options.page = arg;
        else
            return null;
    }
    if(options.page === null || !(options.timeout > 0))
        return null;
    return options;
}

const mimeTypes = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.mjs': 'text/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
    '.png': 'image/png',
};

// Serves the files under root, nothing outside it, on a port of the OS's
// choosing on the loopback interface.
function serve(root)
{
    const server = createServer((request, response) =>
    {
        const path = normalize(join(root, decodeURIComponent(new URL(request.url, 'http://localhost').pathname)));
        let body = null;
        if(path === root || path.startsWith(root + sep))
        {
            try
            {
                if(statSync(path).isFile())
                    body = readFileSync(path);
            }
            catch(e)
            {
                body = null;
            }
        }
        if(body === null)
        {
            response.writeHead(404);
            response.end();
            return;
        }
        response.writeHead(200, {
            'Content-Type': mimeTypes[extname(path)] ?? 'application/octet-stream',
            'Cache-Control': 'no-store',
        });
        response.end(body);
    });
    return new Promise((resolvePromise, reject) =>
    {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', () => resolvePromise(server));
    });
}

// Every file under each mount's host directory, as {path, data} with the
// page-side path and the bytes in base64, for web/pre.js to write before main.
function mountedFiles(mounts)
{
    const files = [];
    for(const mount of mounts)
    {
        (function walk(dir, path)
        {
            for(const entry of readdirSync(dir, { withFileTypes: true }))
            {
                if(entry.isDirectory())
                    walk(join(dir, entry.name), `${path}/${entry.name}`);
                else
                    files.push({ path: `${path}/${entry.name}`, data: readFileSync(join(dir, entry.name)).toString('base64') });
            }
        })(mount.dir, mount.path.replace(/\/+$/, ''));
    }
    return files;
}

function printOutput(output)
{
    for(const line of output)
        (line.stream === 'stderr' ? process.stderr : process.stdout).write(line.text + '\n');
}

// Runs the page as the command line describes (parseArgs) and returns the exit
// status the runner exits with.
export async function runPage(options)
{
    const page = resolve(options.page);
    const name = basename(page);
    const query = new URLSearchParams();
    for(const arg of options.args)
        query.append('argv', arg);

    const { chromium } = loadPlaywright();
    const server = await serve(dirname(page));
    const browser = await chromium.launch(launchOptions(options.swiftShader,
        options.webgl2 ? [] : ['--disable-webgl2']));
    const errors = [];
    try
    {
        const tab = await browser.newPage();
        // an uncaught exception ends the run (main does not return), so it stops
        // the wait below rather than leaving it to the timeout
        let crashed;
        const crash = new Promise(resolvePromise => { crashed = resolvePromise; });
        tab.on('console', msg => { if(msg.type() === 'error') errors.push(`console error: ${msg.text()}`); });
        tab.on('pageerror', err =>
        {
            errors.push(`uncaught exception: ${err.stack || err.message}`);
            crashed();
        });

        // before any of the page's scripts: what web/pre.js writes to the file
        // system before main
        const mounts = mountedFiles(options.mounts);
        if(mounts.length)
            await tab.addInitScript(files => { window.gweniMounts = files; }, mounts);

        const url = `http://127.0.0.1:${server.address().port}/${name}?${query}`;
        // 'commit': main may start (and, a capture being one blocking call, run to
        // its end) before the page's load event
        await tab.goto(url, { waitUntil: 'commit' });

        let timedOut = false;
        try
        {
            await Promise.race([crash, tab.waitForFunction(() => window.gweniRun && window.gweniRun.done,
                null, { timeout: options.timeout*1000, polling: 250 })]);
        }
        catch(e)
        {
            timedOut = true;
        }

        if(timedOut || !(await tab.evaluate(() => window.gweniRun && window.gweniRun.done)))
        {
            // what it wrote before it stopped, to show where
            printOutput(await tab.evaluate(() => window.gweniRun ? window.gweniRun.output : []));
            for(const e of errors)
                fail(`page error: ${e}`);
            return fail(timedOut ? `${name} did not finish in ${options.timeout} s` : `${name} stopped before main returned`);
        }

        const run = await tab.evaluate(() => ({
            exitCode: gweniRun.exitCode,
            aborted: gweniRun.aborted,
            output: gweniRun.output,
            webgl: gweniRun.webgl,
            files: gweniRun.aborted === null ? gweniRun.files() : [],
        }));

        if(!options.quiet)
            printOutput(run.output);

        if(options.out)
        {
            for(const file of run.files)
            {
                const data = await tab.evaluate(path => gweniRun.readFile(path), file);
                const target = join(options.out, file.slice('/out/'.length));
                mkdirSync(dirname(target), { recursive: true });
                writeFileSync(target, Buffer.from(data, 'base64'));
            }
        }

        if(run.webgl)
            console.log(`run.mjs: ${name}: ${run.webgl.version}, ${run.webgl.renderer}, DEPTH_BITS ${run.webgl.depthBits}`);

        if(errors.length)
        {
            for(const e of errors)
                fail(`page error: ${e}`);
            return 1;
        }
        if(run.aborted !== null)
            return fail(`${name} aborted: ${run.aborted}`);
        if(!run.webgl || !run.webgl.webgl2)
            return fail(`${name} made no WebGL 2 context`);
        if(options.swiftShader && !isSwiftShader(run.webgl.renderer))
            return fail(`${name}: asked for ANGLE on SwiftShader, got ${run.webgl.renderer}`);
        if(!(run.webgl.depthBits >= 24))
            return fail(`${name}'s WebGL 2 context has DEPTH_BITS ${run.webgl.depthBits}, the z layering needs at least 24`);

        console.log(`run.mjs: ${name} exited ${run.exitCode}${options.out ? `, ${run.files.length} file(s) copied to ${options.out}` : ''}`);
        return run.exitCode;
    }
    finally
    {
        await browser.close();
        server.close();
    }
}

// Only when run as a script (see probe.mjs), so shot.mjs can import runPage.
if(process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href)
{
    const options = parseArgs(process.argv.slice(2));
    if(!options)
    {
        console.error(usage);
        process.exit(1);
    }
    process.exit(await runPage(options));
}
