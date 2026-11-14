/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Base::onVisibleChanged() is called on a control exactly once for each real
 *  change of its visible(), and never otherwise:
 *
 *    own hide      setHidden(true) and (false), once each; a second setHidden
 *                  with the same flag, none.
 *    ancestor      an ancestor's hide and show reach every shown descendant,
 *                  once each, but not one that hid itself, nor its subtree.
 *    under hidden  hiding or showing a control under a hidden ancestor calls
 *                  it on nothing (nothing below it changes either way); the
 *                  ancestor shown again calls it on what is now shown alone.
 *    attach        a control added to a hidden parent (or moved to one) is
 *                  called, one added to a shown parent is not; removed from a
 *                  hidden parent it is shown on its own and is called, removed
 *                  from a shown one it is not.
 *
 *  Each call sees visible() already at its new value, on itself and on its
 *  descendants.
 */
#include "headlessFixture.h"

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! Counts its onVisibleChanged() calls and records visible() at each.
class Watcher: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Watcher, controls::Base)
    {}

    void onVisibleChanged() override
    {
        ++calls;
        seen.push_back(visible());
        for(controls::Base *child : getChildren())
            childSeen.push_back(child->visible());
    }

    int calls=0;
    std::vector<bool> seen;
    std::vector<bool> childSeen;
};

struct Tree
{
    Tree()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(200, 200);
        top=canvas->newChild<Watcher>();
        middle=top->newChild<Watcher>();
        leaf=middle->newChild<Watcher>();
    }

    Tree(const Tree &)=delete;
    Tree &operator=(const Tree &)=delete;

    ~Tree() { delete canvas; }

    void reset()
    {
        for(Watcher *w : {top, middle, leaf})
        {
            w->calls=0;
            w->seen.clear();
            w->childSeen.clear();
        }
    }

    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    Watcher *top=nullptr;
    Watcher *middle=nullptr;
    Watcher *leaf=nullptr;
};

}//namespace

TEST_CASE("onVisibleChanged: its own hide and show call it once each, a repeated flag not at all")
{
    Tree tree;

    tree.reset();
    tree.leaf->setHidden(true);
    CHECK(tree.leaf->calls == 1);
    CHECK(tree.leaf->seen == std::vector<bool>{false});
    tree.leaf->setHidden(true);
    CHECK(tree.leaf->calls == 1);
    tree.leaf->setHidden(false);
    CHECK(tree.leaf->calls == 2);
    CHECK(tree.leaf->seen == std::vector<bool>{false, true});
    tree.leaf->show();
    CHECK(tree.leaf->calls == 2);
    // its ancestors' visible() did not change
    CHECK(tree.top->calls == 0);
    CHECK(tree.middle->calls == 0);
}

TEST_CASE("onVisibleChanged: an ancestor's hide reaches each shown descendant once, after its subtree is updated")
{
    Tree tree;

    tree.reset();
    tree.top->hide();
    CHECK(tree.top->calls == 1);
    CHECK(tree.middle->calls == 1);
    CHECK(tree.leaf->calls == 1);
    CHECK(tree.middle->seen == std::vector<bool>{false});
    CHECK(tree.middle->childSeen == std::vector<bool>{false});
    CHECK(tree.top->childSeen == std::vector<bool>{false});

    tree.top->show();
    CHECK(tree.top->calls == 2);
    CHECK(tree.middle->calls == 2);
    CHECK(tree.leaf->calls == 2);
    CHECK(tree.leaf->seen == std::vector<bool>{false, true});
    CHECK(tree.top->childSeen == std::vector<bool>{false, true});
}

TEST_CASE("onVisibleChanged: a descendant that hid itself, and its subtree, are not called for an ancestor")
{
    Tree tree;

    tree.middle->hide();
    tree.reset();
    tree.top->hide();
    CHECK(tree.top->calls == 1);
    CHECK(tree.middle->calls == 0);
    CHECK(tree.leaf->calls == 0);
    tree.top->show();
    CHECK(tree.top->calls == 2);
    CHECK(tree.middle->calls == 0);
    CHECK(tree.leaf->calls == 0);
}

TEST_CASE("onVisibleChanged: a hide or show under a hidden ancestor calls nothing, the ancestor's show what it shows")
{
    Tree tree;

    tree.top->hide();
    tree.reset();
    tree.middle->hide();
    tree.middle->show();
    tree.middle->hide();
    CHECK(tree.middle->calls == 0);
    CHECK(tree.leaf->calls == 0);

    // shown again, top is visible and middle, which hid itself meanwhile, not
    tree.top->show();
    CHECK(tree.top->calls == 1);
    CHECK(tree.middle->calls == 0);
    CHECK(tree.leaf->calls == 0);

    tree.middle->show();
    CHECK(tree.middle->calls == 1);
    CHECK(tree.leaf->calls == 1);
    CHECK(tree.leaf->seen == std::vector<bool>{true});
}

TEST_CASE("onVisibleChanged: attached to a hidden parent or detached from one it is called, a shown one not")
{
    Tree tree;
    Watcher *loose=controls::newControl<Watcher>();
    Watcher *child=loose->newChild<Watcher>();

    tree.reset();
    // to a shown parent: visible() stays true
    loose->setParent(tree.top);
    CHECK(loose->calls == 0);
    CHECK(child->calls == 0);
    // detached from a shown parent: still true
    loose->setParent(nullptr);
    CHECK(loose->calls == 0);

    tree.top->hide();
    tree.reset();
    // to a hidden parent: false, on it and its child
    loose->setParent(tree.middle);
    CHECK(loose->calls == 1);
    CHECK(child->calls == 1);
    CHECK(loose->seen == std::vector<bool>{false});
    CHECK(child->seen == std::vector<bool>{false});
    // moved between two hidden parents: no change
    loose->setParent(tree.leaf);
    CHECK(loose->calls == 1);
    // detached from a hidden parent: shown on its own
    loose->setParent(nullptr);
    CHECK(loose->calls == 2);
    CHECK(child->calls == 2);
    CHECK(loose->seen == std::vector<bool>{false, true});
    // one that hid itself is not shown either way
    loose->hide();
    loose->calls=0;
    child->calls=0;
    loose->setParent(tree.middle);
    loose->setParent(nullptr);
    CHECK(loose->calls == 0);
    CHECK(child->calls == 0);

    delete loose;
}
