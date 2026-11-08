/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The GL headers OpenGLCore, the examples, the tests and the bench are compiled
 *  against, chosen in this one place:
 *  - GWENI_GL_ES (the GLES tree, ./generate.sh linux gles) and Emscripten:
 *    <GLES3/gl3.h> with <GLES2/gl2ext.h> for the extensions' enums, and no
 *    loader: the core entry points are linked from libGLESv2 directly, so a call
 *    newer than GLES 3.0 does not compile. GWENI_GL_ES is defined in both.
 *  - otherwise: glew, whose glewInit (capture::initGL) loads the desktop entry
 *    points.
 */
#ifndef _gweni_renders_glApi_h_
#define _gweni_renders_glApi_h_

#if defined(__EMSCRIPTEN__) && !defined(GWENI_GL_ES)
#   define GWENI_GL_ES 1
#endif

#if defined(GWENI_GL_ES)
#   include <GLES3/gl3.h>
#   include <GLES2/gl2ext.h>
#elif defined(__APPLE__)
#   include <GL/glew.h>
#   include <OpenGL/gl.h>
#else
#   include <GL/glew.h>
#   include <GL/gl.h>
#endif

#endif//_gweni_renders_glApi_h_
