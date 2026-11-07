//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  web_probe: does the image's pinned Chromium give a real WebGL 2 context?
//
//      node tests/web/probe.mjs [--no-swiftshader] [--no-webgl2]
//
//  Launches the headless Chromium that Playwright installed into the image
//  (PLAYWRIGHT_BROWSERS_PATH; the playwright package itself is found through
//  NODE_PATH), opens a blank page and asks a canvas for
//  getContext('webgl2', {depth: true}). It prints the context's RENDERER and
//  VERSION strings (the unmasked ones where WEBGL_debug_renderer_info gives
//  them) and DEPTH_BITS, and exits 1 unless the context exists, is WebGL 2
//  and has at least 24 depth bits (the z layering's need, as capture.cpp's
//  GL_DEPTH_COMPONENT24), or if the page logs a console error.
//
//  launchArgs() asks for ANGLE on SwiftShader explicitly. Recent desktop Chrome
//  no longer falls back to SwiftShader for WebGL on its own; the pinned
//  headless shell (Chromium 158) still does, but the tests do not rely on a
//  default that a later pin may drop. --no-swiftshader leaves those flags out
//  (to see what the default gives); --no-webgl2 launches with
//  --disable-webgl2, so getContext('webgl2') is null and the probe must fail
//  (web_probe_fails_without_webgl2 runs it so, expecting failure). The browser
//  runner of the later web tests starts from launchArgs() and webgl2Report()
//  here.

import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

export function loadPlaywright()
{
    try
    {
        return require('playwright');
    }
    catch(e)
    {
        console.error(`web_probe: cannot load playwright (NODE_PATH=${process.env.NODE_PATH ?? ''}): ${e.message}`);
        process.exit(1);
    }
}

// The flags that give headless Chromium a WebGL 2 context with no GPU: ANGLE on
// SwiftShader, Chromium's CPU Vulkan/GL implementation.
export function launchArgs(swiftShader = true)
{
    return swiftShader ? ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] : [];
}

// Runs in the page: what a WebGL 2 context on a fresh canvas reports, or null
// fields when there is none.
export async function webgl2Report(page)
{
    return page.evaluate(() =>
    {
        const canvas = document.createElement('canvas');
        canvas.width = 64;
        canvas.height = 64;
        const gl = canvas.getContext('webgl2', { depth: true });
        if(!gl)
            return { webgl2: false };
        const info = gl.getExtension('WEBGL_debug_renderer_info');
        return {
            webgl2: gl instanceof WebGL2RenderingContext,
            renderer: gl.getParameter(info ? info.UNMASKED_RENDERER_WEBGL : gl.RENDERER),
            vendor: gl.getParameter(info ? info.UNMASKED_VENDOR_WEBGL : gl.VENDOR),
            version: gl.getParameter(gl.VERSION),
            glsl: gl.getParameter(gl.SHADING_LANGUAGE_VERSION),
            depthBits: gl.getParameter(gl.DEPTH_BITS),
        };
    });
}

async function main()
{
    const swiftShader = !process.argv.includes('--no-swiftshader');
    const args = launchArgs(swiftShader);
    if(process.argv.includes('--no-webgl2'))
        args.push('--disable-webgl2');
    const { chromium } = loadPlaywright();
    const browser = await chromium.launch({ headless: true, args });
    const errors = [];
    try
    {
        console.log(`web_probe: ${chromium.name()} ${browser.version()}, args: ${args.join(' ') || '(none)'}`);
        const page = await browser.newPage();
        page.on('console', msg => { if(msg.type() === 'error') errors.push(msg.text()); });
        page.on('pageerror', err => errors.push(err.message));
        await page.goto('about:blank');
        const r = await webgl2Report(page);
        if(!r.webgl2)
        {
            console.error('web_probe: FAIL: canvas.getContext(\'webgl2\') gave no WebGL 2 context');
            return 1;
        }
        console.log(`web_probe: RENDERER ${r.renderer}`);
        console.log(`web_probe: VENDOR ${r.vendor}`);
        console.log(`web_probe: VERSION ${r.version}`);
        console.log(`web_probe: SHADING_LANGUAGE_VERSION ${r.glsl}`);
        console.log(`web_probe: DEPTH_BITS ${r.depthBits}`);
        if(!(r.depthBits >= 24))
        {
            console.error(`web_probe: FAIL: DEPTH_BITS ${r.depthBits}, the z layering needs at least 24`);
            return 1;
        }
        if(errors.length)
        {
            for(const e of errors)
                console.error(`web_probe: FAIL: page error: ${e}`);
            return 1;
        }
        console.log('web_probe: OK, WebGL 2');
        return 0;
    }
    finally
    {
        await browser.close();
    }
}

// Only when run as a script, so the runner can import the helpers above.
if(import.meta.url === `file://${process.argv[1]}`)
    process.exit(await main());
