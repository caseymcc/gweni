/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! YAML, a line at a time:
//! - "---" and "..." at a line's start are Meta (document markers), and so is a
//!   %directive line;
//! - '#' at a line's start or after a space is a Comment to the line's end;
//! - a scalar followed by ':' and a space (or the line's end, or in a flow
//!   collection ',' ']' '}') is a Key, its ':' Punctuation; so are the '-' of a
//!   sequence entry, '?' and the flow indicators [ ] { } ,;
//! - a quoted scalar ("..." with \ escapes, '...' with '') is a String; a plain
//!   one that is a number (decimal, 0x, 0o, .inf, .nan) is a Number, one of
//!   true/false/null/~/yes/no/on/off (any case) a Keyword, any other Plain;
//! - &anchor, *alias and !tag are Meta;
//! - '|' or '>' (with its indicators) ending a line's value opens a block
//!   scalar: every following line that is blank or indented past the node it
//!   belongs to (the key's column, or the '-' of the entry it is the value of)
//!   is a String, and the first line that is not ends it and is read as YAML.
//! A quoted scalar or a flow collection does not run over lines here: the rest
//! of the line is its, and the next line starts afresh.
//!
//! The state: word 0 is 1 in a block scalar, word 1 the column its lines must
//! be indented past.

#include "languages.h"
#include "scan.h"

