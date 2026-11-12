/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Content alignment is not placement (docs/layout.md, "Placing a dock-None
 *  control"). A Label, TextBox or Button has no alignment in its parent unless
 *  one is set, so a position it is given is kept; where its text sits inside
 *  it is setTextAlignment(), and the control's own setAlignment() never moves
 *  its text. A TextBox's text is placed by the text alignment while the string
 *  fits, and keeps its scroll position once it does not.
 *
 *  The Label family checked: Label, TextBox, TextBoxMultiline, Button and
 *  StatusBar, each keeping a setPos through passes, its text placed by
 *  setTextAlignment / getTextAlignment(). A TextBox's string wider than the
 *  box keeps its scroll position: the tail at the right padding with the caret
 *  at the end, the head at the left padding with it at the start.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/label.h>
#include <gweni/controls/statusBar.h>
#include <gweni/controls/textBox.h>

#include <doctest/doctest.h>

#include <string>

using namespace gweni;

namespace
{

//! Rect as text, so a failed check prints both sides.
std::string rectText(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *parent;

    //! A 300x200 dock-None parent at (0, 0), padding 5, 6, 7, 8.
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        parent=canvas->newChild<controls::Base>("parent");
        parent->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        parent->setBounds(0, 0, 300, 200);
        parent->setPadding(Padding(5, 6, 7, 8));
    }

    ~Scene()
    {
        delete canvas;
    }

    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }
    }
};

//! The Text child a Label draws its string with.
controls::internal::Text *textOf(controls::Base *label)
{
    for(controls::Base *child:label->getActualChildren())
    {
        if(controls::internal::Text *text=gweni_cast<controls::internal::Text>(child))
            return text;
    }
    return nullptr;
}

bool same(Alignment a, Alignment b)
{
    return a == b;
}

}//namespace

TEST_CASE("content alignment: the Label family has no alignment in its parent")
{
    Scene scene;
    controls::Label *label=scene.parent->newChild<controls::Label>();
    controls::TextBox *box=scene.parent->newChild<controls::TextBox>();
    controls::TextBoxMultiline *multiline=scene.parent->newChild<controls::TextBoxMultiline>();
    controls::Button *button=scene.parent->newChild<controls::Button>();
    controls::StatusBar *status=scene.parent->newChild<controls::StatusBar>();

    for(controls::Base *control:std::initializer_list<controls::Base *>{label, box, multiline, button, status})
    {
        CAPTURE(control->getTypeName());
        CHECK(same(control->getAlignment(), Alignment::Absolute));
    }

    // where each one's text sits, by default
    CHECK(same(label->getTextAlignment(), Alignment::Left|Alignment::CenterV));
    CHECK(same(box->getTextAlignment(), Alignment::Left|Alignment::CenterV));
    CHECK(same(multiline->getTextAlignment(), Alignment::Left|Alignment::Top));
    CHECK(same(button->getTextAlignment(), Alignment::Center));
    CHECK(same(textOf(button)->getAlignment(), Alignment::Center));
}

TEST_CASE("content alignment: a Label keeps the position it is given")
{
    Scene scene;
    controls::Label *label=scene.parent->newChild<controls::Label>();
    label->setText("label");
    label->sizeToContents();
    label->setPos(37, 41);
    scene.settle();
    const Size size(label->getWidth(), label->getHeight());
    CHECK(rectText(label->getBounds()) == rectText(Rect(37, 41, size.width, size.height)));

    // a second pass
    label->invalidate();
    scene.parent->invalidate();
    scene.settle();
    CHECK(rectText(label->getBounds()) == rectText(Rect(37, 41, size.width, size.height)));

    // a Button too
    controls::Button *button=scene.parent->newChild<controls::Button>();
    button->setPos(60, 90);
    scene.settle();
    CHECK(button->getX() == 60);
    CHECK(button->getY() == 90);
}

TEST_CASE("content alignment: a TextBox's text follows setTextAlignment, not the box's alignment")
{
    Scene scene;
    controls::TextBox *box=scene.parent->newChild<controls::TextBox>();
    box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    box->setBounds(20, 30, 200, 20);
    box->setText("short");
    scene.settle();

    controls::internal::Text *string=textOf(box);
    REQUIRE(string != nullptr);
    const Rect fitted=string->getBounds();
    // Left|CenterV in the box's padding (4, 2, 4, 2)
    CHECK(same(string->getAlignment(), Alignment::Left|Alignment::CenterV));
    CHECK(fitted.x == 4);
    CHECK(fitted.y == 2+(16-fitted.h)/2);

    // placing the box moves the box, not its text
    box->setAlignment(Alignment::Right|Alignment::Bottom);
    scene.settle();
    CHECK(rectText(box->getBounds()) == rectText(Rect(300-7-200, 200-8-20, 200, 20)));
    CHECK(same(string->getAlignment(), Alignment::Left|Alignment::CenterV));
    CHECK(rectText(string->getBounds()) == rectText(fitted));

    // the text alignment moves the text
    box->setTextAlignment(Alignment::Right|Alignment::Bottom);
    scene.settle();
    CHECK(same(box->getTextAlignment(), Alignment::Right|Alignment::Bottom));
    CHECK(same(string->getAlignment(), Alignment::Right|Alignment::Bottom));
    CHECK(rectText(string->getBounds()) == rectText(Rect(200-4-fitted.w, 20-2-fitted.h, fitted.w, fitted.h)));
}

TEST_CASE("content alignment: a TextBox's scrolled text keeps its position")
{
    Scene scene;
    controls::TextBox *box=scene.parent->newChild<controls::TextBox>();
    box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    box->setBounds(20, 30, 100, 20);
    box->setText("a string that is a good deal wider than the box it is in");
    box->moveCaretToend();
    scene.settle();

    controls::internal::Text *string=textOf(box);
    REQUIRE(string != nullptr);
    REQUIRE(string->getWidth() > 100);
    // the tail shows: the string ends at the box's right padding
    CHECK(string->getX()+string->getWidth() == 100-4);
    const Rect scrolled=string->getBounds();

    // and stays there through another pass
    box->invalidate();
    scene.parent->invalidate();
    scene.settle();
    CHECK(rectText(string->getBounds()) == rectText(scrolled));

    // the head shows again from the left padding
    box->moveCaretToStart();
    scene.settle();
    CHECK(string->getX() == 4);
}
