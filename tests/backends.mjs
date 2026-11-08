//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  renderer::Vulkan held to OpenGLCore (tests/CMakeLists.txt registers each
//  mode as a test, labels gpu and vulkan): the same runs of one tree's exes on
//  --renderer gl and --renderer vulkan, and what they report or draw compared.
//
//      node tests/backends.mjs counters --bin <dir> --bench <gweni_bench>
//      node tests/backends.mjs pixels --bin <dir> [--scale <s>]
//                              --max-delta T --max-fraction F
//
//  --bin is the tree's examples/openglcore (ControlTest and OpenGLCore). The
//  caller's environment is the Vulkan test environment (cmake/Vulkan.cmake: the
//  validation layer, lavapipe); each run names its backend with --renderer.
//  Modes:
//    counters  every scene of gweni_bench --list at --n 50 --frames 3, every
//              control of ControlTest --list with --headless --stats (one
//              process each, Demo included) and OpenGLCore --headless --frames 3
//              --stats, on both backends: every counter of every frame and
//              summary line identical, but the keys EXCLUDED lists below (times
//              and what the backend itself allocates or is called). What a frame
//              changes, uploads and draws is counted by the shared core
//              (SlotRenderer), so it does not depend on the backend. It also
//              fails unless on Vulkan every settled frame (a ControlTest frame
//              after the first that changed and rendered no control, and every
//              measured frame of the bench's idle scene) writes and uploads
//              nothing.
//    pixels    ControlTest --headless --shot-all and the hero (OpenGLCore
//              --headless --demo ShowcaseContainersDemo --screenshot, as
//              tools/capture_samples.sh takes it), [--scale s], on both
//              backends: every PNG within the bound (web/pngdiff.mjs: no channel
//              of any pixel more than T off, at most F % of the pixels differing
//              at all). The bound and the measurement it comes from are in
//              tests/CMakeLists.txt.
//
//  Each mode works in a mktemp -d directory (os.tmpdir()/gweni-backends-*),
//  holding every run's output and capture. It is removed when everything matched,
//  and kept on a failure, its path printed: there the PNGs, gl/ and vulkan/, and
//  for each PNG over the bound diff/<name>.diff.png, GL's, Vulkan's and the
//  difference side by side.
//
//  Fails with "backends.mjs: FAIL: ..." lines naming each scene, control, frame
//  and counter, or PNG, that differs, with both values, and ends with
//  "backends.mjs: OK, ..." when everything matched.
//

import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { compareDirs } from './web/pngdiff.mjs';

const usage = 'usage: node backends.mjs counters --bin <dir> --bench <gweni_bench>\n'
    + '       node backends.mjs pixels --bin <dir> [--scale <s>] --max-delta T --max-fraction F';

const BACKENDS = ['gl', 'vulkan'];

// The keys left out of the counter comparison, each with the reason. Every
// other key of a frame or summary line must be the same on both backends.
const EXCLUDED = [
    [/_us(_|$)/, 'a wall time (input_us, flush_us, construct_us, flush_us_median, ...)'],
    [/^(allocs|alloc_bytes)(_[a-z]+)?(_total|_max)?$/,
        "gweni_bench's operator new calls in a frame (allocs, alloc_bytes, allocs_<phase>), which count the "
        + "backend's own with gweni's: renderer::Vulkan's command recording and the validation layer's in the "
        + "flush, its target resized in a scene's input (resize, scale) and the atlas textures it makes for text laid "
        + "out at a new scale in render (scale); construct_allocs, Scene::build() before any frame, is compared"],
    [/^(renderer|device)$/, "gweni_bench's summary: which backend and device ran"],
];

function excluded(key)
{
    return EXCLUDED.some(([pattern]) => pattern.test(key));
}

let failures = 0;
function fail(message)
{
    console.error(`backends.mjs: FAIL: ${message}`);
    ++failures;
}

function parseArgs(argv)
{
    const options = { mode: argv[0], bin: null, bench: null, scale: null, maxDelta: null, maxFraction: null };
    for(let i = 1; i < argv.length; i += 2)
    {
        const arg = argv[i], value = argv[i + 1];
        if(value === undefined)
            return null;
        if(arg === '--bin') options.bin = value;
        else if(arg === '--bench') options.bench = value;
        else if(arg === '--scale') options.scale = value;
        else if(arg === '--max-delta') options.maxDelta = Number(value);
        else if(arg === '--max-fraction') options.maxFraction = Number(value);
        else
            return null;
    }
    if(!options.bin)
        return null;
    if(options.mode === 'counters')
        return options.bench ? options : null;
    if(options.mode === 'pixels')
        return Number.isFinite(options.maxDelta) && Number.isFinite(options.maxFraction) ? options : null;
    return null;
}

