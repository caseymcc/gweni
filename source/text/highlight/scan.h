/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! What the built-in tokenizers share: character classes over bytes (anything
//! past ASCII is a word byte, so a UTF-8 sequence is never split), and
//! appending a token.

#ifndef _gweni_text_highlight_scan_h_
#define _gweni_text_highlight_scan_h_

#include <gweni/text/tokenizer.h>

#include <algorithm>
#include <cstring>

namespace gweni
{
namespace text
{
namespace highlight
{

inline bool isDigit(char c) { return (c >= '0') && (c <= '9'); }
inline bool isHexDigit(char c) { return isDigit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')); }
inline bool isAlpha(char c) { return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')); }
inline bool isSpace(char c) { return (c == ' ') || (c == '\t') || (c == '\r'); }
//! a letter, digit, '_' or a byte of a multi-byte UTF-8 sequence
inline bool isWordChar(char c) { return isAlpha(c) || isDigit(c) || (c == '_') || (static_cast<unsigned char>(c) >= 0x80); }

//! Whether \p c is one of the bytes of \p set (never for a NUL)
inline bool inSet(char c, const char *set) { return (c != '\0') && (strchr(set, c) != nullptr); }

//! Appends [begin, end) as a token of \p kind; an empty range, or a Plain one,
//! appends nothing.
inline void addToken(std::vector<Token> &tokens, size_t begin, size_t end, TokenKind kind)
{
    if((end <= begin) || (kind == TokenKind::Plain))
        return;
    tokens.push_back(Token{ static_cast<uint32_t>(begin), static_cast<uint32_t>(end), kind });
}

//! The index of the first byte at or after \p i that is not a space or tab
inline size_t skipSpaces(const String &line, size_t i)
{
    while((i < line.size()) && isSpace(line[i]))
        ++i;
    return i;
}

//! Whether \p line holds \p text at \p i
inline bool startsWithAt(const String &line, size_t i, const char *text)
{
    const size_t length=strlen(text);

    return (i+length <= line.size()) && (line.compare(i, length, text) == 0);
}

//! Whether [begin, end) of \p line is one of \p words (a null-terminated list)
inline bool isOneOf(const String &line, size_t begin, size_t end, const char *const *words)
{
    for(const char *const *word=words; *word; ++word)
    {
        const size_t length=strlen(*word);

        if((length == end-begin) && (line.compare(begin, length, *word) == 0))
            return true;
    }
    return false;
}

//! 32-bit FNV-1a of [begin, end) of \p text, for a state that has to remember a
//! word (a heredoc's delimiter)
inline uint32_t hashBytes(const String &text, size_t begin, size_t end)
{
    uint32_t hash=2166136261u;

    for(size_t i=begin; i < end; ++i)
    {
        hash^=static_cast<unsigned char>(text[i]);
        hash*=16777619u;
    }
    return hash;
}

}//namespace highlight
}//namespace text
}//namespace gweni

#endif//_gweni_text_highlight_scan_h_
