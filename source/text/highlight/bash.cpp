/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! bash: a '#' at a word's start to the line's end is a Comment; '...' and
//! "..." are Strings (a "..." with its $variables cut out of it, as Meta), and
//! either may run over lines; $name, ${...}, $1, $@, $?, ... are Meta; the
//! reserved words and the common builtins that shape a script (local, export,
//! return, ...) are Keywords when they are a whole word; a NAME= assignment's
//! name is a Key; a number that is a whole word a Number; the operators | & ;
//! < > ( ) { } [[ ]] and $( Punctuation. A heredoc (<<EOF, <<-EOF, <<'EOF',
//! <<"EOF") starts on the next line: its body is a String up to the line that
//! is its delimiter alone (with <<-, after leading tabs), which is Meta, as the
//! delimiter is where it is opened.
//!
//! The state: word 0 is the mode (0 code, 1 in '...', 2 in "...", 3 in a
//! heredoc's body, 4 in a <<- heredoc's body); for a heredoc, words 1 and 2
//! are its delimiter's hash and length. A line that opens a heredoc and leaves
//! a string open ends in the heredoc.

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
    Code=0,
    SingleQuoted=1,
    DoubleQuoted=2,
    Heredoc=3,
    HeredocTabs=4
};

const char *const keywords[]=
{
    "if", "then", "else", "elif", "fi", "for", "while", "until", "do", "done", "case", "esac", "in",
    "function", "select", "time", "return", "local", "export", "declare", "readonly", "typeset",
    "unset", "break", "continue", "exit", "shift", "source", "trap", "set", "eval", "exec", nullptr
};

//! Whether the byte before \p i lets a word start at \p i: the line's start, a
//! space or an operator
bool wordStart(const String &line, size_t i)
{
    if(i == 0)
        return true;

    const char c=line[i-1];

    return isSpace(c) || (c == ';') || (c == '|') || (c == '&') || (c == '(') || (c == ')') ||
        (c == '{') || (c == '}') || (c == '`');
}

//! The end of a $variable at \p i (line[i] == '$'), \p i+1 when no variable follows
size_t variableEnd(const String &line, size_t i)
{
    size_t end=i+1;

    if(end >= line.size())
        return end;
    if(line[end] == '{')
    {
        const size_t close=line.find('}', end);

        return (close == String::npos)?line.size():close+1;
    }
    if(isAlpha(line[end]) || (line[end] == '_'))
    {
        while((end < line.size()) && (isAlpha(line[end]) || isDigit(line[end]) || (line[end] == '_')))
            ++end;
        return end;
    }
    if(isDigit(line[end]) || inSet(line[end], "@*#?$!-"))
        return end+1;
    return end;
}

//! Tokenizes a "..." body from \p i, the string's first piece starting at
//! \p from (its opening quote, or the line's start for one that runs over):
//! Strings around the $variables in it. Returns the index after the closing
//! '"' and sets \p closed, or the line's length with \p closed false.
size_t doubleQuoted(const String &line, size_t from, size_t i, std::vector<Token> &tokens, bool &closed)
{
    closed=false;
    while(i < line.size())
    {
        const char c=line[i];

        if(c == '\\')
        {
            i+=2;
            continue;
        }
        if(c == '"')
        {
            addToken(tokens, from, i+1, TokenKind::String);
            closed=true;
            return i+1;
        }
        if(c == '$')
        {
            const size_t end=variableEnd(line, i);

            if(end > i+1)
            {
                addToken(tokens, from, i, TokenKind::String);
                addToken(tokens, i, end, TokenKind::Meta);
                from=end;
                i=end;
                continue;
            }
        }
        ++i;
    }
    addToken(tokens, from, line.size(), TokenKind::String);
    return line.size();
}

