/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A renderer, skin and skin::Controls that need no GL context, for unit tests
 *  of skin/control/id-bank behaviour. The renderer draws nothing; it forwards
 *  primitive and text id requests to the real, process-global id banks
 *  (primitiveIds.h) and records what every request did, so a test can check
 *  the banks and who held which id.
 */
#ifndef _gweni_tests_unit_headlessFixture_h_
#define _gweni_tests_unit_headlessFixture_h_

#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/primitiveIds.h>
#include <gweni/skin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/utility.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace test
{

//! Snapshot of both id banks. Tests compare deltas between two of these,
//! never absolute values: the banks are process-global and every test case
//! in the executable shares them.
struct IdBankCounts
{
    size_t primitives;          //!< high-water mark
    size_t releasedPrimitives;
    size_t texts;               //!< high-water mark
    size_t releasedTexts;

    size_t livePrimitives() const { return primitives-releasedPrimitives; }
    size_t liveTexts() const { return texts-releasedTexts; }

    static IdBankCounts now()
    {
        return {gweni::primitiveIdCount(), gweni::releasedPrimitiveIdCount(),
            gweni::textIdCount(), gweni::releasedTextIdCount()};
    }
};

//! True if \p ids holds some id twice.
inline bool hasDuplicates(const std::vector<size_t> &ids)
{
    return std::set<size_t>(ids.begin(), ids.end()).size() != ids.size();
}

class StubSkin;

//! A renderer::Base that draws nothing and hands out ids from the real banks.
class StubRenderer: public gweni::renderer::Base
{
public:
    enum class Op
    {
        AcquirePrimitive,
        ReleasePrimitive,
        AcquireText,
        ReleaseText
    };

    struct Event
    {
        Op op;
        size_t id;
    };

    StubRenderer(): Base(resourcePaths()) {}
    //! A stub that resolves resource names through \p paths, which must
    //! outlive it.
    explicit StubRenderer(gweni::ResourcePaths &paths): Base(paths) {}

    void generatePrimitive(size_t &primitiveId) override
    {
        primitiveId=gweni::getPrimitiveId(nullptr);
        acquired(livePrimitives, Op::AcquirePrimitive, primitiveId);
    }

    void releasePrimitive(size_t primitiveId) override
    {
        released(livePrimitives, Op::ReleasePrimitive, primitiveId);
        gweni::releasePrimitiveId(primitiveId);
        //SlotRenderer clears a released slot, which its next flush writes
        wrotePrimitive(primitiveId);
    }

    void generateTextPrimitive(size_t &primitiveId) override
    {
        primitiveId=gweni::getTextId(nullptr);
        acquired(liveTexts, Op::AcquireText, primitiveId);
    }

    void releaseTextPrimitive(size_t primitiveId) override
    {
        released(liveTexts, Op::ReleaseText, primitiveId);
        gweni::releaseTextId(primitiveId);
    }

    //! One drawLine call, as it was made.
    struct LineDraw
    {
        size_t id;
        gweni::PointF p0, p1;
        float thickness;
        int zIndex;
        gweni::Color color;     //!< the draw colour when it was made
        std::optional<gweni::PointF> prev;  //!< the neighbour point before p0, if given
        std::optional<gweni::PointF> next;  //!< the neighbour point after p1, if given
    };

    void setDrawColor(gweni::Color color) override { drawColor=color; }

    //! Every text run laid out so far is stale, as a runtime text AA switch
    //! makes it: raises textLayoutSerial().
    void changeTextLayout() { textLayoutChanged(); }

    //! Recorded in `lines`, not drawn: a control test checks its segments there.
    void drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1, float thickness,
        int zIndex, const gweni::PointF *prev=nullptr, const gweni::PointF *next=nullptr) override
    {
        lines.push_back({primitiveId, p0, p1, thickness, zIndex, drawColor,
            prev ? std::optional<gweni::PointF>(*prev) : std::nullopt,
            next ? std::optional<gweni::PointF>(*next) : std::nullopt});
        wrotePrimitive(primitiveId);
    }

    // The draws below draw nothing; each counts its slot as written in this
    // frame (frameStats()). The stub compares no bytes, so every call is a write,
    // unlike SlotRenderer, where a draw that changes nothing writes nothing. The
    // renderer::Base defaults of drawBilinearRect, drawCheckerRect,
    // drawMissingImage and the multi-rect draws come through these.
    void drawFilledRect(size_t primitiveId, gweni::Rect, int) override { wrotePrimitive(primitiveId); }
    void drawRoundedRect(size_t primitiveId, gweni::Rect, float, float, gweni::Color, gweni::Color,
        gweni::Color, int) override
    {
        roundedEdges[primitiveId]=0;
        roundedReach[primitiveId]=0.0f;
        wrotePrimitive(primitiveId);
    }
    //! Records the edge mask in `roundedEdges` and the reach in `roundedReach`
    //! and draws through the call without one, which a test's subclass may
    //! override to record the rest; a call without a mask records 0 for both.
    void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
        gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex,
        uint8_t edges, float reach=0.0f) override
    {
        drawRoundedRect(primitiveId, rect, radius, border, fillTop, fillBottom, borderColor, zIndex);
        roundedEdges[primitiveId]=edges;
        roundedReach[primitiveId]=reach;
    }
    void drawIcon(size_t primitiveId, gweni::Rect, int, int, gweni::Color, int) override
    {
        wrotePrimitive(primitiveId);
    }
    void drawGrid(size_t primitiveId, gweni::Rect, gweni::PointF, float, int, gweni::Color,
        gweni::Color, int) override
    {
        wrotePrimitive(primitiveId);
    }
    void drawTexturedRect(const gweni::Texture &, size_t primitiveId, gweni::Rect, int, float, float,
        float, float) override
    {
        wrotePrimitive(primitiveId);
    }
    void showPrimitive(int, size_t primitiveId) override { wrotePrimitive(primitiveId); }
    void hidePrimitive(size_t primitiveId) override { wrotePrimitive(primitiveId); }

    //! Counts the string's codepoints as the run's glyph slots, all written.
    void renderText(size_t textId, const gweni::Font &, gweni::Point, int,
        const gweni::String &text) override
    {
        size_t glyphs=0;

        for(size_t pos=0; pos < text.size(); pos=gweni::utf8Next(text, pos))
            ++glyphs;
        textGlyphs[textId]=glyphs;
        wroteText(textId, glyphs);
    }
    //! Write the glyph slots the text id's last renderText counted.
    void showText(size_t textId) override { wroteText(textId, textGlyphs[textId]); }
    void hideText(size_t textId) override { wroteText(textId, textGlyphs[textId]); }

    //! Ends the frame the counts are for: what the draws since the last end()
    //! wrote becomes frameStats(), writtenPrimitiveIds() and writtenTextIds().
    void end() override
    {
        m_counts.counted=true;
        ++m_counts.frameIndex;
        m_writtenPrimitives.assign(m_pendingPrimitives.begin(), m_pendingPrimitives.end());
        m_writtenTexts.clear();
        m_counts.textSlotsWritten=0;
        for(const auto &entry: m_pendingTexts)
        {
            if(entry.second == 0)
                continue;
            m_writtenTexts.push_back({entry.first, entry.second});
            m_counts.textSlotsWritten+=entry.second;
        }
        m_counts.primitiveSlotsWritten=m_writtenPrimitives.size();
        m_pendingPrimitives.clear();
        m_pendingTexts.clear();
    }

    gweni::renderer::FrameCounts frameStats() const override { return m_counts; }
    const std::vector<size_t> &writtenPrimitiveIds() const override { return m_writtenPrimitives; }
    const std::vector<gweni::renderer::WrittenText> &writtenTextIds() const override { return m_writtenTexts; }

    //! Each text id's glyph count at its last renderText
    std::map<size_t, size_t> textGlyphs;
    //! Each primitive id's edge mask at its last drawRoundedRect (0 without one)
    std::map<size_t, uint8_t> roundedEdges;
    //! Each primitive id's reach at its last drawRoundedRect (0 without one)
    std::map<size_t, float> roundedReach;

    //! Counted in `measures`, then measured as renderer::Base does.
    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override
    {
        ++measures;
        return Base::measureText(font, text);
    }

    //! Counted in `lineHeights`, then measured as renderer::Base does: one
    //! measureText() of " ", which `measures` counts too.
    int lineHeight(const gweni::Font &font) override
    {
        ++lineHeights;
        return Base::lineHeight(font);
    }

    //! Counted in `caretMeasures`, then measured as renderer::Base does: one
    //! measureText() per prefix, counted in `caretPrefixMeasures` rather than
    //! in `measures`, so `measures` holds only the measures made from outside.
    void measureCarets(const gweni::Font &font, const gweni::String &text,
        std::vector<int> &xs) override
    {
        const size_t outside=measures;

        ++caretMeasures;
        Base::measureCarets(font, text, xs);
        caretPrefixMeasures+=measures-outside;
        measures=outside;
    }

    //! measureText() calls so far, but those inside a measureCarets(). Tests
    //! take deltas.
    size_t measures=0;
    //! lineHeight() calls so far, each also one of `measures`.
    size_t lineHeights=0;
    //! measureCarets() calls so far, and the measureText() calls made inside them.
    size_t caretMeasures=0;
    size_t caretPrefixMeasures=0;

    //! What maxZIndex() reports: set it small to make the canvas run out of z.
    int maxZ=65535;

    int maxZIndex() const override { return maxZ; }

    //IResourceLoader, nothing to load
    gweni::Font::Status loadFont(const gweni::Font &) override { return gweni::Font::Status::Loaded; }
    void freeFont(const gweni::Font &) override {}
    gweni::Texture::Status loadTexture(gweni::Texture &) override { return textureStatus; }
    void freeTexture(const gweni::Texture &) override {}
    gweni::TextureData getTextureData(const gweni::Texture &) const override { return gweni::TextureData(); }

    //! Ids this renderer handed out and has not been given back, per bank.
    std::set<size_t> livePrimitives;
    std::set<size_t> liveTexts;

    //! Every acquisition and release, in call order. Tests clear it as they go.
    std::vector<Event> events;

    //! The last setDrawColor().
    gweni::Color drawColor;

    //! What every loadTexture returns: set it to an error to fail the loads.
    gweni::Texture::Status textureStatus=gweni::Texture::Status::Loaded;

    //! Every drawLine call, in call order. Tests clear it as they go.
    std::vector<LineDraw> lines;

    //! An id handed out while this renderer still had it live (two owners), or
    //! released while it was not live (released twice). Both should stay 0.
    size_t doubleAcquired=0;
    size_t doubleReleased=0;

    //! The skin whose Control last generated each control's primitives;
    //! removed when that Control's remove() runs.
    std::map<gweni::controls::Base *, const StubSkin *> generatedBy;

    //! Each control's getZIndex() when a StubControl first generated it, the
    //! z its first draw is at; removed with generatedBy. A generate() with no
    //! control (a skin Control driven on its own) records nothing.
    std::map<gweni::controls::Base *, int> firstDrawZ;

