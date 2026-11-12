/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Only the app's calls are requests; bounds the library computes are not.
 *  A control places the children it owns (Placement::Owner) through
 *  Base::placeChild, which records no request: the generic arrange leaves such
 *  a child alone and it does not count in its parent's preferred size. So the
 *  preferred sizes of a CrossSplitter, a Properties and a slider come from
 *  requests and content only, and are the same after a trip to another canvas
 *  size as on a fresh canvas. A request made while a pass runs is counted in
 *  LayoutStats::requests.
 *
 *  A wrapped Text asks for the width the app asked its parent to be, less the
 *  parent's padding, or with no such request for its longest word; its height
 *  is that of its lines at the width it is given.
 *
 *  Outside a pass too, what the library writes is computed: a slider's value,
 *  a click or a drag on its thumb and a drag of a CrossSplitter bar leave
 *  every request as it was. A window dragged by its title bar is where the
 *  user put it, a request.
 *
 *  See docs/layout.md, "Requested and computed bounds". A Placement::Owner
 *  child is placed by placeChild alone (not docked, its request untouched) and
 *  counted again once given back; a request made from a layout() is counted in
 *  LayoutStats::requests / requestedType. The CrossSplitter holds wrapped
 *  Labels, and of the two Properties one is docked and one in an unsized box:
 *  they list the same bounds and preferred sizes after a trip to 1024x768 as
 *  before it and as a fresh canvas, recording no request. A Properties row
 *  asks for the label's margin, the split and a value column, the wider of
 *  Properties::getValueWidth() (100, changed by setValueWidth) and its
 *  property's preferred width, so one in an unsized box gives every value that
 *  width and an app's setWidth() on a property is no minimum; a ColorSelector
 *  row fits its "255 0 0" beside its swatch (a TextBox's string counts beside
 *  its children docked Left or Right). A ComboBox asks for its widest item's
 *  text with the text padding and the chevron column, measured again when an
 *  item is added, deleted, renamed or cleared and never when one is selected,
 *  and a property::ComboBox row's value column fits it. A PropertyTree at its
 *  preferred width fits every node's Properties, and keeps that width when a
 *  node is closed: a closed TreeNode's rows still count in its width, so a
 *  tree built closed is as wide as one opened and closed. Its rows are
 *  measured again only when one changed: relabelling a closed node enters as
 *  many controls at 100 rows as at 2000, and a row changed, added or deleted
 *  under it widens or narrows the tree as layout verification's full pass
 *  finds. A VerticalSlider
 *  asks for its request at value 0 and 1 and takes a smaller layout cell in
 *  one pass.
 */
#include "headlessFixture.h"

#include <gweni/controls/comboBox.h>
#include <gweni/controls/crossSplitter.h>
#include <gweni/controls/label.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/controls/properties.h>
#include <gweni/controls/property/colorSelector.h>
#include <gweni/controls/property/comboBox.h>
#include <gweni/controls/propertyTree.h>
#include <gweni/controls/text.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/verticalSlider.h>
#include <gweni/controls/windowControl.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

std::string rectText(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

//! One line per control, depth-first: type, bounds and preferred size, so two
//! trees compare as text and a failure shows the line that differs.
void listTree(controls::Base *control, int depth, std::vector<std::string> &out)
{
    const Size &p=control->getPreferredSize();

    out.push_back(std::string(static_cast<size_t>(depth)*2, ' ')+control->getTypeName()+
        " bounds="+rectText(control->getBounds())+" pref=("+std::to_string(p.width)+", "+
        std::to_string(p.height)+")");

    for(auto &&child : control->getChildren())
        listTree(child, depth+1, out);
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    //! Requests counted over every frame() so far.
    int requests=0;
    const char *requestedType=nullptr;

    explicit Scene(int width=640, int height=480)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, height);
    }

    ~Scene()
    {
        delete canvas;
    }

    controls::LayoutStats frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &stats=canvas->getLayoutStats();

        requests+=stats.requests;
        if(!requestedType)
            requestedType=stats.requestedType;
        return stats;
    }

    void settle()
    {
        for(int i=0; i<10 && frame().passes != 0; ++i)
            ;
    }

    std::vector<std::string> tree()
    {
        std::vector<std::string> out;

        listTree(canvas, 0, out);
        return out;
    }
};

void checkSameTree(const std::vector<std::string> &a, const std::vector<std::string> &b)
{
    REQUIRE(a.size() == b.size());

    for(size_t i=0; i<a.size(); ++i)
        CHECK(a[i] == b[i]);
}

