/*
 *  Gweni - the names a script gives keys (gweni_app, gweni::app): the named
 *  keys and chords ControlTest's --key and app::Headless::key() take, as the
 *  GLFW key events a keyboard sends, and UTF-8 text as the codepoints the char
 *  callback delivers. One table, so the two take the same names.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_app_keys_h_
#define _gweni_app_keys_h_

#include <string>
#include <vector>

namespace gweni
{
namespace app
{

//! A named key: its name ("return", "left", "f9", ...) and GLFW key code.
struct NamedKey
{
    const char *name;
    int key;
};

//! The named keys, in their table's order.
const std::vector<NamedKey> &namedKeys();

//! A chord's last part as a GLFW key code: a named key, or one letter
//! (either case) or digit. \return false for anything else.
bool keyCode(const std::string &part, int &key);

//! A GLFW key code's name: its named key's, a letter or digit itself, else
//! the number.
std::string keyName(int key);

//! One GLFW key event, as glfwSetKeyCallback delivers it.
struct KeyStroke
{
    int key;
    int action;     //!< GLFW_PRESS or GLFW_RELEASE
    int mods;       //!< the GLFW_MOD_* bits held
};

//! What a script's key names: a printable ASCII character to type (the char
//! callback), or a chord of modifiers and a key joined by '+' ("ctrl+shift+z",
//! "f4", "x" is a character) as key events: each modifier pressed in order,
//! the key pressed and released with every modifier's bit set, then the
//! modifiers released in reverse. A chord's key goes only down the key path,
//! as on a keyboard, where GLFW sends no char for Ctrl+letter.
struct Chord
{
    bool isCharacter=false;
    unsigned int character=0;
    std::vector<KeyStroke> strokes;
};

//! Why parseChord() refused a name.
enum class ChordError
{
    None,
    NotAModifier,   //!< a part before the last is none of ctrl, control, shift, alt, command
    UnknownKey      //!< the last part is no key keyCode() knows
};

//! Reads \p name into \p chord. \return ChordError::None, or why not, with
//! \p badPart the part at fault.
ChordError parseChord(const std::string &name, Chord &chord, std::string &badPart);

//! UTF-8 to codepoints. A malformed sequence (a stray continuation byte, a
//! truncated, overlong or surrogate encoding) decodes to U+FFFD and decoding
//! resumes at the next byte.
std::vector<unsigned int> decodeUtf8(const std::string &text);

}//namespace app
}//namespace gweni

#endif//_gweni_app_keys_h_
