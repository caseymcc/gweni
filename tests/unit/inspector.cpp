/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni::tools::Inspector on a real Canvas, with a StubSkin whose Controls
 *  draw a rect into each of their slots whenever their control changes, so
 *  the StubRenderer counts every changed control's slots as written:
 *  - its tree's top-level nodes are the canvas's children less the inspector,
 *    in order; a node opened shows its control's actual children; a child
 *    added, deleted or moved to another parent is shown there after the next
 *    frame, an opened node staying open;
 *  - a selected control's details name its type and id, its z and layers,
 *    its primitive ids and its pending state changes (Background before the
 *    frame renders it, none after); selecting a node selects its control, and
 *    a deleted selected control leaves nothing selected;
 *  - over a settled app it settles: after a few frames it shows 0 slots
 *    written and 0 controls rendered, sets no label, and the frames write no
 *    slot at all, its own included; an app change shows that frame's slots
 *    written (the app's alone, its own taken out) on the next frame, and 0
 *    again on the one after, and the frame after that writes nothing;
 *  - its highlight box is the canvas's last child, kept there when the app
 *    adds a child; it frames the selected control (its rect grown by
 *    InspectorHighlight::Edge), follows the hovered node's control and goes
 *    back to the selected one, hides when its target is hidden, and is drawn
 *    only on a frame its rect or visibility changed;
 *  - a control selected whose node was never made opens the way down to it;
 *  - closed, it gives back every primitive and text id it took.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/tools/inspector.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace gweni;
using tools::Inspector;
using tools::InspectorNode;

namespace
{

//! A StubControl that draws a rect into each of its slots when its control
//! has a state change, so the StubRenderer counts them as written.
class DrawingControl: public test::StubControl
{
public:
    using StubControl::StubControl;

    void update(renderer::Base *renderer, controls::Base *control) override
    {
        if(control->getStateChange() == controls::StateChange_Nothing)
            return;
        for(size_t id : getPrimitives())
            renderer->drawFilledRect(id, control->getRenderBounds(), control->getZIndex());
    }
};

class DrawingSkin: public test::StubSkin
{
public:
    using StubSkin::StubSkin;

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        const bool takesText=typeName == test::StubText::getTypeNameStatic();

        return std::make_unique<DrawingControl>(this, takesText, std::move(typeName));
    }
};

struct Scene
{
    test::StubRenderer renderer;
    DrawingSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    controls::Base *other;
    std::vector<controls::Label *> labels;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1024, 768);
        panel=canvas->newChild<controls::Base>("panel");
        panel->setBounds(10, 10, 300, 200);
        for(int i=0; i < 3; ++i)
        {
            controls::Label *label=panel->newChild<controls::Label>();

            label->setText(std::string("label ")+char('a'+i));
            label->setPos(0, i*20);
            labels.push_back(label);
        }
        other=canvas->newChild<controls::Base>("other");
        other->setBounds(10, 300, 300, 100);
        frames(3);
    }

    ~Scene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void frames(int count)
    {
        for(int i=0; i < count; ++i)
            frame();
    }
};

//! The targets of node's child nodes (the tree's top level for null).
std::vector<controls::Base *> targets(Inspector *inspector, controls::TreeNode *node=nullptr)
{
    std::vector<controls::Base *> out;

    for(controls::Base *child : (node?node:inspector->tree())->GetChildNodes())
    {
        if(InspectorNode *inspectorNode=gweni_cast<InspectorNode>(child))
            out.push_back(inspectorNode->target());
    }
    return out;
}

std::vector<controls::Base *> childrenLessInspector(controls::Base *parent)
{
    std::vector<controls::Base *> out;

    for(controls::Base *child : parent->getActualChildren())
    {
        if(!gweni_cast<Inspector>(child) && !gweni_cast<tools::InspectorHighlight>(child))
            out.push_back(child);
    }
    return out;
}

bool contains(const String &line, const String &part)
{
    return line.find(part) != String::npos;
}

}//namespace

