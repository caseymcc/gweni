/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TabControl's overflow and its tab list on a real Canvas, drawn by the
 *  (un-init()'d) textured skin on a recording StubRenderer and driven through
 *  the input handler.
 *
 *  A Top or Bottom strip whose tabs fit shows no button and clips nothing.
 *  Once they do not fit, the left and right scroll buttons show in a column at
 *  the strip's trailing end, centred on the row, and the strip is clipped to
 *  the row, from its origin (where the first tab starts) to where the visible
 *  tabs end (getTabsRight(), ColumnGap before the column), so no tab draws or
 *  takes the mouse under the buttons or in the strip's padding. A press on
 *  the right button scrolls by one tab, putting the next tab where the first
 *  one starts, until the last tab's right edge is at getTabsRight(); the left
 *  one goes back a tab at a time to 0; a press past either end changes
 *  nothing. A press marks nothing outside the strip's and the buttons'
 *  subtrees, and draws into no slot but theirs. Selecting a tab scrolls it fully into view. Growing the
 *  control until the tabs fit, or closing tabs until they do, hides the
 *  buttons and puts the offset back to 0, and a control that shrinks keeps
 *  the last tab at the column. A Left strip never overflows.
 *
 *  setTabListButton(true) shows a down chevron at the column's right end,
 *  whether or not the tabs overflow. A press opens the canvas's Popup below it
 *  with one Selectable row per tab in strip order, captioned as the tab and
 *  the selected tab's row selected; the rows are made when it opens and
 *  reused by the next opening. Choosing a row selects that tab, scrolls it
 *  fully into view and closes the popup; a second press on the button,
 *  Escape and a press outside close it too, and a tab closed while it is open
 *  is not selected from it. Deleting the TabControl with the list open closes
 *  and deletes the popup; a canvas deleted with it open deletes both. The
 *  popup is the TabControl's (Base::createPopup, #490), so a canvas deleted
 *  with the list opened (open or closed again) and the TabControl in a
 *  WindowControl brought to the front after it deletes each once, and a
 *  TabControl taken off the canvas and deleted after it touches nothing of
 *  the popup the canvas deleted.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/selectable.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/windowControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
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
    void drawFilledRect(size_t primitiveId, Rect, int) override
    {
        touched.push_back(primitiveId);
    }

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color, int) override
    {
        touched.push_back(primitiveId);
    }

    void drawIcon(size_t primitiveId, Rect, int, int, Color, int) override
    {
        touched.push_back(primitiveId);
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
};

const char *const g_captions[]=
{
    "main.cpp", "app.h", "app.cpp", "window.h", "window.cpp", "render.cpp",
    "shaders.glsl", "README.md", "CMakeLists.txt", "notes.txt", "todo.md", "LICENSE"
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
        canvas->setSize(500, 400);
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

    //! A \p width wide TabControl (Fixed, so its tabs cannot widen it) with the
    //! first \p count captions, the first tab selected.
    controls::TabControl *tabControl(int count, int width=300)
    {
        controls::TabControl *control=canvas->newChild<controls::TabControl>();

        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(10, 10, width, 150);
        for(int i=0; i < count; ++i)
            control->addPage(g_captions[i]);
        return control;
    }

    void frame()
    {
        renderer.touched.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
            frame();
    }

    void moveTo(Point at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void moveOff()
    {
        moveTo(Point(499, 399));
    }

    //! A whole left click at \p at, without a frame.
    void click(Point at)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
    }

    void clickAt(Point at)
    {
        click(at);
        frame();
    }

    void escape()
    {
        input::onKeyEvent(canvas, keys::Escape, true);
        input::onKeyEvent(canvas, keys::Escape, false);
        frame();
    }

    static Point centreOf(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }
};

//! The shown tabs, in strip order.
std::vector<controls::TabButton *> tabs(controls::TabControl *control)
{
    std::vector<controls::TabButton *> result;

    for(int i=0; i < control->tabCount(); ++i)
    {
        if(!control->getTab(i)->hidden())
            result.push_back(control->getTab(i));
    }
    return result;
}

