/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! JSON, with the comments of JSONC (// to the line's end, /* */ over lines):
//! a string followed by ':' is a Key, any other a String; numbers, the literals
//! true, false and null (Keyword) and the punctuation { } [ ] , :. A key's ':'
//! must be on its line. The one state is an open block comment (word 0 = 1).

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

class JsonTokenizer: public Tokenizer
{
public:
    TokenState tokenize(const String &line, const TokenState &start, std::vector<Token> &tokens) const override
    {
        TokenState state=start;
        size_t i=0;

        while(i < line.size())
        {
            if(state.words[0] == 1)
            {
                // a block comment's rest, from its first byte that is not a space
                const size_t begin=skipSpaces(line, i);
                const size_t close=line.find("*/", begin);
                const size_t end=(close == String::npos)?line.size():close+2;

                addToken(tokens, begin, end, TokenKind::Comment);
                if(close != String::npos)
                    state.words[0]=0;
                i=end;
                continue;
            }

            const char c=line[i];

            if(isSpace(c))
            {
                ++i;
                continue;
            }
            if(startsWithAt(line, i, "//"))
            {
                addToken(tokens, i, line.size(), TokenKind::Comment);
                break;
            }
            if(startsWithAt(line, i, "/*"))
            {
                const size_t close=line.find("*/", i+2);
                const size_t end=(close == String::npos)?line.size():close+2;

                addToken(tokens, i, end, TokenKind::Comment);
                if(close == String::npos)
                    state.words[0]=1;
                i=end;
                continue;
            }
            if(c == '"')
            {
                size_t end=i+1;

                while(end < line.size())
                {
                    if(line[end] == '\\')
                        end+=2;
                    else if(line[end++] == '"')
                        break;
                }
                end=std::min(end, line.size());

                const size_t next=skipSpaces(line, end);
                const bool key=(next < line.size()) && (line[next] == ':');

                addToken(tokens, i, end, key?TokenKind::Key:TokenKind::String);
                i=end;
                continue;
            }
            if((c == '{') || (c == '}') || (c == '[') || (c == ']') || (c == ',') || (c == ':'))
            {
                addToken(tokens, i, i+1, TokenKind::Punctuation);
                ++i;
                continue;
            }
            if(isDigit(c) || (c == '-'))
            {
                size_t end=i+1;

                while((end < line.size()) && (isDigit(line[end]) || (line[end] == '.') || (line[end] == 'e') ||
                    (line[end] == 'E') || (line[end] == '+') || (line[end] == '-')))
                    ++end;
                addToken(tokens, i, end, TokenKind::Number);
                i=end;
                continue;
            }
            if(isWordChar(c))
            {
                size_t end=i;
                static const char *const literals[]={ "true", "false", "null", nullptr };

                while((end < line.size()) && isWordChar(line[end]))
                    ++end;
                if(isOneOf(line, i, end, literals))
                    addToken(tokens, i, end, TokenKind::Keyword);
                i=end;
                continue;
            }
            ++i;
        }
        return state;
    }
};

}//namespace

std::unique_ptr<Tokenizer> makeJsonTokenizer()
{
    return std::make_unique<JsonTokenizer>();
}

}//namespace highlight
}//namespace text
}//namespace gweni
