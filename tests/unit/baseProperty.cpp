/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A plain property::Base, one with no getPropertyValue() override (a user
 *  subclass that does not override it, or property::Base::newInstance()
 *  handed to Properties::add), has an empty value: getPropertyValue()
 *  returns an empty String and does not throw, and doChanged() delivers ""
 *  to onChange. It used to be `return 0;`, a std::string built from a null
 *  const char* (undefined behaviour; libstdc++ throws std::logic_error).
 */
#include "headlessFixture.h"

#include <gweni/controls/property/baseProperty.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    ~Scene()
    {
        delete canvas;
    }
};

struct ChangeRecorder: public event::Handler
{
    int count=0;
    String value="not called";

    void changed(event::Info info)
    {
        ++count;
        value=info.string;
    }
};

} // namespace

TEST_CASE("a plain property::Base has an empty value and delivers \"\" to onChange")
{
    Scene scene;

    controls::property::Base *property=scene.canvas->newChild<controls::property::Base>();

    String value="not called";
    CHECK_NOTHROW(value=property->getPropertyValue());
    CHECK(value.empty());

    ChangeRecorder recorder;
    property->onChange.add(&recorder, &ChangeRecorder::changed);

    CHECK_NOTHROW(property->doChanged());
    CHECK(recorder.count == 1);
    CHECK(recorder.value == "");
}
