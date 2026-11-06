/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The one OpenGLCore renderer the unit tests share. main.cpp creates it on a
 *  surfaceless EGL context before any test case runs, so a test can call
 *  measureText/renderText, which need a GL context and a loaded font.
 */
#ifndef _gweni_tests_gpuRenderer_h_
#define _gweni_tests_gpuRenderer_h_

#include <gweni/platforms/renderers/openglCore.h>

namespace gweni
{
namespace test
{

//! The renderer, init()'d, drawing into an offscreen framebuffer of viewSize().
renderer::OpenGLCore &gpuRenderer();

//! OpenSans (resources/OpenSans.ttf), already loaded through loadFont.
const Font &openSans();

//! Size of the offscreen framebuffer, which is bound while the tests run.
Point viewSize();

}//namespace test
}//namespace gweni

#endif//_gweni_tests_gpuRenderer_h_
