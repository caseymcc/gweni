/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A skin Control draws only at the z values it reserves: getZIndex() up to
 *  its depthLayers(), and getOverlayZIndex() up to its overlayLayers(). One it
 *  draws at but does not reserve is also the next control's in the numbering
 *  (Base::renumberZRecursive), where an overlay fights that control under
 *  GL_LESS. Each row of the table is a textured renderer that draws an overlay,
 *  run on a real control in the state that shows it; a new overlay renderer is
 *  one more row.
 *
 *  The rows: ProgressBar, ColorSlider, ColorLerpBox, CheckBox, RadioButton,
 *  ComboBox, ScrollBarButton, the three window buttons, TreeNode's
 *  OpenToggleButton, and TextBox's caret; every other type gets a
 *  StubControl. A row fails, naming the renderer, if
 *  any draw into its slots is at a z outside [getZIndex(),
 *  getZIndex()+depthLayers()) and [getOverlayZIndex(),
 *  getOverlayZIndex()+overlayLayers()), if no draw is at the overlay's z, or
 *  unless its first child is numbered at getZIndex()+depthLayers() and its
 *  next sibling after its whole subtree.
 */
#include "headlessFixture.h"

#include <gweni/inputHandler.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/colorControls.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/progressBar.h>
#include <gweni/controls/radioButton.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/treeNode.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! Records the slot and z of every draw.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        int zIndex;
    };

    void drawFilledRect(size_t primitiveId, Rect, int zIndex) override
    {
        drawn.push_back({primitiveId, zIndex});
    }

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color,
        int zIndex) override
    {
        drawn.push_back({primitiveId, zIndex});
    }

    void drawBilinearRect(size_t primitiveId, Rect, int zIndex, Color, Color, Color, Color,
        float, float, Color) override
    {
        drawn.push_back({primitiveId, zIndex});
    }

    void drawCheckerRect(size_t primitiveId, Rect, int zIndex, Color, Color, Color,
        float, float, Color) override
    {
        drawn.push_back({primitiveId, zIndex});
    }

    void drawIcon(size_t primitiveId, Rect, int, int, Color, int zIndex) override
    {
        drawn.push_back({primitiveId, zIndex});
    }

    void drawTexturedRect(const Texture &, size_t primitiveId, Rect, int zIndex,
        float, float, float, float) override
    {
        drawn.push_back({primitiveId, zIndex});
    }

    std::vector<Draw> drawn;
};

//! The textured skin's own renderers for the types in the table, a
//! StubControl (one z, no overlay) for the rest. The textured skin is never
//! init()'d: these renderers draw tokyonight surfaces, not atlas entries.
class OverlaySkin: public test::StubSkin
{
public:
    OverlaySkin(test::StubRenderer *renderer, std::set<std::string> textured)
        : StubSkin(renderer), m_texturedSkin(renderer), m_textured(std::move(textured))
    {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(m_textured.count(typeName))
            return m_texturedSkin.getControlRenderer(typeName);
        return StubSkin::getControlRenderer(typeName);
    }

private:
    skin::textured::TexturedSkin m_texturedSkin;
    std::set<std::string> m_textured;
};

//! Where a row's overlay is drawn.
enum class Overlay
{
    Above,          //!< at getZIndex()+1, reserved by depthLayers()
    AfterSubtree    //!< at getOverlayZIndex(), reserved by overlayLayers()
};

struct Row
{
    const char *name;   //!< the renderer's (and the control's) type name
    Overlay overlay;
    //! Builds the control on the canvas, in the state that shows its overlay.
    std::function<controls::Base *(controls::Canvas *)> build;
};

const std::vector<Row> &rows()
{
    static const std::vector<Row> table={
        {"ProgressBar", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *bar=canvas->newChild<controls::ProgressBar>();
                bar->setBounds(10, 10, 120, 20);
                bar->setProgress(0.5f);
                return bar;
            }},
        {"ColorSlider", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *slider=canvas->newChild<controls::ColorSlider>();
                slider->setPos(10, 10);
                slider->setColor(Color(0, 255, 0, 255));
                return slider;
            }},
        {"ColorLerpBox", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *box=canvas->newChild<controls::ColorLerpBox>();
                box->setPos(10, 10);
                box->setColor(Color(200, 80, 40, 255), false);
                return box;
            }},
        {"CheckBox", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *check=canvas->newChild<controls::CheckBox>();
                check->setPos(10, 10);
                check->setChecked(true);
                return check;
            }},
        {"RadioButton", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *radio=canvas->newChild<controls::RadioButton>();
                radio->setPos(10, 10);
                radio->setChecked(true);
                return radio;
            }},
        {"ComboBox", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *combo=canvas->newChild<controls::ComboBox>();
                combo->setBounds(10, 10, 120, 24);
                combo->addItem("one");
                return combo;
            }},
        {"ScrollBarButton", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *button=canvas->newChild<controls::internal::ScrollBarButton>();
                button->setBounds(10, 10, 16, 16);
                button->setDirectionDown();
                return button;
            }},
        {"WindowCloseButton", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *button=canvas->newChild<controls::WindowCloseButton>();
                button->setBounds(10, 10, 29, 24);
                return button;
            }},
        {"WindowMaximizeButton", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *button=canvas->newChild<controls::WindowMaximizeButton>();
                button->setBounds(10, 10, 29, 24);
                return button;
            }},
        {"WindowMinimizeButton", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *button=canvas->newChild<controls::WindowMinimizeButton>();
                button->setBounds(10, 10, 29, 24);
                return button;
            }},
        {"OpenToggleButton", Overlay::Above, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *toggle=canvas->newChild<controls::OpenToggleButton>();
                toggle->setPos(10, 10);
                return toggle;
            }},
        // the caret, drawn in front of the box's text
        {"TextBox", Overlay::AfterSubtree, [](controls::Canvas *canvas) -> controls::Base *
            {
                auto *box=canvas->newChild<controls::TextBox>();
                box->setBounds(10, 10, 160, 24);
                box->setText("hello");
                box->focus();
                box->moveCaretToend();
                return box;
            }},
    };
    return table;
}

