/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Menus draw as one backdrop with flat rows on it, at scales 1, 1.5 and 2.
 *  With the Menu, MenuStrip and MenuHover surfaces set to solid test colours:
 *  an idle MenuItem draws nothing of its own (its pixels outside its children
 *  are the backdrop's); a hovered one, and one whose submenu is open, draws
 *  MenuHover over its bounds inside the menu's padding, and loses it when the
 *  mouse leaves or the submenu closes (however it closes); a disabled one
 *  never does. A divider is a max(1, round(s)) px Line rule from
 *  menu.x+pad+24 to right-pad-4 with backdrop on both sides of it and 3 units
 *  of backdrop above and below. A label is left-aligned after the icon column
 *  and clear of its accelerator. A MenuStrip is square, in its own surface.
 *  Each Menu and MenuStrip holds one slot and each MenuItem two (its highlight
 *  and its check), a settled frame writes none, and a hover move from one item
 *  to the next changes only those two items' pixels.
 *
 *  The menu's padding is 2 units. A disabled item keeps the Disabled text
 *  colour. An item stays highlighted while its submenu is open with the mouse
 *  in the submenu, and loses it when Menu::closeAll or Canvas::closeMenus
 *  closes it. Labels are Left|CenterV at the item's padding, their Text ending
 *  before the accelerator with no ink between. A MenuStrip's items are flat
 *  until hovered or open. Opening and closing a submenu leaves the live ids as
 *  they were.
 *
 *  A checkable, checked item draws a check in its icon column (the 24-unit left
 *  text padding): ink inside the check's rect alone, centred in the column,
 *  some of it the item's text colour, idle and hovered, and in place of an
 *  image the item has; an unchecked item, a checked one that is not checkable
 *  and a plain one leave the column the backdrop's (#464). setChecked and
 *  setCheckable on a shown item change its pixels alone on the next frame,
 *  which counts one changed control and writes at most its 2 slots (more
 *  controls with an image, which shows or hides), and give the frame before
 *  back when undone; a settled frame writes nothing. A click toggles a
 *  checkable item in a MenuStrip's menu and in a context menu, closing it, and
 *  reopening shows the new state; a click on a plain item checks nothing.
 *  Opening and closing a menu holding a checked item leaves the live ids as
 *  they were, and deleting it gives every id back.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::controls::Base;
using gweni::controls::Label;
using gweni::controls::Menu;
using gweni::controls::MenuItem;
using gweni::controls::MenuStrip;
namespace tokyonight=gweni::skin::textured::tokyonight;

const Color MenuFill(200, 30, 40, 255);
const Color MenuBorder(30, 200, 60, 255);
const Color StripFill(30, 40, 200, 255);
const Color StripBorder(220, 200, 20, 255);
const Color HoverFill(20, 190, 190, 255);
const Color HoverBorder(190, 20, 190, 255);

const float Scales[]={1.0f, 1.5f, 2.0f};

//! A Menu's padding (on its inner panel): its items are this far inside it.
const int MenuPad=2;

struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }
};

bool near(const Color &a, const Color &b, int tolerance=2)
{
    return std::abs(a.r-b.r) <= tolerance && std::abs(a.g-b.g) <= tolerance &&
        std::abs(a.b-b.b) <= tolerance;
}

//! Sets a surface to solid test colours, and puts it back.
struct SurfaceOverride
{
    tokyonight::Surface &surface;
    tokyonight::Surface saved;

    SurfaceOverride(tokyonight::Surface &target, const Color &fill, const Color &border):
        surface(target), saved(target)
    {
        surface.top=fill;
        surface.bottom=fill;
        surface.border=border;
    }

    ~SurfaceOverride() { surface=saved; }
};

//! A logical canvas rect in device pixels, its edges snapped as
//! renderer::Base::translate() snaps them.
struct DeviceRect
{
    int x0, y0, x1, y1;