//! A tab's left and right edges in its TabControl's coordinates.
int leftIn(controls::TabControl *control, controls::Base *tab)
{
    return tab->localPosToCanvas().x-control->localPosToCanvas().x;
}

int rightIn(controls::TabControl *control, controls::Base *tab)
{
    return leftIn(control, tab)+tab->getWidth();
}

bool inSubtree(controls::Base *control, controls::Base *root)
{
    for(controls::Base *at=control; at; at=at->getParent())
    {
        if(at == root)
            return true;
    }
    return false;
}

//! The controls with a change pending that are neither under \p root nor one
//! of \p others.
void pendingBesides(controls::Base *control, controls::Base *root,
    const std::vector<controls::Base *> &others, std::vector<controls::Base *> &found)
{
    if(control->getStateChange() != controls::StateChange_Nothing && !inSubtree(control, root)
        && std::find(others.begin(), others.end(), control) == others.end())
        found.push_back(control);
    for(auto &&child : control->getActualChildren())
        pendingBesides(child, root, others, found);
}

//! Every primitive id the controls under \p root own.
void ownedIds(controls::Base *root, std::vector<size_t> &ids)
{
    if(const skin::Control *control=root->getSkinControl())
    {
        const std::vector<size_t> &mine=control->getPrimitives();

        ids.insert(ids.end(), mine.begin(), mine.end());
    }
    for(auto &&child : root->getActualChildren())
        ownedIds(child, ids);
}

std::vector<controls::Base *> buttonsOf(controls::TabControl *control)
{
    return {control->getScrollButton(0), control->getScrollButton(1), control->getTabListButton()};
}

//! Presses a scroll button until the offset stops changing, at most \p limit times.
void pressUntilStill(Scene &scene, controls::TabControl *control, int button, int limit=32)
{
    for(int i=0; i < limit; ++i)
    {
        const int before=control->getScrollOffset();

        scene.clickAt(Scene::centreOf(control->getScrollButton(button)));
        if(control->getScrollOffset() == before)
            return;
    }
}

}//namespace

TEST_CASE("TabControl overflow: the buttons show only when the tabs do not fit, and the tabs end before them")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(3);

    scene.settle();
    CHECK_FALSE(control->isOverflowing());
    CHECK_FALSE(control->getScrollButton(0)->visible());
    CHECK_FALSE(control->getScrollButton(1)->visible());
    CHECK_FALSE(control->getTabListButton()->visible());
    CHECK((control->getChildClipRect(control->getTabStrip()) == nullptr));
    CHECK(control->getScrollOffset() == 0);

    for(int i=3; i < 12; ++i)
        control->addPage(g_captions[i]);
    scene.settle();

    REQUIRE(control->isOverflowing());
    controls::Base *left=control->getScrollButton(0);
    controls::Base *right=control->getScrollButton(1);

    CHECK(left->visible());
    CHECK(right->visible());
    CHECK_FALSE(control->getTabListButton()->visible());
    CHECK(left->getWidth() == controls::TabControl::ButtonSize);
    CHECK(left->getHeight() == controls::TabControl::ButtonSize);

    // the column: left, then right, ending ColumnInset in from the right edge
    CHECK(right->getX() == left->getX()+controls::TabControl::ButtonSize+controls::TabControl::ButtonSpacing);
    CHECK(right->getX()+right->getWidth() == control->getWidth()-controls::TabControl::ColumnInset);
    CHECK(control->getTabsRight() == left->getX()-controls::TabControl::ColumnGap);

    // centred on the row
    controls::TabStrip *strip=control->getTabStrip();
    const int rowMiddle=strip->getY()+strip->getHeight()/2;

    CHECK(std::abs(left->getY()+left->getHeight()/2-rowMiddle) <= 1);
    CHECK(right->getY() == left->getY());

    // the strip is cut where the visible tabs end: no tab is hit under the gap
    // or the column, and the buttons are
    const Rect *clip=control->getChildClipRect(strip);

    REQUIRE((clip != nullptr));
    // from where the first tab starts, unscrolled
    CHECK(clip->x == leftIn(control, tabs(control).front()));
    CHECK(clip->x+clip->w == control->getTabsRight());

    const Point origin=control->localPosToCanvas();
    const int rowY=origin.y+rowMiddle;
    controls::Base *inGap=scene.canvas->getControlAt(origin.x+control->getTabsRight()+1, rowY);

    CHECK(gweni_cast<controls::TabButton>(inGap) == nullptr);
    CHECK(inSubtree(inGap, strip) == false);
    CHECK(scene.canvas->getControlAt(Scene::centreOf(right).x, Scene::centreOf(right).y) == right);

    // a tab under the column is still laid out there, past the clip
    bool pastClip=false;

    for(controls::TabButton *tab : tabs(control))
    {
        if(rightIn(control, tab) > control->getTabsRight())
            pastClip=true;
    }
    CHECK(pastClip);
}