//! Places its one part itself, 5 in from each side and 7 high, from layout().
class Holder: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Holder, controls::Base)
    {
        part=newChild<controls::Base>("part");
        part->setPlacement(controls::Placement::Owner);
        // Ignored: its owner places it.
        part->setDock(DockPosition::Center);
        part->setSize(300, 300);
    }

    controls::Base *part;

protected:
    void layout(skin::Base *skin) override
    {
        placeChild(part, 5, 5, getWidth()-10, 7);
        ParentClass::layout(skin);
    }
};

//! Moves its one child with setPos from layout(): a request made in a pass.
class Requester: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Requester, controls::Base)
    {
        child=newChild<controls::Base>("child");
    }

    controls::Base *child;

protected:
    void layout(skin::Base *skin) override
    {
        child->setPos(getWidth()/2, 0);
        ParentClass::layout(skin);
    }
};

}//namespace

TEST_CASE("an Owner child is placed by its owner alone, records no request and does not count in its size")
{
    Scene scene;
    Holder *holder=scene.canvas->newChild<Holder>();

    holder->setBounds(10, 10, 100, 50);

    const Rect requested=holder->part->getRequestedBounds();

    scene.settle();

    // The generic arrange would have docked it Center over the whole holder.
    CHECK(rectText(holder->part->getBounds()) == rectText(Rect(5, 5, 90, 7)));
    // Only the constructor's setSize asked: placeChild left the request alone.
    CHECK(rectText(holder->part->getRequestedBounds()) == rectText(requested));
    // Its 300x300 does not count in the holder's size, which is only padding.
    CHECK(holder->getPreferredSize().width == 0);
    CHECK(holder->getPreferredSize().height == 0);
    CHECK(scene.requests == 0);

    holder->setSize(200, 50);
    scene.settle();

    CHECK(rectText(holder->part->getBounds()) == rectText(Rect(5, 5, 190, 7)));
    CHECK(rectText(holder->part->getRequestedBounds()) == rectText(requested));
    CHECK(scene.requests == 0);
}

TEST_CASE("a child given back to the generic arrange is docked and counted again")
{
    Scene scene;
    controls::Base *parent=scene.canvas->newChild<controls::Base>("parent");
    controls::Label *child=parent->newChild<controls::Label>("child");

    parent->setPos(10, 10);
    child->setText("Owned");
    child->setPos(4, 6);
    child->setPlacement(controls::Placement::Owner);
    scene.settle();

    CHECK(parent->getPreferredSize().width == 0);
    CHECK(parent->getPreferredSize().height == 0);

    child->setPlacement(controls::Placement::Parent);
    scene.settle();

    // Counted at the position it asked for, with its preferred size.
    const Size &preferred=child->getPreferredSize();

    CHECK(preferred.width > 0);
    CHECK(parent->getPreferredSize().width == 4+preferred.width);
    CHECK(parent->getPreferredSize().height == 6+preferred.height);
    CHECK(rectText(child->getBounds()) == rectText(Rect(4, 6, preferred.width, preferred.height)));
}

TEST_CASE("a request recorded while a pass runs is counted, with the first type that made it")
{
    Scene scene;
    Requester *requester=scene.canvas->newChild<Requester>();

    requester->setBounds(0, 0, 100, 20);

    // App calls between frames are requests too, but no pass is running.
    const controls::LayoutStats stats=scene.frame();

    CHECK(stats.requests >= 1);
    REQUIRE(stats.requestedType != nullptr);
    CHECK(std::string(stats.requestedType) == "Base");
    CHECK(requester->child->getRequestedBounds().x == 50);
}

TEST_CASE("CrossSplitter's preferred sizes do not depend on the canvas sizes it had before")
{
    // LabelMultilineDemo's arrangement: wrapped Labels in the four panels of a
    // CrossSplitter docked Center with margin 10 in a VerticalLayout.
    auto build=[](Scene &scene)
    {
        controls::Base *host=scene.canvas->newChild<controls::Base>("host");
        host->setLayout(new controls::VerticalLayout());
        host->setDock(DockPosition::Center);

        controls::CrossSplitter *splitter=host->newChild<controls::CrossSplitter>("splitter");
        splitter->setDock(DockPosition::Center);
        splitter->setMargin(Margin(10, 10, 10, 10));

        for(int i=0; i<4; ++i)
        {
            controls::Label *label=splitter->newChild<controls::Label>();

            label->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod");
            label->setWrap(true);
            splitter->setPanel(i, label);
        }
    };

    Scene fresh;
    build(fresh);
    fresh.settle();

    Scene trip;
    build(trip);
    trip.settle();

    const std::vector<std::string> first=trip.tree();

    trip.canvas->setSize(1024, 768);
    trip.settle();
    trip.canvas->setSize(640, 480);
    trip.settle();

    checkSameTree(trip.tree(), first);
    checkSameTree(trip.tree(), fresh.tree());
    CHECK(trip.requests == 0);
    CHECK(fresh.requests == 0);
}