    bool contains(int x, int y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

int snap(float v, float scale)
{
    return static_cast<int>(std::floor(v*scale+0.5f));
}

DeviceRect device(const Rect &r, float scale)
{
    return DeviceRect{snap(r.x, scale), snap(r.y, scale), snap(r.x+r.w, scale), snap(r.y+r.h, scale)};
}

Rect canvasRect(Base *control)
{
    const gweni::Point at=control->localPosToCanvas(gweni::Point(0, 0));

    return Rect(at.x, at.y, control->getWidth(), control->getHeight());
}

//! The device rects of \p control's visible descendants, each grown by a pixel
//! for its anti-aliased edge.
void descendants(Base *control, float scale, std::vector<DeviceRect> &rects)
{
    for(Base *child : control->getChildren())
    {
        if(child->hidden())
            continue;

        DeviceRect r=device(canvasRect(child), scale);

        rects.push_back(DeviceRect{r.x0-1, r.y0-1, r.x1+1, r.y1+1});
        descendants(child, scale, rects);
    }
}

bool inside(const std::vector<DeviceRect> &rects, int x, int y)
{
    for(const DeviceRect &r : rects)
    {
        if(r.contains(x, y))
            return true;
    }
    return false;
}

//! Every pixel of \p rect outside \p excluded is \p fill. Counts what it
//! checked, so a rect covered by its children cannot pass by checking nothing.
void checkSolid(const Frame &shot, const DeviceRect &rect, const std::vector<DeviceRect> &excluded,
    const Color &fill, const std::string &what)
{
    CAPTURE(what);
    size_t checked=0;
    size_t wrong=0;

    for(int y=rect.y0; y < rect.y1; ++y)
    {
        for(int x=rect.x0; x < rect.x1; ++x)
        {
            if(inside(excluded, x, y))
                continue;

            ++checked;
            if(!near(shot.at(x, y), fill))
            {
                if(wrong == 0)
                {
                    const Color c=shot.at(x, y);

                    MESSAGE(what<<": first wrong pixel at "<<x<<","<<y<<": "<<int(c.r)<<","<<int(c.g)<<","<<int(c.b));
                }
                ++wrong;
            }
        }
    }

    CHECK(checked >= 20);
    CHECK(wrong == 0);
}

//! The squares at the corners of \p backdrop that its rounded border (radius
//! \p radius) may reach into.
void cornerSquares(Base *backdrop, float radius, float scale, std::vector<DeviceRect> &rects)
{
    if(radius <= 0.0f)
        return;

    const DeviceRect r=device(canvasRect(backdrop), scale);
    const int c=static_cast<int>(std::ceil(radius*scale))+2;

    rects.push_back(DeviceRect{r.x0, r.y0, r.x0+c, r.y0+c});
    rects.push_back(DeviceRect{r.x1-c, r.y0, r.x1, r.y0+c});
    rects.push_back(DeviceRect{r.x0, r.y1-c, r.x0+c, r.y1});
    rects.push_back(DeviceRect{r.x1-c, r.y1-c, r.x1, r.y1});
}

//! The backdrop an item sits on: the menu (or strip) it is in, inside its
//! border ring and away from its rounded corners.
struct Backdrop
{
    DeviceRect inner;
    std::vector<DeviceRect> corners;
    Color fill;
};

Backdrop backdropOf(Base *menu, float radius, float scale, const Color &fill)
{
    const DeviceRect r=device(canvasRect(menu), scale);
    const int ring=std::max(1, snap(1.0f, scale));
    Backdrop backdrop{DeviceRect{r.x0+ring, r.y0+ring, r.x1-ring, r.y1-ring}, {}, fill};

    cornerSquares(menu, radius, scale, backdrop.corners);
    return backdrop;
}

//! An item drawn idle: its pixels outside its children, on its menu's
//! backdrop, are the backdrop's fill.
void checkIdle(const Frame &shot, MenuItem *item, float scale, const Backdrop &backdrop)
{
    const DeviceRect &clip=backdrop.inner;
    DeviceRect r=device(canvasRect(item), scale);

    r.x0=std::max(r.x0, clip.x0);
    r.y0=std::max(r.y0, clip.y0);
    r.x1=std::min(r.x1, clip.x1);
    r.y1=std::min(r.y1, clip.y1);

    std::vector<DeviceRect> children=backdrop.corners;

    descendants(item, scale, children);
    checkSolid(shot, r, children, backdrop.fill, "idle "+item->getText());
}

//! An item drawn highlighted: MenuHover's fill inside it (away from its
//! rounded corners, its children and \p covered), its border on its outer ring.
void checkHighlighted(const Frame &shot, MenuItem *item, float scale,
    std::vector<DeviceRect> covered={})
{
    const std::string what="highlighted "+item->getText();

    CAPTURE(what);
    const DeviceRect r=device(canvasRect(item), scale);
    std::vector<DeviceRect> children=std::move(covered);

    descendants(item, scale, children);

    const int corner=static_cast<int>(std::ceil(tokyonight::MenuHover.radius*scale))+2;
    const int ring=std::max(1, snap(1.0f, scale));
    size_t fillChecked=0, fillWrong=0, borderChecked=0, borderWrong=0;

    for(int y=r.y0; y < r.y1; ++y)
    {
        for(int x=r.x0; x < r.x1; ++x)
        {
            const int dx=std::min(x-r.x0, r.x1-1-x);
            const int dy=std::min(y-r.y0, r.y1-1-y);

            if((dx < corner && dy < corner) || inside(children, x, y))
                continue;

            const Color c=shot.at(x, y);

            if(dx < ring || dy < ring)
            {
                ++borderChecked;
                borderWrong+=near(c, HoverBorder) ? 0 : 1;
            }
            else if(dx >= ring+1 && dy >= ring+1)
            {
                ++fillChecked;
                fillWrong+=near(c, HoverFill) ? 0 : 1;
            }
        }
    }

    CHECK(fillChecked >= 20);
    CHECK(fillWrong == 0);
    CHECK(borderChecked >= 10);
    CHECK(borderWrong == 0);
}

size_t slots(Base *control)
{
    const gweni::skin::Control *skinControl=control->getSkinControl();

    return skinControl ? skinControl->getPrimitives().size() : 0;
}

size_t liveIds()
{
    return gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount();
}

Label *accelerator(MenuItem *item)
{
    for(Base *child : item->getChildren())
    {
        if(Label *label=gweni::gweni_cast<Label>(child))
            return label;
    }
    return nullptr;
}

//! A textured-skin canvas over the whole view at \p scale.
struct Scene
{
    float scale;
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;

    explicit Scene(float s):
        scale(s)
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(std::lround(viewSize.x/scale)),
            static_cast<int>(std::lround(viewSize.y/scale)));
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    Frame frame()
    {
        gweni::test::clearView(gweni::Color(0, 0, 0, 255));
        canvas->updateCanvas();
        canvas->renderCanvas();

        Frame shot;

        gweni::test::readView(shot.pixels);
        return shot;
    }

    //! Frames until nothing changes, then the settled one.
    Frame settle()
    {
        frame();
        frame();
        return frame();
    }

    void move(const Rect &target)
    {
        gweni::input::onMouseMoved(canvas, target.x+target.w/2, target.y+target.h/2, 0, 0);
    }

    void moveAway()
    {
        gweni::input::onMouseMoved(canvas, canvas->getWidth()-1, canvas->getHeight()-1, 0, 0);
    }

    size_t slotsWritten() const
    {
        return gweni::test::gpuRenderer().getFrameStats().primitiveSlotsWritten;
    }
};

//! "Save As.." (with its accelerator), a divider and "Quit".
struct FileMenu
{
    Menu *menu;
    MenuItem *saveAs;
    MenuItem *quit;
    Base *divider=nullptr;