private:
    //! Nothing is loaded, so no path is ever looked up.
    struct NoPaths: gweni::ResourcePaths
    {
        gweni::String getPath(Type, gweni::String const &name) override { return name; }
    };

    static gweni::ResourcePaths &resourcePaths()
    {
        static NoPaths paths;
        return paths;
    }

    void wrotePrimitive(size_t id) { m_pendingPrimitives.insert(id); }

    //! A run written twice in a frame counts its larger glyph count once.
    void wroteText(size_t id, size_t glyphs)
    {
        size_t &pending=m_pendingTexts[id];

        pending=std::max(pending, glyphs);
    }

    std::set<size_t> m_pendingPrimitives;
    std::map<size_t, size_t> m_pendingTexts;
    gweni::renderer::FrameCounts m_counts;
    std::vector<size_t> m_writtenPrimitives;
    std::vector<gweni::renderer::WrittenText> m_writtenTexts;

    void acquired(std::set<size_t> &live, Op op, size_t id)
    {
        if(!live.insert(id).second)
            ++doubleAcquired;
        events.push_back({op, id});
    }

    void released(std::set<size_t> &live, Op op, size_t id)
    {
        if(live.erase(id) == 0)
            ++doubleReleased;
        events.push_back({op, id});
    }
};

//! A control type whose skin Control also takes a text id, the way the
//! textured skin's Text does.
class StubText: public gweni::controls::Base
{
public:
    GWENI_CONTROL_INLINE(StubText, gweni::controls::Base)
    {}
};