TEST_CASE("Properties' preferred sizes do not depend on the widths it had before")
{
    // One Properties docked Top, one inside a dock-None box with no size,
    // which starts narrower than the split (the nested one in a PropertyTree).
    auto build=[](Scene &scene)
    {
        controls::Properties *top=scene.canvas->newChild<controls::Properties>("top");
        top->setDock(DockPosition::Top);
        top->add("First Name");
        top->add("Middle Name");
        top->add("Last Name");

        controls::Base *box=scene.canvas->newChild<controls::Base>("box");
        box->setPos(0, 200);

        controls::Properties *nested=box->newChild<controls::Properties>("nested");
        nested->setDock(DockPosition::Top);
        nested->add("Name");
        nested->add("Age");
    };

    Scene fresh;
    build(fresh);
    fresh.settle();

    Scene trip;
    build(trip);
    trip.settle();

    const std::vector<std::string> first=trip.tree();

    trip.canvas->setSize(1024, 768);
    trip.settle();
    trip.canvas->setSize(640, 480);
    trip.settle();

    checkSameTree(trip.tree(), first);
    checkSameTree(trip.tree(), fresh.tree());
    CHECK(trip.requests == 0);
    CHECK(fresh.requests == 0);

    // A row asks for its label's margin, the split (80) and its value column:
    // the value width (100) or its property's preferred width, the wider.
    for(auto &&child : fresh.canvas->getChildren())
    {
        for(auto &&grandChild : child->getChildren())
        {
            auto *properties=gweni_cast<controls::Properties>(grandChild);

            if(!properties)
                continue;

            for(auto &&row : properties->getChildren())
            {
                auto *propertyRow=gweni_cast<controls::PropertyRow>(row);

                if(!propertyRow)
                    continue;

                const int expected=propertyRow->getLabel()->getMargin().left+80+
                    std::max(100, propertyRow->getProperty()->getPreferredSize().width);

                CHECK(propertyRow->getPreferredSize().width == expected);
            }
        }
    }
}

TEST_CASE("a Properties in an unsized box gives every value its value width")
{
    // Laid out at its preferred size, as a centred page or a Popup lays it out:
    // the value column is the value width, whatever the property's own width.
    Scene scene;
    controls::Base *box=scene.canvas->newChild<controls::Base>("box");
    box->setPos(10, 10);

    controls::Properties *properties=box->newChild<controls::Properties>();
    properties->setDock(DockPosition::Top);
    REQUIRE(properties->getValueWidth() == 100);

    std::vector<controls::PropertyRow *> rows;

    rows.push_back(properties->add("First Name"));
    rows.push_back(properties->add("Middle Name", "Ann"));
    // An app's own width on a property is not a minimum: the row places it.
    controls::property::Text *wide=properties->newChild<controls::property::Text>();
    wide->setWidth(300);
    rows.push_back(properties->add("Last Name", wide));
    scene.settle();

    const int split=properties->getRequestedSplitWidth();

    CHECK(box->getWidth() == properties->getWidth());
    CHECK(properties->getWidth() == rows[0]->getLabel()->getMargin().left+split+100);

    for(auto *row : rows)
    {
        controls::property::Base *property=row->getProperty();

        CHECK(row->getWidth() == properties->getWidth());
        CHECK(property->getWidth() >= properties->getValueWidth());
        CHECK(property->getWidth() == 100);
        CHECK(property->getX() == row->getLabel()->getMargin().left+split);
    }

    // The value width is the preferred width's: setting it lays the rows out
    // again, in one frame, and the box follows.
    const int before=properties->getPreferredSize().width;

    properties->setValueWidth(160);
    scene.settle();

    CHECK(properties->getPreferredSize().width == before+60);
    CHECK(box->getWidth() == before+60);

    for(auto *row : rows)
        CHECK(row->getProperty()->getWidth() == 160);

    properties->setValueWidth(10);
    scene.settle();

    // Below the properties' own preferred widths, theirs is the column's.
    for(auto *row : rows)
    {
        controls::property::Base *property=row->getProperty();

        CHECK(row->getPreferredSize().width == row->getLabel()->getMargin().left+split+
            std::max(10, property->getPreferredSize().width));
    }

    CHECK(scene.requests == 0);
}

