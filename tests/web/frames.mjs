//
//  Gweni
//  Copyright (c) 2010 Facepunch Studios
//  MIT License, see LICENSE
//
//  Waiting on an interactive page's window (capture::runWindowLoop on
//  input::GlfwLoop): the loop is idle, so a page draws a frame
//  (gweniRun.frames, web/pre.js) only when its canvas has work and otherwise
//  returns from its animation-frame callback at once (gweniRun.ticks counts
//  them). "Some more frames" is then no wait at all for a page that has
//  settled, so the scripts wait for the page to settle instead.
//
//  settle(page, count): resolves once the page has drawn `count` frames since
//  the call (a page that keeps drawing: the stats overlay, an animation), or
//  once a tick has passed with no frame drawn in it (an idle page: whatever
//  the input before the call asked for has been drawn, in the ticks before),
//  or its run has ended. The page's own clock does the waiting, so it is as
//  quick as the page.
//
//  firstFrame(page, timeout): resolves once the page has drawn its first
//  frame and settled, or its run has ended; rejects on the timeout.
//

export async function settle(page, count = 3, timeout = 60000)
{
    const token = `${Date.now()}-${Math.random()}`;

    await page.evaluate(token => {
        const run = window.gweniRun;
        window.gweniSettle = { token, frames0: run.frames, ticks: run.ticks, frames: run.frames };
    }, token);
    await page.waitForFunction(([token, count]) => {
        const run = window.gweniRun;
        const state = window.gweniSettle;

        if(!run || !state || state.token !== token)
            return false;
        if(run.done || run.frames >= state.frames0+count)
            return true;
        if(run.ticks === state.ticks)
            return false;
        // a tick (or more) since the last look: idle if it drew nothing
        const idle = run.frames === state.frames;
        state.ticks = run.ticks;
        state.frames = run.frames;
        return idle;
    }, [token, count], { timeout, polling: 16 });
}

export async function firstFrame(page, timeout = 300000)
{
    await page.waitForFunction(() => window.gweniRun && (window.gweniRun.done || window.gweniRun.frames >= 1),
        undefined, { timeout, polling: 100 });
    if(!(await page.evaluate(() => window.gweniRun.done)))
        await settle(page, 3, timeout);
}
