/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/markdownDocument.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
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

void MarkdownColumn::setSpacing(Spacing spacing)
{
    if(spacing == m_spacing)
        return;

    m_spacing=spacing;
    invalidate();
}

int MarkdownColumn::getSpacing()
{
    skin::Base *skin=getSkin();

    if(!skin)
        return 0;
    return (m_spacing == Spacing::Tight)?skin->Metrics.MarkdownTightSpacing:skin->Metrics.MarkdownBlockSpacing;
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
    case Type::CodeBlock:
        return newChild<MarkdownCodeBlock>();
    case Type::Table:
        return newChild<MarkdownTable>();
    case Type::Paragraph:
        if(MarkdownImage::isImageParagraph(block))
            return newChild<MarkdownImage>();
        return newChild<MarkdownText>();
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

        // a block its control cannot show (a paragraph turned image or back)
        // is built anew, the old control going as a removed block's
        bool build=(match.kind == Kind::New);

        if(!build)
        {
            const MarkdownBlockControl *old=dynamic_cast<const MarkdownBlockControl *>(m_blockControls[match.old]);

            build=old && !old->shows(blocks[i]);
        }
        if(build)
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

    const int spacing=getSpacing();
    int height=0;
    bool first=true;

    for(const Base *block:m_blockControls)
    {
        if(block->hidden())
            continue;
        if(!first)
            height+=spacing;
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
        const int spacing=getSpacing();
        int y=padding.top;

        for(Base *block:m_blockControls)
        {
            if(block->hidden())
                continue;

            const int height=block->getPreferredSize().height;

            placeChild(block, block->getX(), y, block->getWidth(), height);
            y+=height+spacing;
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
        {
            out.style|=RichSpan::Italic|RichSpan::Placeholder;
            if(out.text.empty())
                out.text="image";
        }
        rich.push_back(out);
    }
    return rich;
}

bool MarkdownText::shows(const text::MarkdownBlock &block) const
{
    return !MarkdownImage::isImageParagraph(block);
}

void MarkdownText::setBlock(const text::MarkdownBlock &block)
{
    using Type=text::MarkdownBlock::Type;

    m_fromBlock=true;
    m_headingLevel=(block.type == Type::Heading)?block.level:0;
    applyFont();

    switch(block.type)
    {
    case Type::Html:
        setSpans({ RichSpan::plain(block.text) });
        break;
    default:
        setSpans(richSpans(block.spans));
        break;
    }
}

void MarkdownText::applyFont()
{
    MarkdownDocument *document=documentOf(this);

    if(!document)
        return;

    Font font=(m_headingLevel > 0)?document->headingFont(m_headingLevel):document->getFont();

    if(m_header)
        font.bold=true;
    setFont(font);
}

void MarkdownText::updateColors()
{
    ParentClass::updateColors();
    // a code block's text and a marker take their fonts from their owners
    if(m_fromBlock)
        applyFont();
}

void MarkdownText::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    MarkdownDocument *document=documentOf(this);

    // a drag from this text: into whichever text is under the mouse
    if(document && m_selecting && (MouseFocus == this))
    {
        setHoveredLink(linkAt(canvasPosToLocal(Point(x, y))));
        document->selectionModel().extendTo(document->positionAt(Point(x, y)));
        document->selectionChanged();
        return;
    }
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);
}

void MarkdownText::onMouseClickLeft(int x, int y, bool down)
{
    ParentClass::onMouseClickLeft(x, y, down);
    if(MarkdownDocument *document=documentOf(this))
        document->selectionChanged();
}

void MarkdownText::onMouseDoubleClickLeft(int x, int y)
{
    ParentClass::onMouseDoubleClickLeft(x, y);
    if(MarkdownDocument *document=documentOf(this))
        document->selectionChanged();
}

bool MarkdownText::onKeyEscape(bool down)
{
    MarkdownDocument *document=documentOf(this);

    if(!document)
        return ParentClass::onKeyEscape(down);
    if(!document->hasSelection())
        return false;
    if(down)
        document->clearSelection();
    return true;
}

void MarkdownText::selectAll()
{
    if(MarkdownDocument *document=documentOf(this))
        document->selectAll();
    else
        ParentClass::selectAll();
}

void MarkdownText::clearSelection()
{
    if(MarkdownDocument *document=documentOf(this))
        document->clearSelection();
    else
        ParentClass::clearSelection();
}

bool MarkdownText::copySelection()
{
    if(MarkdownDocument *document=documentOf(this))
        return document->copySelection();
    return ParentClass::copySelection();
}

