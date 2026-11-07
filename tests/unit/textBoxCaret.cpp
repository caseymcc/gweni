/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's TextBox renderer draws the box in its first slot, the
 *  selection in the second and the caret in the third, the last two only while
 *  the box has the focus. A blink edge only shows or hides the caret slot; the
 *  geometry is rewritten when the cursor, the selection, the text or where the
 *  box is drawn changes. The selection sits at the text's z (behind every
 *  glyph, renderer::Base::renderText) and the caret at the box's overlay z,
 *  past its whole subtree and before the next control.
 *
 *  On a recording StubRenderer: nothing but hides while unfocused and no call
 *  at all on idle frames; the caret at the cursor, each blink edge a single
 *  show or hide of it; the geometry redrawn when the box moved while the caret
 *  was off; the selection over the selected glyphs; a scrolled box's caret
 *  following its text and the selection cut to the box; and a
 *  TextBoxMultiline's caret past its line children, with no selection.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! Records the draw, show and hide calls, each rect at its canvas position.
class CaretRecorder: public test::StubRenderer
{
public:
    enum class Call
    {
        Draw,
        Show,
        Hide
    };

    struct Record
    {
        Call call;
        size_t id;
        Rect rect;
        int z;
    };

    void drawFilledRect(size_t primitiveId, Rect rect, int zIndex) override
    {
        draw(primitiveId, rect, zIndex);
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color, Color, Color,
        int zIndex) override
    {
        draw(primitiveId, rect, zIndex);
    }

    void showPrimitive(int, size_t primitiveId) override
    {
        calls.push_back({Call::Show, primitiveId, Rect(), 0});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        calls.push_back({Call::Hide, primitiveId, Rect(), 0});
    }

    //! The calls on \p id this frame.
    std::vector<Record> on(size_t id) const
    {
        std::vector<Record> result;

        for(const Record &record : calls)
        {
            if(record.id == id)
                result.push_back(record);
        }
        return result;
    }

    std::vector<Record> calls;

private:
    void draw(size_t id, Rect rect, int zIndex)
    {
        rect.x+=getRenderOffset().x;
        rect.y+=getRenderOffset().y;
        calls.push_back({Call::Draw, id, rect, zIndex});
    }
};

//! The textured skin's TextBox renderer, a StubControl for the rest; the
//! TextBox types resolve to it through the parent chain as in the textured skin.
class TextBoxSkin: public test::StubSkin
{
public:
    explicit TextBoxSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "TextBox")
            return std::make_unique<skin::textured::TextBox>();
        if(typeName == "TextBoxMultiline")
            return nullptr;
        return StubSkin::getControlRenderer(typeName);
    }
};

struct CaretScene
{
    CaretScene()
    {
        platform::setTimeSource([this]() { return clock; });

        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        // in a panel, which moves it the way a dragged window would
        panel=canvas->newChild<controls::Base>("panel");
        panel->setAlignment(Alignment::Absolute);
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setBounds(20, 30, 250, 100);
        box=panel->newChild<controls::TextBox>("box");
        box->setText("hello world");
        after=canvas->newChild<controls::Base>("after");
        frame();
    }

    CaretScene(const CaretScene &)=delete;
    CaretScene &operator=(const CaretScene &)=delete;

    ~CaretScene()
    {
        KeyboardFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.calls.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! The box's slot \p i: 0 the box, 1 the selection, 2 the caret.
    size_t slot(size_t i) const
    {
        return box->getSkinControl()->getPrimitives().at(i);
    }

    //! Where the caret goes on the canvas.
    Rect caretOnCanvas() const
    {
        return onCanvas(box->getCaretBounds());
    }

    //! A rect in the box's coordinates, on the canvas.
    Rect onCanvas(const Rect &local) const
    {
        const Point origin=box->localPosToCanvas();
        return Rect(origin.x+local.x, origin.y+local.y, local.w, local.h);
    }

    //! Frames until the caret's blink phase flips, at 1/60 s each.
    void untilBlink()
    {
        const bool lit=box->isCaretVisible();

        for(int i=0; i < 200 && box->isCaretVisible() == lit; ++i)
        {
            clock+=1.0f/60.0f;
            frame();
        }
        REQUIRE(box->isCaretVisible() != lit);
    }

    float clock=10.0f;
    CaretRecorder renderer;
    TextBoxSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Base *panel=nullptr;
    controls::TextBox *box=nullptr;
    controls::Base *after=nullptr;
};

bool onlyHides(const std::vector<CaretRecorder::Record> &records)
{
    for(const CaretRecorder::Record &record : records)
    {
        if(record.call != CaretRecorder::Call::Hide)
            return false;
    }
    return true;
}

}//namespace

