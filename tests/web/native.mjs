//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  The browser build held to the native GLES tree (tests/web/native.cmake
//  registers each mode as a web_*_native test): the same capture runs in the
//  browser (run.mjs) and natively, and what they wrote is compared.
//
//      node tests/web/native.mjs <mode> --native-bin <dir> --page-bin <dir> --scratch <dir>
//                                [--scale <s>] [pixel bound options]
//
//  --native-bin is the native GLES tree's examples/openglcore (ControlTest and
//  OpenGLCore), --page-bin the web tree's (ControlTest.html, OpenGLCore.html).
//  Modes:
//    bounds  ControlTest --headless --shot-all --dump-bounds [--scale s]: every
//            listing (<demo>.txt, the laid-out tree with its bounds and preferred
//            sizes) byte-identical to native, the same set of them. Layout does
//            not depend on the backend.
//    stats   ControlTest --headless --stats --control <name> for every control
//            of ControlTest --list, and OpenGLCore --headless --frames 3 --stats:
//            every counter of every frame and summary line identical to native,
//            the times (keys ending in _us, or holding _us_) left out. What a
//            frame changes, uploads and draws does not depend on the backend.
//    pixels  --shot-all [--scale s] and OpenGLCore --headless --screenshot
//            [--scale s]: every PNG within the bound (pngdiff.mjs: --max-delta,
//            --max-fraction, --count-over), diff images under <scratch>/diff.
//            --exclude <demo> leaves a PNG out (repeatable), each with its issue.
//
//  A missing native exe or page fails, naming the path and the commands that
//  build it: these tests never skip. The native runs need a GL context, which
//  on a shared host is the "render" lock: each native run goes through
//  `mm run --lock render --` when mm is on PATH (one run per lock, so the lock
//  is free between them), and runs unlocked with one line saying so otherwise.
//  The browser runs need no lock (run.mjs keeps Chromium on its own SwiftShader).
//
//  Fails with "native.mjs: FAIL: ..." lines naming each file, control or line
//  that differs, and ends with "native.mjs: OK, ..." when everything matched.
//

import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync } from 'node:fs';
import { delimiter, join } from 'node:path';

import { compareDirs } from './pngdiff.mjs';
import { runPage } from './run.mjs';

const usage = 'usage: node native.mjs bounds|stats|pixels --native-bin <dir> --page-bin <dir> --scratch <dir> [--scale <s>] '
    + '[--max-delta T] [--max-fraction F] [--count-over K] [--exclude <demo>]...';

let failures = 0;
function fail(message)
{
    console.error(`native.mjs: FAIL: ${message}`);
    ++failures;
}

function parseArgs(argv)
{
    const options = { mode: argv[0], nativeBin: null, pageBin: null, scratch: null, scale: null,
        maxDelta: null, maxFraction: null, countOver: 0, exclude: [] };
    for(let i = 1; i < argv.length; ++i)
    {
        const arg = argv[i], value = argv[i + 1];
        if(value === undefined)
            return null;
        if(arg === '--native-bin') options.nativeBin = value;
        else if(arg === '--page-bin') options.pageBin = value;
        else if(arg === '--scratch') options.scratch = value;
        else if(arg === '--scale') options.scale = value;
        else if(arg === '--max-delta') options.maxDelta = Number(value);
        else if(arg === '--max-fraction') options.maxFraction = Number(value);
        else if(arg === '--count-over') options.countOver = Number(value);
        else if(arg === '--exclude') options.exclude.push(value);
        else
            return null;
        ++i;
    }
    if(!['bounds', 'stats', 'pixels'].includes(options.mode) || !options.nativeBin || !options.pageBin || !options.scratch)
        return null;
    return options;
}

function onPath(name)
{
    return (process.env.PATH ?? '').split(delimiter).some(dir => dir && existsSync(join(dir, name)));
}

const useMm = onPath('mm');
let saidUnlocked = false;

// Runs a native exe (under the render lock when mm is there); its stdout,
// stderr and status.
function runNative(exe, args)
{
    let command = exe, commandArgs = args;
    if(useMm)
    {
        command = 'mm';
        commandArgs = ['run', '--lock', 'render', '--', exe, ...args];
    }
    else if(!saidUnlocked)
    {
        console.log('native.mjs: mm is not on PATH, so the native runs take no render lock');
        saidUnlocked = true;
    }
    const result = spawnSync(command, commandArgs, { encoding: 'utf8', maxBuffer: 256*1024*1024 });
    if(result.error)
        return { status: 1, stdout: '', stderr: `${result.error.message}\n` };
    return { status: result.status, stdout: result.stdout, stderr: result.stderr };
}