void MarkdownText::onLink(event::Info info)
{
    if(MarkdownDocument *document=documentOf(this))
    {
        info.control=document;
        document->onLinkClicked.call(document, info);
    }
}

///////////////////////////////////////////////////////////////////////////
// MarkdownImage

GWENI_CONTROL_CONSTRUCTOR(MarkdownImage)
{
    m_alt=newChild<MarkdownText>();
    m_alt->setPlacement(Placement::Owner);
}

bool MarkdownImage::isImageParagraph(const text::MarkdownBlock &block)
{
    if(block.type != text::MarkdownBlock::Type::Paragraph)
        return false;

    int images=0;

    for(const text::MarkdownSpan &span:block.spans)
    {
        if(span.image)
        {
            ++images;
            continue;
        }
        if(!span.url.empty() || (span.text.find_first_not_of(" \t\n") != String::npos))
            return false;
    }
    return images == 1;
}

Size MarkdownImage::boxSize(int width, int height, int available)
{
    if((width < 1) || (height < 1))
        return Size(0, 0);
    if((available < 1) || (width <= available))
        return Size(width, height);

    // rounded to the nearest unit, at least 1
    const int64_t scaled=(int64_t(height)*available*2+width)/(int64_t(width)*2);

    return Size(available, int(std::max<int64_t>(1, scaled)));
}

Size MarkdownImage::getBoxSize() const
{
    if(m_look == Look::Alt)
        return Size(0, 0);
    return boxSize(m_imageWidth, m_imageHeight, getWidth());
}

void MarkdownImage::setBlock(const text::MarkdownBlock &block)
{
    const text::MarkdownSpan *image=nullptr;

    for(const text::MarkdownSpan &span:block.spans)
    {
        if(span.image)
            image=&span;
    }

    // the alt text is a paragraph of the image span alone
    m_spans.clear();
    if(image)
        m_spans.push_back(*image);

    text::MarkdownBlock alt=block;

    alt.spans=m_spans;
    m_alt->setBlock(alt);

    const String source=image?image->src:String();

    // asked once per source (a null answer too), and again for a new one
    if(!m_resolved || (source != m_source))
    {
        m_source=source;
        resolve();
    }
    // the alt text's spans for the look (the same source: the look as it was)
    showAlt();
}

void MarkdownImage::resolve()
{
    MarkdownDocument *document=documentOf(this);
    std::shared_ptr<ImageHandle> handle=document?document->resolveImage(m_source):nullptr;

    m_resolved=true;
    if(handle != m_watch.handle())
    {
        // a serial counts one handle's changes: another handle's bytes are
        // loaded whatever its serial
        m_shownSerial=NoSerial;
        if(handle)
            m_watch.reset(handle, [this](ImageHandle &changed) { handleChanged(changed); });
        else
            m_watch.reset();
    }
    applyHandle(true);
}

void MarkdownImage::handleChanged(ImageHandle &)
{
    applyHandle(true);
}

void MarkdownImage::applyHandle(bool mark)
{
    const ImageHandle *handle=m_watch.handle().get();
    Look look=Look::Alt;

    if(handle)
    {
        if(handle->state() == ImageHandle::State::Bytes)
            look=Look::Image;
        else if(handle->state() == ImageHandle::State::Sized)
            look=Look::Box;
    }

    const int width=(look == Look::Alt)?0:handle->width();
    const int height=(look == Look::Alt)?0:handle->height();
    const uint64_t serial=handle?handle->serial():0;
    const bool lookChanged=(look != m_look);
    const bool sizeChanged=(width != m_imageWidth) || (height != m_imageHeight);

    if(look == Look::Image)
    {
        if(!m_image)
        {
            m_image=newChild<ImagePanel>();
            m_image->setPlacement(Placement::Owner);
        }
        if(lookChanged || (serial != m_shownSerial))
            m_image->setImageData(handle->bytes().data(), handle->bytes().size(), MaxImageSide);
        m_image->setHidden(false);
    }
    else if(m_image)
    {
        // the texture given back as the image goes
        m_image->clearImage();
        m_image->setHidden(true);
    }

    m_look=look;
    m_imageWidth=width;
    m_imageHeight=height;
    m_shownSerial=serial;
    if(lookChanged)
        showAlt();

    if(mark && (lookChanged || sizeChanged))
    {
        invalidate();
        addStateChange(StateChange_Background);
    }
}

