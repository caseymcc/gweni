/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! Jinja as an overlay over a host language (makeJinjaOverlay()): the tags are
//! cut out of each line first and the host tokenizes what is left, each tag
//! standing in it as a word of as many 'x's.
//!
//! The state: word 0 is the tag open at the line's end (0 none, 1 {{, 2 {%,
//! 3 {#), the host's state in the words after.

#include "languages.h"
#include "scan.h"

namespace gweni
{
namespace text
{

namespace
{

using highlight::addToken;
using highlight::isSpace;
using highlight::isWordChar;
using highlight::skipSpaces;

enum Tag: uint32_t
{
    NoTag=0,
    Expression=1,
    Statement=2,
    Comment=3
};

const char *closing(uint32_t tag)
{
    return (tag == Expression)?"}}":((tag == Statement)?"%}":"#}");
}

//! Where \p tag's closing delimiter ends on \p line, searching from \p from
//! (npos when it does not close there): in a {{ }} or {% %}, past the quoted
//! strings, whose bytes cannot close it
size_t findClose(const String &line, size_t from, uint32_t tag)
{
    const char *close=closing(tag);

    if(tag == Comment)
    {
        const size_t at=line.find(close, from);

        return (at == String::npos)?at:at+2;
    }
    for(size_t i=from; i < line.size(); ++i)
    {
        const char c=line[i];

        if((c == '"') || (c == '\''))
        {
            ++i;
            while((i < line.size()) && (line[i] != c))
                i+=(line[i] == '\\')?2:1;
            continue;
        }
        if((c == close[0]) && (i+1 < line.size()) && (line[i+1] == close[1]))
            return i+2;
    }
    return String::npos;
}

//! One tag's bytes on a line, and whether it opened on an earlier line
struct TagRange
{
    size_t begin;
    size_t end;
    uint32_t tag;
    bool continued;
};

//! The tokens of a {{ }} or {% %} tag's bytes: its statement name (the first
//! word of a {% %} that opened on this line) a Keyword, its quoted strings
//! Strings, the rest Template
void tagTokens(const String &line, const TagRange &range, std::vector<Token> &tokens)
{
    if(range.tag == Comment)
    {
        addToken(tokens, range.begin, range.end, TokenKind::Comment);
        return;
    }

    size_t from=range.begin;
    size_t i=range.begin;

    if(!range.continued)
    {
        i+=2;
        if((i < range.end) && ((line[i] == '-') || (line[i] == '+')))
            ++i;
        if(range.tag == Statement)
        {
            const size_t word=skipSpaces(line, i);
            size_t end=word;

            while((end < range.end) && isWordChar(line[end]))
                ++end;
            if(end > word)
            {
                addToken(tokens, from, word, TokenKind::Template);
                addToken(tokens, word, end, TokenKind::Keyword);
                from=end;
                i=end;
            }
        }
    }
    while(i < range.end)
    {
        const char c=line[i];

        if((c == '"') || (c == '\''))
        {
            size_t end=i+1;

            while((end < range.end) && (line[end] != c))
                end+=(line[end] == '\\')?2:1;
            end=std::min(end+1, range.end);
            addToken(tokens, from, i, TokenKind::Template);
            addToken(tokens, i, end, TokenKind::String);
            from=end;
            i=end;
            continue;
        }
        ++i;
    }
    addToken(tokens, from, range.end, TokenKind::Template);
}

class JinjaOverlay: public Tokenizer
{
public:
    explicit JinjaOverlay(const Tokenizer &host): m_host(host) {}

    TokenState initialState() const override
    {
        return TokenState::nested(NoTag, m_host.initialState());
    }

    TokenState tokenize(const String &line, const TokenState &start, std::vector<Token> &tokens) const override
    {
        uint32_t open=start.words[0];
        std::vector<TagRange> tags;
        size_t i=0;

        // the tag the line starts in, then each tag opened on it
        if(open != NoTag)
        {
            const size_t close=findClose(line, 0, open);
            const size_t end=(close == String::npos)?line.size():close;

            tags.push_back({0, end, open, true});
            if(close != String::npos)
                open=NoTag;
            i=end;
        }
        while((open == NoTag) && (i < line.size()))
        {
            const size_t brace=line.find('{', i);

            if((brace == String::npos) || (brace+1 >= line.size()))
                break;

            const char kind=line[brace+1];
            const uint32_t tag=(kind == '{')?Expression:((kind == '%')?Statement:((kind == '#')?Comment:NoTag));

            if(tag == NoTag)
            {
                i=brace+1;
                continue;
            }

            const size_t close=findClose(line, brace+2, tag);
            const size_t end=(close == String::npos)?line.size():close;

            tags.push_back({brace, end, tag, false});
            if(close == String::npos)
                open=tag;
            i=end;
        }

        const TokenState hostStart=start.inner();
        TokenState hostEnd=hostStart;

        if(tags.empty())
        {
            hostEnd=m_host.tokenize(line, hostStart, tokens);
            return TokenState::nested(open, hostEnd);
        }

        size_t covered=0;

        for(const TagRange &range:tags)
            covered+=range.end-range.begin;

        std::vector<Token> host;

        // a line wholly inside tags is none of the host's
        if(covered < line.size())
        {
            String masked(line);

            for(const TagRange &range:tags)
                std::fill(masked.begin()+range.begin, masked.begin()+range.end, 'x');
            hostEnd=m_host.tokenize(masked, hostStart, host);
        }

        // the host's tokens cut around the tags, the tags' own between them
        size_t next=0;

        for(const TagRange &range:tags)
        {
            for(; next < host.size(); ++next)
            {
                Token token=host[next];

                if(token.begin >= range.begin)
                    break;
                if(token.end > range.begin)
                {
                    // the piece before the tag; the rest, past it, is cut again below
                    addToken(tokens, token.begin, range.begin, token.kind);
                    if(token.end > range.end)
                    {
                        host[next].begin=static_cast<uint32_t>(range.end);
                        break;
                    }
                    continue;
                }
                tokens.push_back(token);
            }
            tagTokens(line, range, tokens);
            // the host's tokens inside the tag are the tag's
            while((next < host.size()) && (host[next].end <= range.end))
                ++next;
            if((next < host.size()) && (host[next].begin < range.end))
                host[next].begin=static_cast<uint32_t>(range.end);
        }
        for(; next < host.size(); ++next)
            tokens.push_back(host[next]);
        return TokenState::nested(open, hostEnd);
    }

private:
    const Tokenizer &m_host;
};

}//namespace

std::unique_ptr<Tokenizer> makeJinjaOverlay(const Tokenizer &host)
{
    return std::make_unique<JinjaOverlay>(host);
}

}//namespace text
}//namespace gweni