TEST_CASE("a ColorSelector row fits its value's text and its swatch")
{
    Scene scene;
    controls::Base *box=scene.canvas->newChild<controls::Base>("box");

    controls::Properties *properties=box->newChild<controls::Properties>();
    properties->setDock(DockPosition::Top);
    // Narrower than the value, so the property's own width decides.
    properties->setValueWidth(0);

    auto *selector=properties->newChild<controls::property::ColorSelector>();
    controls::PropertyRow *row=properties->add("Colour", selector, "255 0 0");

    scene.settle();

    controls::TextBox *textBox=selector->m_textBox;
    controls::Base *swatch=selector->m_button;
    const Margin &swatchMargin=swatch->getMargin();
    controls::Base *text=nullptr;

    for(auto &&child : textBox->getChildren())
    {
        if(gweni_cast<controls::internal::Text>(child))
            text=child;
    }

    REQUIRE(text);
    const int textWidth=text->getPreferredSize().width;

    REQUIRE(textWidth > 0);
    CHECK(selector->getWidth() == selector->getPreferredSize().width);
    CHECK(selector->getWidth() >= textWidth+swatch->getWidth()+swatchMargin.left+swatchMargin.right);
    // The text ends left of the swatch, which is inside the box.
    CHECK(text->getX()+textWidth <= swatch->getX()-swatchMargin.left);
    CHECK(swatch->getX()+swatch->getWidth()+swatchMargin.right <= textBox->getWidth());
    CHECK(swatch->getWidth() == 20);
    CHECK(row->getPreferredSize().width == row->getLabel()->getMargin().left+
        properties->getRequestedSplitWidth()+selector->getPreferredSize().width);
}

TEST_CASE("a ComboBox asks for its widest item, whatever is selected")
{
    Scene scene;
    controls::Base *box=scene.canvas->newChild<controls::Base>("box");
    controls::ComboBox *combo=box->newChild<controls::ComboBox>();

    scene.settle();

    // With no items, the Button's width from the shown (empty) text.
    const int empty=combo->getPreferredSize().width;
    const Padding &textPadding=combo->getTextPadding();
    const Padding &padding=combo->getPadding();
    auto widthFor=[&](const String &widest)
    {
        return scene.renderer.measureText(combo->getFont(), widest).x+
            textPadding.left+textPadding.right+padding.left+padding.right;
    };

    combo->addItem("One", "one");
    controls::MenuItem *longest=combo->addItem("The longest item", "longest");
    combo->addItem("Middle one", "middle");
    scene.settle();

    // The chevron column is the right padding.
    CHECK(padding.right == controls::ComboBox::ChevronSize+controls::ComboBox::ChevronInset);
    CHECK(combo->getText() == "One");
    CHECK(combo->getPreferredSize().width == widthFor("The longest item"));
    CHECK(combo->getWidth() == combo->getPreferredSize().width);

    // Selecting measures no item and changes neither width.
    const int width=combo->getPreferredSize().width;

    for(const char *name : {"longest", "middle", "one"})
    {
        const size_t measures=scene.renderer.measures;

        combo->selectItemByName(name);
        scene.settle();

        CHECK(combo->getPreferredSize().width == width);
        CHECK(combo->getWidth() == width);
        // the shown text's own measure, no more
        CHECK(scene.renderer.measures-measures <= 1);
    }

    // An item added or taken away counts at once.
    combo->addItem("An even longer item than that", "longer");
    scene.settle();
    CHECK(combo->getPreferredSize().width == widthFor("An even longer item than that"));

    combo->selectItemByName("longer");
    scene.settle();
    CHECK(combo->getPreferredSize().width == widthFor("An even longer item than that"));
    combo->selectItemByName("one");

    for(auto &&child : combo->getChildren())
    {
        if(auto *menu=gweni_cast<controls::Menu>(child))
        {
            for(auto &&item : menu->getChildren())
            {
                if(item->getName() == "longer")
                {
                    item->delayedDelete();
                    break;
                }
            }
        }
    }

    // A deleted item leaves the menu: the box is laid out again because the
    // menu, its child, lost a child.
    scene.settle();
    CHECK(combo->getPreferredSize().width == widthFor("The longest item"));

    // A renamed item: its text is measured again.
    longest->setText("Short");
    scene.settle();
    CHECK(combo->getPreferredSize().width == widthFor("Middle one"));

    // Cleared, nothing is selected and the width is the empty box's; the next
    // item added is selected again.
    combo->clearItems();
    scene.settle();
    CHECK(combo->getText() == "");
    CHECK(combo->getSelectedItem() == nullptr);
    CHECK(combo->getPreferredSize().width == empty);

    combo->addItem("Again", "again");
    scene.settle();
    CHECK(combo->getText() == "Again");
    CHECK(combo->getPreferredSize().width == widthFor("Again"));
    CHECK(scene.requests == 0);
}

