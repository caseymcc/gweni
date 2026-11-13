/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! Markdown (CommonMark's blocks, a line at a time):
//! - an ATX heading ("# " to "###### ", up to 3 spaces in) is a Heading;
//! - a thematic break (3 or more '-', '*' or '_', spaces between) Punctuation;
//! - a quote's '>' markers and a list item's marker ("- ", "* ", "+ ", "1. ",
//!   "1) ") are Punctuation, and the rest of the line is inline text;
//! - a fence (3 or more '`' or '~', up to 3 spaces in) opens a code block that
//!   the same character, at least as many times, closes: the fence lines are
//!   Punctuation, their info string Meta, and the body is tokenized in the
//!   info string's language (yaml/yml, json, bash/sh/shell, diff/patch), its
//!   state nested in this one's, or is Code for any other;
//! - <!-- ... --> is a Comment, over lines too;
//! - inline: `code` spans (as many backticks closing as opening) are Code,
//!   **strong** and __strong__ Strong, *emphasis* and _emphasis_ Emphasis
//!   (a '_' run not inside a word), [text](url), [text][ref], ![alt](src) and
//!   <scheme://...> Links, and a backslash escapes the byte after it.
//!
//! The state, word 0: bits 0-1 the mode (0 text, 1 in a fence, 2 in an HTML
//! comment), bits 2-4 the fence's language (0 none), bit 5 a '~' fence, bits
//! 8-15 the fence's length; the fence language's state in the words after.

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

enum Mode: uint32_t
{
    Text=0,
    Fence=1,
    HtmlComment=2
};

enum Language: uint32_t
{
    NoLanguage=0,
    Yaml=1,
    Json=2,
    Bash=3,
    Diff=4
};

uint32_t pack(uint32_t mode, uint32_t language, bool tilde, uint32_t length)
{
    return mode | (language << 2) | (tilde?0x20u:0u) | (std::min<uint32_t>(length, 255) << 8);
}

uint32_t modeOf(uint32_t word) { return word & 3; }
uint32_t languageOf(uint32_t word) { return (word >> 2) & 7; }
bool tildeOf(uint32_t word) { return (word & 0x20) != 0; }
uint32_t lengthOf(uint32_t word) { return (word >> 8) & 0xff; }

//! The fence at \p i (after its indent): its character and length, 0 for none
size_t fenceRun(const String &line, size_t i, char &fence)
{
    if((i >= line.size()) || ((line[i] != '`') && (line[i] != '~')))
        return 0;
    fence=line[i];

    size_t end=i;

    while((end < line.size()) && (line[end] == fence))
        ++end;
    return (end-i >= 3)?end-i:0;
}

class MarkdownTokenizer: public Tokenizer
{
public:
    MarkdownTokenizer()
    {
        m_languages[Yaml]=makeYamlTokenizer();
        m_languages[Json]=makeJsonTokenizer();
        m_languages[Bash]=makeBashTokenizer();
        m_languages[Diff]=makeDiffTokenizer();
    }

    TokenState tokenize(const String &line, const TokenState &start, std::vector<Token> &tokens) const override
    {
        const uint32_t word=start.words[0];
        const size_t indent=skipSpaces(line, 0);

        if(modeOf(word) == Fence)
        {
            char fence=0;
            const size_t length=(indent <= 3)?fenceRun(line, indent, fence):0;

            // the closing fence: as long as the opening one at least, the same
            // character, nothing after it but spaces
            if((length >= lengthOf(word)) && ((fence == '~') == tildeOf(word)) &&
                (skipSpaces(line, indent+length) == line.size()))
            {
                addToken(tokens, indent, indent+length, TokenKind::Punctuation);
                return TokenState();
            }

            const uint32_t language=languageOf(word);

            if((language != NoLanguage) && m_languages[language])
            {
                const TokenState inner=m_languages[language]->tokenize(line, start.inner(), tokens);

                return TokenState::nested(word, inner);
            }
            addToken(tokens, 0, line.size(), TokenKind::Code);
            return start;
        }

        TokenState state;
        size_t i=0;

        if(modeOf(word) == HtmlComment)
        {
            const size_t close=line.find("-->");

            if(close == String::npos)
            {
                addToken(tokens, indent, line.size(), TokenKind::Comment);
                return start;
            }
            addToken(tokens, indent, close+3, TokenKind::Comment);
            i=close+3;
        }
        else if(indent <= 3)
        {
            char fence=0;
            const size_t length=fenceRun(line, indent, fence);

            if(length > 0)
            {
                const size_t infoBegin=skipSpaces(line, indent+length);
                size_t infoEnd=infoBegin;

                while((infoEnd < line.size()) && !isSpace(line[infoEnd]))
                    ++infoEnd;
                // a '`' fence's info string holds no '`'
                if((fence == '~') || (line.find('`', indent+length) == String::npos))
                {
                    addToken(tokens, indent, indent+length, TokenKind::Punctuation);
                    addToken(tokens, infoBegin, line.size(), TokenKind::Meta);
                    return TokenState::nested(pack(Fence, languageNamed(line, infoBegin, infoEnd), fence == '~',
                        static_cast<uint32_t>(length)), TokenState());
                }
            }
            if((indent < line.size()) && (line[indent] == '#'))
            {
                size_t hashes=indent;

                while((hashes < line.size()) && (line[hashes] == '#'))
                    ++hashes;
                if((hashes-indent <= 6) && ((hashes == line.size()) || isSpace(line[hashes])))
                {
                    addToken(tokens, indent, line.size(), TokenKind::Heading);
                    return state;
                }
            }
            if(thematicBreak(line, indent))
            {
                addToken(tokens, indent, line.size(), TokenKind::Punctuation);
                return state;
            }
            i=containerMarkers(line, indent, tokens);
        }
        return inlineText(line, i, tokens, state);
    }

private:
    static uint32_t languageNamed(const String &line, size_t begin, size_t end)
    {
        static const char *const yaml[]={ "yaml", "yml", nullptr };
        static const char *const json[]={ "json", "jsonc", nullptr };
        static const char *const bash[]={ "bash", "sh", "shell", nullptr };
        static const char *const diff[]={ "diff", "patch", nullptr };

        if(isOneOf(line, begin, end, yaml))
            return Yaml;
        if(isOneOf(line, begin, end, json))
            return Json;
        if(isOneOf(line, begin, end, bash))
            return Bash;
        if(isOneOf(line, begin, end, diff))
            return Diff;
        return NoLanguage;
    }

    static bool thematicBreak(const String &line, size_t i)
    {
        if((i >= line.size()) || !inSet(line[i], "-*_"))
            return false;

        const char c=line[i];
        size_t count=0;

        for(; i < line.size(); ++i)
        {
            if(line[i] == c)
                ++count;
            else if(!isSpace(line[i]))
                return false;
        }
        return count >= 3;
    }

    //! The quote markers and a list item's marker from \p i, as Punctuation;
    //! returns where the line's text starts
    static size_t containerMarkers(const String &line, size_t i, std::vector<Token> &tokens)
    {
        while(i < line.size())
        {
            if(line[i] == '>')
            {
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                i=skipSpaces(line, i+1);
                continue;
            }
            if(inSet(line[i], "-*+") && ((i+1 == line.size()) || isSpace(line[i+1])))
            {
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                return skipSpaces(line, i+1);
            }

            size_t digits=i;

            while((digits < line.size()) && (digits-i < 9) && isDigit(line[digits]))
                ++digits;
            if((digits > i) && (digits < line.size()) && inSet(line[digits], ".)") &&
                ((digits+1 == line.size()) || isSpace(line[digits+1])))
            {
                addToken(tokens, i, digits+1, TokenKind::Punctuation);
                return skipSpaces(line, digits+1);
            }
            break;
        }
        return i;
    }

    //! The inline text from \p i to the line's end
    static TokenState inlineText(const String &line, size_t i, std::vector<Token> &tokens, TokenState state)
    {
        while(i < line.size())
        {
            const char c=line[i];

            if(c == '\\')
            {
                i+=2;
                continue;
            }
            if(startsWithAt(line, i, "<!--"))
            {
                const size_t close=line.find("-->", i+4);

                if(close == String::npos)
                {
                    addToken(tokens, i, line.size(), TokenKind::Comment);
                    state.words[0]=pack(HtmlComment, NoLanguage, false, 0);
                    return state;
                }
                addToken(tokens, i, close+3, TokenKind::Comment);
                i=close+3;
                continue;
            }
            if(c == '`')
            {
                size_t run=i;

                while((run < line.size()) && (line[run] == '`'))
                    ++run;

                const size_t ticks=run-i;
                size_t search=run;
                size_t close=String::npos;

                // the closing run: exactly as many backticks
                while((search=line.find('`', search)) != String::npos)
                {
                    size_t end=search;

                    while((end < line.size()) && (line[end] == '`'))
                        ++end;
                    if(end-search == ticks)
                    {
                        close=end;
                        break;
                    }
                    search=end;
                }
                if(close == String::npos)
                {
                    i=run;
                    continue;
                }
                addToken(tokens, i, close, TokenKind::Code);
                i=close;
                continue;
            }
            if((c == '*') || (c == '_'))
            {
                size_t end=delimited(line, i);

                if(end > i)
                {
                    const bool strong=(i+1 < line.size()) && (line[i+1] == c);

                    addToken(tokens, i, end, strong?TokenKind::Strong:TokenKind::Emphasis);
                    i=end;
                    continue;
                }
                while((i < line.size()) && (line[i] == c))
                    ++i;
                continue;
            }
            if((c == '[') || ((c == '!') && (i+1 < line.size()) && (line[i+1] == '[')))
            {
                const size_t end=linkEnd(line, (c == '!')?i+1:i);

                if(end > i)
                {
                    addToken(tokens, i, end, TokenKind::Link);
                    i=end;
                    continue;
                }
            }
            if(c == '<')
            {
                const size_t close=line.find('>', i+1);
                const size_t scheme=line.find("://", i+1);

                if((close != String::npos) && (scheme != String::npos) && (scheme < close) &&
                    (line.find(' ', i+1) > close))
                {
                    addToken(tokens, i, close+1, TokenKind::Link);
                    i=close+1;
                    continue;
                }
            }
            ++i;
        }
        return state;
    }

    //! The end of the emphasis or strong span opening at \p i (after its closing
    //! delimiter), \p i when none opens there: one or two of the byte at \p i,
    //! not followed by a space (nor, for '_', preceded by a word byte), and the
    //! same run closing it later on the line, not preceded by a space
    static size_t delimited(const String &line, size_t i)
    {
        const char c=line[i];
        const size_t width=((i+1 < line.size()) && (line[i+1] == c))?2:1;
        const size_t content=i+width;

        if((content >= line.size()) || isSpace(line[content]) || (line[content] == c))
            return i;
        if((c == '_') && (i > 0) && isWordChar(line[i-1]))
            return i;

        const String delimiter(width, c);
        size_t search=content+1;

        while((search=line.find(delimiter, search)) != String::npos)
        {
            const bool run=(search+width < line.size()) && (line[search+width] == c);
            const bool afterWord=(c == '_') && (search+width < line.size()) && isWordChar(line[search+width]);

            if(!isSpace(line[search-1]) && !run && !afterWord)
                return search+width;
            search+=1;
        }
        return i;
    }

    //! The end of a link whose '[' is at \p i: [text](destination) or
    //! [text][label], after its ')' or ']'; \p i when there is none
    static size_t linkEnd(const String &line, size_t i)
    {
        int depth=0;
        size_t close=String::npos;

        for(size_t k=i; k < line.size(); ++k)
        {
            if(line[k] == '\\')
            {
                ++k;
                continue;
            }
            if(line[k] == '[')
                ++depth;
            else if((line[k] == ']') && (--depth == 0))
            {
                close=k;
                break;
            }
        }
        if((close == String::npos) || (close+1 >= line.size()))
            return i;
        if(line[close+1] == '(')
        {
            const size_t end=line.find(')', close+2);

            return (end == String::npos)?i:end+1;
        }
        if(line[close+1] == '[')
        {
            const size_t end=line.find(']', close+2);

            return (end == String::npos)?i:end+1;
        }
        return i;
    }

    std::unique_ptr<Tokenizer> m_languages[Diff+1];
};

}//namespace

std::unique_ptr<Tokenizer> makeMarkdownTokenizer()
{
    return std::make_unique<MarkdownTokenizer>();
}

}//namespace highlight
}//namespace text
}//namespace gweni
