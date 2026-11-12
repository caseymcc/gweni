/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/text/wrap.h>
#include <gweni/utility.h>

namespace gweni
{
namespace text
{

void splitWords(const gweni::String &s, int w, const MeasureWidth &measure, std::vector<gweni::String> &elems)
{
    gweni::String str;

    // A step is one codepoint, so a word broken up for width is never broken
    // inside a UTF-8 sequence.
    for(size_t i=0, next=0; i < s.length(); i=next)
    {
        next=utf8Next(s, i);

        if(s[i] == '\n')
        {
            if(!str.empty())
                elems.push_back(str);

            elems.push_back("\n");
            str.clear();
            continue;
        }

        if(s[i] == ' ')
        {
            str+=s[i];
            elems.push_back(str);
            str.clear();
            continue;
        }

        const size_t charLength=next-i;
        str.append(s, i, charLength);

        // With no width to place it in, a word is not broken up.
        if(w <= 0)
            continue;

        // if adding character makes the word bigger than the textbox size
        if(measure(str) > w)
        {
            if(str.size() == charLength)
            {
                // A single character already overflows the available width;
                // retrying it would never make progress.
                elems.push_back(str);
                str.clear();
                continue;
            }

            //split words
            str.resize(str.size()-charLength);
            elems.push_back(str);
            str.clear();
            next=i;
            continue;
        }
    }

    if(!str.empty())
        elems.push_back(str);
}

// A line ends at each '\n', which it holds but does not show (freetype-gl
// would break the run there, below the line's box), and before a word that
// would take it past w. A blank line is an empty line, and so is the one after
// a final '\n'. With w <= 0 the lines only break at '\n'.
void wrapLines(const gweni::String &s, int w, const MeasureWidth &measure, std::vector<WrappedLine> &lines)
{
    std::vector<gweni::String> words;
    splitWords(s, w, measure, words);

    gweni::String strLine;
    int lineLength=0;

    for(auto &&word : words)
    {
        if(word == "\n")
        {
            lines.push_back({strLine, lineLength+1});
            strLine.clear();
            lineLength=0;
            continue;
        }

        const int wordLength=static_cast<int>(word.length());

        // Only w counts (a wrapped Text's parent's inner width): the
        // Text's own width is what the last wrap made it, and comparing
        // with it kept lines that no longer fit once the parent got
        // narrower.
        if(w > 0 && !strLine.empty() && measure(strLine+word) > w)
        {
            lines.push_back({strLine, lineLength});
            // the new line starts with the word that did not fit
            strLine=word;
            lineLength=wordLength;
            continue;
        }

        strLine+=word;
        lineLength+=wordLength;
    }

    lines.push_back({strLine, lineLength});
}

}//namespace text
}//namespace gweni