TEST_CASE("a property::ComboBox row's value column fits the widest item and the chevron")
{
    Scene scene;
    controls::Base *box=scene.canvas->newChild<controls::Base>("box");
    controls::Properties *properties=box->newChild<controls::Properties>();

    properties->setDock(DockPosition::Top);
    // Narrower than the items, so the ComboBox's width decides.
    properties->setValueWidth(0);

    auto *property=properties->newChild<controls::property::ComboBox>();
    controls::ComboBox *combo=property->getComboBox();

    combo->addItem("Option One", "one");
    combo->addItem("A much longer option", "long");
    combo->addItem("Two", "two");

    controls::PropertyRow *row=properties->add("ComboBox", property, "one");

    scene.settle();

    const Padding &textPadding=combo->getTextPadding();
    const Padding &padding=combo->getPadding();
    const int comboWidth=scene.renderer.measureText(combo->getFont(), "A much longer option").x+
        textPadding.left+textPadding.right+padding.left+padding.right;
    const Margin &comboMargin=combo->getMargin();
    const int split=properties->getRequestedSplitWidth();
    const int labelMargin=row->getLabel()->getMargin().left;

    CHECK(combo->getPreferredSize().width == comboWidth);
    CHECK(property->getPreferredSize().width == comboWidth+comboMargin.left+comboMargin.right);
    CHECK(row->getPreferredSize().width == labelMargin+split+property->getPreferredSize().width);
    CHECK(combo->getWidth() == comboWidth);

    const int rowWidth=row->getWidth();

    combo->selectItemByName("long");
    scene.settle();
    CHECK(row->getWidth() == rowWidth);
    CHECK(combo->getWidth() == comboWidth);

    combo->selectItemByName("two");
    scene.settle();
    CHECK(row->getWidth() == rowWidth);
    CHECK(combo->getWidth() == comboWidth);

    // At the default value width the column is the wider of the two.
    properties->setValueWidth(100);
    scene.settle();
    CHECK(row->getPreferredSize().width ==
        labelMargin+split+std::max(100, property->getPreferredSize().width));
    CHECK(scene.requests == 0);
}