std::set<std::string> rowNames()
{
    std::set<std::string> names;

    for(const Row &row : rows())
        names.insert(row.name);
    return names;
}

//! One row's control and a sibling after it on a real Canvas, one frame drawn.
struct OverlayScene
{
    explicit OverlayScene(const Row &row)
    {
        // the TextBox's caret blinks on the clock; at a fixed time it stays lit
        platform::setTimeSource([]() { return 10.0f; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        control=row.build(canvas);
        after=canvas->newChild<controls::Base>("after");

        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    OverlayScene(const OverlayScene &)=delete;
    OverlayScene &operator=(const OverlayScene &)=delete;

    ~OverlayScene()
    {
        // the focus is process-global
        KeyboardFocus=nullptr;
        HoveredControl=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! The z of every draw into the control's own slots this frame.
    std::vector<int> drawnZ() const
    {
        const std::vector<size_t> &own=control->getSkinControl()->getPrimitives();
        std::vector<int> z;

        for(const DrawRecorder::Draw &draw : renderer.drawn)
        {
            if(std::find(own.begin(), own.end(), draw.id) != own.end())
                z.push_back(draw.zIndex);
        }
        return z;
    }

    DrawRecorder renderer;
    OverlaySkin skin{&renderer, rowNames()};
    controls::Canvas *canvas=nullptr;
    controls::Base *control=nullptr;
    controls::Base *after=nullptr;
};

//! The control's skin Control. depthLayers() and overlayLayers() are not
//! const, and Base hands the Control out only as const.
skin::Control *skinControlOf(controls::Base *control)
{
    return const_cast<skin::Control *>(control->getSkinControl());
}

size_t depthLayersOf(controls::Base *control)
{
    return control->getSkinControl()?skinControlOf(control)->depthLayers():1;
}

size_t overlayLayersOf(controls::Base *control)
{
    return control->getSkinControl()?skinControlOf(control)->overlayLayers():0;
}

//! The z after every value \p control and its subtree reserve.
int subtreeEnd(controls::Base *control)
{
    int end=control->getZIndex()+(int)depthLayersOf(control);

    for(controls::Base *child : control->getChildren())
        end=std::max(end, subtreeEnd(child));
    if(overlayLayersOf(control))
        end=std::max(end, control->getOverlayZIndex()+(int)overlayLayersOf(control));
    return end;
}

}//namespace

TEST_CASE("every textured overlay renderer draws only at the z values it reserves")
{
    for(const Row &row : rows())
    {
        const std::string renderer=row.name;
        CAPTURE(renderer);
        OverlayScene scene(row);

        REQUIRE(scene.control->getTypeName() == std::string(row.name));
        REQUIRE(scene.control->getSkinControl() != nullptr);
        REQUIRE(skinControlOf(scene.control)->getTypeName() == std::string(row.name));

        const int z=scene.control->getZIndex();
        const int layers=(int)depthLayersOf(scene.control);
        const int overlayZ=scene.control->getOverlayZIndex();
        const int overlayLayers=(int)overlayLayersOf(scene.control);
        const std::vector<int> drawn=scene.drawnZ();

        CAPTURE(z);
        CAPTURE(layers);
        CAPTURE(overlayZ);
        CAPTURE(overlayLayers);
        REQUIRE(!drawn.empty());

        for(int drawnZ : drawn)
        {
            CAPTURE(drawnZ);
            const bool own=drawnZ >= z && drawnZ < z+layers;
            const bool overlay=drawnZ >= overlayZ && drawnZ < overlayZ+overlayLayers;
            CHECK((own || overlay));
        }

        // the state shows the overlay, so the row checks what it is there for
        const int overlayAt=row.overlay == Overlay::Above?z+1:overlayZ;
        CAPTURE(overlayAt);
        CHECK(std::find(drawn.begin(), drawn.end(), overlayAt) != drawn.end());
        if(row.overlay == Overlay::AfterSubtree)
            CHECK(overlayLayers > 0);
    }
}

TEST_CASE("a textured overlay renderer's first child and next sibling come after the z values it reserves")
{
    for(const Row &row : rows())
    {
        const std::string renderer=row.name;
        CAPTURE(renderer);
        OverlayScene scene(row);

        const int z=scene.control->getZIndex();
        const int end=z+(int)depthLayersOf(scene.control);
        const controls::Base::List &children=scene.control->getChildren();

        CAPTURE(z);
        CAPTURE(end);
        if(!children.empty())
            CHECK(children.front()->getZIndex() == end);

        // the next sibling: a control may move one of its own to the canvas
        // (a ComboBox's open menu), so it is not always the scene's "after"
        const controls::Base::List &siblings=scene.canvas->getChildren();
        auto it=std::find(siblings.begin(), siblings.end(), scene.control);
        REQUIRE(it != siblings.end());
        ++it;
        REQUIRE(it != siblings.end());
        const controls::Base *next=*it;

        const std::string nextType=next->getTypeName();
        CAPTURE(nextType);
        CHECK(next->getZIndex() >= end);
        CHECK(next->getZIndex() == subtreeEnd(scene.control));
    }
}
