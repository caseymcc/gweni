/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TabButton::setClosable and TabControl::onClose on a real Canvas, drawn by
 *  the (un-init()'d) textured skin on a recording StubRenderer and driven
 *  through the input handler.
 *
 *  A closable tab owns a TabCloseButton, which resolves to the window close
 *  button's renderer (two slots, a cross of icon kind 2), in a
 *  CloseButtonSize square at the start of the tab's right padding, so the tab
 *  is that much wider than a non-closable one with the same caption and does
 *  not change width as the button shows and hides; on a Left/Right strip the
 *  square is at the tab's right end. The button shows on the selected tab and
 *  on the hovered one (the tab, or the button itself under the mouse), and a
 *  hover writes no slot outside that tab and its button.
 *
 *  A click on it fires onClose once, the TabButton as the event's control. A
 *  handler that vetoes keeps the tab and the selection where they were: the
 *  press neither selects the tab nor starts a tab drag. Unvetoed, the tab
 *  leaves the strip (onLoseTabCaller fires); closing the selected tab selects
 *  the next one, or the previous one when it was the last; closing another
 *  leaves the selection alone; closing the only one leaves an empty
 *  TabControl. The button, its page and their ids are gone a frame later.
 */
#include "headlessFixture.h"

#include <gweni/controls/label.h>
#include <gweni/controls/tabControl.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

//! Records every slot drawn into, shown or hidden.
class SlotRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color, int) override
    {
        touched.push_back(primitiveId);
    }

    void drawIcon(size_t primitiveId, Rect, int kind, int, Color, int) override
    {
        touched.push_back(primitiveId);
        iconKinds.push_back(kind);
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        touched.push_back(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        touched.push_back(primitiveId);
    }

    std::vector<size_t> touched;
    std::vector<int> iconKinds;
};

struct Scene
{
    SlotRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! A TabControl of closable tabs, one per caption, the first selected.
    controls::TabControl *tabControl(std::vector<String> captions, bool closable=true)
    {
        controls::TabControl *control=canvas->newChild<controls::TabControl>();
        control->setDock(DockPosition::Center);

        for(auto &&caption : captions)
            control->addPage(caption)->setClosable(closable);
        return control;
    }

    void frame()
    {
        renderer.touched.clear();
        renderer.iconKinds.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void moveTo(Point at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    static Point centreOf(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    //! A point on the tab clear of its close button's square.
    static Point bodyOf(controls::TabButton *tab)
    {
        const Point at=tab->localPosToCanvas();

        return Point(at.x+4, at.y+tab->getHeight()/2);
    }

    void moveOff()
    {
        moveTo(Point(399, 299));
    }

    //! A whole left click at \p at, then a frame.
    void clickAt(Point at)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    void clickClose(controls::TabButton *tab)
    {
        // the button shows on a hovered tab, so the mouse comes onto the tab first
        moveTo(bodyOf(tab));
        REQUIRE(tab->getCloseButton()->visible());
        clickAt(centreOf(tab->getCloseButton()));
    }

    static const std::vector<size_t> &ids(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    //! True if every slot touched since the last frame() is one of these controls'.
    bool touchedOnly(std::vector<controls::Base *> controls) const
    {
        for(size_t id : renderer.touched)
        {
            bool own=false;

            for(controls::Base *control : controls)
            {
                const std::vector<size_t> &mine=ids(control);

                if(std::find(mine.begin(), mine.end(), id) != mine.end())
                    own=true;
            }
            if(!own)
                return false;
        }
        return true;
    }
};

std::vector<controls::TabButton *> tabs(controls::TabControl *control)
{
    std::vector<controls::TabButton *> result;

    for(int i=0; i < control->tabCount(); ++i)
        result.push_back(control->getTab(i));
    return result;
}

//! Counts a Caller's calls and keeps the control of the last one; vetoes when asked.
class Counter: public event::Handler
{
public:
    void count(event::Info info)
    {
        ++calls;
        last=info.control;
        caller=info.controlCaller;

        if(veto)
            gweni_cast<controls::TabControl>(info.controlCaller)->vetoClose();
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
    bool veto=false;
    controls::Base *last=nullptr;
    controls::Base *caller=nullptr;
};

}//namespace

TEST_CASE("a closable tab's close button is the window close button's renderer in a square of its padding")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"Same", "Same", "Other"});
    controls::TabButton *plain=control->getTab(1);
    plain->setClosable(false);
    scene.frame();

    controls::TabButton *closable=control->getTab(0);
    controls::TabCloseButton *close=closable->getCloseButton();
    REQUIRE(close != nullptr);
    CHECK(closable->isClosable());
    CHECK(!plain->isClosable());
    CHECK(plain->getCloseButton() == nullptr);

    CHECK(dynamic_cast<const skin::textured::WindowCloseButton *>(close->getSkinControl()) != nullptr);
    CHECK(Scene::ids(close).size() == 2);

    // the same caption, the close square more
    const int size=controls::TabButton::CloseButtonSize;
    CHECK(closable->getWidth() == plain->getWidth()+size);
    CHECK(closable->getPadding().right == plain->getPadding().right+size);
    CHECK(closable->getHeight() == plain->getHeight());

    // at the start of the right padding, centred between the top and bottom padding
    const Rect &bounds=close->getBounds();
    const Padding &padding=closable->getPadding();
    CHECK(bounds.w == size);
    CHECK(bounds.h == size);
    CHECK(bounds.x == closable->getWidth()-padding.right);
    CHECK(bounds.y == padding.top+(closable->getHeight()-padding.top-padding.bottom-size)/2);
    // the text ends before it
    CHECK(closable->textWidth()+padding.left <= bounds.x);

    // the cross
    scene.frame();
    close->addStateChange(controls::StateChange_Background);
    scene.frame();
    CHECK(std::find(scene.renderer.iconKinds.begin(), scene.renderer.iconKinds.end(), 2)
        != scene.renderer.iconKinds.end());

    // turned off, the button goes and the tab is its plain width again
    closable->setClosable(false);
    scene.frame();
    CHECK(closable->getCloseButton() == nullptr);
    CHECK(closable->getWidth() == plain->getWidth());
}

TEST_CASE("on a Left or Right strip the close button is at the tab's right end")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"A", "Longer caption"});

    for(DockPosition dock : {DockPosition::Left, DockPosition::Right, DockPosition::Bottom})
    {
        const String dockName=getDockPositionName(dock);
        CAPTURE(dockName);
        control->setTabStripPosition(dock);
        scene.frame();

        for(controls::TabButton *tab : tabs(control))
        {
            const Rect &bounds=tab->getCloseButton()->getBounds();
            const Padding &padding=tab->getPadding();

            CHECK(bounds.x+bounds.w == tab->getWidth()-(padding.right-controls::TabButton::CloseButtonSize));
            CHECK(bounds.x >= padding.left);
            CHECK(bounds.y >= 0);
            CHECK(bounds.y+bounds.h <= tab->getHeight());
        }
    }
}