    explicit FileMenu(Scene &scene)
    {
        menu=scene.canvas->newChild<Menu>();
        menu->setPos(2, 2);
        saveAs=menu->addItem("Save As..", "", "Ctrl+Shift+S");
        menu->addDivider();
        quit=menu->addItem("Quit", "", "Ctrl+Q");

        for(Base *child : saveAs->getParent()->getChildren())
        {
            if(gweni::gweni_cast<gweni::controls::MenuDivider>(child))
                divider=child;
        }
        REQUIRE(divider != nullptr);
    }
};

}//namespace

TEST_CASE("Menu rows: idle items are flat on the backdrop, a hovered one draws MenuHover inside the padding")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);
    SurfaceOverride hoverSurface(tokyonight::MenuHover, HoverFill, HoverBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        FileMenu file(scene);

        scene.moveAway();
        Frame idle=scene.settle();
        CHECK(scene.slotsWritten() == 0);

        CHECK(slots(file.menu) == 1);
        CHECK(slots(file.saveAs) == 2);
        CHECK(slots(file.quit) == 2);

        const Backdrop clip=backdropOf(file.menu, tokyonight::Menu.radius, scale, MenuFill);

        checkIdle(idle, file.saveAs, scale, clip);
        checkIdle(idle, file.quit, scale, clip);

        // the items sit inside the menu's padding, clear of its border
        const Rect menuRect=canvasRect(file.menu);

        for(MenuItem *item : {file.saveAs, file.quit})
        {
            const Rect r=canvasRect(item);

            CHECK(r.x == menuRect.x+MenuPad);
            CHECK(r.x+r.w == menuRect.x+menuRect.w-MenuPad);
        }
        CHECK(canvasRect(file.saveAs).y == menuRect.y+MenuPad);
        CHECK(canvasRect(file.quit).y+canvasRect(file.quit).h == menuRect.y+menuRect.h-MenuPad);

        // hovered: MenuHover over its bounds, the other item still flat
        scene.move(canvasRect(file.saveAs));
        Frame hovered=scene.frame();

        checkHighlighted(hovered, file.saveAs, scale);
        checkIdle(hovered, file.quit, scale, clip);

        scene.frame();
        CHECK(scene.slotsWritten() == 0);

        // a move to the next item changes only the two items' pixels
        Frame before=scene.frame();

        scene.move(canvasRect(file.quit));
        Frame after=scene.frame();

        checkHighlighted(after, file.quit, scale);
        checkIdle(after, file.saveAs, scale, clip);
        CHECK(scene.slotsWritten() <= 2);

        const DeviceRect a=device(canvasRect(file.saveAs), scale);
        const DeviceRect b=device(canvasRect(file.quit), scale);
        const gweni::Point view=gweni::test::viewSize();
        size_t outside=0;

        for(int y=0; y < view.y; ++y)
        {
            for(int x=0; x < view.x; ++x)
            {
                if(a.contains(x, y) || b.contains(x, y))
                    continue;

                if(!near(before.at(x, y), after.at(x, y), 0))
                    ++outside;
            }
        }
        CHECK(outside == 0);

        // the mouse leaving gives the idle frame back
        scene.moveAway();
        Frame left=scene.settle();

        CHECK(left.pixels == idle.pixels);
    }
}