void MarkdownImage::showAlt()
{
    // over the image the alt text shows nothing, and holds no slot: it stays
    // shown (and laid out), so the block looks the same however it got here
    if(m_look == Look::Image)
    {
        m_alt->setSpans({});
        return;
    }

    std::vector<RichSpan> spans=MarkdownText::richSpans(m_spans);

    // in its box the alt text is on no placeholder of its own
    if(m_look != Look::Alt)
    {
        for(RichSpan &span:spans)
            span.style&=~unsigned(RichSpan::Placeholder);
    }
    m_alt->setSpans(std::move(spans));
}

void MarkdownImage::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        const int width=(m_look == Look::Alt)?m_alt->getPreferredSize().width:m_imageWidth;

        m_preferredSize.width=preferredFromContent(width, Dim::X);
        return;
    }
    if(inLayoutPass() && !laidOutThisPass(Dim::X))
        return;

    const int height=(m_look == Look::Alt)?m_alt->getPreferredSize().height:getBoxSize().height;

    m_preferredSize.height=preferredFromContent(height, Dim::Y);
}

void MarkdownImage::arrange(skin::Base *skin, Dim dim)
{
    const Size box=getBoxSize();

    if(dim == Dim::X)
    {
        if(m_look == Look::Alt)
            placeChild(m_alt, 0, 0, getWidth(), m_alt->getHeight());
        else if(m_look == Look::Image)
            placeChild(m_alt, 0, 0, box.width, m_alt->getHeight());
        else
            placeChild(m_alt, AltInset, AltInset, std::max(0, box.width-2*AltInset), m_alt->getHeight());
        if(m_image && (m_look == Look::Image))
            placeChild(m_image, 0, 0, box.width, m_image->getHeight());
    }
    else
    {
        if(m_look != Look::Box)
            placeChild(m_alt, m_alt->getX(), 0, m_alt->getWidth(), m_alt->getPreferredSize().height);
        else
            placeChild(m_alt, m_alt->getX(), AltInset, m_alt->getWidth(),
                std::max(0, std::min(m_alt->getPreferredSize().height, box.height-2*AltInset)));
        if(m_image && (m_look == Look::Image))
            placeChild(m_image, m_image->getX(), 0, m_image->getWidth(), box.height);
    }

    ParentClass::arrange(skin, dim);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownCodeScroll

GWENI_CONTROL_CONSTRUCTOR(MarkdownCodeScroll)
{
    // the code where a wrapped block's is: no inset round it
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    setScroll(true, false);
    setAutoHideBars(true);
}

bool MarkdownCodeScroll::onMouseWheeled(int delta)
{
    // the page's: a vertical wheel never moves the horizontal bar here
    return Base::onMouseWheeled(delta);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownCodeBlock

GWENI_CONTROL_CONSTRUCTOR(MarkdownCodeBlock)
{
    setPadding(gweni::Padding(Padding, Padding, Padding, Padding));
    m_text=newChild<MarkdownText>();
    m_text->setPlacement(Placement::Owner);
}

void MarkdownCodeBlock::setBlock(const text::MarkdownBlock &block)
{
    m_code=block.text;
    m_language=block.language;
    if(MarkdownDocument *document=documentOf(this))
        setWrap(document->getCodeWrap());
    showCode();
}

void MarkdownCodeBlock::showCode()
{
    MarkdownDocument *document=documentOf(this);
    std::vector<RichSpan> spans;

    if(document && document->getCodeHighlighter())
    {
        spans=document->getCodeHighlighter()(m_language, m_code);

        String joined;

        for(RichSpan &span:spans)
        {
            span.style&=~unsigned(RichSpan::Code);
            joined+=span.text;
        }
        if(joined != m_code)
            spans.clear();
    }
    if(spans.empty())
        spans.push_back(RichSpan::plain(m_code));
    if(document)
        m_text->setFont(document->codeFont());
    m_text->setSpans(std::move(spans));
}

void MarkdownCodeBlock::setWrap(bool wrap)
{
    if(wrap == m_wrap)
        return;

    m_wrap=wrap;
    if(wrap)
    {
        m_text->setParent(this);
        m_text->setDock(DockPosition::None);
        m_text->setPlacement(Placement::Owner);
        if(m_scroll)
            m_scroll->setHidden(true);
    }
    else
    {
        if(!m_scroll)
        {
            m_scroll=newChild<MarkdownCodeScroll>();
            m_scroll->setPlacement(Placement::Owner);
        }
        m_scroll->setHidden(false);
        // no width yet: measured before it is laid out, its lines are the
        // code's own, never the ones a narrower wrapped width broke it into
        placeChild(m_text, 0, 0, 0, m_text->getHeight());
        m_text->setPlacement(Placement::Parent);
        m_text->setParent(m_scroll);
        m_text->setDock(DockPosition::Top);
        m_scroll->setHorizontalScrollOffset(0);
    }
    invalidate();
}

void MarkdownCodeBlock::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    Base *content=m_wrap?static_cast<Base *>(m_text):static_cast<Base *>(m_scroll);
    const gweni::Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        m_preferredSize.width=preferredFromContent(content->getPreferredSize().width+padding.left+padding.right,
            Dim::X);
        return;
    }
    if(inLayoutPass() && !laidOutThisPass(Dim::X))
        return;

    int height=content->getPreferredSize().height;

    // scrolled: the bar's room under the lines when they are wider than it
    if(!m_wrap && (m_scroll->getPreferredSize().width > getWidth()-padding.left-padding.right))
        height+=skin->Metrics.ScrollBarThickness;
    m_preferredSize.height=preferredFromContent(height+padding.top+padding.bottom, Dim::Y);
}