TEST_CASE("a PropertyTree at its preferred width fits its nodes' Properties, open or closed")
{
    // The first node's Properties is the wider: its ComboBox's widest item
    // is wider than the value width.
    struct Tree
    {
        controls::Base *box;
        controls::PropertyTree *tree;
        controls::Properties *wide;
        controls::Properties *narrow;
        controls::TreeNode *wideNode;
        controls::TreeNode *narrowNode;
    };
    auto build=[](Scene &scene)
    {
        Tree t;

        t.box=scene.canvas->newChild<controls::Base>("box");
        t.tree=t.box->newChild<controls::PropertyTree>();

        t.wide=t.tree->add("Item One");
        t.wide->add("Name");
        auto *property=t.wide->newChild<controls::property::ComboBox>();
        property->getComboBox()->addItem("A much, much longer option than the value width", "long");
        t.wide->add("Mode", property, "long");

        t.narrow=t.tree->add("Item Two");
        t.narrow->add("Name");
        t.narrow->add("Colour", t.narrow->newChild<controls::property::ColorSelector>(), "255 0 0");

        // A Properties' getParent() is its node (the panel holding it is the
        // node's inner panel).
        t.wideNode=gweni_cast<controls::TreeNode>(t.wide->getParent());
        t.narrowNode=gweni_cast<controls::TreeNode>(t.narrow->getParent());
        REQUIRE(t.wideNode);
        REQUIRE(t.narrowNode);
        return t;
    };

    Scene scene;
    Tree t=build(scene);

    t.tree->expandAll();
    scene.settle();

    REQUIRE(t.wide->getPreferredSize().width > t.narrow->getPreferredSize().width);

    // Laid out at its preferred size, every Properties gets at least its own
    // (indent + split + value column) and ends inside the tree.
    CHECK(t.tree->getWidth() == t.tree->getPreferredSize().width);

    const int treeRight=t.tree->localPosToCanvas(gweni::Point(t.tree->getWidth(), 0)).x;

    for(controls::Properties *properties : {t.wide, t.narrow})
    {
        const int x=properties->localPosToCanvas(gweni::Point(0, 0)).x;

        CHECK(properties->getWidth() >= properties->getPreferredSize().width);
        CHECK(x+properties->getWidth() <= treeRight);

        for(auto &&child : properties->getChildren())
        {
            if(auto *row=gweni_cast<controls::PropertyRow>(child))
            {
                CHECK(row->getProperty()->getWidth() >= properties->getValueWidth());
                CHECK(row->getProperty()->getWidth() >= row->getProperty()->getPreferredSize().width);
            }
        }
    }

    const int open=t.tree->getWidth();

    // Collapsing the wider node keeps the tree's width: a closed node's rows
    // still count in its width (TreeNode::calculateSize), so the tree does
    // not jump on a toggle.
    t.wideNode->close();
    scene.settle();
    CHECK(t.tree->getWidth() == open);
    CHECK(t.narrow->getWidth() >= t.narrow->getPreferredSize().width);

    t.wideNode->open();
    scene.settle();
    CHECK(t.tree->getWidth() == open);

    // Every node closed, the tree is as wide still.
    t.wideNode->close();
    t.narrowNode->close();
    scene.settle();
    CHECK(t.tree->getWidth() == open);
    CHECK(scene.requests == 0);

    // So is one built with its wide node closed, never opened.
    Scene fresh;
    Tree f=build(fresh);

    f.narrowNode->open();
    fresh.settle();
    CHECK(f.wideNode->isOpen() == false);
    CHECK(f.tree->getWidth() == open);

    // Opened, the rows are laid out at that width.
    f.wideNode->open();
    fresh.settle();
    CHECK(f.tree->getWidth() == open);
    CHECK(f.wide->getWidth() == t.wide->getWidth());
    CHECK(fresh.requests == 0);
}

TEST_CASE("a closed TreeNode measures its rows again only when one of them changed")
{
    // A closed node's panel is never arranged, so it stays layout-dirty: what
    // keeps a pass that enters the node from measuring the rows again is
    // that their width is current (Base::m_hiddenWidthCurrent). Relabelling
    // the closed node enters as many controls whatever its row count.
    auto relabelVisits=[](int rows)
    {
        Scene scene;
        auto *tree=scene.canvas->newChild<controls::TreeControl>();
        controls::TreeNode *root=tree->addNode("Root");

        for(int i=0; i < rows; ++i)
            root->addNode("Row "+std::to_string(i));
        scene.settle();
        REQUIRE_FALSE(root->isOpen());

        std::vector<size_t> visits;

        for(int frame=0; frame < 4; ++frame)
        {
            root->setText("Root "+std::to_string(frame));
            visits.push_back(scene.frame().visited);
        }
        return visits;
    };

    const std::vector<size_t> small=relabelVisits(100);

    CHECK(small == relabelVisits(2000));
    CHECK(small.front() < 100);

    // A row of a closed node that changes is measured again, and only it:
    // the tree widens to fit it, as a full pass finds (layout verification).
    Scene scene;
    auto *tree=scene.canvas->newChild<controls::TreeControl>();
    controls::TreeNode *root=tree->addNode("Root");
    std::vector<controls::TreeNode *> rows;

    for(int i=0; i < 2000; ++i)
        rows.push_back(root->addNode("Row "+std::to_string(i)));
    scene.settle();

    const int closed=tree->getPreferredSize().width;
    const size_t failures=controls::layoutVerifyFailures();
    const bool verify=controls::layoutVerifyEnabled();

    controls::setLayoutVerify(true);
    rows[1000]->setText("A row label much wider than every other row of the tree");
    const size_t widened=scene.frame().visited;
    scene.settle();
    CHECK(tree->getPreferredSize().width > closed);
    CHECK(widened < 100);

    rows[1000]->setText("Row 1000");
    scene.settle();
    CHECK(tree->getPreferredSize().width == closed);

    // So does a row added to it, and the tree narrows again once it is gone.
    controls::TreeNode *added=root->addNode("An added row label much wider than every other row");
    scene.settle();
    CHECK(tree->getPreferredSize().width > closed);
    delete added;
    scene.settle();
    CHECK(tree->getPreferredSize().width == closed);
    controls::setLayoutVerify(verify);
    CHECK(controls::layoutVerifyFailures() == failures);

    // Opened, every row is laid out: their heights were never measured
    // while the node was closed.
    root->open();
    scene.settle();
    CHECK(rows.back()->getHeight() == rows.front()->getHeight());
    CHECK(rows.back()->getY() > rows.front()->getY());
    CHECK(rows.back()->getHeight() > 0);
}

