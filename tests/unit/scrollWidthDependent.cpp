/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A ScrollControl whose content's height depends on its width (wrapped
 *  text, a layout::Flow of cards) lays out in one pass when its bars cannot
 *  depend on that height: with a stable gutter (vertical scrolling, the
 *  vertical bar always shown, setAutoHideBars(false)) a resize that rewraps
 *  the content takes one update-phase pass, re-dirties nothing, and leaves
 *  the inner panel the content's new height and the bar ranged over it, as a
 *  canvas built at that size has them (its first frame one pass too, the
 *  content's height measured at its first width, not the panel's default);
 *  the scroll offset is kept while the content still overflows by it and
 *  clamped when it shrinks under it, in that pass, and a scrollToBottom() of
 *  that frame lands on the rewrapped content's end. With no vertical
 *  scrolling at all the same holds for the inner panel. With auto-hide on, a
 *  resize after which the bar shows as it did rewraps in one pass too, and
 *  one whose rewrapped content shows or hides the bar re-dirties the
 *  ScrollControl and takes a second pass, which decides the bar from the new
 *  height (docs/architecture.md -> "Width-dependent content in a
 *  ScrollControl").
 */
#include "headlessFixture.h"

#include <gweni/controls/scrollControl.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/controls/label.h>

#include <doctest/doctest.h>

#include <iterator>
#include <string>

using namespace gweni;

namespace
{

std::string paragraph()
{
    std::string s;

    for(int i=0; i < 200; ++i)
        s+="word"+std::to_string(i)+" ";
    return s;
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::ScrollControl *scroll;
    controls::Base *inner;
    controls::BaseScrollBar *verticalBar;
    controls::Label *text;

    explicit Scene(bool autoHide, int width=400, int height=120, bool vertical=true)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, height);
        scroll=canvas->newChild<controls::ScrollControl>("scroll");
        scroll->setDock(DockPosition::Center);
        scroll->setScroll(false, vertical);
        scroll->setAutoHideBars(autoHide);

        const controls::Base::List &parts=scroll->getActualChildren();
        REQUIRE(parts.size() == 3);
        inner=parts.front();
        verticalBar=static_cast<controls::BaseScrollBar *>(*std::next(parts.begin()));

        text=scroll->newChild<controls::Label>();
        text->setDock(DockPosition::Top);
        text->setWrap(true);
        text->setText(paragraph());
    }

    ~Scene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 10 && frame().passes != 0; ++i)
            ;
    }
};

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none")
        << "#" << stats.redirtiedId);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

//! The vertical bar's thumb.
const controls::Base *thumb(controls::BaseScrollBar *bar)
{
    for(controls::Base *child:bar->getChildren())
    {
        if(dynamic_cast<controls::internal::ScrollBarBar *>(child))
            return child;
    }
    return nullptr;
}

//! What a settled scene shows: the text's size, the inner panel's height,
//! the thumb's while the bar shows (it follows what the bar ranges
//! over), and the offset.
struct Shown
{
    int textWidth, textHeight, innerHeight, barContent, offset, maxOffset;

    bool operator==(const Shown &o) const
    {
        return textWidth == o.textWidth && textHeight == o.textHeight
            && innerHeight == o.innerHeight && barContent == o.barContent
            && offset == o.offset && maxOffset == o.maxOffset;
    }
};

Shown shown(const Scene &scene)
{
    return {scene.text->getWidth(), scene.text->getHeight(), scene.inner->getHeight(),
        (!scene.verticalBar->hidden() && thumb(scene.verticalBar))?thumb(scene.verticalBar)->getHeight():-1,
        scene.scroll->getVerticalScrollOffset(), scene.scroll->getMaxVerticalScrollOffset()};
}

doctest::String toString(const Shown &s)
{
    return ("text " + std::to_string(s.textWidth) + "x" + std::to_string(s.textHeight)
        + " inner " + std::to_string(s.innerHeight) + " thumb " + std::to_string(s.barContent)
        + " offset " + std::to_string(s.offset) + "/" + std::to_string(s.maxOffset)).c_str();
}

//! A scene built at the size, settled, as a resize to it should leave one.
Shown freshAt(bool autoHide, int width, int height)
{
    Scene scene(autoHide, width, height);
    scene.settle();
    return shown(scene);
}

}//namespace