void MarkdownCodeBlock::arrange(skin::Base *skin, Dim dim)
{
    const gweni::Padding &padding=getPadding();
    Base *content=m_wrap?static_cast<Base *>(m_text):static_cast<Base *>(m_scroll);

    if(dim == Dim::X)
        placeChild(content, padding.left, padding.top, std::max(0, getWidth()-padding.left-padding.right),
            content->getHeight());
    else
        placeChild(content, content->getX(), padding.top, content->getWidth(),
            std::max(0, getHeight()-padding.top-padding.bottom));

    ParentClass::arrange(skin, dim);
}

///////////////////////////////////////////////////////////////////////////
// MarkdownTable

GWENI_CONTROL_CONSTRUCTOR(MarkdownTable)
{
}

MarkdownText *MarkdownTable::getCell(size_t row, size_t column) const
{
    if((row >= m_cells.size()) || (column >= m_cells[row].size()))
        return nullptr;
    return m_cells[row][column];
}

void MarkdownTable::setBlock(const text::MarkdownBlock &block)
{
    if(m_aligns != block.aligns)
    {
        m_aligns=block.aligns;
        invalidate();
    }

    const size_t columns=m_aligns.size();
    const size_t rows=block.children.size();

    // a cell keeps its control by its row and column
    while(m_cells.size() > rows)
    {
        for(MarkdownText *cell:m_cells.back())
        {
            cell->setHidden(true);
            cell->delayedDelete();
        }
        m_cells.pop_back();
    }
    m_cells.resize(rows);
    for(size_t row=0; row < rows; ++row)
    {
        const text::MarkdownBlock &rowBlock=block.children[row];
        std::vector<MarkdownText *> &cells=m_cells[row];

        while(cells.size() > columns)
        {
            cells.back()->setHidden(true);
            cells.back()->delayedDelete();
            cells.pop_back();
        }
        while(cells.size() < columns)
        {
            cells.push_back(newChild<MarkdownText>());
            cells.back()->setPlacement(Placement::Owner);
        }
        for(size_t column=0; column < columns; ++column)
        {
            MarkdownText *cell=cells[column];

            cell->setHeader(rowBlock.header);
            if(column < rowBlock.children.size())
                cell->setBlock(rowBlock.children[column]);
            else
                cell->setBlock(text::MarkdownBlock());
        }
    }
}

std::vector<int> MarkdownTable::splitColumns(const std::vector<int> &natural, int available)
{
    const size_t count=natural.size();
    int total=0;

    for(int width:natural)
        total+=width;
    if((count == 0) || (total <= available))
        return natural;

    std::vector<int> widths(count, 0);
    std::vector<bool> fixed(count, false);
    int left=std::max(available, int(count));
    size_t open=count;

    // a column wanting no more than an even share of what is left keeps its
    // width, until none does
    for(bool changed=true; changed && (open > 0);)
    {
        changed=false;
        for(size_t i=0; i < count; ++i)
        {
            if(!fixed[i] && (natural[i]*int(open) <= left))
            {
                fixed[i]=true;
                widths[i]=std::max(natural[i], 1);
                left-=widths[i];
                --open;
                changed=true;
            }
        }
    }
    if(open == 0)
        return widths;

    // the rest share what remains in proportion to their widths
    long long wanted=0;

    for(size_t i=0; i < count; ++i)
    {
        if(!fixed[i])
            wanted+=natural[i];
    }

    int given=0;

    for(size_t i=0; i < count; ++i)
    {
        if(fixed[i])
            continue;
        widths[i]=std::max(1, int((long long)(left)*natural[i]/std::max(wanted, 1LL)));
        given+=widths[i];
    }
    for(size_t i=0; (given < left) && (i < count); ++i)
    {
        if(!fixed[i])
        {
            ++widths[i];
            ++given;
        }
    }
    // a share raised to 1 is taken back from the widest
    while(given > left)
    {
        size_t widest=count;

        for(size_t i=0; i < count; ++i)
        {
            if(!fixed[i] && (widths[i] > 1) && ((widest == count) || (widths[i] > widths[widest])))
                widest=i;
        }
        if(widest == count)
            break;
        --widths[widest];
        --given;
    }
    return widths;
}