TEST_CASE("Menu rows: a divider is a whole-pixel Line rule with 3 units of backdrop around it")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        FileMenu file(scene);

        scene.moveAway();
        Frame shot=scene.settle();

        const Rect menuRect=canvasRect(file.menu);
        const Rect d=canvasRect(file.divider);

        // from menu.x+pad+24 to right-pad-4, one unit high, 3 units clear of
        // the items above and below
        CHECK(d.x == menuRect.x+MenuPad+24);
        CHECK(d.x+d.w == menuRect.x+menuRect.w-MenuPad-4);
        CHECK(d.h == 1);
        CHECK(d.y == canvasRect(file.saveAs).y+canvasRect(file.saveAs).h+3);
        CHECK(canvasRect(file.quit).y == d.y+d.h+3);

        // the row band between the two items: Line where the rule is, the
        // backdrop everywhere else inside the border
        const Color line=tokyonight::Separator.top;
        const Backdrop clip=backdropOf(file.menu, tokyonight::Menu.radius, scale, MenuFill);
        const int top=snap(d.y-3, scale);
        const int bottom=snap(d.y+d.h+3, scale);
        const int x0=snap(d.x, scale);
        const int x1=snap(d.x+d.w, scale);
        const int thickness=std::max(1, snap(1.0f, scale));
        int lineRows=0;
        int firstLineRow=-1;
        size_t wrong=0;

        for(int y=top; y < bottom; ++y)
        {
            const bool isLine=near(shot.at((x0+x1)/2, y), line);

            if(isLine)
            {
                if(firstLineRow < 0)
                    firstLineRow=y;
                ++lineRows;
            }

            for(int x=clip.inner.x0; x < clip.inner.x1; ++x)
            {
                const bool onRule=isLine && x >= x0 && x < x1;

                if(!near(shot.at(x, y), onRule ? line : MenuFill))
                {
                    if(wrong == 0)
                        MESSAGE("divider band: first wrong pixel at "<<x<<","<<y);
                    ++wrong;
                }
            }
        }

        CHECK(lineRows == thickness);
        CHECK(wrong == 0);
        // the rule's rows are one block inside the divider's unit
        CHECK(firstLineRow >= snap(d.y, scale)-1);
        CHECK(firstLineRow+thickness <= snap(d.y+d.h, scale)+1);
    }
}

TEST_CASE("Menu rows: labels are left-aligned after the icon column and clear of the accelerator")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        FileMenu file(scene);

        scene.moveAway();
        Frame shot=scene.settle();

        for(MenuItem *item : {file.saveAs, file.quit})
        {
            const std::string what=item->getText();

            CAPTURE(what);
            Label *acc=accelerator(item);

            REQUIRE(acc != nullptr);
            CHECK((item->getTextAlignment() == (gweni::Alignment::Left|gweni::Alignment::CenterV)));
            // the Text starts at the item's padding: the icon column is its
            // left text padding, 24 units
            CHECK(item->textX() == item->getPadding().left);
            CHECK(item->getTextPadding().left == 24);
            // the Text, its 16-unit right padding included, ends before the accelerator
            CHECK(item->textRight() <= acc->getX());

            // and no ink between the label's glyphs and the accelerator's
            const Rect r=canvasRect(item);
            DeviceRect gap=device(Rect(r.x+item->textRight()-item->getTextPadding().right, r.y+item->textY(),
                acc->getX()-(item->textRight()-item->getTextPadding().right), item->textHeight()), scale);

            REQUIRE(gap.x1 > gap.x0);
            checkSolid(shot, gap, {}, MenuFill, "between label and accelerator");
        }
    }
}

TEST_CASE("Menu rows: a disabled item never highlights, an open submenu keeps its item highlighted")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);
    SurfaceOverride hoverSurface(tokyonight::MenuHover, HoverFill, HoverBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        Menu *menu=scene.canvas->newChild<Menu>();

        menu->setPos(2, 2);
        MenuItem *gone=menu->addItem("Gone");
        MenuItem *more=menu->addItem("More");
        MenuItem *other=menu->addItem("Other");

        gone->setDisabled(true);
        more->getMenu()->addItem("One");
        more->getMenu()->addItem("Two");

        scene.moveAway();
        Frame idle=scene.settle();
        const Backdrop clip=backdropOf(menu, tokyonight::Menu.radius, scale, MenuFill);
        const size_t live=liveIds();

        checkIdle(idle, gone, scale, clip);
        CHECK(near(gone->getTextColor(), scene.skin->Colors.Button.Disabled, 0));

        // a disabled item stays flat under the mouse
        scene.move(canvasRect(gone));
        Frame onDisabled=scene.settle();

        checkIdle(onDisabled, gone, scale, clip);
        CHECK(near(gone->getTextColor(), scene.skin->Colors.Button.Disabled, 0));

        // hovering "More" opens its submenu; it stays highlighted with the
        // mouse in the submenu, away from the item
        scene.move(canvasRect(more));
        scene.settle();
        REQUIRE(more->isMenuOpen());
        scene.move(canvasRect(more->getMenu()));
        Frame open=scene.settle();

        CHECK(more->isMenuOpen());
        CHECK_FALSE(more->isHovered());
        // at 2x the canvas is narrow enough that the submenu is moved over the item's end
        const DeviceRect sub=device(canvasRect(more->getMenu()), scale);

        checkHighlighted(open, more, scale, {DeviceRect{sub.x0-1, sub.y0-1, sub.x1+1, sub.y1+1}});

        // closed through the parent menu (Menu::closeAll, as hovering another
        // item does): the highlight goes with it
        scene.moveAway();
        scene.settle();
        CHECK(more->isMenuOpen());
        menu->closeAll();
        Frame closed=scene.settle();

        CHECK_FALSE(more->isMenuOpen());
        checkIdle(closed, more, scale, clip);
        CHECK(closed.pixels == idle.pixels);

        // opened and closed by the canvas closing every menu
        more->openMenu();
        Frame reopened=scene.settle();

        checkHighlighted(reopened, more, scale, {DeviceRect{sub.x0-1, sub.y0-1, sub.x1+1, sub.y1+1}});
        scene.canvas->closeMenus();
        // the canvas hides the menu itself too, so show it again to look
        menu->setHidden(false);
        Frame closedAll=scene.settle();

        CHECK_FALSE(more->isMenuOpen());
        checkIdle(closedAll, more, scale, clip);
        checkIdle(closedAll, other, scale, clip);

        // no slot taken or left behind by opening and closing
        CHECK(liveIds() == live);
    }
}