TEST_CASE("a VerticalSlider asks for its request whatever its value, and fits a smaller layout cell in one pass")
{
    Scene scene;
    controls::Base *host=scene.canvas->newChild<controls::Base>("host");

    host->setBounds(0, 0, 100, 60);
    host->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    host->setLayout(new controls::VerticalLayout());

    controls::VerticalSlider *slider=host->newChild<controls::VerticalSlider>();

    slider->setSize(20, 200);
    // Cut to a cell that is too short, as a Shrink child is.
    slider->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    slider->setRange(0, 1);
    slider->setFloatValue(0);

    const controls::LayoutStats first=scene.frame();

    CHECK(first.passes == 1);
    CHECK(slider->getPreferredSize().height == 200);
    // The cell is the host's 60: the slider gets it, not its 200.
    CHECK(slider->getHeight() == 60);

    const Rect atZero=slider->getBounds();

    slider->setFloatValue(1);

    const controls::LayoutStats second=scene.frame();

    CHECK(second.passes <= 1);
    CHECK(slider->getPreferredSize().height == 200);
    CHECK(rectText(slider->getBounds()) == rectText(atZero));
    CHECK(scene.frame().passes == 0);
    CHECK(scene.requests == 0);
}

TEST_CASE("a wrapped Text asks for its parent's requested width, or with none for its longest word")
{
    const String words="aa bbbbbbbbbb ccc dddd";

    SUBCASE("the app asked the parent for a width")
    {
        Scene scene;
        controls::Base *box=scene.canvas->newChild<controls::Base>("box");

        box->setSize(200, 100);
        box->setPadding(Padding(3, 3, 3, 3));

        auto *wrapped=box->newChild<controls::internal::Text>();

        wrapped->setWrap(true);
        wrapped->setText(words);
        scene.settle();

        CHECK(wrapped->getPreferredSize().width == 200-3-3);
        CHECK(scene.requests == 0);
    }

    SUBCASE("no width was asked: the longest word, the height from the width it is laid out at")
    {
        Scene scene;
        controls::Base *box=scene.canvas->newChild<controls::Base>("box");

        box->setDock(DockPosition::Top);

        auto *wrapped=box->newChild<controls::internal::Text>();

        wrapped->setWrap(true);
        wrapped->setText(words);
        scene.settle();

        const int longest=scene.renderer.measureText(wrapped->getFont(), "bbbbbbbbbb").x;
        const int lineHeight=scene.renderer.measureText(wrapped->getFont(), " ").y;

        CHECK(!box->hasRequestedSize(Dim::X));
        CHECK(wrapped->getPreferredSize().width == longest);
        // 640 wide: one line.
        CHECK(wrapped->numLines() == 1);
        CHECK(wrapped->getPreferredSize().height == lineHeight);

        // As wide as the longest word: several lines, the height theirs.
        scene.canvas->setSize(longest, 480);
        scene.settle();

        CHECK(wrapped->getPreferredSize().width == longest);
        CHECK(wrapped->numLines() > 1);
        CHECK(wrapped->getPreferredSize().height == static_cast<int>(wrapped->numLines())*lineHeight);

        scene.canvas->setSize(640, 480);
        scene.settle();

        CHECK(wrapped->getPreferredSize().width == longest);
        CHECK(wrapped->numLines() == 1);
        CHECK(scene.requests == 0);
    }
}

