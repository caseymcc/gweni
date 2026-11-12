/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#ifndef __UTF8_UTILS_H__
#define __UTF8_UTILS_H__

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {

namespace ftgl {
#endif

/**
 * @file    utf8-utils.h
 * @author  Marcel Metz <mmetz@adrian-broher.net>
 *
 * defgroup utf8-utils UTF-8 Utilities
 *
 * @{
 */

  /**
   * Returns the size in bytes of a given UTF-8 encoded character surrogate
   *
   * @param character  An UTF-8 encoded character
   *
   * @return  The length of the surrogate in bytes.
   */
  size_t
  utf8_surrogate_len( const char* character );

  /**
   * Return the length of the given UTF-8 encoded and
   * NULL terminated string.
   *
   * @param string  An UTF-8 encoded string
   *
   * @return  The length of the string in characters.
   */
  size_t
  utf8_strlen( const char* string );

  /**
   * Converts a given UTF-8 encoded character to its UTF-32 LE equivalent
   *
   * @param character  An UTF-8 encoded character
   *
   * @return  The equivalent of the given character in UTF-32 LE
   *          encoding.
   */
  uint32_t
  utf8_to_utf32( const char * character );

  /**
   * gweni: whether a UTF-8 encoded character is one of the default-ignorable
   * codepoints text_buffer_add_char draws as nothing, with no advance and no
   * glyph: U+200B-200D (zero width space, non-joiner and joiner), U+2060 (word
   * joiner), U+FE00-FE0F (variation selectors) and U+FEFF (zero width no-break
   * space). Each is 3 bytes, so it is a byte compare, with no decoding; it reads
   * no byte past a NUL. Inline, since every text walk asks it of every
   * codepoint: an ASCII one costs the first byte's two compares.
   *
   * @param character  An UTF-8 encoded character
   *
   * @return  1 if it is one of them, 0 otherwise.
   */
  static inline int
  utf8_is_ignorable( const char * character )
  {
      const unsigned char *c = (const unsigned char *) character;

      if( !c || c[0] < 0xE2 )
          return 0;
      /* U+200B-200D: E2 80 8B-8D; U+2060: E2 81 A0 */
      if( c[0] == 0xE2 )
          return ( c[1] == 0x80 && c[2] >= 0x8B && c[2] <= 0x8D )
              || ( c[1] == 0x81 && c[2] == 0xA0 );
      /* U+FE00-FE0F: EF B8 80-8F; U+FEFF: EF BB BF */
      if( c[0] == 0xEF )
          return ( c[1] == 0xB8 && c[2] >= 0x80 && c[2] <= 0x8F )
              || ( c[1] == 0xBB && c[2] == 0xBF );
      return 0;
  }

/**
 * @}
 */

#ifdef __cplusplus
}
}
#endif

#endif /* #define __UTF8_UTILS_H__ */
