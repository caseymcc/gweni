/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The shared renderer as the OpenGLCore it is in a GL run, for the cases of the
 *  doctest test suite "gl_only": those check what only the GL backend has (its
 *  GL objects, its shader dialects, its multi-draw), and a run on another backend
 *  leaves them out (-tse=gl_only).
 */
#ifndef _gweni_tests_glRenderer_h_
#define _gweni_tests_glRenderer_h_

#include "gpuRenderer.h"

#include <gweni/platforms/renderers/openglCore.h>

namespace gweni
{
namespace test
{

inline renderer::OpenGLCore &glRenderer()
{
    return dynamic_cast<renderer::OpenGLCore &>(gpuRenderer());
}

}//namespace test
}//namespace gweni

#endif//_gweni_tests_glRenderer_h_
