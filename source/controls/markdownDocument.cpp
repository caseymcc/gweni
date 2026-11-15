/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/markdownDocument.h>
#include <gweni/skin.h>

#include <algorithm>
#include <unordered_map>

namespace gweni
{
namespace controls
{

namespace
{

//! The document control sits in (null outside one)
MarkdownDocument *documentOf(Base *control)
{
    for(Base *parent=control; parent; parent=parent->getParent())
    {
        if(MarkdownDocument *document=dynamic_cast<MarkdownDocument *>(parent))
            return document;
    }
    return nullptr;
}

//! The indexes of a longest increasing run (not contiguous) of values
std::vector<bool> longestIncreasing(const std::vector<size_t> &values)
{
    const size_t none=size_t(-1);
    std::vector<size_t> tails;      // index of the smallest tail of each length
    std::vector<size_t> previous(values.size(), none);

    for(size_t i=0; i < values.size(); ++i)
    {
        auto at=std::lower_bound(tails.begin(), tails.end(), values[i],
            [&values](size_t index, size_t value) { return values[index] < value; });

        if(at != tails.begin())
            previous[i]=*(at-1);
        if(at == tails.end())
            tails.push_back(i);
        else
            *at=i;
    }

    std::vector<bool> in(values.size(), false);

    for(size_t i=tails.empty()?none:tails.back(); i != none; i=previous[i])
        in[i]=true;
    return in;
}

}//namespace

namespace internal
{

///////////////////////////////////////////////////////////////////////////
// MarkdownColumn

GWENI_CONTROL_CONSTRUCTOR(MarkdownColumn)
{
}

Base *MarkdownColumn::getBlockControl(size_t index) const
{
    return (index < m_blockControls.size())?m_blockControls[index]:nullptr;
}

void MarkdownColumn::setSpacing(int spacing)
{
    spacing=std::max(spacing, 0);
    if(spacing == m_spacing)
        return;

    m_spacing=spacing;
    invalidate();
}

Base *MarkdownColumn::buildBlock(const text::MarkdownBlock &block)
{
    using Type=text::MarkdownBlock::Type;

    switch(block.type)
    {
    case Type::Rule:
        return newChild<MarkdownRule>();
    case Type::Quote:
    {
        MarkdownQuote *quote=newChild<MarkdownQuote>();

        quote->setDocument(m_document);
        return quote;
    }
    case Type::List:
    {
        MarkdownList *list=newChild<MarkdownList>();

        list->setDocument(m_document);
        return list;
    }
    case Type::ListItem:
    {
        MarkdownListItem *item=newChild<MarkdownListItem>();

        item->setDocument(m_document);
        return item;
    }
    default:
        return newChild<MarkdownText>();
    }
}

void MarkdownColumn::setBlocks(const std::vector<text::MarkdownBlock> &blocks)
{
    using Kind=text::MarkdownBlockMatch::Kind;

    std::vector<text::MarkdownBlockMatch> matches=text::markdownDiff(m_blocks, blocks);
    std::vector<Base *> controls(blocks.size(), nullptr);
    std::vector<bool> kept(m_blockControls.size(), false);

    for(size_t i=0; i < blocks.size(); ++i)
    {
        const text::MarkdownBlockMatch &match=matches[i];

        if(match.kind == Kind::New)
        {
            Base *control=buildBlock(blocks[i]);

            control->setPlacement(Placement::Owner);
            if(MarkdownBlockControl *blockControl=dynamic_cast<MarkdownBlockControl *>(control))
                blockControl->setBlock(blocks[i]);
            controls[i]=control;
            continue;
        }

        Base *control=m_blockControls[match.old];

        kept[match.old]=true;
        controls[i]=control;
        if(match.kind == Kind::Changed)
        {
            if(MarkdownBlockControl *blockControl=dynamic_cast<MarkdownBlockControl *>(control))
                blockControl->setBlock(blocks[i]);
        }
    }

    // the controls of blocks that are gone: hidden now, so this frame lays
    // out and draws without them, and deleted between frames (one may be the
    // RichText whose link click called setMarkdown)
    for(size_t old=0; old < m_blockControls.size(); ++old)
    {
        if(kept[old])
            continue;
        m_blockControls[old]->setHidden(true);
        m_blockControls[old]->delayedDelete();
    }

    m_blocks=blocks;
    m_blockControls=std::move(controls);

    // the controls in the blocks' order among the children, moving only the
    // ones out of a longest run already in order
    if(m_blockControls.size() > 1)
    {
        std::unordered_map<const Base *, size_t> positions;
        size_t position=0;

        for(const Base *child:getActualChildren())
            positions[child]=position++;

        std::vector<size_t> order(m_blockControls.size());

        for(size_t i=0; i < m_blockControls.size(); ++i)
            order[i]=positions[m_blockControls[i]];

        std::vector<bool> inOrder=longestIncreasing(order);

        for(size_t i=0; i < m_blockControls.size(); ++i)
        {
            if(inOrder[i])
                continue;
            if(i > 0)
                m_blockControls[i]->bringNextToControl(m_blockControls[i-1], true);
            else
            {
                size_t first=1;

                while(!inOrder[first])
                    ++first;
                m_blockControls[0]->bringNextToControl(m_blockControls[first], false);
            }
        }
    }

    blocksChanged();
}

void MarkdownColumn::calculateSize(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        ParentClass::calculateSize(skin, dim);

        int width=0;

        for(const Base *block:m_blockControls)
        {
            if(!block->hidden())
                width=std::max(width, block->getPreferredSize().width);
        }
        m_preferredSize.width=preferredFromContent(width+padding.left+padding.right, Dim::X);
        return;
    }