namespace
{

//! A control's request and whether a size was asked, as text.
std::string request(const controls::Base *control)
{
    return rectText(control->getRequestedBounds())+" w="+std::to_string(control->hasRequestedSize(Dim::X))+
        " h="+std::to_string(control->hasRequestedSize(Dim::Y));
}

//! The requests of \p control and of its whole subtree, depth-first.
void listRequests(controls::Base *control, std::vector<std::string> &out)
{
    out.push_back(std::string(control->getTypeName())+" "+request(control));

    for(auto &&child : control->getActualChildren())
        listRequests(child, out);
}

std::vector<std::string> requests(controls::Base *control)
{
    std::vector<std::string> out;

    listRequests(control, out);
    return out;
}

//! Presses \p control at \p from (canvas coordinates), moves the mouse to
//! \p to and lets go, through the control's own mouse handlers.
void drag(controls::Base *control, const Point &from, const Point &to)
{
    control->onMouseClickLeft(from.x, from.y, true);
    control->onMouseMoved(to.x, to.y, to.x-from.x, to.y-from.y);
    control->onMouseClickLeft(to.x, to.y, false);
}

}

TEST_CASE("a slider's value change, a click and a drag on its thumb record no request")
{
    Scene scene;
    controls::VerticalSlider *slider=scene.canvas->newChild<controls::VerticalSlider>();

    slider->setBounds(10, 10, 20, 200);
    slider->setRange(0, 1);
    slider->setFloatValue(0);
    scene.settle();

    const std::vector<std::string> before=requests(slider);
    controls::Base *thumb=slider->getChildren().front();
    const int atZero=thumb->getY();

    slider->setFloatValue(1);
    scene.settle();
    CHECK(thumb->getY() != atZero);
    checkSameTree(requests(slider), before);

    // a click on the track jumps the thumb there, which sets the value
    const Point track=slider->localPosToCanvas(Point(10, 150));

    slider->onMouseClickLeft(track.x, track.y, true);
    slider->onMouseClickLeft(track.x, track.y, false);
    scene.settle();
    CHECK(slider->getFloatValue() < 0.5f);
    checkSameTree(requests(slider), before);

    // a drag of the thumb itself, through its Dragger
    const float clicked=slider->getFloatValue();
    const Point grab=thumb->localPosToCanvas(Point(2, 2));

    drag(thumb, grab, Point(grab.x, grab.y-60));
    scene.settle();
    CHECK(slider->getFloatValue() > clicked);
    checkSameTree(requests(slider), before);
    CHECK(scene.requests == 0);
}

TEST_CASE("dragging a CrossSplitter bar records no request")
{
    Scene scene;
    controls::CrossSplitter *splitter=scene.canvas->newChild<controls::CrossSplitter>();

    splitter->setBounds(0, 0, 400, 300);
    scene.settle();

    const std::vector<std::string> before=requests(splitter);
    const float horizontal=splitter->getHorizontalFraction();
    const float vertical=splitter->getVerticalFraction();

    // the bars are its first three children: top/bottom, left/right, centre
    for(int i=0; i < 3; ++i)
    {
        controls::Base *bar=*std::next(splitter->getChildren().begin(), i);
        const Point grab=bar->localPosToCanvas(Point(2, 2));

        drag(bar, grab, Point(grab.x+40, grab.y+30));
        scene.settle();
        checkSameTree(requests(splitter), before);
    }

    CHECK(splitter->getHorizontalFraction() != horizontal);
    CHECK(splitter->getVerticalFraction() != vertical);
    CHECK(scene.requests == 0);
}

TEST_CASE("a window dragged by its title bar asks to be where it was dropped")
{
    Scene scene;
    controls::WindowControl *window=scene.canvas->newChild<controls::WindowControl>();

    window->setBounds(20, 30, 200, 150);
    scene.settle();

    controls::Base *titleBar=nullptr;

    for(auto &&child : window->getActualChildren())
    {
        if(std::string(child->getTypeName()) == "Dragger")
            titleBar=child;
    }
    REQUIRE(titleBar != nullptr);

    const Point grab=titleBar->localPosToCanvas(Point(50, 5));

    drag(titleBar, grab, Point(grab.x+40, grab.y+25));
    scene.settle();

    CHECK(rectText(window->getRequestedBounds()) == rectText(Rect(60, 55, 200, 150)));
    CHECK(rectText(window->getBounds()) == rectText(Rect(60, 55, 200, 150)));
    // a size was asked by the app, and stays asked
    CHECK(window->hasRequestedSize(Dim::X));
    CHECK(window->hasRequestedSize(Dim::Y));
    // made outside any pass: nothing is counted
    CHECK(scene.requests == 0);
}