TEST_CASE("TabControl overflow: each press scrolls by a tab and stops at both ends")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(12);

    scene.settle();
    REQUIRE(control->isOverflowing());

    const std::vector<controls::TabButton *> row=tabs(control);
    const int firstLeft=leftIn(control, row[0]);
    controls::Base *rightButton=control->getScrollButton(1);
    controls::Base *leftButton=control->getScrollButton(0);
    std::vector<controls::Base *> buttons=buttonsOf(control);
    std::vector<size_t> allowed;

    ownedIds(control->getTabStrip(), allowed);
    for(controls::Base *button : buttons)
        ownedIds(button, allowed);

    // a press before the start does nothing
    scene.moveTo(Scene::centreOf(leftButton));
    scene.clickAt(Scene::centreOf(leftButton));
    CHECK(control->getScrollOffset() == 0);

    // each press on the right one puts the next tab where the first started
    scene.moveTo(Scene::centreOf(rightButton));
    size_t next=1;

    for(; next < row.size(); ++next)
    {
        const int before=control->getScrollOffset();

        scene.click(Scene::centreOf(rightButton));
        // the frame's update phase marks the strip's subtree and the buttons only
        scene.renderer.touched.clear();
        scene.canvas->updateCanvas();

        std::vector<controls::Base *> elsewhere;

        pendingBesides(scene.canvas, control->getTabStrip(), buttons, elsewhere);
        CHECK(elsewhere.empty());
        scene.canvas->renderCanvas();

        for(size_t id : scene.renderer.touched)
            CHECK(std::find(allowed.begin(), allowed.end(), id) != allowed.end());

        if(control->getScrollOffset() == control->getMaxScrollOffset())
            break;
        CHECK(control->getScrollOffset() > before);
        CHECK(leftIn(control, row[next]) == firstLeft);
    }

    // stopped with the last tab's right edge where the visible tabs end
    CHECK(control->getScrollOffset() == control->getMaxScrollOffset());
    CHECK(control->getMaxScrollOffset() > 0);
    CHECK(rightIn(control, row.back()) == control->getTabsRight());

    const int atEnd=control->getScrollOffset();

    scene.clickAt(Scene::centreOf(rightButton));
    CHECK(control->getScrollOffset() == atEnd);

    // the left one goes back a tab at a time, to 0
    scene.moveTo(Scene::centreOf(leftButton));
    scene.clickAt(Scene::centreOf(leftButton));
    CHECK(control->getScrollOffset() < atEnd);
    pressUntilStill(scene, control, 0);
    CHECK(control->getScrollOffset() == 0);
    CHECK(leftIn(control, row[0]) == firstLeft);

    // and settled, nothing is drawn
    scene.moveOff();
    scene.settle();
    scene.frame();
    CHECK(scene.renderer.touched.empty());
}