let scratch = null;
let runs = 0;

// Runs exe with args, its output kept in the scratch dir; its stdout, stderr
// and status.
function run(exe, args)
{
    const result = spawnSync(exe, args, { encoding: 'utf8', maxBuffer: 256*1024*1024 });
    const out = {
        status: result.error ? 1 : result.status,
        stdout: result.stdout ?? '',
        stderr: (result.stderr ?? '') + (result.error ? `${result.error.message}\n` : ''),
        command: [exe, ...args].join(' '),
    };
    if(out.status === null)
        out.stderr += `killed by ${result.signal}\n`;
    writeFileSync(join(scratch, `run${String(++runs).padStart(3, '0')}.txt`),
        `$ ${out.command}\nstatus ${out.status}\n--- stdout\n${out.stdout}--- stderr\n${out.stderr}`);
    return out;
}

function checkRun(r)
{
    if(r.status === 0)
        return true;
    fail(`${r.command} exited ${r.status}\n${r.stdout}${r.stderr}`);
    return false;
}

// The frame and summary lines of a run's stdout, each its line's key=value
// pairs in order, the label (the first token that is not one, "summary") kept
// as a key of its own
function counterLines(stdout)
{
    return stdout.split('\n')
        .filter(line => /^(scene=|frame=|summary)/.test(line))
        .map(line => line.split(' ').filter(token => token).map(token =>
        {
            const at = token.indexOf('=');
            return at < 0 ? [token, ''] : [token.slice(0, at), token.slice(at + 1)];
        }));
}

function lineLabel(pairs)
{
    const get = key => pairs.find(([k]) => k === key)?.[1];
    if(pairs[0]?.[0] === 'summary')
        return pairs.slice(0, 2).map(([k, v]) => v === '' ? k : `${k}=${v}`).join(' ');
    return get('frame') !== undefined ? `frame=${get('frame')}` : pairs.map(([k, v]) => `${k}=${v}`).join(' ');
}

// The counters of the gl and vulkan runs of what, key by key; false on any
// difference
function compareCounters(what, gl, vulkan)
{
    const a = counterLines(gl.stdout), b = counterLines(vulkan.stdout);
    if(a.length === 0)
    {
        fail(`${what}: the gl run printed no counter line\n${gl.stdout}${gl.stderr}`);
        return false;
    }
    if(a.length !== b.length)
    {
        fail(`${what}: ${a.length} counter lines on gl, ${b.length} on vulkan`);
        return false;
    }
    let same = true;
    for(let i = 0; i < a.length; ++i)
    {
        const label = lineLabel(a[i]);
        const keys = [...new Set([...a[i], ...b[i]].map(([key]) => key))].filter(key => !excluded(key));
        const ma = new Map(a[i]), mb = new Map(b[i]);
        for(const key of keys)
        {
            const va = ma.get(key), vb = mb.get(key);
            if(va === vb)
                continue;
            fail(`${what} ${label}: ${key} gl ${va ?? '(none)'}, vulkan ${vb ?? '(none)'}`);
            same = false;
        }
    }
    return same;
}

const number = (pairs, key) => Number(new Map(pairs).get(key));

// What a settled frame must leave at 0: nothing written, nothing uploaded
const SETTLED_ZERO = ['prim_slots_written', 'text_slots_written', 'upload_calls', 'upload_bytes',
    'atlas_upload_calls', 'atlas_upload_bytes'];

// false unless every one of lines' frames that isSettled picks writes and
// uploads nothing; settled counts the frames checked
function checkSettled(what, stdout, isSettled, settled)
{
    let ok = true;
    for(const pairs of counterLines(stdout).filter(isSettled))
    {
        ++settled.count;
        for(const key of SETTLED_ZERO)
        {
            if(number(pairs, key) !== 0)
            {
                fail(`${what} ${lineLabel(pairs)}: a settled frame on vulkan with ${key}=${new Map(pairs).get(key)}`);
                ok = false;
            }
        }
    }
    return ok;
}

function controlNames(controlTest)
{
    const list = run(controlTest, ['--list']);
    if(!checkRun(list))
        return [];
    // "  ButtonDemo" lines, and "  Demo (the whole docked demo)", as eachControl.cmake reads them
    const names = [...list.stdout.matchAll(/^ {2}([A-Za-z0-9_]+)/gm)].map(m => m[1]);
    if(names.length === 0)
        fail(`no control names in ControlTest --list:\n${list.stdout}`);
    return names;
}