void MarkdownTable::calculateSize(skin::Base *skin, Dim dim)
{
    const size_t columns=m_aligns.size();

    if(dim == Dim::X)
    {
        ParentClass::calculateSize(skin, dim);

        m_natural.assign(columns, 0);
        for(const std::vector<MarkdownText *> &cells:m_cells)
        {
            for(size_t column=0; column < columns; ++column)
                m_natural[column]=std::max(m_natural[column], cells[column]->getPreferredSize().width);
        }

        int width=Border;

        for(int natural:m_natural)
            width+=natural+2*CellPaddingX+Border;
        m_preferredSize.width=preferredFromContent(width, Dim::X);
        return;
    }
    if(inLayoutPass() && !laidOutThisPass(Dim::X))
        return;

    ParentClass::calculateSize(skin, dim);

    int height=Border;

    m_rowHeights.assign(m_cells.size(), 0);
    for(size_t row=0; row < m_cells.size(); ++row)
    {
        int tallest=0;

        for(const MarkdownText *cell:m_cells[row])
            tallest=std::max(tallest, cell->getPreferredSize().height);
        m_rowHeights[row]=tallest+2*CellPaddingY;
        height+=m_rowHeights[row]+Border;
    }
    m_preferredSize.height=preferredFromContent(height, Dim::Y);
}

void MarkdownTable::arrange(skin::Base *skin, Dim dim)
{
    using Align=text::MarkdownBlock::Align;
    const size_t columns=m_aligns.size();

    if(dim == Dim::X)
    {
        std::vector<int> natural(columns);

        for(size_t column=0; column < columns; ++column)
            natural[column]=((column < m_natural.size())?m_natural[column]:0)+2*CellPaddingX;
        const std::vector<int> oldEdges=m_columnEdges;

        m_columnWidths=splitColumns(natural, getWidth()-int(columns+1)*Border);
        m_columnEdges.assign(columns+1, 0);
        for(size_t column=0; column < columns; ++column)
            m_columnEdges[column+1]=m_columnEdges[column]+Border+m_columnWidths[column];
        // the grid is drawn from the edges: lines moved inside the same
        // bounds are the table's change alone
        if(m_columnEdges != oldEdges)
            addStateChange(StateChange_Background);

        for(const std::vector<MarkdownText *> &cells:m_cells)
        {
            for(size_t column=0; column < columns; ++column)
            {
                MarkdownText *cell=cells[column];
                const int room=std::max(0, m_columnWidths[column]-2*CellPaddingX);
                const int width=std::min(cell->getPreferredSize().width, room);
                int x=m_columnEdges[column]+Border+CellPaddingX;

                if(m_aligns[column] == Align::Right)
                    x+=room-width;
                else if(m_aligns[column] == Align::Center)
                    x+=(room-width)/2;
                placeChild(cell, x, cell->getY(), width, cell->getHeight());
            }
        }
    }
    else
    {
        const std::vector<int> oldEdges=m_rowEdges;

        m_rowEdges.assign(m_cells.size()+1, 0);
        for(size_t row=0; row < m_cells.size(); ++row)
        {
            const int height=(row < m_rowHeights.size())?m_rowHeights[row]:0;

            m_rowEdges[row+1]=m_rowEdges[row]+Border+height;
            for(MarkdownText *cell:m_cells[row])
                placeChild(cell, cell->getX(), m_rowEdges[row]+Border+CellPaddingY, cell->getWidth(),
                    cell->getPreferredSize().height);
        }
        if(m_rowEdges != oldEdges)
            addStateChange(StateChange_Background);
    }

    ParentClass::arrange(skin, dim);
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
    setSpacing(m_tight?Spacing::Tight:Spacing::Block);
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
            item->setSpacing(getSpacingKind());
            item->setMarker(markerText(i));
        }
    }
}

