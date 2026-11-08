/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The one renderer the unit tests share, as the API-independent SlotRenderer, so
 *  a test is written once for every backend. main.cpp creates it (OpenGLCore on a
 *  surfaceless EGL context) before any test case runs, so a test can call
 *  measureText/renderText, which need a device and a loaded font. A test that
 *  checks something only GL has (GL object names, shader dialects, the multi-draw)
 *  is in the doctest test suite "gl_only" and says why at its top.
 */
#ifndef _gweni_tests_gpuRenderer_h_
#define _gweni_tests_gpuRenderer_h_

#include <gweni/platforms/renderers/slotRenderer.h>

#include <cstdint>
#include <vector>

namespace gweni
{
namespace test
{

//! The renderer, init()'d, drawing into an offscreen target of viewSize().
renderer::SlotRenderer &gpuRenderer();

//! A new renderer of gpuRenderer()'s backend, on its device and drawing into the
//! same target, not init()'d yet. The caller deletes it; it keeps a reference to
//! paths.
renderer::SlotRenderer *newRenderer(ResourcePaths &paths, const Rect &viewRect);

//! OpenSans (resources/OpenSans.ttf), already loaded through loadFont.
const Font &openSans();

//! Size of the offscreen target, which is bound while the tests run.
Point viewSize();

//! Clears the target to color, opaque whatever its alpha, and its depth.
inline void clearView(const Color &color)
{
    gpuRenderer().clearTarget(Color(color.r, color.g, color.b, 255));
}

//! The whole target, top-down RGBA8.
inline void readView(std::vector<uint8_t> &pixels)
{
    const Point size=viewSize();

    gpuRenderer().readPixels(size.x, size.y, pixels);
}

}//namespace test
}//namespace gweni

#endif//_gweni_tests_gpuRenderer_h_