class BashTokenizer: public Tokenizer
{
public:
    TokenState tokenize(const String &line, const TokenState &start, std::vector<Token> &tokens) const override
    {
        TokenState state=start;
        size_t i=0;

        if((state.words[0] == Heredoc) || (state.words[0] == HeredocTabs))
        {
            size_t begin=0;

            if(state.words[0] == HeredocTabs)
            {
                while((begin < line.size()) && (line[begin] == '\t'))
                    ++begin;
            }
            if((line.size()-begin == state.words[2]) && (hashBytes(line, begin, line.size()) == state.words[1]))
            {
                addToken(tokens, begin, line.size(), TokenKind::Meta);
                return TokenState();
            }
            addToken(tokens, 0, line.size(), TokenKind::String);
            return state;
        }
        if(state.words[0] == SingleQuoted)
        {
            const size_t close=line.find('\'');

            if(close == String::npos)
            {
                addToken(tokens, 0, line.size(), TokenKind::String);
                return state;
            }
            addToken(tokens, 0, close+1, TokenKind::String);
            i=close+1;
            state.words[0]=Code;
        }
        else if(state.words[0] == DoubleQuoted)
        {
            bool closed=false;

            i=doubleQuoted(line, 0, 0, tokens, closed);
            if(!closed)
                return state;
            state.words[0]=Code;
        }

        // a heredoc opened on this line, its body from the next
        TokenState heredoc;

        while(i < line.size())
        {
            const char c=line[i];

            if(isSpace(c))
            {
                ++i;
                continue;
            }
            if((c == '#') && wordStart(line, i))
            {
                addToken(tokens, i, line.size(), TokenKind::Comment);
                break;
            }
            if(c == '\'')
            {
                const size_t close=line.find('\'', i+1);

                if(close == String::npos)
                {
                    addToken(tokens, i, line.size(), TokenKind::String);
                    state.words[0]=SingleQuoted;
                    break;
                }
                addToken(tokens, i, close+1, TokenKind::String);
                i=close+1;
                continue;
            }
            if(c == '"')
            {
                bool closed=false;

                i=doubleQuoted(line, i, i+1, tokens, closed);
                if(!closed)
                {
                    state.words[0]=DoubleQuoted;
                    break;
                }
                continue;
            }
            if(c == '\\')
            {
                i+=2;
                continue;
            }
            if(c == '$')
            {
                if((i+1 < line.size()) && (line[i+1] == '('))
                {
                    addToken(tokens, i, i+2, TokenKind::Punctuation);
                    i+=2;
                    continue;
                }

                const size_t end=variableEnd(line, i);

                if(end > i+1)
                    addToken(tokens, i, end, TokenKind::Meta);
                i=end;
                continue;
            }
            if(startsWithAt(line, i, "<<") && !startsWithAt(line, i, "<<<"))
            {
                size_t j=i+2;
                uint32_t mode=Heredoc;

                if((j < line.size()) && (line[j] == '-'))
                {
                    mode=HeredocTabs;
                    ++j;
                }
                addToken(tokens, i, j, TokenKind::Punctuation);
                j=skipSpaces(line, j);

                char quote=0;

                if((j < line.size()) && ((line[j] == '\'') || (line[j] == '"')))
                    quote=line[j++];

                const size_t wordBegin=j;

                while((j < line.size()) && (isWordChar(line[j]) || (line[j] == '-') || (line[j] == '.')))
                    ++j;

                const size_t wordEnd=j;

                if(quote && (j < line.size()) && (line[j] == quote))
                    ++j;
                if(wordEnd > wordBegin)
                {
                    addToken(tokens, quote?wordBegin-1:wordBegin, j, TokenKind::Meta);
                    heredoc.words[0]=mode;
                    heredoc.words[1]=hashBytes(line, wordBegin, wordEnd);
                    heredoc.words[2]=static_cast<uint32_t>(wordEnd-wordBegin);
                }
                i=j;
                continue;
            }
            if(startsWithAt(line, i, "[[") || startsWithAt(line, i, "]]") || startsWithAt(line, i, "&&") ||
                startsWithAt(line, i, "||") || startsWithAt(line, i, ";;"))
            {
                addToken(tokens, i, i+2, TokenKind::Punctuation);
                i+=2;
                continue;
            }
            if(inSet(c, "|&;<>(){}"))
            {
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                ++i;
                continue;
            }

            // a word: up to a space, a quote, a '$' or an operator
            size_t end=i;

            while((end < line.size()) && !isSpace(line[end]) && !inSet(line[end], "|&;<>(){}'\"$`\\"))
                ++end;
            if(end == i)
            {
                ++i;
                continue;
            }

            const bool atStart=wordStart(line, i);
            const size_t equals=line.find('=', i);

            if(atStart && (equals != String::npos) && (equals < end) && (equals > i) &&
                (isAlpha(line[i]) || (line[i] == '_')))
            {
                bool name=true;

                for(size_t k=i; k < equals; ++k)
                    name=name && (isAlpha(line[k]) || isDigit(line[k]) || (line[k] == '_'));
                if(name)
                {
                    addToken(tokens, i, equals, TokenKind::Key);
                    addToken(tokens, equals, equals+1, TokenKind::Punctuation);
                    i=equals+1;
                    continue;
                }
            }
            if(atStart && isOneOf(line, i, end, keywords))
                addToken(tokens, i, end, TokenKind::Keyword);
            else
            {
                bool number=true;

                for(size_t k=i; k < end; ++k)
                    number=number && isDigit(line[k]);
                if(number)
                    addToken(tokens, i, end, TokenKind::Number);
            }
            i=end;
        }
        if(heredoc.words[0] != Code)
            return heredoc;
        return state;
    }
};

}//namespace

std::unique_ptr<Tokenizer> makeBashTokenizer()
{
    return std::make_unique<BashTokenizer>();
}

}//namespace highlight
}//namespace text
}//namespace gweni
