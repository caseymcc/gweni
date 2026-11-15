/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/markdown.h>

#include <md4c.h>

#include <algorithm>
#include <cstring>
#include <utility>

namespace gweni
{
namespace text
{

namespace
{

void appendUtf8(gweni::String &out, uint32_t cp)
{
    if((cp == 0) || (cp > 0x10FFFF) || ((cp >= 0xD800) && (cp <= 0xDFFF)))
        cp=0xFFFD;

    if(cp < 0x80)
        out+=char(cp);
    else if(cp < 0x800)
    {
        out+=char(0xC0|(cp>>6));
        out+=char(0x80|(cp&0x3F));
    }
    else if(cp < 0x10000)
    {
        out+=char(0xE0|(cp>>12));
        out+=char(0x80|((cp>>6)&0x3F));
        out+=char(0x80|(cp&0x3F));
    }
    else
    {
        out+=char(0xF0|(cp>>18));
        out+=char(0x80|((cp>>12)&0x3F));
        out+=char(0x80|((cp>>6)&0x3F));
        out+=char(0x80|(cp&0x3F));
    }
}

struct NamedEntity
{
    const char *name;
    uint32_t codepoint;
};

const NamedEntity namedEntities[]=
{
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
    {"nbsp", 0xA0}, {"copy", 0xA9}, {"reg", 0xAE}, {"trade", 0x2122},
    {"hellip", 0x2026}, {"mdash", 0x2014}, {"ndash", 0x2013},
    {"lsquo", 0x2018}, {"rsquo", 0x2019}, {"ldquo", 0x201C}, {"rdquo", 0x201D},
    {"bull", 0x2022}, {"middot", 0xB7}, {"times", 0xD7}, {"divide", 0xF7},
    {"deg", 0xB0}, {"plusmn", 0xB1}, {"para", 0xB6}, {"sect", 0xA7},
    {"laquo", 0xAB}, {"raquo", 0xBB}, {"larr", 0x2190}, {"rarr", 0x2192},
    {"uarr", 0x2191}, {"darr", 0x2193}, {"harr", 0x2194}, {"euro", 0x20AC},
    {"pound", 0xA3}, {"yen", 0xA5}, {"cent", 0xA2}
};

//! An entity md4c reports ("&amp;", "&#123;", "&#x1F;") as UTF-8, or as
//! written for a name not in the table
void appendEntity(gweni::String &out, const char *text, size_t size)
{
    if((size >= 4) && (text[1] == '#'))
    {
        bool hex=((text[2] == 'x') || (text[2] == 'X'));
        uint32_t cp=0;

        for(size_t i=hex?3:2; i+1 < size; ++i)
        {
            char c=text[i];
            uint32_t digit;

            if((c >= '0') && (c <= '9'))
                digit=uint32_t(c-'0');
            else if(hex && (c >= 'a') && (c <= 'f'))
                digit=uint32_t(c-'a'+10);
            else if(hex && (c >= 'A') && (c <= 'F'))
                digit=uint32_t(c-'A'+10);
            else
                break;
            cp=cp*(hex?16:10)+digit;
            if(cp > 0x10FFFF)
                cp=0x110000;    // stays invalid however many digits follow
        }
        appendUtf8(out, cp);
        return;
    }

    if(size >= 3)
    {
        size_t length=size-2;

        for(const NamedEntity &entity:namedEntities)
        {
            if((std::strlen(entity.name) == length) && (std::memcmp(entity.name, text+1, length) == 0))
            {
                appendUtf8(out, entity.codepoint);
                return;
            }
        }
    }
    out.append(text, size);
}

gweni::String attributeText(const MD_ATTRIBUTE &attribute)
{
    gweni::String out;

    if(attribute.text == nullptr)
        return out;

    for(size_t i=0; attribute.substr_offsets[i] < attribute.size; ++i)
    {
        const char *text=attribute.text+attribute.substr_offsets[i];
        size_t size=attribute.substr_offsets[i+1]-attribute.substr_offsets[i];

        switch(attribute.substr_types[i])
        {
        case MD_TEXT_ENTITY:
            appendEntity(out, text, size);
            break;
        case MD_TEXT_NULLCHAR:
            appendUtf8(out, 0xFFFD);
            break;
        default:
            out.append(text, size);
            break;
        }
    }
    return out;
}

MarkdownBlock::Align alignOf(MD_ALIGN align)
{
    switch(align)
    {
    case MD_ALIGN_LEFT:
        return MarkdownBlock::Align::Left;
    case MD_ALIGN_CENTER:
        return MarkdownBlock::Align::Center;
    case MD_ALIGN_RIGHT:
        return MarkdownBlock::Align::Right;
    default:
        return MarkdownBlock::Align::Default;
    }
}

bool isContainer(MarkdownBlock::Type type)
{
    switch(type)
    {
    case MarkdownBlock::Type::List:
    case MarkdownBlock::Type::ListItem:
    case MarkdownBlock::Type::Quote:
    case MarkdownBlock::Type::Table:
    case MarkdownBlock::Type::TableRow:
        return true;
    default:
        return false;
    }
}

//! md4c's callbacks building the blocks
struct Builder
{
    std::vector<MarkdownBlock> root;
    //! The open blocks, outermost first; each is the last child of the one
    //! before it (or of root), whose children do not grow while it is open
    std::vector<MarkdownBlock *> open;
    //! The paragraph a tight list item's text opened, closed by the next
    //! block in the item or the item's end
    bool implicitParagraph=false;
    bool inTableHead=false;