///////////////////////////////////////////////////////////////////////////
// MarkdownListItem

GWENI_CONTROL_CONSTRUCTOR(MarkdownListItem)
{
    m_marker=newChild<MarkdownText>();
    m_marker->setPlacement(Placement::Owner);
}

void MarkdownListItem::setBlock(const text::MarkdownBlock &block)
{
    updateColors();
    setBlocks(block.children);
}

void MarkdownListItem::updateColors()
{
    ParentClass::updateColors();
    if(skin::Base *skin=getSkin())
        setPadding(Padding(skin->Metrics.MarkdownListIndent, 0, 0, 0));
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
}

void MarkdownDocument::setMarkdown(const String &markdown)
{
    if(markdown == m_markdown)
        return;

    // the selection's ends (a caret's too) by the texts that hold them
    const Selectable *anchor=selectableAt(m_selection.anchor().block);
    const Selectable *focus=selectableAt(m_selection.focus().block);
    const bool selected=(anchor != nullptr) || (focus != nullptr);
    const internal::MarkdownText *anchorText=anchor?anchor->text:nullptr;
    const internal::MarkdownText *focusText=focus?focus->text:nullptr;
    const size_t anchorOffset=m_selection.anchor().offset;
    const size_t focusOffset=m_selection.focus().offset;

    m_markdown=markdown;
    setBlocks(text::parseMarkdown(markdown));
    assignPaths();

    if(selected)
    {
        anchor=selectableOf(anchorText);
        focus=selectableOf(focusText);
        // clamped to the new text, back to the start of a code point
        auto at=[](const Selectable *end, size_t offset)
        {
            const String &shown=end->text->getText();

            offset=std::min(offset, shown.size());
            while((offset > 0) && (offset < shown.size()) && ((static_cast<unsigned char>(shown[offset])&0xC0) == 0x80))
                --offset;
            return text::SelectionPosition(end->path, offset);
        };

        if(anchor && focus)
            m_selection.select(at(anchor, anchorOffset), at(focus, focusOffset));
        else
            m_selection.clear();
    }
    selectionChanged();
}

void MarkdownDocument::assignPaths()
{
    using internal::MarkdownCodeBlock;
    using internal::MarkdownColumn;
    using internal::MarkdownListItem;
    using internal::MarkdownTable;
    using internal::MarkdownText;

    std::vector<Selectable> selectables;

    auto add=[this, &selectables](MarkdownText *text, std::vector<size_t> path, Selectable::Kind kind,
        const Base *table, size_t row)
    {
        if((&text->selectionModel() != &m_selection) || (text->selectionBlock() != path))
            text->setSelectionModel(&m_selection, path);

        Selectable selectable;

        selectable.path=std::move(path);
        selectable.text=text;
        selectable.kind=kind;
        selectable.table=table;
        selectable.row=row;
        selectables.push_back(std::move(selectable));
    };

    std::function<void(Base *, const std::vector<size_t> &)> visit=
        [&](Base *control, const std::vector<size_t> &path)
    {
        if(MarkdownListItem *item=dynamic_cast<MarkdownListItem *>(control))
        {
            std::vector<size_t> inner=path;

            inner.push_back(0);
            add(item->getMarker(), inner, Selectable::Kind::Marker, nullptr, 0);
            for(size_t j=0; j < item->getBlockCount(); ++j)
            {
                inner.back()=1+j;
                visit(item->getBlockControl(j), inner);
            }
        }
        else if(MarkdownColumn *column=dynamic_cast<MarkdownColumn *>(control))
        {
            std::vector<size_t> inner=path;

            inner.push_back(0);
            for(size_t j=0; j < column->getBlockCount(); ++j)
            {
                inner.back()=j;
                visit(column->getBlockControl(j), inner);
            }
        }
        else if(MarkdownCodeBlock *code=dynamic_cast<MarkdownCodeBlock *>(control))
            add(code->getText(), path, Selectable::Kind::Text, nullptr, 0);
        else if(internal::MarkdownImage *image=dynamic_cast<internal::MarkdownImage *>(control))
            add(image->getAltText(), path, Selectable::Kind::Text, nullptr, 0);
        else if(MarkdownTable *table=dynamic_cast<MarkdownTable *>(control))
        {
            for(size_t row=0; row < table->getRowCount(); ++row)
            {
                for(size_t column=0; column < table->getColumnCount(); ++column)
                {
                    std::vector<size_t> cell=path;

                    cell.push_back(row);
                    cell.push_back(column);
                    add(table->getCell(row, column), cell, Selectable::Kind::Cell, table, row);
                }
            }
        }
        else if(MarkdownText *text=dynamic_cast<MarkdownText *>(control))
            add(text, path, Selectable::Kind::Text, nullptr, 0);
    };

    for(size_t i=0; i < m_blockControls.size(); ++i)
        visit(m_blockControls[i], std::vector<size_t>(1, i));

    m_selectables=std::move(selectables);

    // what was told to texts that are gone is forgotten
    std::unordered_map<const Base *, std::pair<bool, std::pair<size_t, size_t>>> shown;

    for(const Selectable &selectable:m_selectables)
    {
        auto iter=m_shown.find(selectable.text);

        if(iter != m_shown.end())
            shown.insert(*iter);
    }
    m_shown=std::move(shown);
}