//! The skin::Control StubSkin hands out for every control type: a fixed number
//! of primitives, plus one text id for StubText.
class StubControl: public gweni::skin::Control
{
public:
    static constexpr size_t primitivesPerControl=2;

    //! \p typeName picks the StubSkin::depthLayers and overlayLayers entries
    //! it reports.
    StubControl(StubSkin *skin, bool takesText, std::string typeName={});
    ~StubControl() override;

    std::string getTypeName() override { return "Stub"; }
    size_t requiredPrimitives() override { return primitivesPerControl; }
    size_t depthLayers() override;
    size_t overlayLayers() override;

    void generate(gweni::renderer::Base *renderer, gweni::controls::Base *control) override;
    void remove(gweni::renderer::Base *renderer, gweni::controls::Base *control) override;
    void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const override
    {
        gweni::skin::Control::appendIds(primitiveIds, textIds);
        if(m_hasText)
            textIds.push_back(m_textId);
    }

private:
    StubSkin *m_skin;
    std::string m_typeName;
    bool m_takesText;
    bool m_hasText=false;
    size_t m_textId=0;
};

//! A StubControl whose update() gives back, itself, every slot past the first
//! `keep` and draws a rect into the rest, as a skin Control whose slot list
//! follows its control does (the textured PlotHistogram's): ids released with
//! no remove() and no control deleted. A test lowers `keep` and marks the
//! control changed.
class ShrinkingControl: public StubControl
{
public:
    using StubControl::StubControl;