TEST_CASE("Inspector: the tree follows the canvas's tree")
{
    Scene scene;
    Inspector *inspector=Inspector::open(scene.canvas);

    REQUIRE(inspector);
    CHECK(Inspector::find(scene.canvas) == inspector);
    CHECK(Inspector::open(scene.canvas) == inspector);
    scene.frames(2);

    CHECK(targets(inspector) == childrenLessInspector(scene.canvas));

    InspectorNode *panelNode=inspector->nodeFor(scene.panel);

    REQUIRE(panelNode);
    // children not made until it is opened: one placeholder shows the toggle
    CHECK_FALSE(panelNode->populated());
    CHECK(panelNode->GetChildNodes().size() == 1);
    CHECK(targets(inspector, panelNode).empty());

    panelNode->open();
    scene.frame();
    CHECK(panelNode->populated());
    CHECK(targets(inspector, panelNode) == childrenLessInspector(scene.panel));

    SUBCASE("a child added")
    {
        controls::Label *added=scene.panel->newChild<controls::Label>();

        added->setText("added");
        scene.frame();
        CHECK(targets(inspector, inspector->nodeFor(scene.panel)) == childrenLessInspector(scene.panel));
        CHECK(inspector->nodeFor(added));
        // the panel's node stayed open through the rebuild
        CHECK(inspector->nodeFor(scene.panel)->isOpen());
    }

    SUBCASE("a child deleted")
    {
        delete scene.labels[1];
        scene.frame();
        CHECK(targets(inspector, inspector->nodeFor(scene.panel)) == childrenLessInspector(scene.panel));
    }

    SUBCASE("a child moved to another parent")
    {
        scene.labels[2]->setParent(scene.other);
        scene.frame();
        CHECK(targets(inspector, inspector->nodeFor(scene.panel)) == childrenLessInspector(scene.panel));

        InspectorNode *otherNode=inspector->nodeFor(scene.other);

        REQUIRE(otherNode);
        otherNode->open();
        scene.frame();
        CHECK(targets(inspector, otherNode) == childrenLessInspector(scene.other));
        CHECK(inspector->nodeFor(scene.labels[2])->getParent() != nullptr);
    }

    SUBCASE("a top-level control added")
    {
        controls::Base *top=scene.canvas->newChild<controls::Base>("top");

        scene.frame();
        CHECK(targets(inspector) == childrenLessInspector(scene.canvas));
        CHECK(inspector->nodeFor(top));
        // the opened panel node was made again, open
        CHECK(inspector->nodeFor(scene.panel)->isOpen());
        CHECK(targets(inspector, inspector->nodeFor(scene.panel)) == childrenLessInspector(scene.panel));
    }
}

TEST_CASE("Inspector: a selected control's details")
{
    Scene scene;
    Inspector *inspector=Inspector::open(scene.canvas);
    controls::Label *label=scene.labels[0];

    scene.frames(2);
    CHECK(inspector->detailLines()[Inspector::NameLine] == "no control selected");

    inspector->select(label);
    CHECK(inspector->selected() == label);
    scene.frame();

    const std::vector<String> lines=inspector->detailLines();

    CHECK(contains(lines[Inspector::NameLine], "Label"));
    CHECK(contains(lines[Inspector::NameLine], format("#%zu", label->getControlId())));
    CHECK(contains(lines[Inspector::ZLine], format("z %d (", label->getZIndex())));
    REQUIRE(label->getSkinControl());
    for(size_t id : label->getSkinControl()->getPrimitives())
        CHECK(contains(lines[Inspector::PrimitivesLine], format(" %zu", id)));
    CHECK(lines[Inspector::PendingLine] == "pending: none");

    // a state change not rendered yet is shown pending, then gone once rendered
    label->addStateChange(controls::StateChange_Background);
    inspector->refresh();
    CHECK(inspector->detailLines()[Inspector::PendingLine] == "pending: Background");
    scene.frames(2);
    CHECK(inspector->detailLines()[Inspector::PendingLine] == "pending: none");

    // selecting a node selects its control
    InspectorNode *otherNode=inspector->nodeFor(scene.other);

    REQUIRE(otherNode);
    otherNode->setSelected(true);
    CHECK(inspector->selected() == scene.other);

    // a deleted selected control leaves nothing selected
    delete scene.other;
    scene.other=nullptr;
    scene.frame();
    CHECK(inspector->selected() == nullptr);
    CHECK(inspector->detailLines()[Inspector::NameLine] == "no control selected");
}

TEST_CASE("Inspector: it settles over a settled app and shows the app's writes alone")
{
    Scene scene;
    Inspector *inspector=Inspector::open(scene.canvas);

    inspector->select(scene.labels[1]);
    inspector->nodeFor(scene.panel)->open();
    scene.frames(6);

    const size_t sets=inspector->textSets();
    const String noSlots="slots written: 0 primitive, 0 text";

    for(int i=0; i < 3; ++i)
    {
        scene.frame();
        CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
        CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    }
    CHECK(inspector->textSets() == sets);
    CHECK(inspector->counterLines()[Inspector::SlotsLine] == noSlots);
    CHECK(inspector->counterLines()[Inspector::RenderLine] == "render: 0 rendered, 0 changed");
    CHECK(contains(inspector->counterLines()[Inspector::LayoutLine], "layout: 0 passes"));

    // the app changes: that frame writes the app's slots...
    scene.labels[0]->addStateChange(controls::StateChange_Background);
    scene.frame();

    const size_t appWritten=scene.renderer.frameStats().primitiveSlotsWritten;

    REQUIRE(appWritten > 0);
    // ...which the next frame shows, while it writes its own
    scene.frame();
    CHECK(inspector->counterLines()[Inspector::SlotsLine]
        == format("slots written: %zu primitive, 0 text", appWritten));
    CHECK(inspector->counterLines()[Inspector::RenderLine] == "render: 1 rendered, 1 changed");
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten > 0);
    // then 0 again: what it wrote itself is taken out
    scene.frame();
    CHECK(inspector->counterLines()[Inspector::SlotsLine] == noSlots);
    CHECK(inspector->counterLines()[Inspector::RenderLine] == "render: 0 rendered, 0 changed");
    // and the frame after writes nothing at all
    const size_t settledSets=inspector->textSets();

    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(inspector->textSets() == settledSets);
    CHECK(inspector->counterLines()[Inspector::SlotsLine] == noSlots);
}