    int strong=0;
    int emphasis=0;
    int code=0;
    int strike=0;
    std::vector<gweni::String> links;
    //! Images entered: the outermost one's span takes every text inside it
    int images=0;

    MarkdownBlock &push(MarkdownBlock::Type type)
    {
        std::vector<MarkdownBlock> &siblings=open.empty()?root:open.back()->children;

        siblings.emplace_back();
        siblings.back().type=type;
        open.push_back(&siblings.back());
        return siblings.back();
    }

    void closeImplicitParagraph()
    {
        if(implicitParagraph)
        {
            open.pop_back();
            implicitParagraph=false;
        }
    }

    //! The block text goes into, opening a tight list item's paragraph
    MarkdownBlock *leaf()
    {
        if(open.empty())
            return nullptr;
        if(isContainer(open.back()->type))
        {
            if(open.back()->type != MarkdownBlock::Type::ListItem)
                return nullptr;
            push(MarkdownBlock::Type::Paragraph);
            implicitParagraph=true;
        }
        return open.back();
    }

    unsigned style() const
    {
        unsigned bits=MarkdownSpan::Plain;

        if(strong > 0)
            bits|=MarkdownSpan::Bold;
        if(emphasis > 0)
            bits|=MarkdownSpan::Italic;
        if(code > 0)
            bits|=MarkdownSpan::Code;
        if(strike > 0)
            bits|=MarkdownSpan::Strike;
        return bits;
    }

    //! The span text in the current style goes on
    gweni::String *spanText()
    {
        MarkdownBlock *block=leaf();

        if(block == nullptr)
            return nullptr;
        if(images > 0)
            return &block->spans.back().text;

        unsigned bits=style();
        const gweni::String &url=links.empty()?gweni::String():links.back();
        std::vector<MarkdownSpan> &spans=block->spans;

        if(spans.empty() || spans.back().image || (spans.back().style != bits) || (spans.back().url != url))
        {
            spans.emplace_back();
            spans.back().style=bits;
            spans.back().url=url;
        }
        return &spans.back().text;
    }