// Runs a page in the browser with its output kept (not printed), the files it
// wrote under /out copied to outDir; its stdout, stderr and status.
async function runWeb(page, args, outDir)
{
    const lines = [];
    const write = { out: process.stdout.write, err: process.stderr.write };
    // runPage prints the run's output and its own lines; they are collected
    // here, so a failure can show them and a pass stays short
    process.stdout.write = (chunk) => { lines.push({ stream: 'stdout', text: String(chunk) }); return true; };
    process.stderr.write = (chunk) => { lines.push({ stream: 'stderr', text: String(chunk) }); return true; };
    let status;
    try
    {
        status = await runPage({ out: outDir, mounts: [], timeout: 900, swiftShader: true, webgl2: true,
            quiet: false, page, args });
    }
    finally
    {
        process.stdout.write = write.out;
        process.stderr.write = write.err;
    }
    const pick = stream => lines.filter(l => l.stream === stream).map(l => l.text).join('');
    return { status, stdout: pick('stdout'), stderr: pick('stderr') };
}

function requireFile(path, what, build)
{
    if(existsSync(path))
        return true;
    fail(`${what} ${path} does not exist; build it with: ${build}`);
    return false;
}

function checkRun(what, run)
{
    if(run.status === 0)
        return true;
    fail(`${what} exited ${run.status}\n${run.stdout}${run.stderr}`);
    return false;
}

function scaleArgs(options)
{
    return options.scale === null ? [] : ['--scale', options.scale];
}

function freshDir(dir)
{
    rmSync(dir, { recursive: true, force: true });
    mkdirSync(dir, { recursive: true });
    return dir;
}

// ControlTest --shot-all --dump-bounds natively into <scratch>/native and in the
// browser into <scratch>/web (and, with hero, OpenGLCore's screenshot beside
// them as OpenGLCore.png)
async function captureAll(options, exes, { hero })
{
    const nativeDir = freshDir(join(options.scratch, 'native'));
    const webDir = freshDir(join(options.scratch, 'web'));
    const args = ['--headless', '--shot-all', '--dump-bounds', ...scaleArgs(options)];

    if(!checkRun('native ControlTest --shot-all', runNative(exes.nativeControlTest, [...args, '--shot-dir', nativeDir])))
        return null;
    if(!checkRun('ControlTest.html --shot-all', await runWeb(exes.pageControlTest, [...args, '--shot-dir', '/out'], webDir)))
        return null;
    if(hero)
    {
        const heroArgs = ['--headless', ...scaleArgs(options), '--screenshot'];
        if(!checkRun('native OpenGLCore --screenshot', runNative(exes.nativeOpenGLCore, [...heroArgs, join(nativeDir, 'OpenGLCore.png')])))
            return null;
        if(!checkRun('OpenGLCore.html --screenshot', await runWeb(exes.pageOpenGLCore, [...heroArgs, '/out/OpenGLCore.png'], webDir)))
            return null;
    }
    return { nativeDir, webDir };
}

function filesIn(dir, suffix)
{
    return readdirSync(dir).filter(name => name.endsWith(suffix)).sort();
}

async function bounds(options, exes)
{
    const dirs = await captureAll(options, exes, { hero: false });
    if(!dirs)
        return;
    const native = filesIn(dirs.nativeDir, '.txt'), web = filesIn(dirs.webDir, '.txt');
    if(native.length === 0)
        fail(`native ControlTest wrote no listing to ${dirs.nativeDir}`);
    for(const name of native.filter(n => !web.includes(n)))
        fail(`${name}: written natively, not in the browser`);
    for(const name of web.filter(n => !native.includes(n)))
        fail(`${name}: written in the browser, not natively`);
    let same = 0;
    for(const name of native.filter(n => web.includes(n)))
    {
        // the lines, without the empty one after the last newline
        const lines = path => readFileSync(path, 'utf8').replace(/\n$/, '').split('\n');
        const a = lines(join(dirs.nativeDir, name));
        const b = lines(join(dirs.webDir, name));
        const line = a.findIndex((text, i) => text !== b[i]);
        if(line === -1 && a.length === b.length)
        {
            ++same;
            continue;
        }
        const at = line === -1 ? Math.min(a.length, b.length) : line;
        fail(`${name} differs from line ${at + 1} (native ${join(dirs.nativeDir, name)}, browser ${join(dirs.webDir, name)}):\n`
            + `  native:  ${a[at] ?? '(end of file)'}\n  browser: ${b[at] ?? '(end of file)'}`);
    }
    if(!failures)
        console.log(`native.mjs: OK, ${same} listings identical to native${options.scale ? ` at --scale ${options.scale}` : ''}`);
}

// The counter lines of a --stats run: every "frame=" and "summary" line of its
// stdout, the times left out (a key ending in _us, or holding _us_)
function counterLines(stdout)
{
    return stdout.split('\n')
        .filter(line => line.startsWith('frame=') || line.startsWith('summary'))
        .map(line => line.split(' ').filter(token => !/_us(_|=)/.test(token)).join(' '));
}