TEST_CASE("TabControl overflow: selecting a tab scrolls it fully into view")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(12);

    scene.settle();

    const std::vector<controls::TabButton *> row=tabs(control);

    // the last tab, selected from code, ends at the column
    row.back()->doAction();
    scene.settle();
    CHECK(control->getCurrentButton() == row.back());
    CHECK(rightIn(control, row.back()) == control->getTabsRight());

    // the first, back at the start
    row.front()->doAction();
    scene.settle();
    CHECK(control->getScrollOffset() == 0);

    // a tab cut by the column, clicked where it shows, comes fully into view
    controls::TabButton *cut=nullptr;

    for(controls::TabButton *tab : row)
    {
        if(leftIn(control, tab) < control->getTabsRight() && rightIn(control, tab) > control->getTabsRight())
            cut=tab;
    }
    REQUIRE(cut != nullptr);

    const Point at=control->localPosToCanvas();

    scene.clickAt(Point(at.x+leftIn(control, cut)+3, Scene::centreOf(cut).y));
    scene.settle();
    CHECK(control->getCurrentButton() == cut);
    CHECK(rightIn(control, cut) == control->getTabsRight());
    CHECK(leftIn(control, cut) >= 0);
}

TEST_CASE("TabControl overflow: growing, shrinking and closing tabs keep the offset valid")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(12);

    scene.settle();
    pressUntilStill(scene, control, 1);
    REQUIRE(control->getScrollOffset() > 0);

    const std::vector<controls::TabButton *> row=tabs(control);

    // wide enough for every tab: no buttons, no clip, offset 0
    control->setSize(1000, 150);
    scene.canvas->setSize(1100, 400);
    scene.settle();
    CHECK_FALSE(control->isOverflowing());
    CHECK_FALSE(control->getScrollButton(0)->visible());
    CHECK_FALSE(control->getScrollButton(1)->visible());
    CHECK((control->getChildClipRect(control->getTabStrip()) == nullptr));
    CHECK(control->getScrollOffset() == 0);

    // narrow again: overflowing from the start
    control->setSize(300, 150);
    scene.settle();
    CHECK(control->isOverflowing());
    CHECK(control->getScrollOffset() == 0);

    // scrolled to the end, then a little wider: the last tab stays at the column
    pressUntilStill(scene, control, 1);
    control->setSize(360, 150);
    scene.settle();
    REQUIRE(control->isOverflowing());
    CHECK(control->getScrollOffset() == control->getMaxScrollOffset());
    CHECK(rightIn(control, row.back()) == control->getTabsRight());

    // closing tabs from the end until the rest fit
    while(control->isOverflowing())
    {
        std::vector<controls::TabButton *> left=tabs(control);

        REQUIRE(left.size() > 1);
        control->closeTab(left.back());
        scene.settle();
        CHECK(control->getScrollOffset() <= control->getMaxScrollOffset());
    }
    CHECK(control->getScrollOffset() == 0);
    CHECK_FALSE(control->getScrollButton(0)->visible());
    CHECK_FALSE(control->getScrollButton(1)->visible());
    CHECK((control->getChildClipRect(control->getTabStrip()) == nullptr));
}

TEST_CASE("TabControl overflow: a Bottom strip overflows, a Left strip never does")
{
    Scene scene;
    controls::TabControl *bottom=scene.tabControl(12);

    bottom->setTabStripPosition(DockPosition::Bottom);
    scene.settle();
    REQUIRE(bottom->isOverflowing());

    controls::TabStrip *strip=bottom->getTabStrip();
    controls::Base *button=bottom->getScrollButton(0);

    CHECK(strip->getY() > bottom->getHeight()/2);
    CHECK(std::abs(button->getY()+button->getHeight()/2-(strip->getY()+strip->getHeight()/2)) <= 1);

    bottom->setTabStripPosition(DockPosition::Left);
    bottom->setTabListButton(true);
    scene.settle();
    CHECK_FALSE(bottom->isOverflowing());
    CHECK_FALSE(bottom->getScrollButton(0)->visible());
    CHECK_FALSE(bottom->getTabListButton()->visible());
    CHECK(bottom->getScrollOffset() == 0);
    CHECK(strip->getScrollOffset() == 0);
}

