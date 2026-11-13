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

//! As newRenderer, on the same device as if it had no dual-source blending:
//! an OpenGLCore constructed with GWENI_GL_NO_DUAL_SOURCE=1, a renderer::Vulkan
//! on a copy of the device with VulkanDevice::dualSourceBlend off.
renderer::SlotRenderer *newRendererWithoutDualSource(ResourcePaths &paths, const Rect &viewRect);

//! Whether gpuRenderer()'s device has dual-source blending (GL's
//! glCapabilities(), Vulkan's dualSrcBlend feature)
bool deviceHasDualSource();

//! Gives a renderer the OpenSans family of resources/DefaultSkin.json's "fonts"
//! (OpenSans-Bold, -Italic and -BoldItalic), under the names "OpenSans" and
//! "OpenSans.ttf" (openSans()'s). gpuRenderer() has it; a newRenderer() has no
//! family until a case gives it one.
void setOpenSansFamily(renderer::SlotRenderer &renderer);

//! OpenSans (resources/OpenSans.ttf), already loaded through loadFont.
const Font &openSans();

//! Size of the offscreen target, which is bound while the tests run.
Point viewSize();

//! Gives the target a 32-bit float depth buffer (GL_DEPTH_COMPONENT32F,
//! VK_FORMAT_D32_SFLOAT) or, with \a on false, its usual one (24-bit), its
//! contents undefined until the next clearView(); false if the device cannot.
bool setFloatDepth(bool on);

//! Puts the renderer's render offset at (0,0), its clip region at the whole
//! view and its scale at 1 (the identity transform). A listener in main.cpp
//! calls it before every case and every re-entry for its next subcase, so a
//! case draws from that state whatever the one before it left (a canvas's
//! renderCanvas() leaves its own bounds as the clip); a case need not restore
//! them. Nothing without a renderer.
void resetDrawState();

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
