/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#ifndef __OPEN_GL_H__
#define __OPEN_GL_H__

/* gweni patch: freetype-gl is GL-free in every tree. Its vertex buffers are CPU
 * vertex and index accumulators only (gweni's renderers upload and draw them),
 * so no GL header is included and the vertex buffer's GL calls are gone. The
 * types and attribute-type constants its API names are freetype-gl's own, the
 * same sizes and values as GL's, and never GL's names: a file that includes
 * this and a GL header sees no redefinition. */

typedef char           ftgl_char;
typedef int            ftgl_int;
typedef unsigned int   ftgl_uint;
typedef unsigned int   ftgl_enum;
typedef unsigned char  ftgl_boolean;
typedef int            ftgl_sizei;

/* vertex_attribute_t::type, as GL numbers them */
#define FTGL_BYTE           0x1400
#define FTGL_UNSIGNED_BYTE  0x1401
#define FTGL_SHORT          0x1402
#define FTGL_UNSIGNED_SHORT 0x1403
#define FTGL_INT            0x1404
#define FTGL_UNSIGNED_INT   0x1405
#define FTGL_FLOAT          0x1406
#define FTGL_BOOL           0x8B56

/* vertex_buffer_t::mode, as GL numbers it */
#define FTGL_TRIANGLES      0x0004

#endif /* OPEN_GL_H */