    int enterBlock(MD_BLOCKTYPE type, void *detail)
    {
        if((type != MD_BLOCK_DOC) && (type != MD_BLOCK_THEAD) && (type != MD_BLOCK_TBODY))
            closeImplicitParagraph();

        switch(type)
        {
        case MD_BLOCK_DOC:
            break;
        case MD_BLOCK_QUOTE:
            push(MarkdownBlock::Type::Quote);
            break;
        case MD_BLOCK_UL:
        {
            const MD_BLOCK_UL_DETAIL *ul=static_cast<const MD_BLOCK_UL_DETAIL *>(detail);
            MarkdownBlock &block=push(MarkdownBlock::Type::List);

            block.marker=ul->mark;
            block.tight=(ul->is_tight != 0);
            break;
        }
        case MD_BLOCK_OL:
        {
            const MD_BLOCK_OL_DETAIL *ol=static_cast<const MD_BLOCK_OL_DETAIL *>(detail);
            MarkdownBlock &block=push(MarkdownBlock::Type::List);

            block.ordered=true;
            block.start=ol->start;
            block.marker=ol->mark_delimiter;
            block.tight=(ol->is_tight != 0);
            break;
        }
        case MD_BLOCK_LI:
            push(MarkdownBlock::Type::ListItem);
            break;
        case MD_BLOCK_HR:
            push(MarkdownBlock::Type::Rule);
            break;
        case MD_BLOCK_H:
            push(MarkdownBlock::Type::Heading).level=int(static_cast<const MD_BLOCK_H_DETAIL *>(detail)->level);
            break;
        case MD_BLOCK_CODE:
        {
            const MD_BLOCK_CODE_DETAIL *codeDetail=static_cast<const MD_BLOCK_CODE_DETAIL *>(detail);
            MarkdownBlock &block=push(MarkdownBlock::Type::CodeBlock);

            block.fenced=(codeDetail->fence_char != 0);
            block.language=attributeText(codeDetail->lang);
            break;
        }
        case MD_BLOCK_HTML:
            push(MarkdownBlock::Type::Html);
            break;
        case MD_BLOCK_P:
            push(MarkdownBlock::Type::Paragraph);
            break;
        case MD_BLOCK_TABLE:
            push(MarkdownBlock::Type::Table);
            break;
        case MD_BLOCK_THEAD:
            inTableHead=true;
            break;
        case MD_BLOCK_TBODY:
            inTableHead=false;
            break;
        case MD_BLOCK_TR:
            push(MarkdownBlock::Type::TableRow).header=inTableHead;
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
        {
            MarkdownBlock::Align align=alignOf(static_cast<const MD_BLOCK_TD_DETAIL *>(detail)->align);

            push(MarkdownBlock::Type::TableCell).align=align;
            if(type == MD_BLOCK_TH)
                open[open.size()-3]->aligns.push_back(align);
            break;
        }
        default:
            // not enabled (footnotes, admonitions, blank lines): pushes
            // nothing, and its leave pops nothing
            break;
        }
        return 0;
    }

    int leaveBlock(MD_BLOCKTYPE type)
    {
        switch(type)
        {
        case MD_BLOCK_DOC:
            break;
        case MD_BLOCK_THEAD:
        case MD_BLOCK_TBODY:
            inTableHead=false;
            break;
        case MD_BLOCK_CODE:
        case MD_BLOCK_HTML:
        {
            gweni::String &text=open.back()->text;

            if(!text.empty() && (text.back() == '\n'))
                text.pop_back();
            open.pop_back();
            break;
        }
        case MD_BLOCK_QUOTE:
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
        case MD_BLOCK_LI:
        case MD_BLOCK_HR:
        case MD_BLOCK_H:
        case MD_BLOCK_P:
        case MD_BLOCK_TABLE:
        case MD_BLOCK_TR:
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
            closeImplicitParagraph();
            open.pop_back();
            break;
        default:
            break;
        }
        return 0;
    }