    // a Y measure before this pass's X arrange: the blocks' widths are not
    // this pass's yet (as layout::Flow)
    if(inLayoutPass() && !laidOutThisPass(Dim::X))
        return;

    ParentClass::calculateSize(skin, dim);

    int height=0;
    bool first=true;

    for(const Base *block:m_blockControls)
    {
        if(block->hidden())
            continue;
        if(!first)
            height+=m_spacing;
        height+=block->getPreferredSize().height;
        first=false;
    }
    m_preferredSize.height=preferredFromContent(height+padding.top+padding.bottom, Dim::Y);
}

void MarkdownColumn::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        const int width=std::max(0, getWidth()-padding.left-padding.right);

        for(Base *block:m_blockControls)
        {
            if(!block->hidden())
                placeChild(block, padding.left, block->getY(), width, block->getHeight());
        }
    }
    else
    {
        int y=padding.top;

        for(Base *block:m_blockControls)
        {
            if(block->hidden())
                continue;

            const int height=block->getPreferredSize().height;

            placeChild(block, block->getX(), y, block->getWidth(), height);
            y+=height+m_spacing;
        }
    }

    ParentClass::arrange(skin, dim);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownText

GWENI_CONTROL_CONSTRUCTOR(MarkdownText)
{
    onLinkClicked.add(this, &MarkdownText::onLink);
}

std::vector<RichSpan> MarkdownText::richSpans(const std::vector<text::MarkdownSpan> &spans)
{
    std::vector<RichSpan> rich;

    rich.reserve(spans.size());
    for(const text::MarkdownSpan &span:spans)
    {
        RichSpan out;

        out.text=span.text;
        out.style=span.style;
        out.url=span.url;
        if(span.image)
            out.style|=RichSpan::Italic;
        rich.push_back(out);
    }
    return rich;
}

void MarkdownText::setBlock(const text::MarkdownBlock &block)
{
    using Type=text::MarkdownBlock::Type;

    MarkdownDocument *document=documentOf(this);

    if(document)
        setFont((block.type == Type::Heading)?document->headingFont(block.level):document->getFont());

    switch(block.type)
    {
    case Type::CodeBlock:
        setSpans({ RichSpan::plain(block.text, RichSpan::Code) });
        break;
    case Type::Html:
        setSpans({ RichSpan::plain(block.text) });
        break;
    case Type::Table:
    {
        // the cells' text until tables have their grid
        String textOfRows;

        for(const text::MarkdownBlock &row:block.children)
        {
            if(!textOfRows.empty())
                textOfRows+='\n';
            for(size_t cell=0; cell < row.children.size(); ++cell)
            {
                if(cell > 0)
                    textOfRows+=" | ";
                textOfRows+=row.children[cell].plainText();
            }
        }
        setSpans({ RichSpan::plain(textOfRows) });
        break;
    }
    default:
        setSpans(richSpans(block.spans));
        break;
    }
}

void MarkdownText::onLink(event::Info info)
{
    if(MarkdownDocument *document=documentOf(this))
        document->onLinkClicked.call(document, info);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownRule

GWENI_CONTROL_CONSTRUCTOR(MarkdownRule)
{
}

void MarkdownRule::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);
    if(dim == Dim::Y)
        m_preferredSize.height=preferredFromContent(RuleHeight, Dim::Y);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownQuote

GWENI_CONTROL_CONSTRUCTOR(MarkdownQuote)
{
    setPadding(Padding(BarWidth+BarGap, 0, 0, 0));
    setSpacing(MarkdownDocument::BlockSpacing);
}

void MarkdownQuote::setBlock(const text::MarkdownBlock &block)
{
    setBlocks(block.children);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownList

GWENI_CONTROL_CONSTRUCTOR(MarkdownList)
{
}

Base *MarkdownList::buildBlock(const text::MarkdownBlock &block)
{
    // a list's blocks are its items
    MarkdownListItem *item=newChild<MarkdownListItem>();

    item->setDocument(m_document);
    return item;
}

void MarkdownList::setBlock(const text::MarkdownBlock &block)
{
    m_depth=0;
    for(Base *parent=getParent(); parent; parent=parent->getParent())
    {
        if(dynamic_cast<MarkdownList *>(parent))
            ++m_depth;
    }
    m_ordered=block.ordered;
    m_start=block.start;
    m_marker=block.marker;
    m_tight=block.tight;
    setSpacing(m_tight?MarkdownDocument::TightSpacing:MarkdownDocument::BlockSpacing);
    setBlocks(block.children);
}

String MarkdownList::markerText(size_t index) const
{
    if(m_ordered)
        return std::to_string(m_start+index)+m_marker;

    static const char *bullets[]={ "\xE2\x80\xA2", "\xE2\x97\xA6", "\xE2\x96\xAA" };

    return bullets[std::min(m_depth, 2)];
}

void MarkdownList::blocksChanged()
{
    for(size_t i=0; i < m_blockControls.size(); ++i)
    {
        if(MarkdownListItem *item=dynamic_cast<MarkdownListItem *>(m_blockControls[i]))
        {
            item->setSpacing(getSpacing());
            item->setMarker(markerText(i));
        }
    }
}

///////////////////////////////////////////////////////////////////////////
// MarkdownListItem

GWENI_CONTROL_CONSTRUCTOR(MarkdownListItem)
{
    setPadding(Padding(MarkdownDocument::ListIndent, 0, 0, 0));
    m_marker=newChild<MarkdownText>();
    m_marker->setPlacement(Placement::Owner);
}

void MarkdownListItem::setBlock(const text::MarkdownBlock &block)
{
    setBlocks(block.children);
}

void MarkdownListItem::setMarker(const String &text)
{
    if(MarkdownDocument *document=documentOf(this))
        m_marker->setFont(document->getFont());
    m_marker->setSpans({ RichSpan::plain(text) });
}

void MarkdownListItem::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if((dim == Dim::Y) && !(inLayoutPass() && !laidOutThisPass(Dim::X)))
    {
        const Padding &padding=getPadding();

        m_preferredSize.height=std::max(m_preferredSize.height,
            preferredFromContent(m_marker->getPreferredSize().height+padding.top+padding.bottom, Dim::Y));
    }
}

void MarkdownListItem::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();
    const Size &preferred=m_marker->getPreferredSize();

