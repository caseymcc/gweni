/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's tree connectors and property-tree rail (#219), through
 *  the real TexturedSkin lookup:
 *  - A TreeNode gets skin::textured::TreeNode, a PropertyTreeNode its own
 *    PropertyTreeNode renderer and a TreeControl the Tree panel, never the
 *    TreeNode renderer it would find through its parent type.
 *  - A TreeNode's two slots are icons of kind 6 (a rule) in Tree.Lines at the
 *    node's z: [0] the stub, hidden on a root, from the parent's line to one
 *    unit short of the toggle's chevron (of the title on a node without a
 *    toggle), [1] on an open node with child nodes, the line down from under
 *    its toggle through its last child's stub row, hidden otherwise. A stub
 *    starts on its parent's line and the line ends on its last stub's row.
 *  - The lines follow the tree in the next frame: expanding a child that is
 *    not the last makes the parent's line longer (the parent grows, which marks
 *    it), collapsing hides it, a node added below an open node makes its line
 *    longer, and a leaf's first child shortens its stub to its new toggle.
 *  - A PropertyTreeNode draws no lines but its rail, two filled rects in
 *    Properties.Border: the column down its left as wide as its properties are
 *    indented, and the band behind its title, as high as its title and margin.
 *  A node a TreeControl holds without addNode() (so not marked a root) is drawn
 *  as one. A frame with one node dirty touches its slots only, a settled frame
 *  none.
 */
#include "headlessFixture.h"

#include <gweni/controls/propertyTree.h>
#include <gweni/controls/treeControl.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