TEST_CASE("the close button shows on the selected and the hovered tab, and the tab keeps its width")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"First", "Second", "Third"});
    scene.frame();

    std::vector<controls::TabButton *> tab=tabs(control);
    std::vector<Rect> bounds;
    for(controls::TabButton *t : tab)
        bounds.push_back(t->getBounds());

    auto sameBounds=[&]()
    {
        for(size_t i=0; i < tab.size(); ++i)
        {
            if(!(tab[i]->getBounds() == bounds[i]))
                return false;
        }
        return true;
    };

    CHECK(control->getCurrentButton() == tab[0]);
    CHECK(tab[0]->getCloseButton()->visible());
    CHECK(!tab[1]->getCloseButton()->visible());
    CHECK(!tab[2]->getCloseButton()->visible());

    // hovering the second tab shows its button, and writes only its slots
    scene.moveTo(Scene::bodyOf(tab[1]));
    CHECK(tab[1]->isTabHovered());
    CHECK(tab[1]->getCloseButton()->visible());
    CHECK(!tab[2]->getCloseButton()->visible());
    CHECK(scene.touchedOnly({tab[1], tab[1]->getCloseButton()}));
    CHECK(sameBounds());

    // a settled frame draws nothing
    scene.frame();
    CHECK(scene.renderer.touched.empty());

    // onto the button itself: the tab is still hovered, the button still shown
    scene.moveTo(Scene::centreOf(tab[1]->getCloseButton()));
    CHECK(HoveredControl == tab[1]->getCloseButton());
    CHECK(tab[1]->isTabHovered());
    CHECK(tab[1]->getCloseButton()->visible());
    CHECK(scene.touchedOnly({tab[1], tab[1]->getCloseButton()}));
    CHECK(sameBounds());

    // back onto the tab, then off it
    scene.moveTo(Scene::bodyOf(tab[1]));
    CHECK(tab[1]->getCloseButton()->visible());
    scene.moveOff();
    CHECK(!tab[1]->isTabHovered());
    CHECK(!tab[1]->getCloseButton()->visible());
    CHECK(tab[0]->getCloseButton()->visible());
    CHECK(sameBounds());

    // selecting the third moves the selected tab's button there
    scene.clickAt(Scene::bodyOf(tab[2]));
    scene.moveOff();
    CHECK(control->getCurrentButton() == tab[2]);
    CHECK(!tab[0]->getCloseButton()->visible());
    CHECK(tab[2]->getCloseButton()->visible());
    CHECK(sameBounds());

    // a disabled tab shows none
    tab[2]->setDisabled(true);
    scene.frame();
    CHECK(!tab[2]->getCloseButton()->visible());
}