const MarkdownDocument::Selectable *MarkdownDocument::selectableOf(const internal::MarkdownText *text) const
{
    if(!text)
        return nullptr;
    for(const Selectable &selectable:m_selectables)
    {
        if(selectable.text == text)
            return &selectable;
    }
    return nullptr;
}

const MarkdownDocument::Selectable *MarkdownDocument::selectableAt(const std::vector<size_t> &path) const
{
    auto iter=std::lower_bound(m_selectables.begin(), m_selectables.end(), path,
        [](const Selectable &selectable, const std::vector<size_t> &at) { return selectable.path < at; });

    return ((iter != m_selectables.end()) && (iter->path == path))?&*iter:nullptr;
}

text::SelectionPosition MarkdownDocument::positionAt(const Point &at)
{
    const Selectable *nearest=nullptr;
    int nearestDistance=0;

    for(const Selectable &selectable:m_selectables)
    {
        if(!selectable.text->visible())
            continue;

        const Rect bounds=selectable.text->getCanvasBounds();

        if((at.y < bounds.y) || (at.y >= bounds.y+bounds.h))
            continue;
        if((at.x >= bounds.x) && (at.x < bounds.x+bounds.w))
            return text::SelectionPosition(selectable.path,
                selectable.text->positionAt(selectable.text->canvasPosToLocal(at)));

        // on its line, across from it
        const int distance=(at.x < bounds.x)?bounds.x-at.x:at.x-(bounds.x+bounds.w)+1;

        if(!nearest || (distance < nearestDistance))
        {
            nearest=&selectable;
            nearestDistance=distance;
        }
    }
    if(nearest)
        return text::SelectionPosition(nearest->path, nearest->text->positionAt(nearest->text->canvasPosToLocal(at)));

    // between texts: the start of the first one below
    const Selectable *last=nullptr;

    for(const Selectable &selectable:m_selectables)
    {
        if(!selectable.text->visible())
            continue;
        if(selectable.text->getCanvasBounds().y > at.y)
            return text::SelectionPosition(selectable.path, 0);
        last=&selectable;
    }
    if(last)
        return text::SelectionPosition(last->path, last->text->getText().size());
    return text::SelectionPosition();
}

void MarkdownDocument::select(const text::SelectionPosition &anchor, const text::SelectionPosition &focus)
{
    m_selection.select(anchor, focus);
    selectionChanged();
}

void MarkdownDocument::selectAll()
{
    if(m_selectables.empty())
        return;

    const Selectable &last=m_selectables.back();

    select(text::SelectionPosition(m_selectables.front().path, 0),
        text::SelectionPosition(last.path, last.text->getText().size()));
}

void MarkdownDocument::clearSelection()
{
    if(m_selection.empty())
        return;
    m_selection.setCaret(m_selection.focus());
    selectionChanged();
}

String MarkdownDocument::getSelectedText()
{
    String out;
    const Selectable *previous=nullptr;

    if(m_selection.empty())
        return out;

    const std::vector<size_t> &first=m_selection.start().block;
    const std::vector<size_t> &last=m_selection.end().block;

    for(const Selectable &selectable:m_selectables)
    {
        std::pair<size_t, size_t> bytes;
        const String &shown=selectable.text->getText();

        if(!selectable.text->selectedRange(bytes))
        {
            // an empty cell inside the selection still has its tab
            if(!((selectable.kind == Selectable::Kind::Cell) && shown.empty() && (first < selectable.path)
                && (selectable.path < last)))
                continue;
            bytes={ 0, 0 };
        }
        if(previous)
        {
            if((previous->kind == Selectable::Kind::Cell) && (selectable.kind == Selectable::Kind::Cell)
                && (previous->table == selectable.table) && (previous->row == selectable.row))
                out+='\t';
            else if((previous->kind == Selectable::Kind::Marker)
                && (selectable.path.size() >= previous->path.size())
                && std::equal(previous->path.begin(), previous->path.end()-1, selectable.path.begin()))
                out+=' '; // the marker's item's own text (an empty item's next marker is not)
            else
                out+='\n';
        }
        out+=shown.substr(bytes.first, bytes.second-bytes.first);
        previous=&selectable;
    }
    return out;
}

