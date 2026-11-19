/*
 *  Gweni - the names a script gives keys (gweni/app/keys.h).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/app/keys.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <cctype>

namespace gweni
{
namespace app
{

// Named keys as the GLFW key codes a keyboard sends, so a script's key goes
// through gweni::input::GLFW3's translator exactly as a window's key callback.
const std::vector<NamedKey> &namedKeys()
{
    static const std::vector<NamedKey> keys=
    {
        {"return", GLFW_KEY_ENTER},          {"backspace", GLFW_KEY_BACKSPACE},
        {"delete", GLFW_KEY_DELETE},         {"left", GLFW_KEY_LEFT},
        {"right", GLFW_KEY_RIGHT},           {"shift", GLFW_KEY_LEFT_SHIFT},
        {"tab", GLFW_KEY_TAB},               {"space", GLFW_KEY_SPACE},
        {"home", GLFW_KEY_HOME},             {"end", GLFW_KEY_END},
        {"control", GLFW_KEY_LEFT_CONTROL},  {"up", GLFW_KEY_UP},
        {"down", GLFW_KEY_DOWN},             {"escape", GLFW_KEY_ESCAPE},
        {"alt", GLFW_KEY_LEFT_ALT},          {"command", GLFW_KEY_LEFT_SUPER},
        {"kp-enter", GLFW_KEY_KP_ENTER},     {"f9", GLFW_KEY_F9},
        {"f10", GLFW_KEY_F10},               {"f11", GLFW_KEY_F11},
        {"f12", GLFW_KEY_F12},               {"pageup", GLFW_KEY_PAGE_UP},
        {"pagedown", GLFW_KEY_PAGE_DOWN},    {"f4", GLFW_KEY_F4},
    };
    return keys;
}

namespace
{

// The modifiers a chord may hold, with the mods bit GLFW reports for them.
struct ChordModifier
{
    const char *name;
    int key;
    int mod;
};

const ChordModifier ChordModifiers[]=
{
    {"ctrl", GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL},
    {"control", GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL},
    {"shift", GLFW_KEY_LEFT_SHIFT, GLFW_MOD_SHIFT},
    {"alt", GLFW_KEY_LEFT_ALT, GLFW_MOD_ALT},
    {"command", GLFW_KEY_LEFT_SUPER, GLFW_MOD_SUPER},
};

}//namespace

// GLFW names a letter by its uppercase ASCII code, and a digit by its own.
bool keyCode(const std::string &part, int &key)
{
    for(const NamedKey &named : namedKeys())
    {
        if(part == named.name)
        {
            key=named.key;
            return true;
        }
    }

    if(part.size() == 1 && std::isalpha(static_cast<unsigned char>(part[0])))
    {
        key=GLFW_KEY_A+(std::toupper(static_cast<unsigned char>(part[0]))-'A');
        return true;
    }
    if(part.size() == 1 && part[0] >= '0' && part[0] <= '9')
    {
        key=GLFW_KEY_0+(part[0]-'0');
        return true;
    }
    return false;
}

std::string keyName(int key)
{
    for(const NamedKey &named : namedKeys())
        if(named.key == key)
            return named.name;
    if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return std::string(1, static_cast<char>(key));
    if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return std::string(1, static_cast<char>(key));
    return std::to_string(key);
}

ChordError parseChord(const std::string &name, Chord &chord, std::string &badPart)
{
    chord=Chord();
    badPart.clear();
    if(name.size() == 1 && name[0] >= ' ' && name[0] < '\x7f')
    {
        chord.isCharacter=true;
        chord.character=static_cast<unsigned char>(name[0]);
        return ChordError::None;
    }

    std::vector<std::string> parts;
    for(size_t from=0;;)
    {
        const size_t plus=name.find('+', from);
        parts.push_back(name.substr(from, plus == std::string::npos ? std::string::npos : plus-from));
        if(plus == std::string::npos)
            break;
        from=plus+1;
    }

    std::vector<int> modifierKeys;
    std::vector<int> modifierBits;
    for(size_t i=0; i+1 < parts.size(); ++i)
    {
        bool found=false;
        for(const ChordModifier &modifier : ChordModifiers)
        {
            if(parts[i] != modifier.name)
                continue;
            modifierKeys.push_back(modifier.key);
            modifierBits.push_back(modifier.mod);
            found=true;
            break;
        }
        if(!found)
        {
            badPart=parts[i];
            return ChordError::NotAModifier;
        }
    }

    int key=0;
    if(!keyCode(parts.back(), key))
    {
        badPart=parts.back();
        return ChordError::UnknownKey;
    }

    int mods=0;
    for(size_t i=0; i < modifierKeys.size(); ++i)
    {
        chord.strokes.push_back({modifierKeys[i], GLFW_PRESS, mods});
        mods|=modifierBits[i];
    }

    chord.strokes.push_back({key, GLFW_PRESS, mods});
    chord.strokes.push_back({key, GLFW_RELEASE, mods});

    for(size_t i=modifierKeys.size(); i-- > 0;)
    {
        chord.strokes.push_back({modifierKeys[i], GLFW_RELEASE, mods});
        mods&=~modifierBits[i];
    }
    return ChordError::None;
}

std::vector<unsigned int> decodeUtf8(const std::string &text)
{
    std::vector<unsigned int> codepoints;
    const size_t size=text.size();

    for(size_t i=0; i < size;)
    {
        const unsigned char lead=static_cast<unsigned char>(text[i]);
        size_t length=0;
        unsigned int codepoint=0;
        unsigned int minimum=0;

        if(lead < 0x80){ length=1; codepoint=lead; }
        else if((lead & 0xE0) == 0xC0){ length=2; codepoint=lead & 0x1F; minimum=0x80; }
        else if((lead & 0xF0) == 0xE0){ length=3; codepoint=lead & 0x0F; minimum=0x800; }
        else if((lead & 0xF8) == 0xF0){ length=4; codepoint=lead & 0x07; minimum=0x10000; }

        bool valid=length != 0 && i+length <= size;
        for(size_t j=1; valid && j < length; ++j)
        {
            const unsigned char next=static_cast<unsigned char>(text[i+j]);
            if((next & 0xC0) != 0x80)
                valid=false;
            else
                codepoint=(codepoint << 6) | (next & 0x3F);
        }
        if(valid && (codepoint < minimum || codepoint > 0x10FFFF
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF)))
            valid=false;

        if(valid)
        {
            codepoints.push_back(codepoint);
            i+=length;
        }
        else
        {
            codepoints.push_back(0xFFFD);
            ++i;
        }
    }
    return codepoints;
}

}//namespace app
}//namespace gweni
