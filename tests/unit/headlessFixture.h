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

#include <map>
#include <memory>
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
    };

    void setDrawColor(gweni::Color color) override { drawColor=color; }

    //! Recorded in `lines`, not drawn: a control test checks its segments there.
    void drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1, float thickness,
        int zIndex) override
    {
        lines.push_back({primitiveId, p0, p1, thickness, zIndex, drawColor});
    }

    //! Counted in `measures`, then measured as renderer::Base does.
    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override
    {
        ++measures;
        return Base::measureText(font, text);
    }

    //! measureText() calls so far. Tests take deltas.
    size_t measures=0;

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

    //! \p typeName picks the StubSkin::depthLayers entry it reports.
    StubControl(StubSkin *skin, bool takesText, std::string typeName={});
    ~StubControl() override;

    std::string getTypeName() override { return "Stub"; }
    size_t requiredPrimitives() override { return primitivesPerControl; }
    size_t depthLayers() override;

    void generate(gweni::renderer::Base *renderer, gweni::controls::Base *control) override;
    void remove(gweni::renderer::Base *renderer, gweni::controls::Base *control) override;

private:
    StubSkin *m_skin;
    std::string m_typeName;
    bool m_takesText;
    bool m_hasText=false;
    size_t m_textId=0;
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