TEST_CASE("Menu rows: a MenuStrip is square in its own surface, its items flat until hovered or open")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);
    SurfaceOverride stripSurface(tokyonight::MenuStrip, StripFill, StripBorder);
    SurfaceOverride hoverSurface(tokyonight::MenuHover, HoverFill, HoverBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        MenuStrip *strip=scene.canvas->newChild<MenuStrip>();
        MenuItem *file=strip->addItem("File");
        MenuItem *edit=strip->addItem("Edit");

        file->getMenu()->addItem("New");
        edit->getMenu()->addItem("Undo");

        scene.moveAway();
        Frame idle=scene.settle();

        CHECK(slots(strip) == 1);
        CHECK(slots(file) == 2);

        // square: the corner pixels are the border's
        const DeviceRect r=device(canvasRect(strip), scale);

        CHECK(near(idle.at(r.x0, r.y0), StripBorder));
        CHECK(near(idle.at(r.x1-1, r.y0), StripBorder));
        CHECK(near(idle.at(r.x0, r.y1-1), StripBorder));

        const Backdrop clip=backdropOf(strip, tokyonight::MenuStrip.radius, scale, StripFill);

        checkIdle(idle, file, scale, clip);
        checkIdle(idle, edit, scale, clip);

        // hovered, then open with the mouse in its menu
        scene.move(canvasRect(file));
        Frame hovered=scene.settle();

        checkHighlighted(hovered, file, scale);
        checkIdle(hovered, edit, scale, clip);

        file->openMenu();
        scene.move(canvasRect(file->getMenu()));
        Frame open=scene.settle();

        CHECK(file->isMenuOpen());
        checkHighlighted(open, file, scale);
        checkIdle(open, edit, scale, clip);

        // the open menu is rounded in the Menu surface
        const DeviceRect m=device(canvasRect(file->getMenu()), scale);

        CHECK_FALSE(near(open.at(m.x0, m.y0), MenuBorder));
        CHECK(near(open.at((m.x0+m.x1)/2, m.y0), MenuBorder));

        scene.canvas->closeMenus();
        scene.moveAway();
        Frame closed=scene.settle();

        CHECK_FALSE(file->isMenuOpen());
        CHECK(closed.pixels == idle.pixels);
    }
}