function counters(options, exes)
{
    const settled = { count: 0 };
    let same = 0, compared = 0;
    const both = (exe, args) => BACKENDS.map(backend => run(exe, ['--renderer', backend, ...args]));

    // the bench's scenes: "<name> takes_n=..." lines
    const list = run(exes.bench, ['--list']);
    const scenes = checkRun(list) ? [...list.stdout.matchAll(/^(\S+) takes_n=/gm)].map(m => m[1]) : [];
    if(list.status === 0 && scenes.length === 0)
        fail(`no scene in gweni_bench --list:\n${list.stdout}`);
    for(const scene of scenes)
    {
        ++compared;
        const [gl, vulkan] = both(exes.bench, ['--scene', scene, '--n', '50', '--frames', '3']);
        if(!checkRun(gl) || !checkRun(vulkan))
            continue;
        let ok = compareCounters(`gweni_bench --scene ${scene}`, gl, vulkan);
        if(scene === 'idle')
            ok = checkSettled(`gweni_bench --scene ${scene}`, vulkan.stdout, pairs => pairs[0][0] === 'scene', settled) && ok;
        same += ok ? 1 : 0;
    }

    for(const name of controlNames(exes.controlTest))
    {
        ++compared;
        const [gl, vulkan] = both(exes.controlTest, ['--headless', '--stats', '--control', name]);
        if(!checkRun(gl) || !checkRun(vulkan))
            continue;
        let ok = compareCounters(`ControlTest --control ${name}`, gl, vulkan);
        ok = checkSettled(`ControlTest --control ${name}`, vulkan.stdout,
            pairs => number(pairs, 'frame') > 0 && number(pairs, 'controls_changed') === 0 && number(pairs, 'visited') === 0,
            settled) && ok;
        same += ok ? 1 : 0;
    }

    ++compared;
    const [gl, vulkan] = both(exes.openGLCore, ['--headless', '--frames', '3', '--stats']);
    if(checkRun(gl) && checkRun(vulkan) && compareCounters('OpenGLCore --stats', gl, vulkan))
        ++same;

    if(settled.count === 0)
        fail('no settled frame was checked');
    if(!failures)
        console.log(`backends.mjs: OK, the counters of ${same} of ${compared} runs (${scenes.length} bench scenes, `
            + `${compared - scenes.length - 1} controls and OpenGLCore) identical on gl and vulkan, `
            + `${settled.count} settled Vulkan frames writing and uploading nothing`);
}

function pixels(options, exes)
{
    const scaleArgs = options.scale === null ? [] : ['--scale', options.scale];
    for(const backend of BACKENDS)
    {
        const dir = join(scratch, backend);
        mkdirSync(dir);
        checkRun(run(exes.controlTest, ['--renderer', backend, '--headless', '--shot-all', '--shot-dir', dir, ...scaleArgs]));
        checkRun(run(exes.openGLCore, ['--renderer', backend, '--headless', ...scaleArgs,
            '--demo', 'ShowcaseContainersDemo', '--screenshot', join(dir, 'OpenGLCore.png')]));
    }
    if(failures)
        return;
    if(compareDirs({ expected: join(scratch, 'gl'), actual: join(scratch, 'vulkan'), maxDelta: options.maxDelta,
        maxFraction: options.maxFraction, diffDir: join(scratch, 'diff') }) !== 0)
        fail('a Vulkan PNG is outside the bound of its GL one');
    else
        console.log(`backends.mjs: OK, every Vulkan PNG within the bound of its GL one${options.scale ? ` at --scale ${options.scale}` : ''}`);
}

function main()
{
    const options = parseArgs(process.argv.slice(2));
    if(!options)
    {
        console.error(usage);
        return 1;
    }
    const exes = {
        controlTest: join(options.bin, 'ControlTest'),
        openGLCore: join(options.bin, 'OpenGLCore'),
        bench: options.bench,
    };
    for(const exe of options.mode === 'counters' ? Object.values(exes) : [exes.controlTest, exes.openGLCore])
    {
        if(!existsSync(exe))
            fail(`${exe} does not exist; build the tree first (cmake --build <tree>)`);
    }
    if(failures)
        return 1;

    scratch = mkdtempSync(join(tmpdir(), `gweni-backends-${options.mode}-`));
    if(options.mode === 'counters')
        counters(options, exes);
    else
        pixels(options, exes);

    if(failures)
    {
        console.error(`backends.mjs: kept ${scratch} (every run's output${options.mode === 'pixels' ? ', gl/, vulkan/ and diff/' : ''})`);
        return 1;
    }
    rmSync(scratch, { recursive: true, force: true });
    return 0;
}

process.exit(main());