TEST_CASE("Inspector: closed, it gives back every id it took")
{
    Scene scene;
    const test::IdBankCounts before=test::IdBankCounts::now();
    const size_t livePrimitives=scene.renderer.livePrimitives.size();
    const size_t liveTexts=scene.renderer.liveTexts.size();

    Inspector *inspector=Inspector::open(scene.canvas);

    scene.frames(2);
    inspector->nodeFor(scene.panel)->open();
    inspector->select(scene.labels[0]);
    scene.frames(2);
    CHECK(test::IdBankCounts::now().livePrimitives() > before.livePrimitives());

    CHECK_FALSE(Inspector::toggle(scene.canvas));
    scene.frames(2);
    CHECK(Inspector::find(scene.canvas) == nullptr);
    CHECK(test::IdBankCounts::now().livePrimitives() == before.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == before.liveTexts());
    CHECK(scene.renderer.livePrimitives.size() == livePrimitives);
    CHECK(scene.renderer.liveTexts.size() == liveTexts);
    CHECK(scene.renderer.doubleReleased == 0);

    // and it opens again
    CHECK(Inspector::toggle(scene.canvas));
    CHECK(Inspector::find(scene.canvas));
}

TEST_CASE("Inspector: the highlight box frames the selected or the hovered control")
{
    Scene scene;
    Inspector *inspector=Inspector::open(scene.canvas);
    tools::InspectorHighlight *highlight=inspector->highlight();
    controls::Label *label=scene.labels[1];

    REQUIRE(highlight);
    scene.frames(2);
    CHECK(scene.canvas->getActualChildren().back() == highlight);
    CHECK(highlight->hidden());

    // the label's node is made on the way: its panel's node opened
    inspector->select(label);
    REQUIRE(inspector->nodeFor(label));
    CHECK(inspector->nodeFor(scene.panel)->isOpen());
    scene.frames(2);

    const Point at=label->localPosToCanvas();
    const int edge=tools::InspectorHighlight::Edge;

    CHECK_FALSE(highlight->hidden());
    CHECK(inspector->highlightTarget() == label);
    CHECK(highlight->getBounds().x == at.x-edge);
    CHECK(highlight->getBounds().y == at.y-edge);
    CHECK(highlight->getBounds().w == label->getWidth()+2*edge);
    CHECK(highlight->getBounds().h == label->getHeight()+2*edge);

    // settled: not drawn again
    scene.frames(3);
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);

    // a new top-level control: the highlight stays the last child
    scene.canvas->newChild<controls::Base>("late");
    scene.frames(2);
    CHECK(scene.canvas->getActualChildren().back() == highlight);

    // the mouse on another node's row: that node's control, then back
    HoveredControl=inspector->nodeFor(scene.other);
    scene.frames(1);
    CHECK(inspector->highlightTarget() == scene.other);
    CHECK(highlight->getBounds().y == scene.other->localPosToCanvas().y-edge);
    HoveredControl=nullptr;
    scene.frames(1);
    CHECK(inspector->highlightTarget() == label);

    // its target hidden: hidden too
    label->hide();
    scene.frames(2);
    CHECK(highlight->hidden());
    label->show();
    scene.frames(2);
    CHECK_FALSE(highlight->hidden());

    // its ids are the inspector's own: settled frames show 0 written
    scene.frames(3);
    CHECK(inspector->counterLines()[Inspector::SlotsLine] == "slots written: 0 primitive, 0 text");
    std::vector<size_t> primitives, texts;

    inspector->ownIds(primitives, texts);
    for(size_t id : highlight->getSkinControl()->getPrimitives())
        CHECK(std::find(primitives.begin(), primitives.end(), id) != primitives.end());

    // closed: the highlight goes with it
    Inspector::close(scene.canvas);
    scene.frames(2);
    for(controls::Base *child : scene.canvas->getActualChildren())
        CHECK_FALSE(gweni_cast<tools::InspectorHighlight>(child));
}