namespace
{

//! \p item's icon column in device pixels: its left text padding, its height.
DeviceRect columnOf(MenuItem *item, float scale)
{
    const Rect r=canvasRect(item);
    const DeviceRect column=device(Rect(r.x, r.y, item->getTextPadding().left, r.h), scale);
    const gweni::Point view=gweni::test::viewSize();

    // the view is small (448x160): an item must be on it to be looked at
    REQUIRE(column.x0 >= 0);
    REQUIRE(column.y0 >= 0);
    REQUIRE(column.x1 <= view.x);
    REQUIRE(column.y1 <= view.y);
    return column;
}

//! The rect the textured renderer draws \p item's check in, on the canvas.
Rect checkRectOf(MenuItem *item)
{
    const Rect local=gweni::skin::textured::MenuItem::checkRect(item);
    const Rect r=canvasRect(item);

    // (a render bounds is in the parent's coordinates)
    return Rect(r.x+local.x-item->getX(), r.y+local.y-item->getY(), local.w, local.h);
}

//! What a column check leaves out of \p item's column: a visible image (the
//! Text child spans the column but draws nothing in it), and with \p hovered
//! the MenuHover highlight's border ring and rounded corners.
std::vector<DeviceRect> columnExclusions(MenuItem *item, float scale, bool hovered)
{
    std::vector<DeviceRect> excluded;

    for(Base *child : item->getChildren())
    {
        if(child->getTypeName() == std::string("ImagePanel") && child->visible())
        {
            const DeviceRect r=device(canvasRect(child), scale);

            excluded.push_back(DeviceRect{r.x0-1, r.y0-1, r.x1+1, r.y1+1});
        }
    }

    if(hovered)
    {
        const DeviceRect r=device(canvasRect(item), scale);
        const int ring=std::max(1, snap(1.0f, scale))+1;

        excluded.push_back(DeviceRect{r.x0, r.y0, r.x1, r.y0+ring});
        excluded.push_back(DeviceRect{r.x0, r.y1-ring, r.x1, r.y1});
        excluded.push_back(DeviceRect{r.x0, r.y0, r.x0+ring, r.y1});
        cornerSquares(item, tokyonight::MenuHover.radius, scale, excluded);
    }
    return excluded;
}

//! An item's column holds nothing of its own: every pixel of it, outside a
//! visible image and the backdrop's corners, is \p fill.
void checkColumnEmpty(const Frame &shot, MenuItem *item, float scale, const Color &fill,
    const std::vector<DeviceRect> &corners={})
{
    std::vector<DeviceRect> excluded=columnExclusions(item, scale, false);

    excluded.insert(excluded.end(), corners.begin(), corners.end());
    checkSolid(shot, columnOf(item, scale), excluded, fill, "empty column of "+item->getText());
}

//! An item's column holds its check: the check's rect is a square centred in
//! the column and in the item, every pixel unlike \p fill (outside the
//! backdrop's \p corners) is inside it (grown a pixel for its anti-aliased
//! edge), and some are the item's text colour.
void checkColumnChecked(const Frame &shot, MenuItem *item, float scale, const Color &fill,
    const std::vector<DeviceRect> &corners={}, bool hovered=false)
{
    const std::string what="check of "+item->getText();

    CAPTURE(what);
    const DeviceRect column=columnOf(item, scale);
    const DeviceRect r=device(canvasRect(item), scale);
    const Rect check=checkRectOf(item);
    const DeviceRect allowed=device(check, scale);
    const Color ink=item->getTextColor();
    std::vector<DeviceRect> excluded=columnExclusions(item, scale, hovered);

    excluded.insert(excluded.end(), corners.begin(), corners.end());

    REQUIRE(check.w > 0);
    CHECK(check.w == check.h);
    CHECK(allowed.x0 >= column.x0);
    CHECK(allowed.x1 <= column.x1);
    CHECK(std::abs((allowed.x0+allowed.x1)-(column.x0+column.x1)) <= 2);
    CHECK(std::abs((allowed.y0+allowed.y1)-(r.y0+r.y1)) <= 2);

    size_t checked=0, drawn=0, outside=0, full=0;

    for(int y=column.y0; y < column.y1; ++y)
    {
        for(int x=column.x0; x < column.x1; ++x)
        {
            if(inside(excluded, x, y))
                continue;

            ++checked;
            const Color c=shot.at(x, y);

            if(near(c, fill))
                continue;

            ++drawn;
            if(x < allowed.x0-1 || x >= allowed.x1+1 || y < allowed.y0-1 || y >= allowed.y1+1)
            {
                if(outside == 0)
                    MESSAGE(what<<": first pixel outside the check at "<<x<<","<<y<<": "<<int(c.r)<<","<<int(c.g)<<","<<int(c.b));
                ++outside;
            }
            if(near(c, ink, 8))
                ++full;
        }
    }

    CHECK(checked >= 20);
    // a stroke across the square: at least its side's worth of pixels
    CHECK(drawn >= static_cast<size_t>(allowed.x1-allowed.x0));
    CHECK(outside == 0);
    CHECK(full >= 1);
}

//! The number of pixels that differ between two frames outside \p rect.
size_t changedOutside(const Frame &a, const Frame &b, const DeviceRect &rect)
{
    const gweni::Point view=gweni::test::viewSize();
    size_t changed=0;

    for(int y=0; y < view.y; ++y)
    {
        for(int x=0; x < view.x; ++x)
        {
            if(!rect.contains(x, y) && !near(a.at(x, y), b.at(x, y), 0))
                ++changed;
        }
    }
    return changed;
}

Base *imageOf(MenuItem *item)
{
    for(Base *child : item->getChildren())
    {
        if(child->getTypeName() == std::string("ImagePanel"))
            return child;
    }
    return nullptr;
}

//! A stepped clock, so two clicks are never a double click; the real one back after.
struct SteppedClock
{
    float now=100.0f;

    SteppedClock() { gweni::platform::setTimeSource([this]() { return now; }); }
    ~SteppedClock() { gweni::platform::setTimeSource(nullptr); }
};

void click(Scene &scene, const Rect &target, int button, SteppedClock &clock)
{
    clock.now+=1.0f;
    scene.move(target);
    scene.frame();
    gweni::input::onMouseClicked(scene.canvas, button, true);
    gweni::input::onMouseClicked(scene.canvas, button, false);
    scene.frame();
}

}//namespace