TEST_CASE("closing the selected tab selects the next one, or the previous one when it was the last")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"One", "Two", "Three", "Four"});
    Counter closes, loses;
    closes.listen(control->onClose);
    loses.listen(control->onLoseTabCaller);
    scene.frame();

    std::vector<controls::TabButton *> tab=tabs(control);

    // the second selected, then closed: the third is selected
    scene.clickAt(Scene::bodyOf(tab[1]));
    REQUIRE(control->getCurrentButton() == tab[1]);
    controls::Base *page=tab[1]->getPage();
    scene.clickClose(tab[1]);

    CHECK(closes.calls == 1);
    CHECK(closes.last == tab[1]);
    CHECK(closes.caller == control);
    CHECK(loses.calls == 1);
    CHECK(control->tabCount() == 3);
    CHECK(control->getCurrentButton() == tab[2]);
    CHECK(!tab[2]->getPage()->hidden());
    // the frame after the click deleted the button and its page
    const controls::Base::List &children=control->getChildren();
    CHECK(std::find(children.begin(), children.end(), page) == children.end());

    // the last selected, then closed: the one before it is selected
    scene.clickAt(Scene::bodyOf(tab[3]));
    REQUIRE(control->getCurrentButton() == tab[3]);
    scene.clickClose(tab[3]);
    CHECK(closes.calls == 2);
    CHECK(control->tabCount() == 2);
    CHECK(control->getCurrentButton() == tab[2]);
    CHECK(!tab[2]->getPage()->hidden());

    // another tab closed: the selection stays
    scene.clickClose(tab[0]);
    CHECK(closes.calls == 3);
    CHECK(control->tabCount() == 1);
    CHECK(control->getCurrentButton() == tab[2]);
    CHECK(control->getTab(0) == tab[2]);

    // the only one: an empty TabControl
    scene.clickClose(tab[2]);
    CHECK(closes.calls == 4);
    CHECK(loses.calls == 4);
    CHECK(control->tabCount() == 0);
    CHECK(control->getCurrentButton() == nullptr);

    scene.frame();
    scene.moveOff();
    scene.frame();
    CHECK(control->tabCount() == 0);
}

TEST_CASE("a vetoed close keeps the tab, and the press neither selects it nor drags it")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"Saved", "Unsaved", "Other"});
    control->setAllowReorder(true);
    Counter closes;
    closes.veto=true;
    closes.listen(control->onClose);
    scene.frame();

    std::vector<controls::TabButton *> tab=tabs(control);
    REQUIRE(control->getCurrentButton() == tab[0]);

    scene.clickClose(tab[1]);
    CHECK(closes.calls == 1);
    CHECK(control->tabCount() == 3);
    CHECK(control->getTab(1) == tab[1]);
    // not selected by the press on its button
    CHECK(control->getCurrentButton() == tab[0]);
    CHECK(tab[1]->getPage()->hidden());

    // pressed on the button and dragged along the strip: no tab drag
    const Point on=Scene::centreOf(tab[1]->getCloseButton());
    scene.clock+=1.0f;
    scene.canvas->inputMouseMoved(on.x, on.y, 0, 0);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    scene.moveTo(Point(on.x+40, on.y));
    CHECK(draganddrop::CurrentPackage == nullptr);
    CHECK(!tab[1]->hidden());
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(closes.calls == 1);
    CHECK(control->getTab(1) == tab[1]);
    CHECK(control->getCurrentButton() == tab[0]);

    // the veto lifted, the next click closes it
    closes.veto=false;
    scene.clickClose(tab[1]);
    CHECK(closes.calls == 2);
    CHECK(control->tabCount() == 2);
    CHECK(control->getCurrentButton() == tab[0]);
}

TEST_CASE("a closed tab's button, close button and page give their ids back")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl({"Kept"});
    scene.frame();
    scene.moveOff();

    const test::IdBankCounts baseline=test::IdBankCounts::now();
    controls::TabButton *added=control->addPage("Closed");
    added->setClosable(true);
    added->getPage()->newChild<controls::Label>()->setText("on the page");
    scene.frame();

    const std::vector<size_t> closeIds=Scene::ids(added->getCloseButton());
    REQUIRE(closeIds.size() == 2);
    CHECK(test::IdBankCounts::now().livePrimitives() > baseline.livePrimitives());

    scene.clickClose(added);
    scene.moveOff();
    scene.frame();

    CHECK(control->tabCount() == 1);
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
    const std::vector<size_t> &released=releasedPrimitiveIds();
    for(size_t id : closeIds)
        CHECK(std::find(released.begin(), released.end(), id) != released.end());
}