    if(dim == Dim::X)
        placeChild(m_marker, std::max(0, padding.left-MarkerGap-preferred.width), m_marker->getY(),
            preferred.width, m_marker->getHeight());
    else
        placeChild(m_marker, m_marker->getX(), padding.top, m_marker->getWidth(), preferred.height);

    ParentClass::arrange(skin, dim);
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
// MarkdownDocument

GWENI_CONTROL_CONSTRUCTOR(MarkdownDocument)
{
    setDocument(this);
    setDock(DockPosition::Top);
    setSpacing(BlockSpacing);
}

void MarkdownDocument::setMarkdown(const String &markdown)
{
    if(markdown == m_markdown)
        return;

    m_markdown=markdown;
    setBlocks(text::parseMarkdown(markdown));
}

void MarkdownDocument::setFont(const Font &font)
{
    if(m_hasFont && (m_font == font))
        return;

    m_font=font;
    m_hasFont=true;

    // every text block again in the new font: its own spans, its font
    std::vector<Base *> pending(1, this);

    while(!pending.empty())
    {
        Base *control=pending.back();

        pending.pop_back();
        if(internal::MarkdownColumn *column=dynamic_cast<internal::MarkdownColumn *>(control))
        {
            for(size_t i=0; i < column->getBlockCount(); ++i)
            {
                Base *block=column->getBlockControl(i);

                if(internal::MarkdownText *text=dynamic_cast<internal::MarkdownText *>(block))
                    text->setBlock(column->getBlocks()[i]);
                else
                    pending.push_back(block);
            }
            if(internal::MarkdownListItem *item=dynamic_cast<internal::MarkdownListItem *>(column))
                item->getMarker()->setFont(m_font);
        }
    }
}

Font MarkdownDocument::getFont()
{
    if(m_hasFont)
        return m_font;
    if(skin::Base *skin=getSkin())
        return skin->getDefaultFont();
    return Font();
}

float MarkdownDocument::headingSize(int level)
{
    static const float sizes[]={ 24.0f, 20.0f, 17.0f, 15.0f, 13.0f, 12.0f };

    level=std::min(std::max(level, 1), 6);
    return sizes[level-1]*getFont().size/11.0f;
}

Font MarkdownDocument::headingFont(int level)
{
    Font font=getFont();

    font.size=headingSize(level);
    font.bold=true;
    return font;
}

}//namespace controls
}//namespace gweni