bool MarkdownDocument::copySelection()
{
    if(m_selection.empty())
        return false;

    platform::setClipboardText(getSelectedText());
    return true;
}

void MarkdownDocument::selectionChanged()
{
    for(const Selectable &selectable:m_selectables)
    {
        std::pair<size_t, size_t> bytes;
        const bool has=selectable.text->selectedRange(bytes);
        auto &shown=m_shown[selectable.text];

        if((shown.first != has) || (has && (shown.second != bytes)))
        {
            shown={ has, has?bytes:std::pair<size_t, size_t>(0, 0) };
            selectable.text->selectionChanged();
        }
    }
}

void MarkdownDocument::setFont(const Font &font)
{
    if(m_hasFont && (m_font == font))
        return;

    m_font=font;
    m_hasFont=true;

    // every block again in the new font, from its own block
    forEachBlock([this](Base *control, const text::MarkdownBlock *block)
    {
        if(block)
        {
            if(internal::MarkdownBlockControl *blockControl=dynamic_cast<internal::MarkdownBlockControl *>(control))
                blockControl->setBlock(*block);
        }
        else if(internal::MarkdownText *marker=dynamic_cast<internal::MarkdownText *>(control))
            marker->setFont(m_font);
    });
}

void MarkdownDocument::forEachBlock(const std::function<void(Base *, const text::MarkdownBlock *)> &visit)
{
    std::vector<internal::MarkdownColumn *> pending(1, this);

    while(!pending.empty())
    {
        internal::MarkdownColumn *column=pending.back();

        pending.pop_back();
        if(internal::MarkdownListItem *item=dynamic_cast<internal::MarkdownListItem *>(column))
            visit(item->getMarker(), nullptr);
        for(size_t i=0; i < column->getBlockCount(); ++i)
        {
            Base *block=column->getBlockControl(i);

            if(internal::MarkdownColumn *inner=dynamic_cast<internal::MarkdownColumn *>(block))
                pending.push_back(inner);
            else
                visit(block, &column->getBlocks()[i]);
        }
    }
}

Font MarkdownDocument::codeFont()
{
    const Font body=getFont();
    Font font=body;

    if(skin::Base *skin=getSkin())
        font=skin->getDefaultMonoFont();
    font.size=body.size;
    return font;
}

void MarkdownDocument::setCodeWrap(bool wrap)
{
    if(wrap == m_codeWrap)
        return;

    m_codeWrap=wrap;
    forEachBlock([wrap](Base *control, const text::MarkdownBlock *)
    {
        if(internal::MarkdownCodeBlock *code=dynamic_cast<internal::MarkdownCodeBlock *>(control))
            code->setWrap(wrap);
    });
}

void MarkdownDocument::setCodeHighlighter(MarkdownHighlighter highlighter)
{
    m_highlighter=std::move(highlighter);
    forEachBlock([](Base *control, const text::MarkdownBlock *block)
    {
        if(internal::MarkdownCodeBlock *code=dynamic_cast<internal::MarkdownCodeBlock *>(control))
            code->setBlock(*block);
    });
}

void MarkdownDocument::setImageResolver(MarkdownImageResolver resolver)
{
    m_imageResolver=std::move(resolver);
    forEachBlock([](Base *control, const text::MarkdownBlock *)
    {
        if(internal::MarkdownImage *image=dynamic_cast<internal::MarkdownImage *>(control))
            image->resolve();
    });
}

std::shared_ptr<ImageHandle> MarkdownDocument::resolveImage(const String &url)
{
    return m_imageResolver?m_imageResolver(url):nullptr;
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
    const decltype(skin::Base::Metrics) defaults{};
    const auto &metrics=getSkin()?getSkin()->Metrics:defaults;
    const int sizes[]={ metrics.MarkdownHeading1, metrics.MarkdownHeading2, metrics.MarkdownHeading3,
        metrics.MarkdownHeading4, metrics.MarkdownHeading5, metrics.MarkdownHeading6 };

    level=std::min(std::max(level, 1), 6);
    return float(sizes[level-1])*getFont().size/11.0f;
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
