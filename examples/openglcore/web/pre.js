//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  --pre-js of every web page (page.cmake): runs inside the Emscripten script,
//  before the runtime reads Module, so it sets up the run.
//
//  argv from the URL query, in order: "key=value" is "--key value", a bare
//  "key" (or "key=") is the flag "--key", and "argv=<text>" is <text> as one
//  argument, verbatim, for anything else (a positional argument, a value
//  starting with "-"). So ControlTest.html?control=ButtonDemo&scale=2 runs
//  ControlTest --control ButtonDemo --scale 2, and the runner (tests/web/run.mjs)
//  passes every argument as argv=, exactly.
//
//  A page may set Module.gweniDefaultArgs (a global Module defined before the
//  Emscripten script): its arguments come first, the URL's after them, so the
//  URL's can override them. tools/site/demo.html sets ['--stats-overlay'], so
//  demo.html opens the stats overlay and demo.html?stats-overlay=off (a later
//  --stats-overlay off) does not.
//
//  window.gweniRun, what the runner reads once done is true:
//    done      true once main has returned, exit() was called or the run aborted
//    exitCode  the exit status (null if it aborted)
//    aborted   null, or why it aborted (an assertion, a trap, out of memory)
//    output    every line written to stdout and stderr, in order, each
//              {stream: 'stdout'|'stderr', text}
//    webgl     the WebGL context the run made (capture::initEglContext reports
//              it through Module.gweniWebGLContext): {webgl2, version, renderer,
//              vendor, depthBits}, null if it made none
//    argv      the arguments pre.js gave main (gweniDefaultArgs, then the
//              URL's), without argv[0]; null under node
//    frames    how many frames an interactive window has drawn
//              (capture::runWindowLoop, one per animation frame), 0 for a
//              capture, which draws offscreen in one blocking main
//    files()   the paths of every file under /out (created before main), where
//              a run writes what the runner copies back
//  Before main it also writes window.gweniMounts, the files the runner copies
//  in (run.mjs --mount), e.g. a resources/ copy missing one file.
//    readFile(path)  that file's bytes, base64
//  Stdout and stderr also go to the page's #output panel and to the console
//  (console.log and console.warn: never console.error, which the runner takes
//  for a page error).
//

var gweniRun = { done: false, exitCode: null, aborted: null, output: [], webgl: null, frames: 0, argv: null };
if(typeof window !== 'undefined')
    window.gweniRun = gweniRun;

// The page's canvas, as Emscripten's own shell would set it: the canvas whose
// CSS cursor platform::setCursor sets (source/platforms/crossPlatform.cpp). The
// contexts are made on it by its selector, #canvas.
if(typeof document !== 'undefined' && !Module['canvas'])
    Module['canvas'] = document.getElementById('canvas');

// Under node (no location) Emscripten's own argv is kept, so tools/build_site.sh
// can run "node ControlTest.js --list".
if(typeof location !== 'undefined')
{
    Module['arguments'] = (function()
    {
        var args = (Module['gweniDefaultArgs'] || []).slice();
        new URLSearchParams(location.search).forEach(function(value, key)
        {
            if(key === 'argv')
                args.push(value);
            else
            {
                args.push('--' + key);
                if(value !== '')
                    args.push(value);
            }
        });
        return args;
    })();
    // a copy: callMain puts argv[0] in front of Module.arguments itself
    gweniRun.argv = Module['arguments'].slice();
}

function gweniOutput(stream, text)
{
    gweniRun.output.push({ stream: stream, text: text });
    if(stream === 'stderr')
        console.warn(text);
    else
        console.log(text);
    if(typeof document !== 'undefined')
    {
        var panel = document.getElementById('output');
        if(panel)
        {
            panel.textContent += text + '\n';
            panel.scrollTop = panel.scrollHeight;
        }
    }
}

Module['print'] = function(text) { gweniOutput('stdout', text); };
Module['printErr'] = function(text) { gweniOutput('stderr', text); };

// before main, after the preloaded resources: /out, and the files the runner
// mounts (run.mjs --mount, window.gweniMounts: [{path, data (base64)}])
Module['preRun'] = (Module['preRun'] || []).concat([function()
{
    FS.mkdir('/out');
    var mounts = (typeof window !== 'undefined' && window.gweniMounts) || [];
    mounts.forEach(function(file)
    {
        var text = atob(file.data);
        var bytes = new Uint8Array(text.length);
        for(var i = 0; i < text.length; ++i)
            bytes[i] = text.charCodeAt(i);
        FS.mkdirTree(file.path.slice(0, file.path.lastIndexOf('/')) || '/');
        FS.writeFile(file.path, bytes);
    });
}]);

Module['gweniWebGLContext'] = function(gl)
{
    var info = gl.getExtension('WEBGL_debug_renderer_info');
    gweniRun.webgl = {
        webgl2: (typeof WebGL2RenderingContext !== 'undefined') && (gl instanceof WebGL2RenderingContext),
        version: gl.getParameter(gl.VERSION),
        renderer: gl.getParameter(info ? info.UNMASKED_RENDERER_WEBGL : gl.RENDERER),
        vendor: gl.getParameter(info ? info.UNMASKED_VENDOR_WEBGL : gl.VENDOR),
        depthBits: gl.getParameter(gl.DEPTH_BITS),
    };
};

gweniRun.files = function()
{
    var paths = [];
    (function walk(dir)
    {
        FS.readdir(dir).forEach(function(name)
        {
            if(name === '.' || name === '..')
                return;
            var path = dir + '/' + name;
            if(FS.isDir(FS.stat(path).mode))
                walk(path);
            else
                paths.push(path);
        });
    })('/out');
    return paths;
};

gweniRun.readFile = function(path)
{
    var bytes = FS.readFile(path);
    var text = '';
    for(var i = 0; i < bytes.length; i += 0x8000)
        text += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(text);
};

Module['onExit'] = function(code)
{
    gweniRun.exitCode = code;
    gweniRun.done = true;
};

Module['onAbort'] = function(what)
{
    gweniRun.aborted = String(what);
    gweniRun.done = true;
};