    void update(gweni::renderer::Base *renderer, gweni::controls::Base *control) override
    {
        while(m_primitives.size() > keep)
        {
            renderer->releasePrimitive(m_primitives.back());
            m_primitives.pop_back();
        }
        for(size_t id : m_primitives)
            renderer->drawFilledRect(id, control->getRenderBounds(), control->getZIndex());
    }

    size_t keep=primitivesPerControl;
};

//! A skin that hands out a StubControl for any type name and counts how many of
//! its Controls exist.
class StubSkin: public gweni::skin::Base
{
public:
    explicit StubSkin(StubRenderer *renderer): Base(renderer) {}

    std::unique_ptr<gweni::skin::Control> getControlRenderer(std::string typeName) override
    {
        const bool takesText=typeName == StubText::getTypeNameStatic();
        return std::make_unique<StubControl>(this, takesText, std::move(typeName));
    }

    void drawControl(gweni::controls::Base *) override {}

    StubRenderer *stubRenderer() { return static_cast<StubRenderer *>(m_render); }

    //! StubControls of this skin currently alive (owned by some control).
    size_t liveControls=0;

    //! depthLayers() of the StubControls handed out for a type name, read on
    //! every call; a type name not listed reports 1.
    std::map<std::string, size_t> depthLayers;
    //! overlayLayers() likewise; a type name not listed reports 0.
    std::map<std::string, size_t> overlayLayers;
};

inline StubControl::StubControl(StubSkin *skin, bool takesText, std::string typeName)
    : m_skin(skin), m_typeName(std::move(typeName)), m_takesText(takesText)
{
    ++m_skin->liveControls;
}

inline size_t StubControl::depthLayers()
{
    auto layers=m_skin->depthLayers.find(m_typeName);
    return layers == m_skin->depthLayers.end()?1:layers->second;
}

inline size_t StubControl::overlayLayers()
{
    auto layers=m_skin->overlayLayers.find(m_typeName);
    return layers == m_skin->overlayLayers.end()?0:layers->second;
}