function compareCounters(what, nativeRun, webRun)
{
    const a = counterLines(nativeRun.stdout), b = counterLines(webRun.stdout);
    if(a.length === 0)
    {
        fail(`${what}: the native run printed no --stats line\n${nativeRun.stdout}${nativeRun.stderr}`);
        return false;
    }
    const line = a.findIndex((text, i) => text !== b[i]);
    if(line === -1 && a.length === b.length)
        return true;
    if(line === -1)
    {
        fail(`${what}: ${a.length} --stats lines natively, ${b.length} in the browser`);
        return false;
    }
    // the first key that differs, for a short message; both lines in full after it
    const na = a[line].split(' '), nb = (b[line] ?? '').split(' ');
    const key = na.findIndex((token, i) => token !== nb[i]);
    fail(`${what}: line ${line + 1} differs at ${na[key] ?? '(end)'} (browser ${nb[key] ?? '(end)'}):\n`
        + `  native:  ${a[line]}\n  browser: ${b[line] ?? '(none)'}`);
    return false;
}

async function stats(options, exes)
{
    const list = runNative(exes.nativeControlTest, ['--list']);
    if(!checkRun('native ControlTest --list', list))
        return;
    // "  ButtonDemo" lines, and "  Demo (the whole docked demo)", as eachControl.cmake reads them
    const names = [...list.stdout.matchAll(/^ {2}([A-Za-z0-9_]+)/gm)].map(m => m[1]);
    if(names.length === 0)
    {
        fail(`no control names in ControlTest --list:\n${list.stdout}`);
        return;
    }
    let same = 0;
    for(const name of names)
    {
        const args = ['--headless', '--stats', '--control', name];
        const nativeRun = runNative(exes.nativeControlTest, args);
        const webRun = await runWeb(exes.pageControlTest, args, null);
        if(checkRun(`native ControlTest --stats --control ${name}`, nativeRun)
            && checkRun(`ControlTest.html --stats --control ${name}`, webRun)
            && compareCounters(name, nativeRun, webRun))
            ++same;
    }
    const heroArgs = ['--headless', '--frames', '3', '--stats'];
    const nativeHero = runNative(exes.nativeOpenGLCore, heroArgs);
    const webHero = await runWeb(exes.pageOpenGLCore, heroArgs, null);
    if(checkRun('native OpenGLCore --stats', nativeHero) && checkRun('OpenGLCore.html --stats', webHero)
        && compareCounters('OpenGLCore', nativeHero, webHero))
        ++same;
    if(!failures)
        console.log(`native.mjs: OK, the counters of ${same} runs (${names.length} controls and OpenGLCore) identical to native`);
}

async function pixels(options, exes)
{
    const dirs = await captureAll(options, exes, { hero: true });
    if(!dirs)
        return;
    for(const demo of options.exclude)
    {
        for(const dir of [dirs.nativeDir, dirs.webDir])
            rmSync(join(dir, `${demo}.png`), { force: true });
        console.log(`native.mjs: ${demo}.png left out`);
    }
    if(compareDirs({ expected: dirs.nativeDir, actual: dirs.webDir, maxDelta: options.maxDelta,
        maxFraction: options.maxFraction, countOver: options.countOver, diffDir: join(options.scratch, 'diff') }) !== 0)
        fail(`a browser PNG is outside the bound (native ${dirs.nativeDir}, browser ${dirs.webDir})`);
    else
        console.log(`native.mjs: OK, every PNG within the bound${options.scale ? ` at --scale ${options.scale}` : ''}`);
}

async function main()
{
    const options = parseArgs(process.argv.slice(2));
    if(!options)
    {
        console.error(usage);
        return 1;
    }
    const exes = {
        nativeControlTest: join(options.nativeBin, 'ControlTest'),
        nativeOpenGLCore: join(options.nativeBin, 'OpenGLCore'),
        pageControlTest: join(options.pageBin, 'ControlTest.html'),
        pageOpenGLCore: join(options.pageBin, 'OpenGLCore.html'),
    };
    const nativeBuild = './generate.sh linux gles && cmake --build build_linux_x64_debug_gles -j '
        + '(release: ./generate.sh linux gles release, build_linux_x64_release_gles; another tree: -DGWENI_NATIVE_REFERENCE_DIR=<tree> to ./generate.sh web)';
    let present = requireFile(exes.nativeControlTest, 'the native reference', nativeBuild)
        & requireFile(exes.pageControlTest, 'the page', 'cmake --build <the web tree>');
    if(options.mode !== 'bounds')
    {
        present &= requireFile(exes.nativeOpenGLCore, 'the native reference', nativeBuild)
            & requireFile(exes.pageOpenGLCore, 'the page', 'cmake --build <the web tree>');
    }
    if(!present)
        return 1;
    mkdirSync(options.scratch, { recursive: true });

    if(options.mode === 'bounds')
        await bounds(options, exes);
    else if(options.mode === 'stats')
        await stats(options, exes);
    else
        await pixels(options, exes);
    return failures ? 1 : 0;
}

process.exit(await main());
