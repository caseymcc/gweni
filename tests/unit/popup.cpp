/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Popup on a real Canvas, clicked through input::onMouseClicked and
 *  keyed through input::onKeyEvent. Popup::placement on its own: below the
 *  anchor, above it at the bottom edge, right edges aligned at the right edge,
 *  beside it (Right) and flipped to its left, bottom edges aligned at the
 *  bottom, kept inside, and Menu::placement as the zero-size anchor case.
 *  Open: shown below its anchor at its content's size, re-parented to the
 *  canvas, onOpen once; placed above at the bottom edge, beside and flipped,
 *  kept inside a canvas narrower than it, placed again when the canvas shrinks
 *  and grows back and when its anchor moves.
 *
 *  Closing: a left or right press outside it and its anchor (the press still
 *  reaches what it was on), Escape, the anchor hidden (through its parent) or
 *  disabled, the anchor deleted delayed or outright, close() and
 *  setHidden(true), each with one onClose; the popup deleted, delayed or
 *  outright, leaves no shown popup. A press on the anchor or inside the popup
 *  does not close it, so a button toggles its popup; nor does a press on an
 *  open Menu (a ComboBox's list in it). A Popup and a context menu close each
 *  other, one popup opening closes another, and a popup opened again for
 *  another anchor moves there.
 *
 *  Numbering: an open popup's z is above every control outside it, a control
 *  added to the canvas after it opened included, a Menu opened over it stays
 *  above it, and the ToolTip box is above it and the canvas's last child. A
 *  ComboBox in a popup keeps its list closed as the popup opens (its list is
 *  its child, which Base::setHidden would show), and its list opens over the
 *  popup and is chosen from without closing it; Escape closes that list
 *  before the popup, and the popup closing closes it too. A MenuStrip on the
 *  canvas (a menu bar, which never closes) is outside the popup like any
 *  control: Escape still closes the popup, and a press on the strip does.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <algorithm>

using namespace gweni;
using controls::Popup;
using controls::PopupPlacement;

namespace
{

bool same(const Point &a, const Point &b)
{
    return a.x == b.x && a.y == b.y;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        // a second apart per press, so two presses are never a double click
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
        input::onMouseClicked(canvas, 1, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! A plain control at a fixed place on the canvas (or in \p parent).
    controls::Base *box(int x, int y, int w, int h, controls::Base *parent=nullptr)
    {
        controls::Base *control=(parent ? parent : canvas)->newChild<controls::Base>();

        control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        control->setBounds(x, y, w, h);
        return control;
    }

    //! A popup holding one \p w x \p h control, made on the canvas (or in
    //! \p parent).
    Popup *popup(int w=80, int h=40, controls::Base *parent=nullptr)
    {
        Popup *popup=(parent ? parent : canvas)->newChild<Popup>();

        box(0, 0, w, h, popup);
        return popup;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void click(int x, int y, int button)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        input::onMouseClicked(canvas, button, true);
        input::onMouseClicked(canvas, button, false);
        frame();
    }

    void leftClick(int x, int y) { click(x, y, 0); }
    void rightClick(int x, int y) { click(x, y, 1); }

    //! A left click in the middle of \p control.
    void clickOn(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        leftClick(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void escape()
    {
        input::onKeyEvent(canvas, keys::Escape, true);
        input::onKeyEvent(canvas, keys::Escape, false);
        frame();
    }

    bool onCanvas(controls::Base *control) const
    {
        const auto &children=canvas->getActualChildren();
        return std::find(children.begin(), children.end(), control) != children.end();
    }

    static Rect canvasRect(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();
        return Rect(at.x, at.y, control->getWidth(), control->getHeight());
    }
};

//! Counts a Caller's calls.
class Counter: public event::Handler
{
public:
    void count(event::Info)
    {
        ++calls;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
};

//! A button's handler that opens its popup, or closes it when it is open.
class Toggler: public event::Handler
{
public:
    void toggle(event::Info info)
    {
        if(popup->isOpen())
            popup->close();
        else
            popup->open(info.controlCaller);
    }

    Popup *popup=nullptr;
};

//! The highest z of every control in \p root's subtree but \p skip's.
int highestZ(controls::Base *root, controls::Base *skip)
{
    if(root == skip)
        return 0;

    int highest=root->getZIndex();

    for(controls::Base *child : root->getActualChildren())
        highest=std::max(highest, highestZ(child, skip));
    return highest;
}

//! The lowest z in \p root's subtree.
int lowestZ(controls::Base *root)
{
    int lowest=root->getZIndex();

    for(controls::Base *child : root->getActualChildren())
        lowest=std::min(lowest, lowestZ(child));
    return lowest;
}

}//namespace

TEST_CASE("Popup::placement: below, above at the bottom, beside and flipped, kept inside")
{
    const Rect area(0, 0, 400, 300);
    const Point size(80, 50);
    const int gap=Popup::Gap;

    // below, left edges aligned
    CHECK(same(Popup::placement(Rect(20, 30, 60, 20), size, area, PopupPlacement::Below),
        Point(20, 30+20+gap)));
    // no room below: above
    CHECK(same(Popup::placement(Rect(20, 260, 60, 20), size, area, PopupPlacement::Below),
        Point(20, 260-gap-50)));
    // past the right edge: right edges aligned
    CHECK(same(Popup::placement(Rect(350, 30, 40, 20), size, area, PopupPlacement::Below),
        Point(390-80, 30+20+gap)));
    // both: above, right edges aligned
    CHECK(same(Popup::placement(Rect(350, 260, 40, 20), size, area, PopupPlacement::Below),
        Point(310, 260-gap-50)));

    // beside, top edges aligned
    CHECK(same(Popup::placement(Rect(20, 30, 60, 20), size, area, PopupPlacement::Right),
        Point(20+60+gap, 30)));
    // no room on the right: on the left
    CHECK(same(Popup::placement(Rect(300, 30, 60, 20), size, area, PopupPlacement::Right),
        Point(300-gap-80, 30)));
    // past the bottom: bottom edges aligned
    CHECK(same(Popup::placement(Rect(20, 270, 60, 20), size, area, PopupPlacement::Right),
        Point(82, 290-50)));

    // room on neither side: kept inside, against the left / top edge
    CHECK(same(Popup::placement(Rect(10, 100, 380, 20), Point(390, 50), area, PopupPlacement::Right),
        Point(0, 100)));
    CHECK(same(Popup::placement(Rect(10, 10, 20, 20), Point(80, 290), area, PopupPlacement::Below),
        Point(10, 0)));
    // larger than the area: at its top-left corner
    CHECK(same(Popup::placement(Rect(100, 100, 20, 20), Point(500, 400), area, PopupPlacement::Below),
        Point(0, 0)));
    // an area not at the origin
    CHECK(same(Popup::placement(Rect(105, 105, 10, 10), size, Rect(100, 100, 200, 200), PopupPlacement::Below),
        Point(105, 117)));

    // a zero-size anchor with no gap is a context menu at the cursor
    for(const Point &cursor : {Point(10, 20), Point(250, 20), Point(10, 180), Point(390, 290), Point(50, 30)})
    {
        CHECK(same(Popup::placement(Rect(cursor.x, cursor.y, 0, 0), size, area, PopupPlacement::Below, 0),
            controls::Menu::placement(cursor, size, area)));
    }
}

TEST_CASE("Popup: opens below its anchor at its content's size, on the canvas")
{
    Scene scene;
    controls::Base *panel=scene.box(50, 40, 200, 150);
    controls::Base *anchor=scene.box(10, 10, 60, 20, panel);
    // made inside another control: opening moves it onto the canvas
    Popup *popup=scene.popup(80, 40, panel);
    Counter opens, closes;

    opens.listen(popup->onOpen);
    closes.listen(popup->onClose);
    CHECK(popup->hidden());
    CHECK(!popup->isOpen());
    scene.frame();

    popup->open(anchor);
    CHECK(popup->isOpen());
    CHECK(opens.calls == 1);
    CHECK(closes.calls == 0);
    CHECK(scene.canvas->shownPopup() == popup);
    CHECK(scene.canvas->popupAnchor() == anchor);
    CHECK(popup->getAnchor() == anchor);
    // not a context menu
    CHECK(!scene.canvas->shownContextMenu());
    CHECK(!scene.canvas->contextMenuOwner());
    CHECK((popup->getParent() == scene.canvas));
    scene.frame();

    const Padding &padding=popup->getPadding();
    CHECK(popup->visible());
    CHECK(popup->getWidth() == 80+padding.left+padding.right);
    CHECK(popup->getHeight() == 40+padding.top+padding.bottom);
    CHECK(popup->getX() == 60);
    CHECK(popup->getY() == 40+10+20+Popup::Gap);

    // laid out, it stays put
    const Rect placed=popup->getBounds();
    scene.frame();
    CHECK(same(Point(popup->getX(), popup->getY()), Point(placed.x, placed.y)));
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.canvas->getLayoutStats().requests == 0);
}

TEST_CASE("Popup: flipped and kept inside at the canvas's edges, placed again as the canvas and anchor move")
{
    Scene scene;
    Popup *popup=scene.popup(80, 40);

    scene.frame();

    SUBCASE("above at the bottom edge, right edges aligned at the right")
    {
        controls::Base *anchor=scene.box(350, 270, 40, 20);

        popup->open(anchor);
        scene.frame();
        CHECK(popup->getX()+popup->getWidth() == 390);
        CHECK(popup->getY()+popup->getHeight() == 270-Popup::Gap);
    }

    SUBCASE("beside, and flipped to the left at the right edge")
    {
        controls::Base *left=scene.box(20, 30, 60, 20);
        controls::Base *right=scene.box(330, 30, 60, 20);

        popup->open(left, PopupPlacement::Right);
        scene.frame();
        CHECK(popup->getPlacement() == PopupPlacement::Right);
        CHECK(popup->getX() == 80+Popup::Gap);
        CHECK(popup->getY() == 30);

        popup->open(right, PopupPlacement::Right);
        scene.frame();
        CHECK(popup->getX()+popup->getWidth() == 330-Popup::Gap);
        CHECK(popup->getY() == 30);
    }

    SUBCASE("kept inside when it fits on neither side")
    {
        Popup *wide=scene.popup(370, 40);
        controls::Base *anchor=scene.box(150, 30, 100, 20);

        wide->open(anchor, PopupPlacement::Right);
        scene.frame();
        CHECK(wide->getX() >= 0);
        CHECK(wide->getX()+wide->getWidth() <= 400);
        CHECK(wide->getY() == 30);
    }

    SUBCASE("the canvas shrinks and grows back, the anchor moves")
    {
        controls::Base *anchor=scene.box(250, 180, 60, 20);

        popup->open(anchor);
        scene.frame();
        const Rect below=popup->getBounds();
        CHECK(below.y == 200+Popup::Gap);

        // no room below it any more: above
        scene.canvas->setSize(300, 220);
        scene.frame();
        CHECK(popup->getY()+popup->getHeight() == 180-Popup::Gap);
        CHECK(popup->getX()+popup->getWidth() <= 300);

        scene.canvas->setSize(400, 300);
        scene.frame();
        CHECK(same(Point(popup->getX(), popup->getY()), Point(below.x, below.y)));

        // the anchor moves: the popup follows in the same frame
        anchor->setPos(40, 50);
        scene.frame();
        CHECK(popup->getX() == 40);
        CHECK(popup->getY() == 70+Popup::Gap);
    }
}

TEST_CASE("Popup: a press outside, Escape, close() or a hidden, disabled or deleted anchor closes it")
{
    Scene scene;
    controls::Base *parent=scene.box(10, 10, 200, 100);
    controls::Base *anchor=scene.box(10, 10, 60, 20, parent);
    controls::Base *elsewhere=scene.box(300, 200, 50, 50);
    Popup *popup=scene.popup();
    Counter closes, pressed;

    closes.listen(popup->onClose);
    scene.frame();

    auto reopen=[&]()
    {
        popup->open(anchor);
        scene.frame();
        REQUIRE(popup->isOpen());
        REQUIRE(popup->visible());
    };

    auto closed=[&](int calls)
    {
        CHECK(!popup->isOpen());
        CHECK(popup->hidden());
        CHECK(!scene.canvas->shownPopup());
        CHECK(!scene.canvas->popupAnchor());
        CHECK(!popup->getAnchor());
        CHECK(closes.calls == calls);
    };

    reopen();
    scene.leftClick(320, 220);
    closed(1);

    // a right press too, and no context menu opens
    reopen();
    scene.rightClick(320, 220);
    closed(2);
    CHECK(!scene.canvas->shownContextMenu());

    // a press outside still reaches the control it is on
    controls::Button *button=scene.canvas->newChild<controls::Button>();
    button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    button->setBounds(300, 100, 60, 20);
    pressed.listen(button->onPressCaller);
    reopen();
    scene.clickOn(button);
    closed(3);
    CHECK(pressed.calls == 1);

    reopen();
    scene.escape();
    closed(4);
    // Escape with nothing open is the focused control's
    scene.escape();
    CHECK(closes.calls == 4);

    reopen();
    popup->close();
    closed(5);

    reopen();
    popup->setHidden(true);
    closed(6);

    // the anchor's parent hidden: the next update phase closes it
    reopen();
    parent->setHidden(true);
    scene.frame();
    closed(7);
    parent->setHidden(false);
    scene.frame();
    CHECK(popup->hidden());

    reopen();
    anchor->setDisabled(true);
    scene.frame();
    closed(8);
    anchor->setDisabled(false);

    // the anchor deleted, delayed and outright
    reopen();
    anchor->delayedDelete();
    scene.frame();
    closed(9);
    CHECK(scene.onCanvas(popup));

    anchor=scene.box(10, 10, 60, 20, parent);
    reopen();
    delete anchor;
    closed(10);
    scene.frame();
    CHECK(scene.onCanvas(popup));

    // deleting the anchor's parent deletes the anchor too
    anchor=scene.box(10, 10, 60, 20, parent);
    reopen();
    parent->delayedDelete();
    scene.frame();
    closed(11);
    (void)elsewhere;
}

TEST_CASE("Popup: deleting it leaves no shown popup")
{
    Scene scene;
    controls::Base *anchor=scene.box(10, 10, 60, 20);

    SUBCASE("delayed")
    {
        Popup *popup=scene.popup();

        popup->open(anchor);
        scene.frame();
        popup->delayedDelete();
        scene.frame();
        CHECK(!scene.canvas->shownPopup());
        CHECK(!scene.canvas->popupAnchor());
        CHECK(!scene.onCanvas(popup));
    }

    SUBCASE("outright")
    {
        Popup *popup=scene.popup();

        popup->open(anchor);
        scene.frame();
        delete popup;
        CHECK(!scene.canvas->shownPopup());
        scene.frame();
        // and the next popup opens as usual
        Popup *next=scene.popup();
        next->open(anchor);
        scene.frame();
        CHECK(scene.canvas->shownPopup() == next);
    }

    SUBCASE("the canvas deleted with it open")
    {
        scene.popup()->open(anchor);
        scene.frame();
        // ~Scene deletes the canvas, the anchor and the popup in one go
    }
}

TEST_CASE("Popup: a press on its anchor, in it or on an open Menu keeps it, a button toggles it")
{
    Scene scene;
    controls::Base *anchor=scene.box(10, 10, 60, 20);
    Popup *popup=scene.popup(80, 40);

    scene.frame();
    popup->open(anchor);
    scene.frame();

    scene.clickOn(anchor);
    CHECK(popup->isOpen());
    scene.leftClick(popup->getX()+5, popup->getY()+5);
    CHECK(popup->isOpen());
    scene.clickOn(popup->getChildren().front());
    CHECK(popup->isOpen());
    // a right press in it too
    scene.rightClick(popup->getX()+5, popup->getY()+5);
    CHECK(popup->isOpen());

    // a Menu opened over it, as a ComboBox in it opens its list
    controls::Menu *list=scene.canvas->newChild<controls::Menu>();
    controls::Base *item=list->addItem("One");
    list->addItem("Two");
    list->openAt(Point(250, 150));
    scene.frame();
    REQUIRE(list->visible());
    scene.clickOn(item);
    CHECK(popup->isOpen());
    CHECK(list->hidden());
    popup->close();

    // a button that opens and closes it
    controls::Button *button=scene.canvas->newChild<controls::Button>();
    Toggler toggler;

    button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    button->setBounds(200, 10, 60, 20);
    toggler.popup=popup;
    button->onPressCaller.add(&toggler, &Toggler::toggle);
    scene.frame();

    scene.clickOn(button);
    CHECK(popup->isOpen());
    CHECK(popup->getAnchor() == button);
    scene.clickOn(button);
    CHECK(!popup->isOpen());
    scene.clickOn(button);
    CHECK(popup->isOpen());
}

TEST_CASE("Popup: one shown at a time, a context menu included")
{
    Scene scene;
    controls::Base *first=scene.box(10, 10, 60, 20);
    controls::Base *second=scene.box(200, 10, 60, 20);
    controls::Base *owner=scene.box(10, 150, 150, 100);
    Popup *one=scene.popup();
    Popup *two=scene.popup();
    Counter oneOpens, oneCloses;
    controls::Menu *menu=owner->createContextMenu();

    menu->addItem("Cut");
    menu->addItem("Paste");
    oneOpens.listen(one->onOpen);
    oneCloses.listen(one->onClose);
    scene.frame();

    one->open(first);
    scene.frame();
    two->open(second);
    scene.frame();
    CHECK(!one->isOpen());
    CHECK(one->hidden());
    CHECK(oneCloses.calls == 1);
    CHECK(two->isOpen());

    // opened again for another anchor: closed, then opened there
    two->open(first);
    scene.frame();
    CHECK(two->isOpen());
    CHECK(two->getAnchor() == first);
    CHECK(two->getX() == 10);

    // a context menu closes it
    scene.rightClick(50, 200);
    CHECK(!two->isOpen());
    CHECK(two->hidden());
    CHECK(scene.canvas->shownContextMenu() == menu);
    CHECK(scene.canvas->shownPopup() == menu);
    CHECK(scene.canvas->popupAnchor() == owner);

    // and a popup closes the context menu
    one->open(first);
    scene.frame();
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownContextMenu());
    CHECK(!scene.canvas->contextMenuOwner());
    CHECK(scene.canvas->shownPopup() == one);
    CHECK(oneOpens.calls == 2);

    // Escape and closePopup close a context menu too
    scene.rightClick(50, 200);
    REQUIRE(scene.canvas->shownContextMenu() == menu);
    scene.canvas->closePopup();
    CHECK(menu->hidden());
    CHECK(!scene.canvas->shownPopup());
}

TEST_CASE("Popup: numbered above every other control and below the ToolTip box")
{
    Scene scene;
    controls::Base *anchor=scene.box(10, 10, 60, 20);

    scene.box(100, 100, 200, 150);
    Popup *popup=scene.popup(80, 40);
    scene.box(200, 50, 100, 100);

    scene.frame();
    popup->open(anchor);
    scene.frame();
    CHECK(lowestZ(popup) == popup->getZIndex());
    CHECK(popup->getZIndex() > highestZ(scene.canvas, popup));

    // a control added to the canvas after it opened stays below it
    controls::Base *later=scene.box(20, 20, 100, 100);
    scene.frame();
    CHECK(popup->getZIndex() > later->getZIndex());
    CHECK(popup->getZIndex() > highestZ(scene.canvas, popup));

    // a Menu opened over it (a ComboBox's list in it) stays above it
    controls::Menu *list=scene.canvas->newChild<controls::Menu>();
    list->addItem("One");
    list->openAt(Point(popup->getX()+10, popup->getY()+10));
    scene.frame();
    controls::Base *again=scene.box(30, 30, 10, 10);
    scene.frame();
    CHECK(list->getZIndex() > popup->getZIndex());
    CHECK(popup->getZIndex() > again->getZIndex());
    list->close();

    // a tooltip shows over it
    controls::Base *tipped=scene.box(300, 10, 60, 20);
    tipped->setToolTipText("Over the popup");
    scene.canvas->inputMouseMoved(330, 20, 0, 0);
    scene.frame();
    scene.clock+=scene.canvas->getToolTipDelay();
    scene.frame();
    controls::ToolTip *box=scene.canvas->toolTipControl();
    REQUIRE(box);
    REQUIRE(scene.canvas->toolTipShown());
    CHECK(popup->isOpen());
    CHECK(scene.canvas->getActualChildren().back() == box);
    CHECK(box->getZIndex() > highestZ(scene.canvas, box));
    CHECK(box->getZIndex() > highestZ(popup, nullptr));
}

TEST_CASE("Popup: a ComboBox in it keeps its list closed when it opens, and its list opens over it")
{
    Scene scene;
    controls::Base *anchor=scene.box(10, 10, 60, 20);
    Popup *popup=scene.canvas->newChild<Popup>();
    controls::ComboBox *combo=popup->newChild<controls::ComboBox>();

    combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    combo->setBounds(0, 0, 120, 20);
    combo->addItem("One");
    combo->addItem("Two");
    scene.frame();

    // the list is the box's child until it first opens, and keeps its own
    // hidden flag when the popup holding the box is shown
    popup->open(anchor);
    scene.frame();
    CHECK(!combo->isMenuOpen());
    popup->close();
    popup->open(anchor);
    scene.frame();
    CHECK(!combo->isMenuOpen());

    scene.clickOn(combo);
    REQUIRE(combo->isMenuOpen());
    CHECK(popup->isOpen());

    controls::Base *list=nullptr;
    for(controls::Base *child : scene.canvas->getActualChildren())
    {
        if(gweni_cast<controls::Menu>(child) && child->visible())
            list=child;
    }
    REQUIRE(list);
    CHECK(list->getZIndex() > highestZ(popup, nullptr));

    // choosing an item closes the list and keeps the popup
    controls::Base *item=list->getChildren().back();
    scene.clickOn(item);
    CHECK(!combo->isMenuOpen());
    CHECK(popup->isOpen());
    CHECK(combo->getSelectedItem()->getText() == "Two");
}

TEST_CASE("Popup: Escape closes a ComboBox list open over it first, closing it closes the list")
{
    Scene scene;
    controls::Base *anchor=scene.box(10, 10, 60, 20);
    Popup *popup=scene.canvas->newChild<Popup>();
    controls::ComboBox *combo=popup->newChild<controls::ComboBox>();

    combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    combo->setBounds(0, 0, 120, 20);
    combo->addItem("One");
    combo->addItem("Two");
    scene.frame();
    popup->open(anchor);
    scene.frame();

    scene.clickOn(combo);
    REQUIRE(combo->isMenuOpen());
    scene.escape();
    CHECK(!combo->isMenuOpen());
    CHECK(popup->isOpen());
    scene.escape();
    CHECK(!popup->isOpen());

    // closed some other way with its list open: the list closes too
    popup->open(anchor);
    scene.frame();
    scene.clickOn(combo);
    REQUIRE(combo->isMenuOpen());
    anchor->setHidden(true);
    scene.frame();
    CHECK(!popup->isOpen());
    CHECK(!combo->isMenuOpen());
}

TEST_CASE("Popup: a MenuStrip on the canvas is outside it: Escape and a press on the strip close it")
{
    Scene scene;
    controls::MenuStrip *strip=scene.canvas->newChild<controls::MenuStrip>();

    strip->setDock(DockPosition::Top);
    strip->addItem("File");
    controls::Base *anchor=scene.box(10, 50, 60, 20);
    Popup *popup=scene.popup();
    scene.frame();
    REQUIRE(strip->visible());

    popup->open(anchor);
    scene.frame();
    REQUIRE(popup->isOpen());
    scene.escape();
    CHECK(!popup->isOpen());
    CHECK(strip->visible());

    // a press on the strip, away from its items
    popup->open(anchor);
    scene.frame();
    REQUIRE(popup->isOpen());
    scene.leftClick(300, strip->getHeight()/2);
    REQUIRE(HoveredControl);
    CHECK(HoveredControl->isMenuComponent());
    CHECK(!popup->isOpen());

    // and on one of its items
    popup->open(anchor);
    scene.frame();
    scene.clickOn(strip->getChildren().front());
    CHECK(!popup->isOpen());
}