inline StubControl::~StubControl()
{
    --m_skin->liveControls;
}

inline void StubControl::generate(gweni::renderer::Base *renderer, gweni::controls::Base *control)
{
    gweni::skin::Control::generate(renderer, control);

    if(m_takesText && !m_hasText)
    {
        renderer->generateTextPrimitive(m_textId);
        m_hasText=true;
    }
    m_skin->stubRenderer()->generatedBy[control]=m_skin;
    if(control)
        m_skin->stubRenderer()->firstDrawZ.emplace(control, control->getZIndex());
}

inline void StubControl::remove(gweni::renderer::Base *renderer, gweni::controls::Base *control)
{
    gweni::skin::Control::remove(renderer, control);

    if(m_hasText)
    {
        renderer->releaseTextPrimitive(m_textId);
        m_hasText=false;
    }
    m_skin->stubRenderer()->generatedBy.erase(control);
    m_skin->stubRenderer()->firstDrawZ.erase(control);
}

//! One renderer and two skins on it, A and B, for swapping between.
struct TwoSkins
{
    StubRenderer renderer;
    StubSkin skinA{&renderer};
    StubSkin skinB{&renderer};
};

//! Calls \p func on \p control and every control below it.
template<typename Func>
void forEachControl(gweni::controls::Base *control, Func &&func)
{
    func(control);
    for(gweni::controls::Base *child : control->getChildren())
        forEachControl(child, func);
}

//! Empty if, walking \p canvas's tree in order (Base::getActualChildren(), as
//! the z renumber does), every control's range [getZIndex(),
//! getZIndex()+depthLayers()) and, after its subtree, its overlay range
//! [getOverlayZIndex(), getOverlayZIndex()+overlayLayers()) come strictly after
//! every range before them, every z is at least 1 and none ends past \p maxZ;
//! else the first range that does not, named by its control's type and id.
inline std::string zOrderViolation(gweni::controls::Base *canvas, int maxZ)
{
    int last=0;
    std::string violation;

    auto range=[&](gweni::controls::Base *control, const char *what, int z, int layers)
    {
        if(!violation.empty())
            return;

        if((z <= last) || (z < 1) || (z+layers-1 > maxZ))
            violation=std::string(control->getTypeName())+"#"+std::to_string(control->getControlId())
                +" "+what+" ["+std::to_string(z)+", "+std::to_string(z+layers)+") after "
                +std::to_string(last)+" (max "+std::to_string(maxZ)+")";
        last=z+layers-1;
    };

    std::function<void(gweni::controls::Base *)> walk=[&](gweni::controls::Base *control)
    {
        gweni::skin::Control *skinControl=const_cast<gweni::skin::Control *>(control->getSkinControl());
        range(control, "z", control->getZIndex(), skinControl?(int)skinControl->depthLayers():1);

        for(gweni::controls::Base *child : control->getActualChildren())
            walk(child);

        const int overlay=skinControl?(int)skinControl->overlayLayers():0;
        if(overlay)
            range(control, "overlay z", control->getOverlayZIndex(), overlay);
        else if(violation.empty() && (control->getOverlayZIndex() != 0))
            violation=std::string(control->getTypeName())+"#"+std::to_string(control->getControlId())
                +" has an overlay z with no overlay layers";
    };
    walk(canvas);
    return violation;
}

//! The tokyonight surfaces as they are when it is made, written back when it
//! goes. They are process globals, so a case that changes them (a theme, a
//! skin config that does not load) must leave them as it found them.
struct SurfaceSnapshot
{
    SurfaceSnapshot()
    {
        for(const auto &entry : gweni::skin::textured::tokyonight::surfaceRegistry())
            saved.emplace(entry.first, *entry.second);
    }

    ~SurfaceSnapshot()
    {
        auto &registry=gweni::skin::textured::tokyonight::surfaceRegistry();

        for(const auto &entry : saved)
            *registry.at(entry.first)=entry.second;
    }

    std::map<std::string, gweni::skin::textured::tokyonight::Surface> saved;
};

}//namespace test

#endif//_gweni_tests_unit_headlessFixture_h_