TEST_CASE("Menu rows: a checkable, checked item draws a check in its icon column, the others leave it empty")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);
    SurfaceOverride hoverSurface(tokyonight::MenuHover, HoverFill, HoverBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        // three rows are all that fit at 2x: the second menu is shown, at the
        // same place, once the first is hidden
        Menu *menu=scene.canvas->newChild<Menu>();
        Menu *second=scene.canvas->newChild<Menu>();

        menu->setPos(2, 2);
        second->setPos(2, 2);
        second->setHidden(true);
        MenuItem *plain=menu->addItem("Plain");
        MenuItem *unchecked=menu->addItem("Unchecked");
        MenuItem *checked=menu->addItem("Checked", "", "Ctrl+K");
        MenuItem *notCheckable=second->addItem("Not checkable");
        MenuItem *imaged=second->addItem("Imaged", "test16.png");

        unchecked->setCheckable(true);
        checked->setCheckable(true);
        checked->setChecked(true);
        notCheckable->setChecked(true);
        imaged->setCheckable(true);
        imaged->setChecked(true);

        scene.moveAway();
        Frame idle=scene.settle();
        CHECK(scene.slotsWritten() == 0);

        const Backdrop clip=backdropOf(menu, tokyonight::Menu.radius, scale, MenuFill);

        for(MenuItem *item : {plain, unchecked, checked, notCheckable, imaged})
            CHECK(slots(item) == 2);

        checkColumnChecked(idle, checked, scale, MenuFill, clip.corners);
        for(MenuItem *item : {plain, unchecked})
        {
            checkColumnEmpty(idle, item, scale, MenuFill, clip.corners);
            checkIdle(idle, item, scale, clip);
        }

        menu->setHidden(true);
        second->setHidden(false);
        Frame secondShot=scene.settle();
        const Backdrop secondClip=backdropOf(second, tokyonight::Menu.radius, scale, MenuFill);

        checkColumnEmpty(secondShot, notCheckable, scale, MenuFill, secondClip.corners);
        checkIdle(secondShot, notCheckable, scale, secondClip);

        // the image hidden, the check in its place
        Base *image=imageOf(imaged);

        REQUIRE(image != nullptr);
        CHECK_FALSE(image->visible());
        checkColumnChecked(secondShot, imaged, scale, MenuFill, secondClip.corners);

        second->setHidden(true);
        menu->setHidden(false);
        scene.settle();

        // hovered: the check is drawn over MenuHover, in the hover text colour
        scene.move(canvasRect(checked));
        Frame hovered=scene.settle();

        REQUIRE(checked->isHovered());
        CHECK((checked->getTextColor() == scene.skin->Colors.Button.Hover));
        checkColumnChecked(hovered, checked, scale, HoverFill, clip.corners, true);
        CHECK(scene.slotsWritten() == 0);
    }
}

TEST_CASE("Menu rows: setChecked and setCheckable on a shown item change its pixels alone on the next frame")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        // the item with an image is in a second menu at the same place, shown
        // once the first is hidden: three rows are all that fit at 2x
        Menu *menu=scene.canvas->newChild<Menu>();
        Menu *second=scene.canvas->newChild<Menu>();

        menu->setPos(2, 2);
        second->setPos(2, 2);
        second->setHidden(true);
        MenuItem *above=menu->addItem("Above");
        MenuItem *item=menu->addItem("Toggled", "", "Ctrl+T");
        MenuItem *below=menu->addItem("Below");
        MenuItem *imaged=second->addItem("Imaged", "test16.png");

        second->addItem("After");
        item->setCheckable(true);
        item->setChecked(true);
        imaged->setCheckable(true);

        scene.moveAway();
        Frame checkedShot=scene.settle();
        const Backdrop clip=backdropOf(menu, tokyonight::Menu.radius, scale, MenuFill);
        const DeviceRect itemRect=device(canvasRect(item), scale);
        const size_t live=liveIds();

        checkColumnChecked(checkedShot, item, scale, MenuFill, clip.corners);

        // unchecked: the column empties on the very next frame, nothing else changes
        item->setChecked(false);
        gweni::controls::resetChangeStats();
        Frame uncheckedShot=scene.frame();

        CHECK(gweni::controls::getChangeStats().controls == 1);
        CHECK(scene.slotsWritten() >= 1);
        CHECK(scene.slotsWritten() <= 2);
        checkColumnEmpty(uncheckedShot, item, scale, MenuFill, clip.corners);
        CHECK(changedOutside(checkedShot, uncheckedShot, itemRect) == 0);
        CHECK(uncheckedShot.pixels != checkedShot.pixels);
        checkIdle(uncheckedShot, above, scale, clip);
        checkIdle(uncheckedShot, below, scale, clip);

        scene.frame();
        CHECK(scene.slotsWritten() == 0);

        // checked again: the frame before back
        item->setChecked(true);
        gweni::controls::resetChangeStats();
        Frame rechecked=scene.frame();

        CHECK(gweni::controls::getChangeStats().controls == 1);
        CHECK(scene.slotsWritten() <= 2);
        CHECK(rechecked.pixels == checkedShot.pixels);

        // no longer checkable: drawn as unchecked
        item->setCheckable(false);
        gweni::controls::resetChangeStats();
        Frame notCheckable=scene.frame();

        CHECK(gweni::controls::getChangeStats().controls == 1);
        CHECK(scene.slotsWritten() <= 2);
        CHECK(notCheckable.pixels == uncheckedShot.pixels);

        item->setCheckable(true);
        gweni::controls::resetChangeStats();
        Frame checkableAgain=scene.frame();

        CHECK(gweni::controls::getChangeStats().controls == 1);
        CHECK(checkableAgain.pixels == checkedShot.pixels);

        scene.frame();
        CHECK(scene.slotsWritten() == 0);

        // no slot taken or given back by the toggles
        CHECK(liveIds() == live);

        // with an image: checking hides it, the check in its place, and only
        // that item's pixels change
        menu->setHidden(true);
        second->setHidden(false);
        Frame imageShown=scene.settle();
        const Backdrop secondClip=backdropOf(second, tokyonight::Menu.radius, scale, MenuFill);
        Base *image=imageOf(imaged);

        REQUIRE(image != nullptr);
        CHECK(image->visible());
        const DeviceRect imagedRect=device(canvasRect(imaged), scale);
        const size_t secondLive=liveIds();

        imaged->setChecked(true);
        gweni::controls::resetChangeStats();
        Frame imageChecked=scene.settle();

        CHECK_FALSE(image->visible());
        checkColumnChecked(imageChecked, imaged, scale, MenuFill, secondClip.corners);
        CHECK(changedOutside(imageShown, imageChecked, imagedRect) == 0);
        CHECK(gweni::controls::getChangeStats().controls >= 2);

        imaged->setChecked(false);
        Frame imageBack=scene.settle();

        CHECK(image->visible());
        CHECK(imageBack.pixels == imageShown.pixels);
        CHECK(liveIds() == secondLive);
    }
}