    int enterSpan(MD_SPANTYPE type, void *detail)
    {
        if(images > 0)
        {
            // an image's alt text is flat: a nested image adds its own alt
            if(type == MD_SPAN_IMG)
                ++images;
            return 0;
        }

        switch(type)
        {
        case MD_SPAN_EM:
            ++emphasis;
            break;
        case MD_SPAN_STRONG:
            ++strong;
            break;
        case MD_SPAN_CODE:
            ++code;
            break;
        case MD_SPAN_DEL:
            ++strike;
            break;
        case MD_SPAN_A:
        {
            const MD_SPAN_A_DETAIL *a=static_cast<const MD_SPAN_A_DETAIL *>(detail);
            gweni::String url=attributeText(a->href);

            if(a->is_autolink && (url.find(':') == gweni::String::npos))
            {
                if(url.find('@') != gweni::String::npos)
                    url="mailto:"+url;
                else
                    url="http://"+url;
            }
            links.push_back(url);
            break;
        }
        case MD_SPAN_IMG:
        {
            MarkdownBlock *block=leaf();

            if(block == nullptr)
                break;

            MarkdownSpan span;

            span.style=style();
            span.url=links.empty()?gweni::String():links.back();
            span.image=true;
            span.src=attributeText(static_cast<const MD_SPAN_IMG_DETAIL *>(detail)->src);
            block->spans.push_back(span);
            images=1;
            break;
        }
        default:
            break;
        }
        return 0;
    }

    int leaveSpan(MD_SPANTYPE type)
    {
        if(images > 0)
        {
            if(type == MD_SPAN_IMG)
                --images;
            return 0;
        }

        switch(type)
        {
        case MD_SPAN_EM:
            --emphasis;
            break;
        case MD_SPAN_STRONG:
            --strong;
            break;
        case MD_SPAN_CODE:
            --code;
            break;
        case MD_SPAN_DEL:
            --strike;
            break;
        case MD_SPAN_A:
            links.pop_back();
            break;
        default:
            break;
        }
        return 0;
    }

    int text(MD_TEXTTYPE type, const char *text, size_t size)
    {
        if(!open.empty())
        {
            MarkdownBlock::Type blockType=open.back()->type;

            if((blockType == MarkdownBlock::Type::CodeBlock) || (blockType == MarkdownBlock::Type::Html))
            {
                open.back()->text.append(text, size);
                return 0;
            }
        }

        gweni::String *out=spanText();

        if(out == nullptr)
            return 0;

        switch(type)
        {
        case MD_TEXT_NULLCHAR:
            appendUtf8(*out, 0xFFFD);
            break;
        case MD_TEXT_BR:
            *out+='\n';
            break;
        case MD_TEXT_SOFTBR:
            *out+=' ';
            break;
        case MD_TEXT_ENTITY:
            appendEntity(*out, text, size);
            break;
        default:
            out->append(text, size);
            break;
        }
        return 0;
    }
};

int enterBlockCallback(MD_BLOCKTYPE type, void *detail, void *user)
{
    return static_cast<Builder *>(user)->enterBlock(type, detail);
}

int leaveBlockCallback(MD_BLOCKTYPE type, void *, void *user)
{
    return static_cast<Builder *>(user)->leaveBlock(type);
}

int enterSpanCallback(MD_SPANTYPE type, void *detail, void *user)
{
    return static_cast<Builder *>(user)->enterSpan(type, detail);
}

int leaveSpanCallback(MD_SPANTYPE type, void *, void *user)
{
    return static_cast<Builder *>(user)->leaveSpan(type);
}

int textCallback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *user)
{
    return static_cast<Builder *>(user)->text(type, text, size);
}

struct Hasher
{
    uint64_t value=0xcbf29ce484222325ull;

