/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A textured renderer's staticInit runs once per process, not once per
 *  renderer: ControlRegister::create sets m_staticInit itself after calling
 *  it, so a staticInit that does not set the flag (the textured CheckBox's,
 *  which is empty) is not run again for every control it draws (#514).
 *
 *  On a real, un-init()'d TexturedSkin over the StubRenderer, two CheckBoxes
 *  on a Canvas get textured::CheckBox renderers and the flag is set after
 *  them. A probe renderer whose staticInit counts its calls and never sets
 *  the flag is made three times through create: its staticInit runs once,
 *  for the first.
 */
#include "headlessFixture.h"

#include <gweni/controls/checkBox.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <memory>

using namespace gweni;

namespace
{

//! A renderer whose staticInit forgets to set m_staticInit.
class StaticInitProbe: public skin::textured::ControlRegister<StaticInitProbe>
{
public:
    static void staticInit(skin::textured::TexturedSkin *) { ++calls; }

    inline static const std::string_view name="StaticInitProbe";
    inline static int calls=0;
};

}//namespace

TEST_CASE("Static init: the textured CheckBox renderer is initialised once, by ControlRegister::create")
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin(&renderer);
    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(300, 200);

    controls::CheckBox *first=canvas->newChild<controls::CheckBox>();
    controls::CheckBox *second=canvas->newChild<controls::CheckBox>();

    CHECK(dynamic_cast<const skin::textured::CheckBox *>(first->getSkinControl()) != nullptr);
    CHECK(dynamic_cast<const skin::textured::CheckBox *>(second->getSkinControl()) != nullptr);
    CHECK(skin::textured::CheckBox::isStaticInit());

    delete canvas;
}

TEST_CASE("Static init: create runs a staticInit that does not set the flag only for the first renderer")
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin(&renderer);

    // nothing else makes the probe
    REQUIRE_FALSE(StaticInitProbe::isStaticInit());

    for(int i=0; i<3; ++i)
    {
        std::unique_ptr<skin::Control> control=StaticInitProbe::create(&skin);
        CHECK(control != nullptr);
    }

    CHECK(StaticInitProbe::isStaticInit());
    CHECK(StaticInitProbe::calls == 1);
}
