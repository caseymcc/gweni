/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! A unified diff. Outside a hunk the file headers ("diff ", "index ", "--- ",
//! "+++ ", "new file mode", "deleted file mode", "similarity index", "rename ",
//! "old mode", "new mode", "Binary files") are Meta and anything else (a commit
//! message) Plain. A hunk header "@@ -a,b +c,d @@" is Hunk up to its second
//! "@@" (the function context after it Plain) and opens a hunk of b old and d
//! new lines: in it '-' lines are Removed and '+' lines Added, whatever follows
//! (so a removed "-- x" is no header), ' ' and empty lines context, and
//! "\ No newline at end of file" a Comment (after a hunk too); the hunk ends
//! when both counts are used up. The state is in a hunk (word 0 = 1) and the
//! old and new lines left (words 1 and 2).

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

//! Reads the count of a "-a,b" or "+c,d" range at \p i (1 without ",b"), moving \p i past it
bool readRange(const String &line, size_t &i, char sign, uint32_t &count)
{
    if((i >= line.size()) || (line[i] != sign))
        return false;
    ++i;
    if((i >= line.size()) || !isDigit(line[i]))
        return false;
    while((i < line.size()) && isDigit(line[i]))
        ++i;
    count=1;
    if((i < line.size()) && (line[i] == ','))
    {
        ++i;
        count=0;
        if((i >= line.size()) || !isDigit(line[i]))
            return false;
        while((i < line.size()) && isDigit(line[i]))
        {
            count=count*10+static_cast<uint32_t>(line[i]-'0');
            ++i;
        }
    }
    return true;
}

class DiffTokenizer: public Tokenizer
{
public:
    TokenState tokenize(const String &line, const TokenState &start, std::vector<Token> &tokens) const override
    {
        TokenState state=start;

        if(state.words[0] == 1)
        {
            uint32_t &oldLeft=state.words[1];
            uint32_t &newLeft=state.words[2];
            const char c=line.empty()?' ':line[0];

            if(c == '\\')
                addToken(tokens, 0, line.size(), TokenKind::Comment);
            else if((c == '-') && (oldLeft > 0))
            {
                addToken(tokens, 0, line.size(), TokenKind::Removed);
                --oldLeft;
            }
            else if((c == '+') && (newLeft > 0))
            {
                addToken(tokens, 0, line.size(), TokenKind::Added);
                --newLeft;
            }
            else if((c == ' ') && (oldLeft > 0) && (newLeft > 0))
            {
                --oldLeft;
                --newLeft;
            }
            else
            {
                // a line the counts did not expect: the hunk is over, and this
                // line is read as one outside it
                state=TokenState();
                return tokenize(line, state, tokens);
            }
            if((oldLeft == 0) && (newLeft == 0))
                state=TokenState();
            return state;
        }

        if(startsWithAt(line, 0, "@@ "))
        {
            size_t i=3;
            uint32_t oldCount=0, newCount=0;
            const size_t close=line.find(" @@", 3);
            const size_t end=(close == String::npos)?line.size():close+3;

            addToken(tokens, 0, end, TokenKind::Hunk);
            if(readRange(line, i, '-', oldCount) && (i < line.size()) && (line[i] == ' ') &&
                readRange(line, ++i, '+', newCount) && ((oldCount > 0) || (newCount > 0)))
            {
                state.words[0]=1;
                state.words[1]=oldCount;
                state.words[2]=newCount;
            }
            return state;
        }

        // a "\ No newline at end of file" after a hunk's last line
        if(startsWithAt(line, 0, "\\ "))
        {
            addToken(tokens, 0, line.size(), TokenKind::Comment);
            return state;
        }

        static const char *const headers[]=
        {
            "diff ", "index ", "--- ", "+++ ", "new file mode", "deleted file mode", "similarity index",
            "dissimilarity index", "rename ", "copy ", "old mode", "new mode", "Binary files", nullptr
        };

        for(const char *const *header=headers; *header; ++header)
        {
            if(startsWithAt(line, 0, *header))
            {
                addToken(tokens, 0, line.size(), TokenKind::Meta);
                break;
            }
        }
        return state;
    }
};

}//namespace

std::unique_ptr<Tokenizer> makeDiffTokenizer()
{
    return std::make_unique<DiffTokenizer>();
}

}//namespace highlight
}//namespace text
}//namespace gweni