TEST_CASE("an unfocused TextBox draws no caret or selection, and an idle one nothing")
{
    CaretScene scene;

    REQUIRE(scene.box->getSkinControl()->getPrimitives().size() >= 3);
    scene.box->setText("hello again");
    scene.frame();
    CHECK(onlyHides(scene.renderer.on(scene.slot(1))));
    CHECK(onlyHides(scene.renderer.on(scene.slot(2))));

    for(int i=0; i < 120; ++i)
    {
        scene.clock+=1.0f/60.0f;
        scene.frame();
        CHECK(scene.renderer.calls.empty());
    }
}

TEST_CASE("a focused TextBox draws its caret in front of its text, and a blink only shows or hides it")
{
    CaretScene scene;

    scene.box->focus();
    scene.box->moveCaretToend();
    scene.frame();

    const std::vector<CaretRecorder::Record> caret=scene.renderer.on(scene.slot(2));
    REQUIRE(caret.size() == 1);
    CHECK(caret[0].call == CaretRecorder::Call::Draw);
    CHECK((caret[0].rect == scene.caretOnCanvas()));
    CHECK(caret[0].rect.w == 1);
    CHECK(caret[0].rect.h > 0);
    // the caret follows the text: at the end of the string, past its last glyph
    CHECK(scene.box->getCaretBounds().x >
        scene.box->getCharacterPosition(0).x);
    // in front of the box and its text, behind the next control
    const int textZ=scene.box->getTextZIndex();
    CHECK(caret[0].z > textZ);
    CHECK(caret[0].z == scene.box->getOverlayZIndex());
    CHECK(scene.after->getZIndex() > caret[0].z);
    CHECK(onlyHides(scene.renderer.on(scene.slot(1))));

    // lit for a while, then each edge is a hide or a show of the caret alone
    scene.untilBlink();
    CHECK(!scene.box->isCaretVisible());
    REQUIRE(scene.renderer.calls.size() == 1);
    CHECK(scene.renderer.calls[0].call == CaretRecorder::Call::Hide);
    CHECK(scene.renderer.calls[0].id == scene.slot(2));

    scene.untilBlink();
    REQUIRE(scene.renderer.calls.size() == 1);
    CHECK(scene.renderer.calls[0].call == CaretRecorder::Call::Show);
    CHECK(scene.renderer.calls[0].id == scene.slot(2));

    // between edges, nothing
    scene.clock+=0.1f;
    scene.frame();
    CHECK(scene.renderer.calls.empty());

    // moved while the caret is off: shown again at the new place
    scene.untilBlink();
    scene.panel->setPos(60, 90);
    scene.frame();
    const Point by(40, 60);
    CHECK(scene.renderer.on(scene.slot(2)).back().call == CaretRecorder::Call::Hide);
    scene.untilBlink();
    const std::vector<CaretRecorder::Record> moved=scene.renderer.on(scene.slot(2));
    REQUIRE(moved.size() == 1);
    CHECK(moved[0].call == CaretRecorder::Call::Draw);
    CHECK((moved[0].rect == scene.caretOnCanvas()));
    CHECK(moved[0].rect.x == caret[0].rect.x+by.x);
    CHECK(moved[0].rect.y == caret[0].rect.y+by.y);

    // losing the focus hides both
    scene.box->unfocus();
    scene.frame();
    CHECK(onlyHides(scene.renderer.on(scene.slot(1))));
    CHECK(onlyHides(scene.renderer.on(scene.slot(2))));
}