TEST_CASE("Menu rows: a click toggles a checkable item in a MenuStrip's menu and a context menu")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        SteppedClock clock;
        Scene scene(scale);
        MenuStrip *strip=scene.canvas->newChild<MenuStrip>();
        MenuItem *view=strip->addItem("View");
        MenuItem *wrap=view->getMenu()->addItem("Wrap");
        MenuItem *save=view->getMenu()->addItem("Save");

        wrap->setCheckable(true);
        wrap->setChecked(true);

        view->openMenu();
        scene.moveAway();
        Frame open=scene.settle();
        // (where the menu opens can differ between openings: its corners are
        // taken from where it is each time)
        auto corners=[&](Menu *m) { return backdropOf(m, tokyonight::Menu.radius, scale, MenuFill).corners; };

        checkColumnChecked(open, wrap, scale, MenuFill, corners(view->getMenu()));
        checkColumnEmpty(open, save, scale, MenuFill, corners(view->getMenu()));

        // a plain item: selected, the menu closed, nothing checked
        click(scene, canvasRect(save), 0, clock);
        CHECK_FALSE(save->isChecked());
        CHECK_FALSE(view->isMenuOpen());

        view->openMenu();
        scene.moveAway();
        Frame reopened=scene.settle();

        checkColumnEmpty(reopened, save, scale, MenuFill, corners(view->getMenu()));
        checkColumnChecked(reopened, wrap, scale, MenuFill, corners(view->getMenu()));

        // the checkable one: unchecked, the menu closed; reopened without its check
        click(scene, canvasRect(wrap), 0, clock);
        CHECK_FALSE(wrap->isChecked());
        CHECK_FALSE(view->isMenuOpen());

        view->openMenu();
        scene.moveAway();
        Frame unchecked=scene.settle();

        checkColumnEmpty(unchecked, wrap, scale, MenuFill, corners(view->getMenu()));
        scene.canvas->closeMenus();
        scene.settle();

        // a context menu
        Base *box=scene.canvas->newChild<Base>();

        box->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        // at the canvas's right, away from the strip's menu (the canvas is
        // 224x80 at 2x)
        box->setBounds(scene.canvas->getWidth()-70, 4, 60, 20);
        Menu *context=box->createContextMenu();
        MenuItem *pin=context->addItem("Pin");

        context->addItem("Other");
        pin->setCheckable(true);
        pin->setChecked(true);

        click(scene, canvasRect(box), 1, clock);
        REQUIRE(context->visible());
        scene.moveAway();
        Frame menuShown=scene.settle();

        checkColumnChecked(menuShown, pin, scale, MenuFill, corners(context));

        click(scene, canvasRect(pin), 0, clock);
        CHECK_FALSE(pin->isChecked());
        CHECK_FALSE(context->visible());

        click(scene, canvasRect(box), 1, clock);
        REQUIRE(context->visible());
        scene.moveAway();
        Frame shownAgain=scene.settle();

        checkColumnEmpty(shownAgain, pin, scale, MenuFill, corners(context));
        scene.canvas->closeMenus();
        scene.settle();
    }
}

TEST_CASE("Menu rows: a menu holding a checked item keeps its ids over open and close and gives them all back")
{
    SurfaceOverride menuSurface(tokyonight::Menu, MenuFill, MenuBorder);

    for(float scale : Scales)
    {
        CAPTURE(scale);
        Scene scene(scale);
        scene.moveAway();
        scene.settle();
        const size_t before=liveIds();

        Menu *menu=scene.canvas->newChild<Menu>();

        menu->setPos(2, 2);
        MenuItem *checked=menu->addItem("Checked");
        MenuItem *imaged=menu->addItem("Imaged", "test16.png");

        menu->addItem("Plain");
        checked->setCheckable(true);
        checked->setChecked(true);
        imaged->setCheckable(true);
        imaged->setChecked(true);

        Frame shown=scene.settle();
        const size_t live=liveIds();

        CHECK(live > before);
        checkColumnChecked(shown, checked, scale, MenuFill,
            backdropOf(menu, tokyonight::Menu.radius, scale, MenuFill).corners);

        menu->setHidden(true);
        scene.settle();
        CHECK(scene.slotsWritten() == 0);
        CHECK(liveIds() == live);

        menu->setHidden(false);
        Frame again=scene.settle();

        CHECK(scene.slotsWritten() == 0);
        CHECK(liveIds() == live);
        CHECK(again.pixels == shown.pixels);

        menu->delayedDelete();
        scene.settle();
        CHECK(liveIds() == before);
    }
}