namespace gweni
{
namespace text
{
namespace highlight
{

namespace
{

const char *const literals[]=
{
    "true", "True", "TRUE", "false", "False", "FALSE", "null", "Null", "NULL", "~",
    "yes", "Yes", "YES", "no", "No", "NO", "on", "On", "ON", "off", "Off", "OFF", nullptr
};

bool isNumber(const String &line, size_t begin, size_t end)
{
    size_t i=begin;

    if((i < end) && ((line[i] == '-') || (line[i] == '+')))
        ++i;
    if(i >= end)
        return false;

    static const char *const special[]={ ".inf", ".Inf", ".INF", ".nan", ".NaN", ".NAN", nullptr };

    if(isOneOf(line, i, end, special))
        return true;
    if((end-i > 2) && (line[i] == '0') && ((line[i+1] == 'x') || (line[i+1] == 'o')))
    {
        const bool hex=(line[i+1] == 'x');

        for(size_t k=i+2; k < end; ++k)
        {
            if(hex?!isHexDigit(line[k]):!((line[k] >= '0') && (line[k] <= '7')))
                return false;
        }
        return true;
    }

    bool digits=false, dot=false, exponent=false;

    for(; i < end; ++i)
    {
        const char c=line[i];

        if(isDigit(c) || ((c == '_') && digits))
            digits=true;
        else if((c == '.') && !dot && !exponent)
            dot=true;
        else if(((c == 'e') || (c == 'E')) && digits && !exponent)
        {
            exponent=true;
            if((i+1 < end) && ((line[i+1] == '+') || (line[i+1] == '-')))
                ++i;
            if((i+1 >= end) || !isDigit(line[i+1]))
                return false;
        }
        else
            return false;
    }
    return digits;
}

//! Whether a ':' at \p i is a mapping indicator: followed by a space, the
//! line's end, or in a flow collection one of , ] }
bool mappingColon(const String &line, size_t i, int flow)
{
    if((i >= line.size()) || (line[i] != ':'))
        return false;
    if(i+1 >= line.size())
        return true;

    const char next=line[i+1];

    return isSpace(next) || ((flow > 0) && inSet(next, ",]}"));
}

//! The end of a quoted scalar starting at \p i (its closing quote included, or
//! the line's end)
size_t quotedEnd(const String &line, size_t i)
{
    const char quote=line[i];
    size_t end=i+1;

    while(end < line.size())
    {
        if((quote == '"') && (line[end] == '\\'))
        {
            end+=2;
            continue;
        }
        if(line[end] == quote)
        {
            // '' is a quote inside a '...' scalar
            if((quote == '\'') && (end+1 < line.size()) && (line[end+1] == '\''))
            {
                end+=2;
                continue;
            }
            return end+1;
        }
        ++end;
    }
    return line.size();
}

class YamlTokenizer: public Tokenizer
{
public:
    TokenState tokenize(const String &line, const TokenState &start, std::vector<Token> &tokens) const override
    {
        const size_t indent=skipSpaces(line, 0);

        if(start.words[0] == 1)
        {
            // a blank line, or one indented past the node, is the block's
            if((indent == line.size()) || (indent > start.words[1]))
            {
                addToken(tokens, indent, line.size(), TokenKind::String);
                return start;
            }
        }

        TokenState state;

        if((startsWithAt(line, 0, "---") || startsWithAt(line, 0, "...")) &&
            ((line.size() == 3) || isSpace(line[3])))
        {
            addToken(tokens, 0, 3, TokenKind::Meta);
            return value(line, 3, 0, tokens, state);
        }
        if(startsWithAt(line, 0, "%"))
        {
            addToken(tokens, 0, line.size(), TokenKind::Meta);
            return state;
        }

        // the sequence entries' dashes (and a complex key's '?'), the node
        // column moving past each
        size_t i=indent;
        size_t node=indent;

        while((i < line.size()) && ((line[i] == '-') || (line[i] == '?')) &&
            ((i+1 == line.size()) || isSpace(line[i+1])))
        {
            addToken(tokens, i, i+1, TokenKind::Punctuation);
            node=i;
            i=skipSpaces(line, i+1);
        }
        return value(line, i, node, tokens, state);
    }

private:
    //! Tokenizes from \p i to the line's end: keys, scalars, flow collections,
    //! anchors, tags and a comment. \p node is the column of the node the line's
    //! value belongs to, which a block scalar opened here is indented past.
    TokenState value(const String &line, size_t i, size_t node, std::vector<Token> &tokens,
        TokenState state) const
    {
        int flow=0;
        // whether the value seen last was a key's (the node moving to that key)
        bool keyStart=true;

        while(i < line.size())
        {
            const char c=line[i];

            if(isSpace(c))
            {
                ++i;
                continue;
            }
            if((c == '#') && ((i == 0) || isSpace(line[i-1])))
            {
                addToken(tokens, i, line.size(), TokenKind::Comment);
                break;
            }
            if(inSet(c, "[{"))
            {
                ++flow;
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                ++i;
                continue;
            }
            if((flow > 0) && inSet(c, "]},"))
            {
                if(c != ',')
                    --flow;
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                ++i;
                continue;
            }
            if(mappingColon(line, i, flow))
            {
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                ++i;
                continue;
            }
            if((c == '&') || (c == '*') || (c == '!'))
            {
                size_t end=i+1;

                while((end < line.size()) && !isSpace(line[end]) && !((flow > 0) && inSet(line[end], ",[]{}")))
                    ++end;
                addToken(tokens, i, end, TokenKind::Meta);
                i=end;
                continue;
            }
            if(((c == '|') || (c == '>')) && (flow == 0))
            {
                // a block scalar's header: the indicator, then +, - or a digit
                size_t end=i+1;

                while((end < line.size()) && (inSet(line[end], "+-") || isDigit(line[end])))
                    ++end;

                const size_t after=skipSpaces(line, end);

                if((after == line.size()) || (line[after] == '#'))
                {
                    addToken(tokens, i, end, TokenKind::Punctuation);
                    if(after < line.size())
                        addToken(tokens, after, line.size(), TokenKind::Comment);
                    state.words[0]=1;
                    state.words[1]=static_cast<uint32_t>(node);
                    break;
                }
            }

            const size_t begin=i;
            size_t end;
            TokenKind kind;

            if((c == '"') || (c == '\''))
            {
                end=quotedEnd(line, i);
                kind=TokenKind::String;
            }
            else
            {
                // a plain scalar: up to a mapping ':', a " #", or in a flow
                // collection one of , [ ] { }; its trailing spaces are not its
                end=i;
                while((end < line.size()) && !mappingColon(line, end, flow) &&
                    !(line[end] == '#' && isSpace(line[end-1])) && !((flow > 0) && inSet(line[end], ",[]{}")))
                    ++end;

                size_t last=end;

                while((last > begin) && isSpace(line[last-1]))
                    --last;
                end=last;
                kind=isNumber(line, begin, end)?TokenKind::Number:
                    (isOneOf(line, begin, end, literals)?TokenKind::Keyword:TokenKind::Plain);
            }

            const size_t next=skipSpaces(line, end);

            if(mappingColon(line, next, flow))
            {
                kind=TokenKind::Key;
                if(keyStart && (flow == 0))
                    node=begin;
            }
            keyStart=false;
            addToken(tokens, begin, end, kind);
            i=std::max(end, begin+1);
        }
        return state;
    }
};

}//namespace

std::unique_ptr<Tokenizer> makeYamlTokenizer()
{
    return std::make_unique<YamlTokenizer>();
}

}//namespace highlight
}//namespace text
}//namespace gweni
