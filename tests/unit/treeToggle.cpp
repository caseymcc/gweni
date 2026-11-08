/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A TreeNode's expand toggle (OpenToggleButton) was a caption-less Shrink
 *  Button: it came out the size of its empty Text, 7x15, and with no renderer
 *  of its own it drew as Button's empty pill, the same whether the node was
 *  open or closed. Now it is a Fixed TreeNode::ToggleSize square with a
 *  textured renderer of its own.
 *
 *  Layout (StubSkin): a node with children has a 15x15 toggle at x 0,
 *  vertically centred on its title row, and the title starts at x 16; a leaf
 *  node's toggle is hidden.
 *
 *  Drawing (the real, un-init()'d TexturedSkin on a recording StubRenderer):
 *  the toggle takes 2 slots. At rest the surface slot is hidden and the glyph
 *  slot is a kind-1 chevron at the toggle's z+1 in FgDark, centred,
 *  min(w,h)*6/10 square, pointing right (direction 3) while the node is closed
 *  and down (0) while it is open. Hover draws ButtonHover and an Fg glyph,
 *  and only the toggle's slots; moving off gives the rest look back. A click
 *  through the input handler opens the node: ButtonDown while pressed, the
 *  glyph pointing down and the toggle still hovered (the release hit-tests
 *  the hover before the layout has moved the shown child rows out from under
 *  the mouse, so TreeNode::open asks the canvas to hit-test it again after),
 *  and once the mouse has left, a settled frame draws nothing.
 *  TreeNode::open() / close() from code draw no surface and redraw that
 *  toggle's glyph alone among the toggles above it. A disabled toggle hides
 *  its surface and draws a Muted glyph.
 */
#include "headlessFixture.h"

#include <gweni/controls/treeControl.h>
#include <gweni/controls/treeNode.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <vector>

using namespace gweni;

namespace
{

namespace tn=skin::textured::tokyonight;

controls::OpenToggleButton *toggleOf(controls::TreeNode *node)
{
    for(controls::Base *child : node->getActualChildren())
    {
        if(controls::OpenToggleButton *toggle=gweni_cast<controls::OpenToggleButton>(child))
            return toggle;
    }
    return nullptr;
}

controls::Base *titleOf(controls::TreeNode *node)
{
    for(controls::Base *child : node->getActualChildren())
    {
        if(std::strcmp(child->getTypeName(), "TreeNodeText") == 0)
            return child;
    }
    return nullptr;
}

//! Records rounded rects, icons and hides, by slot.
class ToggleRecorder: public test::StubRenderer
{
public:
    struct Rounded
    {
        size_t id;
        Color top;
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

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color, int) override
    {
        rounded.push_back({primitiveId, top});
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        icons.push_back({primitiveId, rect, kind, direction, color, zIndex});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        shown.push_back(primitiveId);
    }

    bool drewRounded(size_t id) const
    {
        return std::any_of(rounded.begin(), rounded.end(), [id](const Rounded &r) { return r.id == id; });
    }

    bool drewRounded(size_t id, const Color &top) const
    {
        return std::any_of(rounded.begin(), rounded.end(),
            [&](const Rounded &r) { return r.id == id && r.top == top; });
    }

    bool hid(size_t id) const
    {
        return std::find(hidden.begin(), hidden.end(), id) != hidden.end();
    }

    //! The last icon drawn into the slot, or null.
    const Icon *lastIcon(size_t id) const
    {
        for(auto it=icons.rbegin(); it != icons.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    //! Every slot drawn into, shown or hidden since the last clear().
    std::vector<size_t> touched() const
    {
        std::vector<size_t> ids;

        for(const Rounded &r : rounded)
            ids.push_back(r.id);
        for(const Icon &icon : icons)
            ids.push_back(icon.id);
        ids.insert(ids.end(), hidden.begin(), hidden.end());
        ids.insert(ids.end(), shown.begin(), shown.end());
        return ids;
    }

    //! Every slot drawn into or shown (not only hidden) since the last clear().
    std::vector<size_t> drawn() const
    {
        std::vector<size_t> ids;

        for(const Rounded &r : rounded)
            ids.push_back(r.id);
        for(const Icon &icon : icons)
            ids.push_back(icon.id);
        ids.insert(ids.end(), shown.begin(), shown.end());
        return ids;
    }

    void clear()
    {
        rounded.clear();
        icons.clear();
        hidden.clear();
        shown.clear();
    }

    std::vector<Rounded> rounded;
    std::vector<Icon> icons;
    std::vector<size_t> hidden;
    std::vector<size_t> shown;
};

//! A TreeControl of three top-level nodes, the first and second with
//! children, on a real Canvas.
template<typename Renderer, typename Skin>
struct TreeScene
{
    Renderer renderer;
    Skin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::TreeControl *tree=nullptr;
    controls::TreeNode *first=nullptr;
    controls::TreeNode *second=nullptr;
    controls::TreeNode *leaf=nullptr;

    TreeScene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);

        tree=canvas->newChild<controls::TreeControl>();
        tree->setBounds(20, 20, 200, 200);

        first=tree->addNode("First");
        first->addNode("First child");
        second=tree->addNode("Second");
        second->addNode("Second child")->addNode("Grandchild");
        leaf=tree->addNode("Leaf");
        frame();
    }

    TreeScene(const TreeScene &)=delete;
    TreeScene &operator=(const TreeScene &)=delete;

    ~TreeScene()
    {
        // the hovered control and the focus are process-global
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        if constexpr(std::is_same_v<Renderer, ToggleRecorder>)
            renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void moveTo(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    void moveOnto(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        moveTo(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    //! Off the tree: the canvas, which draws nothing.
    void moveOff()
    {
        moveTo(350, 280);
    }
};

using LayoutScene=TreeScene<test::StubRenderer, test::StubSkin>;
using DrawScene=TreeScene<ToggleRecorder, skin::textured::TexturedSkin>;

const std::vector<size_t> &ids(controls::Base *control)
{
    return control->getSkinControl()->getPrimitives();
}

}//namespace

TEST_CASE("a TreeNode's toggle is a ToggleSize square centred on its title, hidden on a leaf")
{
    LayoutScene scene;

    for(controls::TreeNode *node : {scene.first, scene.second})
    {
        CAPTURE(node->getText());
        controls::OpenToggleButton *toggle=toggleOf(node);
        controls::Base *title=titleOf(node);

        REQUIRE(toggle != nullptr);
        REQUIRE(title != nullptr);
        CHECK(toggle->visible());
        CHECK(toggle->getWidth() == controls::TreeNode::ToggleSize);
        CHECK(toggle->getHeight() == controls::TreeNode::ToggleSize);
        CHECK(toggle->getX() == 0);
        CHECK(toggle->getY() == (title->getHeight()-controls::TreeNode::ToggleSize)/2);
        CHECK(title->getX() == 16);
    }

    REQUIRE(toggleOf(scene.leaf) != nullptr);
    CHECK(toggleOf(scene.leaf)->hidden());
    REQUIRE(titleOf(scene.leaf) != nullptr);
    CHECK(titleOf(scene.leaf)->getX() == 16);

    // opened, the toggle keeps its size and place
    scene.second->open();
    scene.frame();
    controls::OpenToggleButton *toggle=toggleOf(scene.second);
    CHECK(toggle->getWidth() == controls::TreeNode::ToggleSize);
    CHECK(toggle->getHeight() == controls::TreeNode::ToggleSize);
    CHECK(toggle->getY() == (titleOf(scene.second)->getHeight()-controls::TreeNode::ToggleSize)/2);
}

// The skin is never init()'d: the toggle draws tokyonight surfaces and an SDF
// icon, not atlas entries.
TEST_CASE("a closed node's toggle draws a right-pointing FgDark chevron at z+1 and no surface")
{
    // the constructor's frame is the first, still recorded
    DrawScene scene;

    controls::OpenToggleButton *toggle=toggleOf(scene.first);
    REQUIRE(toggle != nullptr);
    CHECK(dynamic_cast<const skin::textured::OpenToggleButton *>(toggle->getSkinControl()) != nullptr);
    REQUIRE(ids(toggle).size() == 2);

    CHECK(scene.renderer.hid(ids(toggle)[0]));
    CHECK(!scene.renderer.drewRounded(ids(toggle)[0]));

    const ToggleRecorder::Icon *glyph=scene.renderer.lastIcon(ids(toggle)[1]);
    REQUIRE(glyph != nullptr);
    CHECK(glyph->kind == 1);
    CHECK(glyph->direction == 3);
    CHECK((glyph->color == tn::FgDark));
    CHECK(glyph->zIndex == toggle->getZIndex()+1);

    // min(w,h)*6/10 square, centred in the toggle
    const Rect bounds=toggle->getRenderBounds();
    CHECK(glyph->rect.w == controls::TreeNode::ToggleSize*6/10);
    CHECK(glyph->rect.h == glyph->rect.w);
    CHECK(std::abs((glyph->rect.x*2+glyph->rect.w)-(bounds.x*2+bounds.w)) <= 1);
    CHECK(std::abs((glyph->rect.y*2+glyph->rect.h)-(bounds.y*2+bounds.h)) <= 1);

    // a settled frame draws nothing
    scene.frame();
    CHECK(scene.renderer.touched().empty());
}

TEST_CASE("hovering a toggle draws ButtonHover and an Fg glyph in its own slots, and moving off gives the rest look back")
{
    DrawScene scene;
    scene.moveOff();
    scene.frame();
    REQUIRE(scene.renderer.touched().empty());

    controls::OpenToggleButton *toggle=toggleOf(scene.first);
    const std::vector<size_t> own=ids(toggle);

    scene.moveOnto(toggle);
    CHECK(toggle->isHovered());
    CHECK(scene.renderer.drewRounded(own[0], tn::ButtonHover.top));
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK((scene.renderer.lastIcon(own[1])->color == tn::Fg));
    CHECK(scene.renderer.lastIcon(own[1])->direction == 3);
    for(size_t id : scene.renderer.drawn())
    {
        CAPTURE(id);
        CHECK(std::find(own.begin(), own.end(), id) != own.end());
    }

    scene.frame();
    CHECK(scene.renderer.touched().empty());

    scene.moveOff();
    CHECK(!toggle->isHovered());
    CHECK(scene.renderer.hid(own[0]));
    CHECK(!scene.renderer.drewRounded(own[0]));
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK((scene.renderer.lastIcon(own[1])->color == tn::FgDark));
    for(size_t id : scene.renderer.drawn())
    {
        CAPTURE(id);
        CHECK(std::find(own.begin(), own.end(), id) != own.end());
    }

    scene.frame();
    CHECK(scene.renderer.touched().empty());
}

TEST_CASE("clicking a toggle opens its node and turns the chevron down")
{
    DrawScene scene;
    scene.moveOff();

    controls::OpenToggleButton *toggle=toggleOf(scene.first);
    const std::vector<size_t> own=ids(toggle);
    REQUIRE(!toggle->getToggleState());

    scene.moveOnto(toggle);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(toggle->isDepressed());
    CHECK(scene.renderer.drewRounded(own[0], tn::ButtonDown.top));

    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(toggle->getToggleState());
    // hit-tested again after the layout, not against the child rows where
    // they were before it
    CHECK(toggle->isHovered());
    // the node's child row is laid out and shown
    controls::TreeNode *child=gweni_cast<controls::TreeNode>(scene.first->GetChildNodes().front());
    REQUIRE(child != nullptr);
    CHECK(child->visible());
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(own[1])->direction == 0);
    CHECK((scene.renderer.lastIcon(own[1])->color == tn::Fg));

    // once the mouse has left, the rest look, pointing down, then nothing
    scene.moveOff();
    CHECK(scene.renderer.hid(own[0]));
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(own[1])->direction == 0);
    CHECK((scene.renderer.lastIcon(own[1])->color == tn::FgDark));
    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // a second click closes it again
    scene.moveOnto(toggle);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(!toggle->getToggleState());
    CHECK(!child->visible());
    CHECK(scene.renderer.lastIcon(own[1])->direction == 3);
}

TEST_CASE("TreeNode::open and close from code redraw only that toggle's glyph")
{
    DrawScene scene;
    scene.moveOff();
    scene.frame();
    REQUIRE(scene.renderer.touched().empty());

    controls::OpenToggleButton *above=toggleOf(scene.first);
    controls::OpenToggleButton *toggle=toggleOf(scene.second);
    const std::vector<size_t> own=ids(toggle);

    scene.second->open();
    scene.frame();
    CHECK(toggle->getToggleState());
    CHECK(!scene.renderer.drewRounded(own[0]));
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(own[1])->direction == 0);
    CHECK((scene.renderer.lastIcon(own[1])->color == tn::FgDark));
    // the node above does not move, so its toggle is not touched
    const std::vector<size_t> opened=scene.renderer.touched();
    for(size_t id : ids(above))
    {
        CAPTURE(id);
        CHECK(std::find(opened.begin(), opened.end(), id) == opened.end());
    }

    scene.frame();
    CHECK(scene.renderer.touched().empty());

    scene.second->close();
    scene.frame();
    CHECK(!toggle->getToggleState());
    CHECK(!scene.renderer.drewRounded(own[0]));
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(own[1])->direction == 3);
    const std::vector<size_t> touched=scene.renderer.touched();
    for(size_t id : ids(above))
        CHECK(std::find(touched.begin(), touched.end(), id) == touched.end());

    // the same state again changes nothing
    scene.second->close();
    scene.frame();
    CHECK(scene.renderer.touched().empty());
}

TEST_CASE("a disabled toggle hides its surface and draws a Muted chevron")
{
    DrawScene scene;
    scene.moveOff();

    controls::OpenToggleButton *toggle=toggleOf(scene.first);
    const std::vector<size_t> own=ids(toggle);

    toggle->setDisabled(true);
    scene.frame();
    CHECK(scene.renderer.hid(own[0]));
    CHECK(!scene.renderer.drewRounded(own[0]));
    REQUIRE(scene.renderer.lastIcon(own[1]) != nullptr);
    CHECK((scene.renderer.lastIcon(own[1])->color == tn::Muted));
}
