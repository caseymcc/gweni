// REVIEW PROBE, not to be committed.
#include "headlessFixture.h"

#include <gweni/controls/treeTable.h>
#include <doctest/doctest.h>

#include <cstdio>

using namespace gweni;

namespace
{
using Tree=controls::TreeTable;
using Id=Tree::NodeId;
constexpr Id Root=Tree::Root;

struct ProbeScene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas canvas{&skin};
    Tree *tree=canvas.newChild<Tree>();

    ProbeScene()
    {
        canvas.setSize(600, 400);
        tree->setBounds(10, 10, 400, 200);
        tree->setColumnCount(1);
        tree->setRowHeight(20);
    }
    ~ProbeScene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
    }
    void frame()
    {
        canvas.updateCanvas();
        canvas.renderCanvas();
    }
    std::vector<Id> visible() const
    {
        std::vector<Id> ids;
        for(const auto &node : tree->getVisibleNodes())
            ids.push_back(node.id);
        return ids;
    }
};

std::string str(const std::vector<Id> &ids)
{
    std::string s;
    for(Id id : ids)
        s+=(id == Tree::NoNode?std::string("PH"):std::to_string(id))+" ";
    return s;
}
}//namespace

TEST_CASE("PROBE hidden selection after a move, then Left")
{
    ProbeScene scene;
    Tree *tree=scene.tree;
    tree->addNode(Root, 1);
    tree->addNode(Root, 2);
    tree->addNode(1, 10);
    tree->addNode(2, 20);
    tree->setExpanded(1, true);
    scene.frame();
    REQUIRE(tree->setSelectedNode(10));
    REQUIRE(tree->moveNode(10, 2, 0));   // 2 is collapsed: the selection hides
    scene.frame();
    printf("selected node %zu item %zu (NoItem %zu)\n", tree->getSelectedNode(),
        tree->getSelectedItem(), Tree::NoItem);
    CHECK(tree->getSelectedNode() == 10);
    CHECK(tree->getSelectedItem() == Tree::NoItem);
    tree->focus();
    REQUIRE(KeyboardFocus == tree);
    // node 10 is not expanded and has a parent: onKeyLeft reads m_visible[getSelectedItem()]
    tree->onKeyLeft(true);
    printf("after Left: selected node %zu item %zu\n", tree->getSelectedNode(), tree->getSelectedItem());
}

TEST_CASE("PROBE hidden selection after a move, then Right on an expanded node")
{
    ProbeScene scene;
    Tree *tree=scene.tree;
    tree->addNode(Root, 1);
    tree->addNode(Root, 2);
    tree->addNode(1, 10);
    tree->addNode(10, 100);
    tree->addNode(2, 20);
    tree->setExpanded(1, true);
    tree->setExpanded(10, true);
    scene.frame();
    REQUIRE(tree->setSelectedNode(10));
    REQUIRE(tree->moveNode(10, 2, 0));
    scene.frame();
    tree->focus();
    tree->onKeyRight(true);
    printf("after Right: selected node %zu item %zu\n", tree->getSelectedNode(), tree->getSelectedItem());
}

TEST_CASE("PROBE move into a loading node")
{
    ProbeScene scene;
    Tree *tree=scene.tree;
    tree->addNode(Root, 1, true);   // lazy
    tree->addNode(Root, 2);
    tree->addNode(Root, 3);
    tree->setExpanded(1, true);     // request, placeholder
    scene.frame();
    printf("lazy expanded: %s\n", str(scene.visible()).c_str());
    REQUIRE(tree->moveNode(3, 1, 0));
    scene.frame();
    printf("moved 3 into loading 1: %s (children of 1: %zu, loading %d)\n", str(scene.visible()).c_str(),
        tree->getNode(1)->children.size(), (int)tree->getNode(1)->loading);
    // a rebuild: collapse and expand again
    tree->setExpanded(1, false);
    tree->setExpanded(1, true);
    printf("collapse+expand: %s\n", str(scene.visible()).c_str());
    // the answer, appended after the moved node in the app's order
    REQUIRE(tree->insertNodes(1, 99, {100, 101}));
    scene.frame();
    printf("answered at end: %s app children:", str(scene.visible()).c_str());
    for(Id id : tree->getNode(1)->children)
        printf(" %zu", id);
    printf("\n");
}

TEST_CASE("PROBE move into a loading node, answer at end, no collapse")
{
    ProbeScene scene;
    Tree *tree=scene.tree;
    tree->addNode(Root, 1, true);
    tree->addNode(Root, 2);
    tree->addNode(Root, 3);
    tree->setExpanded(1, true);
    scene.frame();
    REQUIRE(tree->moveNode(3, 1, 0));
    scene.frame();
    printf("moved: %s\n", str(scene.visible()).c_str());
    REQUIRE(tree->insertNodes(1, 99, {100, 101}));
    scene.frame();
    printf("answered at end: %s app children:", str(scene.visible()).c_str());
    for(Id id : tree->getNode(1)->children)
        printf(" %zu", id);
    printf("\n");
}