//! Records the slots each frame draws into, shows or hides.
class SlotRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
        int kind;      // -1 for a filled rect
        int direction;
        Color color;
        int zIndex;
    };

    void setDrawColor(Color color) override
    {
        m_color=color;
    }

    void drawFilledRect(size_t primitiveId, Rect rect, int zIndex) override
    {
        draws.push_back({primitiveId, rect, -1, 0, m_color, zIndex});
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color, Color, int zIndex) override
    {
        draws.push_back({primitiveId, rect, -2, 0, top, zIndex});
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        draws.push_back({primitiveId, rect, kind, direction, color, zIndex});
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        shown.push_back(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    //! The last draw into the slot this frame, or null.
    const Draw *last(size_t id) const
    {
        for(auto it=draws.rbegin(); it != draws.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    bool hid(size_t id) const
    {
        return std::find(hidden.begin(), hidden.end(), id) != hidden.end();
    }

    std::vector<size_t> touched() const
    {
        std::vector<size_t> ids;

        for(const Draw &draw : draws)
            ids.push_back(draw.id);
        ids.insert(ids.end(), shown.begin(), shown.end());
        ids.insert(ids.end(), hidden.begin(), hidden.end());
        return ids;
    }

    void clear()
    {
        draws.clear();
        shown.clear();
        hidden.clear();
    }

    std::vector<Draw> draws;
    std::vector<size_t> shown;
    std::vector<size_t> hidden;

private:
    Color m_color;
};

struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
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

    static const std::vector<size_t> &ids(const controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    bool touchedOnly(std::initializer_list<const controls::Base *> controls) const
    {
        std::vector<size_t> own;

        for(const controls::Base *control : controls)
            own.insert(own.end(), ids(control).begin(), ids(control).end());
        for(size_t id : renderer.touched())
        {
            if(std::find(own.begin(), own.end(), id) == own.end())
                return false;
        }
        return true;
    }

    //! The canvas offset of the coordinates a control's render bounds are in
    //! (its actual parent's).
    static Point renderOrigin(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x-control->getX(), at.y-control->getY());
    }

    //! The last draw into one of the node's slots this frame, or null.
    const SlotRecorder::Draw *drawOf(const controls::Base *node, size_t slot) const
    {
        return renderer.last(ids(node)[slot]);
    }
};

//! root "A" with children a1, a2 (which has a child of its own) and a3, and a
//! root leaf "B".
struct TreeScene: Scene
{
    controls::TreeControl *tree=nullptr;
    controls::TreeNode *a=nullptr;
    controls::TreeNode *a1=nullptr;
    controls::TreeNode *a2=nullptr;
    controls::TreeNode *a2x=nullptr;
    controls::TreeNode *a3=nullptr;
    controls::TreeNode *b=nullptr;

    TreeScene()
    {
        tree=canvas->newChild<controls::TreeControl>();
        tree->setBounds(10, 10, 300, 400);
        a=tree->addNode("A");
        a1=a->addNode("a1");
        a2=a->addNode("a2");
        a2x=a2->addNode("a2x");
        a3=a->addNode("a3");
        b=tree->addNode("B");
        a->open();
    }

    //! The canvas x of the parent's line, from its slot [1].
    int lineX(controls::TreeNode *node) const
    {
        const SlotRecorder::Draw *line=drawOf(node, 1);

        REQUIRE(line != nullptr);
        return renderOrigin(node).x+line->rect.x+line->rect.w/2;
    }

    //! The canvas y one past the parent's line's last row.
    int lineBottom(controls::TreeNode *node) const
    {
        const SlotRecorder::Draw *line=drawOf(node, 1);

        REQUIRE(line != nullptr);
        return renderOrigin(node).y+line->rect.y+line->rect.h;
    }

    //! The canvas y of a node's stub, from its slot [0].
    int stubY(controls::TreeNode *node) const
    {
        const SlotRecorder::Draw *stub=drawOf(node, 0);

        REQUIRE(stub != nullptr);
        return renderOrigin(node).y+stub->rect.y+stub->rect.h/2;
    }
};

}//namespace

TEST_CASE("the textured skin gives TreeNode and PropertyTreeNode renderers of their own, and TreeControl keeps its panel")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        TreeScene scene;
        controls::PropertyTree *properties=scene.canvas->newChild<controls::PropertyTree>();
        properties->setBounds(320, 10, 300, 400);
        properties->add("Category");
        controls::Base *category=properties->GetChildNodes().front();

        REQUIRE(scene.a->getSkinControl() != nullptr);
        CHECK(dynamic_cast<const skin::textured::TreeNode *>(scene.a->getSkinControl()) != nullptr);
        CHECK(dynamic_cast<const skin::textured::TreeNode *>(scene.a1->getSkinControl()) != nullptr);

        REQUIRE(category->getSkinControl() != nullptr);
        CHECK(category->getTypeName() == std::string("PropertyTreeNode"));
        CHECK(dynamic_cast<const skin::textured::PropertyTreeNode *>(category->getSkinControl()) != nullptr);
        CHECK(dynamic_cast<const skin::textured::TreeNode *>(category->getSkinControl()) == nullptr);

        for(controls::Base *control : {static_cast<controls::Base *>(scene.tree), static_cast<controls::Base *>(properties)})
        {
            REQUIRE(control->getSkinControl() != nullptr);
            CHECK(dynamic_cast<const skin::textured::Tree *>(control->getSkinControl()) != nullptr);
            CHECK(dynamic_cast<const skin::textured::TreeNode *>(control->getSkinControl()) == nullptr);
        }

        for(controls::Base *control : {static_cast<controls::Base *>(scene.a), category})
            CHECK(dynamic_cast<const skin::textured::Base *>(control->getSkinControl()) == nullptr);
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("a TreeNode draws a stub unless it is a root, and a line down to its last child while it is open")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        TreeScene scene;
        scene.frame();

        for(controls::TreeNode *node : {scene.a, scene.a1, scene.a2, scene.a3, scene.b})
            REQUIRE(Scene::ids(node).size() == 2);

        const Color lines=scene.skin.Colors.Tree.Lines;

        // roots: no stub; the open one a line, the leaf none
        CHECK(scene.renderer.hid(Scene::ids(scene.a)[0]));
        CHECK(scene.renderer.hid(Scene::ids(scene.b)[0]));
        CHECK(scene.renderer.hid(Scene::ids(scene.b)[1]));

        const SlotRecorder::Draw *line=scene.drawOf(scene.a, 1);
        REQUIRE(line != nullptr);
        CHECK(line->kind == 6);
        CHECK(line->direction == 1);
        CHECK((line->color == lines));
        CHECK(line->zIndex == scene.a->getZIndex());
        // under its rows
        CHECK(line->zIndex < scene.a1->getZIndex());

        // children: a stub each, a horizontal rule from the parent's line
        for(controls::TreeNode *node : {scene.a1, scene.a2, scene.a3})
        {
            const SlotRecorder::Draw *stub=scene.drawOf(node, 0);
            REQUIRE(stub != nullptr);
            CHECK(stub->kind == 6);
            CHECK(stub->direction == 0);
            CHECK((stub->color == lines));
            CHECK(stub->zIndex == node->getZIndex());
            // it starts on the parent's line
            CHECK(Scene::renderOrigin(node).x+stub->rect.x == scene.lineX(scene.a));
            // and ends short of the node's toggle or title, inside the node
            const int end=stub->rect.x+stub->rect.w;
            CHECK(end > node->getX());
            CHECK(end < node->getX()+(node->hasToggle()?node->getToggleBounds().x+3:node->getTitleBounds().x));
        }

        // a2 has a child, so a toggle: its stub stops before it; a1 and a3 reach
        // further, to their titles
        CHECK(scene.a2->hasToggle());
        CHECK_FALSE(scene.a1->hasToggle());
        CHECK(scene.drawOf(scene.a2, 0)->rect.w < scene.drawOf(scene.a1, 0)->rect.w);

        // the closed a2 and the leaves draw no line
        CHECK_FALSE(scene.a2->isOpen());
        for(controls::TreeNode *node : {scene.a1, scene.a2, scene.a3})
            CHECK(scene.renderer.hid(Scene::ids(node)[1]));

        // the line runs through the last child's stub row and no further
        CHECK(scene.lineBottom(scene.a) == scene.stubY(scene.a3)+1);

        // settled: nothing drawn
        scene.frame();
        CHECK(scene.renderer.touched().empty());

        // one node dirty: only its slots
        scene.a1->redraw();
        scene.frame();
        CHECK_FALSE(scene.renderer.touched().empty());
        CHECK(scene.touchedOnly({scene.a1}));

        // hidden, a node's lines are hidden
        scene.tree->setHidden(true);
        scene.frame();
        CHECK(scene.renderer.hid(Scene::ids(scene.a)[1]));
        CHECK(scene.renderer.hid(Scene::ids(scene.a1)[0]));
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("a TreeNode's line follows an expand, a collapse and an added node in the next frame")
{
    TreeScene scene;
    scene.frame();
    const int bottom=scene.lineBottom(scene.a);

    SUBCASE("expanding a child that is not the last makes the parent's line longer")
    {
        scene.a2->open();
        scene.frame();
        // the parent was redrawn because it grew; its line reaches a3's stub,
        // which moved down by a2's child row
        REQUIRE(scene.drawOf(scene.a, 1) != nullptr);
        CHECK(scene.lineBottom(scene.a) > bottom);
        CHECK(scene.lineBottom(scene.a) == scene.stubY(scene.a3)+1);
        // a2 now draws a line of its own, to a2x
        REQUIRE(scene.drawOf(scene.a2, 1) != nullptr);
        CHECK(scene.lineBottom(scene.a2) == scene.stubY(scene.a2x)+1);

        scene.frame();
        CHECK(scene.renderer.touched().empty());

        // and collapsing it gives the shorter line back
        scene.a2->close();
        scene.frame();
        CHECK(scene.renderer.hid(Scene::ids(scene.a2)[1]));
        CHECK(scene.lineBottom(scene.a) == bottom);
    }

    SUBCASE("collapsing a node hides its line, and expanding it shows it again")
    {
        scene.a->close();
        scene.frame();
        CHECK(scene.renderer.hid(Scene::ids(scene.a)[1]));

        scene.a->open();
        scene.frame();
        CHECK(scene.lineBottom(scene.a) == bottom);
    }

    SUBCASE("a node added below an open node makes its line longer")
    {
        controls::TreeNode *a4=scene.a->addNode("a4");
        scene.frame();
        REQUIRE(Scene::ids(a4).size() == 2);
        CHECK(scene.lineBottom(scene.a) > bottom);
        CHECK(scene.lineBottom(scene.a) == scene.stubY(a4)+1);
    }

    SUBCASE("a leaf's first child gives it a toggle, and its stub stops short of it")
    {
        const int width=scene.drawOf(scene.a1, 0)->rect.w;

        scene.a1->addNode("a1x");
        scene.frame();
        // closed, so a1's height is unchanged: the toggle appearing marks it
        REQUIRE(scene.a1->hasToggle());
        REQUIRE(scene.drawOf(scene.a1, 0) != nullptr);
        CHECK(scene.drawOf(scene.a1, 0)->rect.w < width);
        CHECK(scene.renderer.hid(Scene::ids(scene.a1)[1]));
    }
}

TEST_CASE("a PropertyTreeNode draws its rail in Properties.Border and no tree lines")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        controls::PropertyTree *properties=scene.canvas->newChild<controls::PropertyTree>();
        properties->setBounds(10, 10, 300, 400);
        properties->add("First")->add("Name");
        properties->add("Second")->add("Other");
        properties->expandAll();
        scene.frame();

        for(controls::Base *control : properties->GetChildNodes())
        {
            controls::TreeNode *category=gweni_cast<controls::TreeNode>(control);
            REQUIRE(category != nullptr);
            REQUIRE(Scene::ids(category).size() == 2);

            const Color border=scene.skin.Colors.Properties.Border;
            const SlotRecorder::Draw *column=scene.drawOf(category, 0);
            const SlotRecorder::Draw *band=scene.drawOf(category, 1);
            REQUIRE(column != nullptr);
            REQUIRE(band != nullptr);

            for(const SlotRecorder::Draw *rail : {column, band})
            {
                CHECK(rail->kind == -1);
                CHECK((rail->color == border));
                CHECK(rail->zIndex == category->getZIndex());
            }

            // the column: the node's left, as wide as its properties' indent,
            // its whole height
            CHECK(column->rect.x == category->getX());
            CHECK(column->rect.y == category->getY());
            CHECK(column->rect.w == category->childIndent());
            CHECK(column->rect.w > 0);
            CHECK(column->rect.h == category->getHeight());
            // the band: right of the column, behind the title and its margin
            CHECK(band->rect.x == category->getX()+column->rect.w);
            CHECK(band->rect.y == category->getY());
            CHECK(band->rect.w == category->getWidth()-column->rect.w);
            CHECK(band->rect.h == category->childPanelY());
            CHECK(band->rect.h >= category->getTitleBounds().h);
        }

        // settled: nothing drawn
        scene.frame();
        CHECK(scene.renderer.touched().empty());
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("a TreeNode placed in a TreeControl without addNode() draws no stub, as a root")
{
    Scene scene;
    controls::TreeControl *tree=scene.canvas->newChild<controls::TreeControl>();
    tree->setBounds(10, 10, 300, 400);
    controls::TreeNode *node=tree->newChild<controls::TreeNode>();
    node->setText("Placed");
    node->setDock(DockPosition::Top);
    REQUIRE_FALSE(node->isRoot());
    // a TreeControl has no toggle or title of its own
    CHECK(tree->getToggleBounds().w == 0);
    CHECK(tree->getTitleBounds().w == 0);

    scene.frame();
    REQUIRE(Scene::ids(node).size() == 2);
    CHECK(scene.renderer.hid(Scene::ids(node)[0]));
    CHECK(scene.renderer.hid(Scene::ids(node)[1]));
}