TEST_CASE("a TextBox's selection is drawn at its text's z, over the selected glyphs")
{
    CaretScene scene;

    scene.box->focus();
    scene.box->onSelectAll(scene.box);
    scene.frame();

    const std::vector<CaretRecorder::Record> selection=scene.renderer.on(scene.slot(1));
    REQUIRE(selection.size() == 1);
    CHECK(selection[0].call == CaretRecorder::Call::Draw);
    CHECK(selection[0].z == scene.box->getTextZIndex());

    const Rect first=scene.box->getCharacterPosition(0);
    const Rect last=scene.box->getCharacterPosition(static_cast<int>(scene.box->getText().size()));
    CHECK((selection[0].rect == scene.onCanvas(Rect(first.x, first.y, last.x-first.x, first.h))));

    // a cursor move drops it
    scene.box->moveCaretToStart();
    scene.frame();
    CHECK(onlyHides(scene.renderer.on(scene.slot(1))));
}

TEST_CASE("a scrolled TextBox's caret follows its text and stays inside the box")
{
    CaretScene scene;

    scene.box->setSizeFlags(SizeFlags(SizeFlag::Fixed, SizeFlag::Fixed));
    scene.box->setSize(60, 20);
    scene.box->setText("a string that is a good deal wider than the box it is in");
    scene.box->focus();
    scene.box->moveCaretToend();
    scene.frame();

    const Rect caret=scene.caretOnCanvas();
    const Rect box=scene.onCanvas(Rect(0, 0, scene.box->getWidth(), scene.box->getHeight()));
    const std::vector<CaretRecorder::Record> drawn=scene.renderer.on(scene.slot(2));
    REQUIRE(!drawn.empty());
    CHECK(drawn.back().call == CaretRecorder::Call::Draw);
    CHECK((drawn.back().rect == caret));
    CHECK(caret.x >= box.x);
    CHECK(caret.x+caret.w <= box.x+box.w);
    // scrolled: the string starts left of the box
    CHECK(scene.box->getCharacterPosition(0).x < 0);

    // Home scrolls the text back: the caret goes with it, to the start
    scene.box->moveCaretToStart();
    scene.frame();
    const std::vector<CaretRecorder::Record> home=scene.renderer.on(scene.slot(2));
    REQUIRE(!home.empty());
    CHECK((home.back().rect == scene.caretOnCanvas()));
    CHECK(home.back().rect.x < caret.x);

    // a selection wider than the box is cut to it
    scene.box->onSelectAll(scene.box);
    scene.frame();
    const std::vector<CaretRecorder::Record> selection=scene.renderer.on(scene.slot(1));
    REQUIRE(!selection.empty());
    CHECK(selection.back().rect.x == box.x);
    CHECK(selection.back().rect.x+selection.back().rect.w <= box.x+box.w);
}

TEST_CASE("a TextBoxMultiline draws its caret past its line children and no selection")
{
    CaretScene scene;

    controls::TextBoxMultiline *box=scene.canvas->newChild<controls::TextBoxMultiline>("multi");
    box->setPos(20, 100);
    box->setSize(120, 100);
    box->setText("one\ntwo\nthree");
    controls::Base *next=scene.canvas->newChild<controls::Base>("next");
    box->focus();
    box->onSelectAll(box);
    scene.frame();

    const std::vector<size_t> &slots=box->getSkinControl()->getPrimitives();
    REQUIRE(slots.size() >= 3);
    CHECK(onlyHides(scene.renderer.on(slots[1])));

    const std::vector<CaretRecorder::Record> caret=scene.renderer.on(slots[2]);
    REQUIRE(caret.size() == 1);
    CHECK(caret[0].call == CaretRecorder::Call::Draw);
    // the caret is on the last line, below the first
    CHECK(box->getCaretBounds().y > box->getCharacterPosition(0).y);

    int deepest=0;
    test::forEachControl(box, [&deepest](controls::Base *control)
        {
            deepest=std::max(deepest, control->getZIndex());
        });
    CHECK(caret[0].z > deepest);
    CHECK(next->getZIndex() > caret[0].z);
}