    void bytes(const void *data, size_t size)
    {
        const unsigned char *p=static_cast<const unsigned char *>(data);

        for(size_t i=0; i < size; ++i)
        {
            value^=p[i];
            value*=0x100000001b3ull;
        }
    }
    void number(uint64_t n)
    {
        unsigned char b[8];

        for(int i=0; i < 8; ++i)
            b[i]=static_cast<unsigned char>(n>>(8*i));
        bytes(b, 8);
    }
    void string(const gweni::String &s)
    {
        number(s.size());
        bytes(s.data(), s.size());
    }
};

}//namespace

gweni::String MarkdownBlock::plainText() const
{
    gweni::String out;

    for(const MarkdownSpan &span:spans)
        out+=span.text;
    return out;
}

void MarkdownBlock::updateHash()
{
    Hasher h;

    h.number(uint64_t(type));
    h.number(uint64_t(level));
    h.number(ordered?1:0);
    h.number(start);
    h.number(uint64_t(static_cast<unsigned char>(marker)));
    h.number(tight?1:0);
    h.string(language);
    h.number(fenced?1:0);
    h.number(header?1:0);
    h.number(uint64_t(align));
    h.number(aligns.size());
    for(Align columnAlign:aligns)
        h.number(uint64_t(columnAlign));
    h.string(text);
    h.number(spans.size());
    for(const MarkdownSpan &span:spans)
    {
        h.string(span.text);
        h.number(span.style);
        h.string(span.url);
        h.number(span.image?1:0);
        h.string(span.src);
    }
    h.number(children.size());
    for(MarkdownBlock &child:children)
    {
        child.updateHash();
        h.number(child.hash);
    }
    hash=h.value;
}

bool MarkdownBlock::operator==(const MarkdownBlock &other) const
{
    return (type == other.type) && (spans == other.spans) && (text == other.text)
        && (children == other.children) && (level == other.level) && (ordered == other.ordered)
        && (start == other.start) && (marker == other.marker) && (tight == other.tight)
        && (language == other.language) && (fenced == other.fenced) && (header == other.header)
        && (align == other.align) && (aligns == other.aligns) && (hash == other.hash);
}

std::vector<MarkdownBlock> parseMarkdown(const gweni::String &markdown)
{
    Builder builder;
    MD_PARSER parser;

    std::memset(&parser, 0, sizeof(parser));
    parser.flags=MD_FLAG_TABLES|MD_FLAG_STRIKETHROUGH|MD_FLAG_PERMISSIVEAUTOLINKS;
    parser.enter_block=enterBlockCallback;
    parser.leave_block=leaveBlockCallback;
    parser.enter_span=enterSpanCallback;
    parser.leave_span=leaveSpanCallback;
    parser.text=textCallback;

    md_parse(markdown.data(), MD_SIZE(markdown.size()), &parser, &builder);

    for(MarkdownBlock &block:builder.root)
        block.updateHash();
    return std::move(builder.root);
}

std::vector<MarkdownBlockMatch> markdownDiff(const std::vector<MarkdownBlock> &old,
    const std::vector<MarkdownBlock> &now)
{
    using Kind=MarkdownBlockMatch::Kind;
    const size_t none=size_t(-1);
    size_t oldCount=old.size();
    size_t newCount=now.size();
    std::vector<MarkdownBlockMatch> matches(newCount);
    //! the old block each new one keeps, none for one not matched yet
    std::vector<size_t> oldOf(newCount, none);
    std::vector<bool> oldTaken(oldCount, false);
    //! the in-order matches (prefix, subsequence, suffix), the gaps' bounds
    std::vector<std::pair<size_t, size_t>> anchors;

    auto keep=[&](size_t n, size_t o)
    {
        oldOf[n]=o;
        oldTaken[o]=true;
        matches[n].kind=Kind::Kept;
        matches[n].old=o;
    };

    size_t prefix=0;

    while((prefix < oldCount) && (prefix < newCount) && (old[prefix].hash == now[prefix].hash))
    {
        keep(prefix, prefix);
        anchors.emplace_back(prefix, prefix);
        ++prefix;
    }

    size_t suffix=0;

    while((suffix < oldCount-prefix) && (suffix < newCount-prefix)
        && (old[oldCount-1-suffix].hash == now[newCount-1-suffix].hash))
        ++suffix;

    // the middle: old [prefix, oldEnd), new [prefix, newEnd)
    size_t oldEnd=oldCount-suffix;
    size_t newEnd=newCount-suffix;
    size_t a=oldEnd-prefix;
    size_t b=newEnd-prefix;

    if((a > 0) && (b > 0) && (a <= MarkdownDiffLimit/b))
    {
        // lengths[i*(b+1)+j]: the longest common subsequence of old's middle
        // from i and new's from j
        std::vector<uint32_t> lengths((a+1)*(b+1), 0);

        for(size_t i=a; i-- > 0;)
        {
            for(size_t j=b; j-- > 0;)
            {
                uint32_t &length=lengths[i*(b+1)+j];

                if(old[prefix+i].hash == now[prefix+j].hash)
                    length=lengths[(i+1)*(b+1)+j+1]+1;
                else
                    length=std::max(lengths[(i+1)*(b+1)+j], lengths[i*(b+1)+j+1]);
            }
        }

        size_t i=0;
        size_t j=0;

        while((i < a) && (j < b))
        {
            if(old[prefix+i].hash == now[prefix+j].hash)
            {
                keep(prefix+j, prefix+i);
                anchors.emplace_back(prefix+j, prefix+i);
                ++i;
                ++j;
            }
            else if(lengths[(i+1)*(b+1)+j] >= lengths[i*(b+1)+j+1])
                ++i;
            else
                ++j;
        }
    }

    for(size_t k=0; k < suffix; ++k)
    {
        keep(newEnd+k, oldEnd+k);
        anchors.emplace_back(newEnd+k, oldEnd+k);
    }

    // a block moved: the first old block left with its hash
    for(size_t n=prefix; n < newEnd; ++n)
    {
        if(oldOf[n] != none)
            continue;
        for(size_t o=prefix; o < oldEnd; ++o)
        {
            if(!oldTaken[o] && (old[o].hash == now[n].hash))
            {
                keep(n, o);
                break;
            }
        }
    }

    // the rest, paired in order within each gap between two anchors
    size_t previousNew=0;
    size_t previousOld=0;

    for(size_t anchor=0; anchor <= anchors.size(); ++anchor)
    {
        size_t nextNew=(anchor < anchors.size())?anchors[anchor].first:newCount;
        size_t nextOld=(anchor < anchors.size())?anchors[anchor].second:oldCount;
        size_t o=previousOld;

        for(size_t n=previousNew; n < nextNew; ++n)
        {
            if(oldOf[n] != none)
                continue;
            while((o < nextOld) && oldTaken[o])
                ++o;
            if(o >= nextOld)
                break;
            if(old[o].type == now[n].type)
            {
                oldOf[n]=o;
                oldTaken[o]=true;
                matches[n].kind=Kind::Changed;
                matches[n].old=o;
                if(!old[o].children.empty() || !now[n].children.empty())
                    matches[n].children=markdownDiff(old[o].children, now[n].children);
            }
            ++o;
        }
        if(anchor < anchors.size())
        {
            previousNew=anchors[anchor].first+1;
            previousOld=anchors[anchor].second+1;
        }
    }
    return matches;
}

const char *markdownBlockTypeName(MarkdownBlock::Type type)
{
    switch(type)
    {
    case MarkdownBlock::Type::Paragraph: return "Paragraph";
    case MarkdownBlock::Type::Heading: return "Heading";
    case MarkdownBlock::Type::List: return "List";
    case MarkdownBlock::Type::ListItem: return "ListItem";
    case MarkdownBlock::Type::Quote: return "Quote";
    case MarkdownBlock::Type::Rule: return "Rule";
    case MarkdownBlock::Type::CodeBlock: return "CodeBlock";
    case MarkdownBlock::Type::Html: return "Html";
    case MarkdownBlock::Type::Table: return "Table";
    case MarkdownBlock::Type::TableRow: return "TableRow";
    case MarkdownBlock::Type::TableCell: return "TableCell";
    }
    return "?";
}

}//namespace text
}//namespace gweni
