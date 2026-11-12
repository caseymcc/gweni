/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The clip rects a Label gives its children (#571), on a real Canvas with the
 *  StubSkin. A Label does not clip as one (shouldClip() is false): a child
 *  other than its Text is clipped to the Label's bounds {0, 0, w, h}, the
 *  region clipping at the bounds gave it before, and the Text, while it lies
 *  wholly inside the Label, to the bounds widened by Label::inkMargin() of its
 *  font's size on the left and the right; a Text past the Label's edge (a
 *  Label narrower than its text) gets the bounds. The rects are members the
 *  Label keeps (the same pointer every call) and follow its size and its
 *  Text: shrinking a Label below its text, growing it back and changing the
 *  text to one that does not fit and back each mark the Text's subtree
 *  StateChange_Geometry, so its slots are stamped with the new region. A
 *  TabButton, which never clipped, gives no rects. inkMargin() is
 *  ceil(size/12)+1, the margin tests/labelInkOverhang.cpp measured.
 */
#include "headlessFixture.h"

#include <gweni/controls/label.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
    }

    void settle()
    {
        for(int i=0; i < 2; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
    }
};

//! A Label holding \p text with a plain child beside its Text, its size
//! fixed at \p width wide (its text's width if \p width is 0).
controls::Label *fixedLabel(Scene &scene, const String &text, int width, controls::Base **other)
{
    controls::Label *label=scene.canvas->newChild<controls::Label>();

    label->setText(text);
    *other=label->newChild<controls::Base>();
    (*other)->setBounds(1, 1, 2, 2);
    label->setSizeFlags(SizeFlags(SizeFlag::Fixed, SizeFlag::Fixed));
    scene.settle();

    const Point measured=scene.renderer.measureText(label->getFont(), text);

    label->setBounds(10, 10, (width > 0)?width:measured.x, measured.y);
    scene.settle();
    return label;
}

bool geometry(controls::Base *control)
{
    return (control->getStateChange() & controls::StateChange_Geometry) != 0;
}

//! The Text a Label's children hold: the one child that is no plain Base.
controls::Base *textOf(controls::Label *label, controls::Base *other)
{
    for(controls::Base *child : label->getChildren())
    {
        if(child != other)
            return child;
    }
    return nullptr;
}

}//namespace

TEST_CASE("Label clip rects: inkMargin is ceil(size/12)+1")
{
    CHECK(controls::Label::inkMargin(8) == 2);
    CHECK(controls::Label::inkMargin(11) == 2);
    CHECK(controls::Label::inkMargin(12) == 2);
    CHECK(controls::Label::inkMargin(13) == 3);
    CHECK(controls::Label::inkMargin(24) == 3);
    CHECK(controls::Label::inkMargin(25) == 4);
    CHECK(controls::Label::inkMargin(0) == 1);
}

TEST_CASE("Label clip rects: the Text gets the margin while it fits, every other child the bounds")
{
    Scene scene;
    controls::Base *other=nullptr;
    controls::Label *label=fixedLabel(scene, "Some text", 0, &other);
    controls::Base *text=textOf(label, other);
    const int w=label->getWidth();
    const int h=label->getHeight();
    const int margin=controls::Label::inkMargin(label->getFont().size);

    REQUIRE(text != nullptr);
    REQUIRE(label->textWidth() == w);
    CHECK_FALSE(label->shouldClip());

    const Rect *textClip=label->getChildClipRect(text);
    const Rect *otherClip=label->getChildClipRect(other);

    REQUIRE((textClip != nullptr));
    REQUIRE((otherClip != nullptr));
    CHECK((*textClip == Rect(-margin, 0, w+2*margin, h)));
    CHECK((*otherClip == Rect(0, 0, w, h)));
    // kept, not made per call
    CHECK((label->getChildClipRect(text) == textClip));

    SUBCASE("narrower than its text: the bounds")
    {
        label->setWidth(w-4);
        scene.settle();

        CHECK(label->textWidth() == w);
        CHECK((*label->getChildClipRect(text) == Rect(0, 0, w-4, h)));
        CHECK((*label->getChildClipRect(other) == Rect(0, 0, w-4, h)));
    }
}

TEST_CASE("Label clip rects: a change of the Text's rect marks the Text's subtree")
{
    Scene scene;
    controls::Base *other=nullptr;
    controls::Label *label=fixedLabel(scene, "Some text", 0, &other);
    controls::Base *text=textOf(label, other);
    const int w=label->getWidth();

    REQUIRE(!geometry(text));

    // shrunk below the text, the Text's bounds unchanged
    label->setWidth(w-4);
    scene.canvas->updateCanvas();
    CHECK(geometry(text));
    scene.canvas->renderCanvas();
    scene.settle();
    REQUIRE(!geometry(text));

    // grown back
    label->setWidth(w);
    scene.canvas->updateCanvas();
    CHECK(geometry(text));
    scene.canvas->renderCanvas();
    scene.settle();

    // a longer text in the same width, and back
    label->setText("Some text, longer");
    scene.canvas->updateCanvas();
    CHECK((*label->getChildClipRect(text) == Rect(0, 0, w, label->getHeight())));
    CHECK(geometry(text));
    scene.canvas->renderCanvas();
    scene.settle();

    label->setText("Some text");
    scene.canvas->updateCanvas();
    CHECK(label->getChildClipRect(text)->x < 0);
    CHECK(geometry(text));
}

TEST_CASE("Label clip rects: a TabButton clips none of its children")
{
    Scene scene;
    controls::TabControl *tabs=scene.canvas->newChild<controls::TabControl>();
    controls::TabButton *button=tabs->addPage("One");

    button->setClosable(true);
    scene.settle();

    CHECK_FALSE(button->shouldClip());
    for(controls::Base *child : button->getChildren())
        CHECK((button->getChildClipRect(child) == nullptr));
}