TEST_CASE("width-dependent content in a stable-gutter ScrollControl rewraps in the resize's one pass")
{
    Scene scene(false);
    // the first frame too: the content is measured at the inner panel's
    // width before any layout, and again at its first one
    checkOnePass(scene.frame());
    CHECK(scene.frame().passes == 0);
    REQUIRE_FALSE(scene.verticalBar->hidden());
    const Shown wide=shown(scene);
    REQUIRE(wide.textHeight > 120);
    CHECK(wide.innerHeight == wide.textHeight);
    CHECK(wide == freshAt(false, 400, 120));

    // narrower: more lines, a taller inner panel and a longer bar range, in
    // the pass that gives the text its new width
    scene.canvas->setSize(250, 120);
    checkOnePass(scene.frame());
    const Shown narrow=shown(scene);
    CHECK(narrow.textHeight > wide.textHeight);
    CHECK(narrow.innerHeight == narrow.textHeight);
    CHECK(narrow == freshAt(false, 250, 120));
    CHECK(scene.frame().passes == 0);

    // and back
    scene.canvas->setSize(400, 120);
    checkOnePass(scene.frame());
    CHECK(shown(scene) == wide);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("width-dependent content in a stable-gutter ScrollControl keeps the offset it still overflows by, and clamps it in that pass otherwise")
{
    Scene scene(false, 250, 120);
    scene.settle();
    const int narrowMax=scene.scroll->getMaxVerticalScrollOffset();
    REQUIRE(narrowMax > 40);

    // a few units down: the content still overflows by that much when wider
    scene.scroll->setVerticalScrollOffset(20);
    scene.settle();
    REQUIRE(scene.scroll->getVerticalScrollOffset() == 20);
    const int textY=scene.text->getY();

    scene.canvas->setSize(400, 120);
    checkOnePass(scene.frame());
    CHECK(scene.scroll->getVerticalScrollOffset() == 20);
    CHECK(scene.inner->getY() == 5-20);
    CHECK(scene.text->getY() == textY);
    CHECK(scene.frame().passes == 0);

    // at the end: the wider content's end is nearer, so the offset is clamped
    // to it in the resize's pass, and the inner panel's bottom is the viewport's
    scene.canvas->setSize(250, 120);
    scene.settle();
    scene.scroll->setVerticalScrollOffset(narrowMax);
    scene.settle();
    REQUIRE(scene.scroll->getVerticalScrollOffset() == narrowMax);

    scene.canvas->setSize(400, 120);
    checkOnePass(scene.frame());
    const int wideMax=scene.scroll->getMaxVerticalScrollOffset();
    CHECK(wideMax < narrowMax);
    CHECK(scene.scroll->getVerticalScrollOffset() == wideMax);
    CHECK(scene.inner->getY()+scene.inner->getHeight()
        == scene.scroll->getViewport().y+scene.scroll->getViewport().h);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("width-dependent content in a stable-gutter ScrollControl lands a scrollToBottom of the resize's frame on the rewrapped end")
{
    Scene scene(false, 400, 120);
    scene.settle();
    const int wideMax=scene.scroll->getMaxVerticalScrollOffset();

    // narrower: the end is further down than the height measured at the old
    // width puts it
    scene.scroll->scrollToBottom();
    scene.canvas->setSize(250, 120);
    checkOnePass(scene.frame());
    CHECK(scene.scroll->getMaxVerticalScrollOffset() > wideMax);
    CHECK(scene.scroll->getVerticalScrollOffset() == scene.scroll->getMaxVerticalScrollOffset());
    CHECK(scene.inner->getY()+scene.inner->getHeight()
        == scene.scroll->getViewport().y+scene.scroll->getViewport().h);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("width-dependent content in a ScrollControl that does not scroll vertically rewraps in one pass")
{
    Scene scene(true, 400, 300, false);
    scene.settle();
    CHECK(scene.verticalBar->hidden());
    const int wideHeight=scene.text->getHeight();

    scene.canvas->setSize(250, 300);
    checkOnePass(scene.frame());
    CHECK(scene.text->getHeight() > wideHeight);
    CHECK(scene.inner->getHeight() >= scene.text->getHeight());
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("width-dependent content in an auto-hiding ScrollControl rewraps in one pass while its bar stays")
{
    // too tall at either width: the bar shows at both
    Scene scene(true, 400, 120);
    scene.settle();
    REQUIRE_FALSE(scene.verticalBar->hidden());
    const Shown wide=shown(scene);

    scene.canvas->setSize(250, 120);
    checkOnePass(scene.frame());
    CHECK_FALSE(scene.verticalBar->hidden());
    CHECK(shown(scene) == freshAt(true, 250, 120));
    CHECK(scene.frame().passes == 0);

    scene.canvas->setSize(400, 120);
    checkOnePass(scene.frame());
    CHECK(shown(scene) == wide);
}

TEST_CASE("width-dependent content in an auto-hiding ScrollControl takes a second pass when it rewraps past its bar")
{
    // 300 high: the text fits the viewport at 400 wide, not at 250
    Scene scene(true, 400, 300);
    scene.settle();
    REQUIRE(scene.verticalBar->hidden());
    const Shown wide=shown(scene);
    REQUIRE(wide.textHeight <= scene.scroll->getViewport().h);

    // the pass decides the bar from the height at the old width, and the
    // height at the new one shows it: the canvas's next pass decides it again
    scene.canvas->setSize(250, 300);
    const controls::LayoutStats &narrowed=scene.frame();
    CHECK(narrowed.passes == 2);
    CHECK(std::string(narrowed.redirtiedType?narrowed.redirtiedType:"") == "ScrollControl");
    CHECK_FALSE(scene.verticalBar->hidden());
    CHECK(shown(scene) == freshAt(true, 250, 300));
    CHECK(scene.frame().passes == 0);

    // and back: the height at the new width hides it
    scene.canvas->setSize(400, 300);
    const controls::LayoutStats &widened=scene.frame();
    CHECK(widened.passes == 2);
    CHECK(scene.verticalBar->hidden());
    CHECK(shown(scene) == wide);
    CHECK(scene.frame().passes == 0);
}
