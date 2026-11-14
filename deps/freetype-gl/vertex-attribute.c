/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freetype-gl/vec234.h"
#include "freetype-gl/platform.h"
#include "freetype-gl/vertex-attribute.h"



// ----------------------------------------------------------------------------
vertex_attribute_t *
vertex_attribute_new( ftgl_char * name,
                      ftgl_int size,
                      ftgl_enum type,
                      ftgl_boolean normalized,
                      ftgl_sizei stride,
                      void *pointer )
{
    vertex_attribute_t *attribute =
        (vertex_attribute_t *) malloc (sizeof(vertex_attribute_t));

    assert( size > 0 );

    attribute->name       = (ftgl_char *) strdup( name );
    attribute->index      = -1;
    attribute->size       = size;
    attribute->type       = type;
    attribute->normalized = normalized;
    attribute->stride     = stride;
    attribute->pointer    = pointer;
    return attribute;
}



// ----------------------------------------------------------------------------
void
vertex_attribute_delete( vertex_attribute_t * self )
{
    assert( self );

    free( self->name );
    free( self );
}



// ----------------------------------------------------------------------------
vertex_attribute_t *
vertex_attribute_parse( char *format )
{
    ftgl_enum type = 0;
    int size;
    int normalized = 0;
    char ctype;
    char *name;
    vertex_attribute_t *attr;
    char *p = strchr(format, ':');
    if( p != NULL)
    {
        name = strndup(format, p-format);
        if( *(++p) == '\0' )
        {
            fprintf( stderr, "No size specified for '%s' attribute\n", name );
            free( name );
            return 0;
        }
        size = *p - '0';

        if( *(++p) == '\0' )
        {
            fprintf( stderr, "No format specified for '%s' attribute\n", name );
            free( name );
            return 0;
        }
        ctype = *p;

        if( *(++p) != '\0' )
        {
            if( *p == 'n' )
            {
                normalized = 1;
            }
        }

    }
    else
    {
        fprintf(stderr, "Vertex attribute format not understood ('%s')\n", format );
        return 0;
    }

    switch( ctype )
    {
    case 'b': type = FTGL_BYTE;           break;
    case 'B': type = FTGL_UNSIGNED_BYTE;  break;
    case 's': type = FTGL_SHORT;          break;
    case 'S': type = FTGL_UNSIGNED_SHORT; break;
    case 'i': type = FTGL_INT;            break;
    case 'I': type = FTGL_UNSIGNED_INT;   break;
    case 'f': type = FTGL_FLOAT;          break;
    default:  type = 0;                 break;
    }


    attr = vertex_attribute_new( name, size, type, normalized, 0, 0 );
    free( name );
    return attr;
}
