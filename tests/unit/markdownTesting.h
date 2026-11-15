/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What the Markdown tests share: parsed blocks written as text, one block a
 *  line indented two spaces a level, its type, its attributes and its spans,
 *  each span as flags'text' (B bold, I italic, C code, S strike; ! and (src)
 *  for an image, <url> for a link), so the blocks a snippet should parse to
 *  can be written out by hand.
 */
#ifndef _gweni_tests_unit_markdownTesting_h_
#define _gweni_tests_unit_markdownTesting_h_

#include <gweni/text/markdown.h>

#include <string>
#include <vector>

namespace test
{

inline std::string describeSpan(const gweni::text::MarkdownSpan &span)
{
    using S=gweni::text::MarkdownSpan;
    std::string out;

    if(span.image)
        out+='!';
    if(span.style&S::Bold)
        out+='B';
    if(span.style&S::Italic)
        out+='I';
    if(span.style&S::Code)
        out+='C';
    if(span.style&S::Strike)
        out+='S';
    out+="'"+span.text+"'";
    if(span.image)
        out+="("+span.src+")";
    if(!span.url.empty())
        out+="<"+span.url+">";
    return out;
}

inline void describeBlock(const gweni::text::MarkdownBlock &block, int depth, std::string &out)
{
    using T=gweni::text::MarkdownBlock::Type;
    using A=gweni::text::MarkdownBlock::Align;
    static const char *alignNames[]={"-", "left", "center", "right"};

    out+=std::string(size_t(depth)*2, ' ');
    out+=gweni::text::markdownBlockTypeName(block.type);
    switch(block.type)
    {
    case T::Heading:
        out+=" "+std::to_string(block.level);
        break;
    case T::List:
        out+=block.ordered?" ordered "+std::to_string(block.start):std::string(" bullet");
        out+=std::string(" '")+block.marker+"'";
        out+=block.tight?" tight":" loose";
        break;
    case T::CodeBlock:
        out+=block.fenced?" fenced":" indented";
        if(!block.language.empty())
            out+=" "+block.language;
        break;
    case T::Table:
        for(A align:block.aligns)
            out+=std::string(" ")+alignNames[int(align)];
        break;
    case T::TableRow:
        if(block.header)
            out+=" header";
        break;
    case T::TableCell:
        if(block.align != A::Default)
            out+=std::string(" ")+alignNames[int(block.align)];
        break;
    default:
        break;
    }
    if((block.type == T::CodeBlock) || (block.type == T::Html))
        out+=" \""+block.text+"\"";
    for(const gweni::text::MarkdownSpan &span:block.spans)
        out+=" "+describeSpan(span);
    out+="\n";
    for(const gweni::text::MarkdownBlock &child:block.children)
        describeBlock(child, depth+1, out);
}

inline std::string describe(const std::vector<gweni::text::MarkdownBlock> &blocks)
{
    std::string out;

    for(const gweni::text::MarkdownBlock &block:blocks)
        describeBlock(block, 0, out);
    return out;
}

inline std::string describeMarkdown(const std::string &markdown)
{
    return describe(gweni::text::parseMarkdown(markdown));
}

}//namespace test

#endif//_gweni_tests_unit_markdownTesting_h_
