/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A PropertyRow gets the textured skin's PropertyRow renderer (not the Base
 *  one, which draws nothing) and draws only into its own three slots: the label
 *  column's fill (a rounded rect at the row's z, in tokyonight::PropertyColumn)
 *  and two lines (icons of kind 6 at its z+1, the separator vertical and the
 *  bottom line horizontal, in Colors.Properties.Line_Normal).
 *
 *  Its look follows the row's subtree, raised in the update phase (marked after
 *  updateCanvas(), before renderCanvas()): the mouse onto the value's TextBox,
 *  or a ColorSelector's button inside it, marks StateChange_Background on the
 *  row and its label, and the next frame draws the Hover look into the row's
 *  slots and nothing else's; off the row marks both again and gives the Normal
 *  look back. Moving between the label (the row itself is hovered there: a
 *  Label takes no mouse input) and the value of one row marks nothing on it.
 *  focus() on the editor, the focus moving to the next row with Tab, and
 *  unfocus() mark the rows whose Editing look starts or ends; Editing wins over
 *  Hover. A frame with no hover or focus change marks and draws nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/properties.h>
#include <gweni/controls/property/colorSelector.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records the slots drawn into, as rounded rects (with their top colour) and
//! icons, and the slots shown or hidden.
class RowRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color, int zIndex) override
    {
        rects.push_back({primitiveId, top, zIndex});
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        icons.push_back({primitiveId, rect, kind, direction, color, zIndex});
    }

    void showPrimitive(int, size_t primitiveId) override { shown.push_back(primitiveId); }
    void hidePrimitive(size_t primitiveId) override { shown.push_back(primitiveId); }

    struct Fill
    {
        size_t id;
        Color top;
        int zIndex;
    };

    struct Icon
    {
        size_t id;
        Rect rect;
        int kind;
        int direction;
        Color color;
        int zIndex;
    };

    const Fill *lastFill(size_t id) const
    {
        for(auto it=rects.rbegin(); it != rects.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    const Icon *lastIcon(size_t id) const
    {
        for(auto it=icons.rbegin(); it != icons.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    std::vector<size_t> touched() const
    {
        std::vector<size_t> ids(shown);

        for(const Fill &fill : rects)
            ids.push_back(fill.id);
        for(const Icon &icon : icons)
            ids.push_back(icon.id);
        return ids;
    }

    void clear()
    {
        rects.clear();
        icons.clear();
        shown.clear();
    }

    std::vector<Fill> rects;
    std::vector<Icon> icons;
    std::vector<size_t> shown;
};

//! A Properties of two text rows and a ColorSelector row on a real Canvas,
//! drawn by the (un-init()'d) textured skin on a RowRecorder.
struct RowScene
{
    RowRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Properties *properties=nullptr;
    controls::PropertyRow *first=nullptr;
    controls::PropertyRow *second=nullptr;
    controls::PropertyRow *color=nullptr;

    RowScene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        properties=canvas->newChild<controls::Properties>();
        properties->setBounds(10, 10, 200, 100);
        first=properties->add("First");
        second=properties->add("Second");
        color=properties->add("Color", properties->newChild<controls::property::ColorSelector>(), "255 0 0");

        frame();
        frame();
    }

    RowScene(const RowScene &)=delete;
    RowScene &operator=(const RowScene &)=delete;

    ~RowScene()
    {
        // the hovered and focused controls are process-global
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The mouse to the middle of the control, and the update phase only.
    void moveOnto(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        renderer.clear();
        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        canvas->updateCanvas();
    }

    void moveOff()
    {
        renderer.clear();
        canvas->inputMouseMoved(600, 400, 0, 0);
        canvas->updateCanvas();
    }

    void key(int key)
    {
        renderer.clear();
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        canvas->updateCanvas();
    }

    static controls::TextBox *editor(controls::PropertyRow *row)
    {
        return static_cast<controls::property::Text *>(row->getProperty())->m_textBox;
    }

    static const std::vector<size_t> &ids(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    static bool marked(controls::Base *control)
    {
        return (control->getStateChange() & controls::StateChange_Background) != 0;
    }

    //! The row's fill top colour and its lines' colour, as last drawn.
    void checkLook(controls::PropertyRow *row, const tokyonight::Surface &fill, const Color &line) const
    {
        const std::vector<size_t> &own=ids(row);

        REQUIRE(own.size() == 3);
        const RowRecorder::Fill *drawnFill=renderer.lastFill(own[0]);
        REQUIRE(drawnFill != nullptr);
        CHECK((drawnFill->top == fill.top));

        for(size_t i : {1, 2})
        {
            const RowRecorder::Icon *icon=renderer.lastIcon(own[i]);
            REQUIRE(icon != nullptr);
            CHECK((icon->color == line));
        }
    }
};

}//namespace

TEST_CASE("a PropertyRow gets the textured PropertyRow renderer: a column fill and two lines in three slots")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        RowScene scene;
        const auto *control=dynamic_cast<const skin::textured::PropertyRow *>(scene.first->getSkinControl());
        REQUIRE(control != nullptr);

        // a first frame, recorded
        controls::PropertyRow *row=scene.properties->add("Third");
        scene.frame();

        const std::vector<size_t> &own=RowScene::ids(row);
        REQUIRE(own.size() == 3);
        CHECK(own[0] != own[1]);
        CHECK(own[1] != own[2]);
        CHECK(own[0] != own[2]);

        const RowRecorder::Fill *fill=scene.renderer.lastFill(own[0]);
        REQUIRE(fill != nullptr);
        CHECK((fill->top == tokyonight::PropertyColumn.top));
        CHECK(fill->zIndex == row->getZIndex());

        const Rect bounds=row->getRenderBounds();
        const Rect label=row->getLabel()->getBounds();
        const RowRecorder::Icon *separator=scene.renderer.lastIcon(own[1]);
        const RowRecorder::Icon *bottom=scene.renderer.lastIcon(own[2]);
        REQUIRE(separator != nullptr);
        REQUIRE(bottom != nullptr);
        CHECK(separator->kind == 6);
        CHECK(separator->direction == 1);
        CHECK(bottom->kind == 6);
        CHECK(bottom->direction == 0);
        CHECK(separator->zIndex == row->getZIndex()+1);
        CHECK(bottom->zIndex == row->getZIndex()+1);
        CHECK((separator->color == scene.skin.Colors.Properties.Line_Normal));
        // the separator's stroke is the label column's last unit column, the
        // bottom line's the row's last unit row: each quad is centred on it
        CHECK(separator->rect.x+1 == bounds.x+label.x+label.w-1);
        CHECK(bottom->rect.y+1 == bounds.y+bounds.h-1);
        CHECK(bottom->rect.w == bounds.w);

        // the row's label is its first child, after the row's two depth layers
        CHECK(row->getLabel()->getZIndex() == row->getZIndex()+2);

        // settled
        scene.frame();
        CHECK(scene.renderer.touched().empty());
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
}

TEST_CASE("hovering a PropertyRow's value marks the row and its label in the update phase, label to value marks nothing")
{
    RowScene scene;
    controls::TextBox *value=RowScene::editor(scene.first);

    scene.moveOnto(value);
    REQUIRE(HoveredControl == value);
    CHECK(scene.first->isHovered());
    CHECK(RowScene::marked(scene.first));
    CHECK(RowScene::marked(scene.first->getLabel()));
    CHECK_FALSE(RowScene::marked(scene.second));

    scene.canvas->renderCanvas();
    scene.checkLook(scene.first, tokyonight::PropertyColumnHover, scene.skin.Colors.Properties.Line_Hover);
    CHECK((scene.first->getLabel()->getTextColor() == scene.skin.Colors.Properties.Label_Hover));

    // only the row's slots and the hovered TextBox's own
    {
        std::vector<size_t> owned(RowScene::ids(scene.first));
        const std::vector<size_t> &box=RowScene::ids(value);
        owned.insert(owned.end(), box.begin(), box.end());

        for(size_t id : scene.renderer.touched())
        {
            CAPTURE(id);
            CHECK(std::find(owned.begin(), owned.end(), id) != owned.end());
        }
    }

    // a frame with nothing changed marks and draws nothing
    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // from the value to the label of the same row: the row itself is hovered
    scene.moveOnto(scene.first->getLabel());
    REQUIRE(HoveredControl == scene.first);
    CHECK_FALSE(RowScene::marked(scene.first));
    CHECK_FALSE(RowScene::marked(scene.first->getLabel()));
    scene.canvas->renderCanvas();
    {
        const std::vector<size_t> touched=scene.renderer.touched();

        for(size_t id : RowScene::ids(scene.first))
            CHECK(std::find(touched.begin(), touched.end(), id) == touched.end());
    }

    // and back to the value
    scene.moveOnto(value);
    CHECK_FALSE(RowScene::marked(scene.first));
    scene.canvas->renderCanvas();

    // onto the next row's label: both rows change
    scene.moveOnto(scene.second->getLabel());
    CHECK(RowScene::marked(scene.first));
    CHECK(RowScene::marked(scene.first->getLabel()));
    CHECK(RowScene::marked(scene.second));
    CHECK(RowScene::marked(scene.second->getLabel()));
    scene.canvas->renderCanvas();
    scene.checkLook(scene.first, tokyonight::PropertyColumn, scene.skin.Colors.Properties.Line_Normal);
    scene.checkLook(scene.second, tokyonight::PropertyColumnHover, scene.skin.Colors.Properties.Line_Hover);
    CHECK((scene.first->getLabel()->getTextColor() == scene.skin.Colors.Properties.Label_Normal));

    // off the rows
    scene.moveOff();
    CHECK(RowScene::marked(scene.second));
    CHECK(RowScene::marked(scene.second->getLabel()));
    scene.canvas->renderCanvas();
    scene.checkLook(scene.second, tokyonight::PropertyColumn, scene.skin.Colors.Properties.Line_Normal);
}

TEST_CASE("a ColorSelector's button inside the value's TextBox hovers its PropertyRow")
{
    RowScene scene;
    auto *selector=static_cast<controls::property::ColorSelector *>(scene.color->getProperty());

    scene.moveOnto(selector->m_button);
    REQUIRE(HoveredControl == selector->m_button);
    CHECK(scene.color->isHovered());
    CHECK(RowScene::marked(scene.color));
    scene.canvas->renderCanvas();
    scene.checkLook(scene.color, tokyonight::PropertyColumnHover, scene.skin.Colors.Properties.Line_Hover);

    // onto the TextBox around it: still the same row's hover
    scene.moveOnto(selector->m_textBox);
    REQUIRE(HoveredControl == selector->m_textBox);
    CHECK_FALSE(RowScene::marked(scene.color));
}

TEST_CASE("focus on a PropertyRow's editor marks the row in the update phase, and Tab and unfocus mark it again")
{
    RowScene scene;
    controls::TextBox *firstEditor=RowScene::editor(scene.first);
    controls::TextBox *secondEditor=RowScene::editor(scene.second);

    scene.renderer.clear();
    firstEditor->focus();
    CHECK(scene.first->isEditing());
    CHECK(RowScene::marked(scene.first));
    CHECK(RowScene::marked(scene.first->getLabel()));
    CHECK_FALSE(RowScene::marked(scene.second));
    scene.canvas->updateCanvas();
    scene.canvas->renderCanvas();
    scene.checkLook(scene.first, tokyonight::PropertyColumnEditing, scene.skin.Colors.Properties.Line_Selected);
    CHECK((scene.first->getLabel()->getTextColor() == scene.skin.Colors.Properties.Label_Selected));

    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // Editing wins over Hover
    scene.moveOnto(firstEditor);
    scene.canvas->renderCanvas();
    scene.checkLook(scene.first, tokyonight::PropertyColumnEditing, scene.skin.Colors.Properties.Line_Selected);
    scene.moveOff();
    scene.canvas->renderCanvas();

    // Tab hands the focus to the next row's editor
    scene.key(keys::Tab);
    REQUIRE(KeyboardFocus == secondEditor);
    CHECK(RowScene::marked(scene.first));
    CHECK(RowScene::marked(scene.second));
    CHECK(RowScene::marked(scene.second->getLabel()));
    scene.canvas->renderCanvas();
    scene.checkLook(scene.first, tokyonight::PropertyColumn, scene.skin.Colors.Properties.Line_Normal);
    scene.checkLook(scene.second, tokyonight::PropertyColumnEditing, scene.skin.Colors.Properties.Line_Selected);

    scene.renderer.clear();
    secondEditor->unfocus();
    CHECK_FALSE(scene.second->isEditing());
    CHECK(RowScene::marked(scene.second));
    scene.canvas->updateCanvas();
    scene.canvas->renderCanvas();
    scene.checkLook(scene.second, tokyonight::PropertyColumn, scene.skin.Colors.Properties.Line_Normal);
}

TEST_CASE("a PropertyRow's editor hidden while focused gives the row its Normal look back")
{
    RowScene scene;
    controls::TextBox *editor=RowScene::editor(scene.first);

    editor->focus();
    scene.frame();

    // the input handler drops the focus of a hidden control when it next runs
    scene.first->getProperty()->hide();
    scene.renderer.clear();
    scene.canvas->inputMouseMoved(600, 400, 0, 0);
    scene.canvas->updateCanvas();
    CHECK(KeyboardFocus == nullptr);
    CHECK_FALSE(scene.first->isEditing());
    CHECK(RowScene::marked(scene.first));
    CHECK(RowScene::marked(scene.first->getLabel()));
}