TEST_CASE("TabControl tab list: a Popup of every tab, choosing one selects it and scrolls it into view")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(3);

    control->setTabListButton(true);
    scene.settle();

    controls::Base *list=control->getTabListButton();

    // shown while the tabs fit, alone at the column's right end
    CHECK_FALSE(control->isOverflowing());
    CHECK(list->visible());
    CHECK_FALSE(control->getScrollButton(0)->visible());
    CHECK(list->getX()+list->getWidth() == control->getWidth()-controls::TabControl::ColumnInset);
    CHECK(control->getTabsRight() == list->getX()-controls::TabControl::ColumnGap);
    CHECK(rightIn(control, tabs(control).back()) <= control->getTabsRight());

    for(int i=3; i < 12; ++i)
        control->addPage(g_captions[i]);
    scene.settle();
    REQUIRE(control->isOverflowing());
    // the list button stays last, the arrows before it
    CHECK(control->getScrollButton(1)->getX()+controls::TabControl::ButtonSize+controls::TabControl::ButtonSpacing
        == list->getX());

    const std::vector<controls::TabButton *> row=tabs(control);

    scene.moveTo(Scene::centreOf(list));
    scene.clickAt(Scene::centreOf(list));
    scene.frame();

    controls::Popup *popup=control->getTabListPopup();

    REQUIRE(popup != nullptr);
    CHECK(popup->isOpen());
    CHECK(scene.canvas->shownPopup() == popup);
    CHECK(popup->getAnchor() == list);
    // below the button
    CHECK(popup->localPosToCanvas().y >= list->localPosToCanvas().y+list->getHeight());

    // one row per tab, in strip order, the selected tab's row selected
    std::vector<controls::Selectable *> rows;

    for(auto &&child : popup->getChildren())
    {
        if(controls::Selectable *selectable=gweni_cast<controls::Selectable>(child))
            rows.push_back(selectable);
    }
    REQUIRE(rows.size() == row.size());
    for(size_t i=0; i < rows.size(); ++i)
    {
        CHECK(rows[i]->getText() == row[i]->getText());
        CHECK(rows[i]->isSelected() == (row[i] == control->getCurrentButton()));
    }

    // a second press closes it
    scene.clickAt(Scene::centreOf(list));
    CHECK_FALSE(popup->isOpen());

    // the next opening reuses the rows
    scene.clickAt(Scene::centreOf(list));
    scene.frame();
    REQUIRE(popup->isOpen());
    std::vector<controls::Selectable *> again;

    for(auto &&child : popup->getChildren())
    {
        if(controls::Selectable *selectable=gweni_cast<controls::Selectable>(child))
            again.push_back(selectable);
    }
    CHECK(again == rows);

    // choosing the tenth: selected, fully in view, the popup closed
    scene.moveTo(Scene::centreOf(rows[9]));
    scene.clickAt(Scene::centreOf(rows[9]));
    scene.settle();
    CHECK_FALSE(popup->isOpen());
    CHECK(control->getCurrentButton() == row[9]);
    CHECK(rightIn(control, row[9]) <= control->getTabsRight());
    CHECK(leftIn(control, row[9]) >= 0);

    // reopened, the tenth row is the selected one
    scene.clickAt(Scene::centreOf(list));
    scene.frame();
    REQUIRE(popup->isOpen());
    CHECK(rows[9]->isSelected());
    CHECK_FALSE(rows[0]->isSelected());

    // Escape closes it
    scene.escape();
    CHECK_FALSE(popup->isOpen());

    // and so does a press outside it
    scene.clickAt(Scene::centreOf(list));
    scene.frame();
    REQUIRE(popup->isOpen());
    scene.clickAt(Point(450, 350));
    CHECK_FALSE(popup->isOpen());

    // the first tab, chosen from the list, scrolls the row back to the start
    scene.clickAt(Scene::centreOf(list));
    scene.frame();
    REQUIRE(popup->isOpen());
    scene.moveTo(Scene::centreOf(rows[0]));
    scene.clickAt(Scene::centreOf(rows[0]));
    scene.settle();
    CHECK(control->getCurrentButton() == row[0]);
    CHECK(control->getScrollOffset() == 0);

    // turned off, the button goes and the tabs may use its room
    control->setTabListButton(false);
    scene.settle();
    CHECK_FALSE(list->visible());
}

TEST_CASE("TabControl tab list: a tab closed while the list is open is not selected from it")
{
    Scene scene;
    controls::TabControl *control=scene.tabControl(4);

    control->setTabListButton(true);
    scene.settle();

    controls::Base *list=control->getTabListButton();

    scene.clickAt(Scene::centreOf(list));
    scene.frame();

    controls::Popup *popup=control->getTabListPopup();

    REQUIRE(popup != nullptr);
    REQUIRE(popup->isOpen());

    std::vector<controls::Selectable *> rows;

    for(auto &&child : popup->getChildren())
    {
        if(controls::Selectable *selectable=gweni_cast<controls::Selectable>(child))
            rows.push_back(selectable);
    }
    REQUIRE(rows.size() == 4);

    controls::TabButton *selected=control->getCurrentButton();

    control->closeTab(control->getTab(3));
    scene.frame();
    REQUIRE(popup->isOpen());
    scene.clickAt(Scene::centreOf(rows[3]));
    scene.settle();
    CHECK_FALSE(popup->isOpen());
    CHECK(control->getCurrentButton() == selected);
}

TEST_CASE("TabControl tab list: deleting the control, or the canvas, with the list open")
{
    {
        Scene scene;
        controls::TabControl *control=scene.tabControl(4);

        control->setTabListButton(true);
        scene.settle();
        scene.clickAt(Scene::centreOf(control->getTabListButton()));
        scene.frame();
        REQUIRE(control->getTabListPopup() != nullptr);
        REQUIRE(control->getTabListPopup()->isOpen());

        // the popup goes with it: closed, then deleted with it
        control->delayedDelete();
        scene.settle();
        CHECK(scene.canvas->shownPopup() == nullptr);
        for(auto &&child : scene.canvas->getChildren())
            CHECK(gweni_cast<controls::Popup>(child) == nullptr);
    }
    {
        // the canvas deletes the open popup and the control, in either order
        Scene scene;
        controls::TabControl *control=scene.tabControl(4);

        control->setTabListButton(true);
        scene.settle();
        scene.clickAt(Scene::centreOf(control->getTabListButton()));
        scene.frame();
        REQUIRE(control->getTabListPopup()->isOpen());
    }
}

TEST_CASE("TabControl tab list: a canvas deleted with the control in a WindowControl brought to the front, or detached first (#490)")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    for(const bool leaveOpen : {false, true})
    {
        for(const bool detach : {false, true})
        {
            CAPTURE(leaveOpen);
            CAPTURE(detach);
            SlotRecorder renderer;
            skin::textured::TexturedSkin skin{&renderer};
            controls::Canvas *canvas=new controls::Canvas(&skin);
            canvas->setSize(500, 400);

            auto *window=canvas->newChild<controls::WindowControl>();
            window->setBounds(20, 20, 400, 300);
            controls::TabControl *control=window->newChild<controls::TabControl>();
            control->setDock(DockPosition::Center);
            for(int i=0; i < 4; ++i)
                control->addPage(g_captions[i]);
            control->setTabListButton(true);
            for(int i=0; i < 3; ++i)
            {
                canvas->updateCanvas();
                canvas->renderCanvas();
            }

            control->toggleTabList();
            REQUIRE(control->getTabListPopup() != nullptr);
            CHECK(control->getTabListPopup()->getOwner() == control);
            REQUIRE(control->getTabListPopup()->isOpen());
            if(!leaveOpen)
                control->toggleTabList();
            window->bringToFront();
            canvas->updateCanvas();
            canvas->renderCanvas();
            if(detach)
                control->setParent(nullptr);

            HoveredControl=nullptr;
            KeyboardFocus=nullptr;
            MouseFocus=nullptr;
            delete canvas;
            if(detach)
                delete control;
        }
    }
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}
