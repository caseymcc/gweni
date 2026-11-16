/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What a browser page's own input listeners hand gweni (platform::
 *  BrowserInput, through input::GLFW3::useBrowserInput), taken by the GLFW
 *  translator on its own, with a recording IInputEvent: a touch at window
 *  coordinates reaches inputTouch in logical units with its fractions kept,
 *  at the window's framebuffer ratio and scale, in its phase; a text field's
 *  key (Backspace) is a press and a release; its typed text is characters,
 *  never taken for the letter of a Ctrl+letter just before it. The page side
 *  itself (the listeners, the hidden textarea) is the web tree's, checked by
 *  tests/web; natively useBrowserInput does nothing.
 */
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

//! Writes every call down, one line each.
class Recorder: public IInputEvent
{
public:
    bool inputMouseMoved(int, int, int, int) override { return true; }
    bool inputMouseButton(int, bool) override { return true; }
    bool inputModifierKey(int key, bool down) override
    {
        calls.push_back("key "+std::to_string(key)+(down ? " down" : " up"));
        return true;
    }
    bool inputCharacter(UnicodeChar chr) override
    {
        calls.push_back("char "+std::to_string(static_cast<unsigned>(chr)));
        return true;
    }
    bool inputMouseWheel(int) override { return true; }
    bool inputTouch(int id, TouchPhase phase, float x, float y) override
    {
        touches.push_back({id, phase, x, y});
        return true;
    }
    bool inputQuit() override { return true; }

    struct Touch
    {
        int id;
        TouchPhase phase;
        float x, y;
    };

    std::vector<std::string> calls;
    std::vector<Touch> touches;
};

}//namespace

TEST_CASE("Browser input: a touch is in logical units with its fractions, at the framebuffer ratio and scale")
{
    Recorder recorder;
    input::GLFW3 glfw;

    glfw.initialize(&recorder);
    // a page at devicePixelRatio 2, the canvas at scale 1.5 on top of it:
    // a CSS px is 2 device px, a logical unit 3
    glfw.setWindowMetrics(Point(400, 300), Point(800, 600), 3.0f);

    CHECK(glfw.processTouch(7, TouchPhase::Down, 31.5, 10.25));
    CHECK(glfw.processTouch(7, TouchPhase::Move, 32.0, 11.0));
    CHECK(glfw.processTouch(7, TouchPhase::Up, 32.0, 11.0));
    REQUIRE(recorder.touches.size() == 3);
    CHECK(recorder.touches[0].id == 7);
    CHECK(recorder.touches[0].phase == TouchPhase::Down);
    CHECK(recorder.touches[0].x == doctest::Approx(21.0));
    CHECK(recorder.touches[0].y == doctest::Approx(10.25*2.0/3.0));
    CHECK(recorder.touches[1].phase == TouchPhase::Move);
    CHECK(recorder.touches[1].x == doctest::Approx(64.0/3.0));
    CHECK(recorder.touches[2].phase == TouchPhase::Up);
}

TEST_CASE("Browser input: a text field's key is a press and a release, its text characters")
{
    Recorder recorder;
    input::GLFW3 glfw;

    glfw.initialize(&recorder);
    CHECK(glfw.processKeyTap(keys::Backspace));
    // Ctrl+V's letter is dropped as the char that follows it; text the field
    // took after it is not that char
    glfw.processKeyEvent({GLFW_KEY_V, 0, GLFW_PRESS, GLFW_MOD_CONTROL});
    recorder.calls.clear();
    CHECK(glfw.processTypedCharacter(U'v'));

    REQUIRE(recorder.calls.size() == 1);
    CHECK(recorder.calls[0] == "char 118");

    recorder.calls.clear();
    CHECK(glfw.processKeyTap(keys::Backspace));
    REQUIRE(recorder.calls.size() == 2);
    CHECK(recorder.calls[0] == "key "+std::to_string(keys::Backspace)+" down");
    CHECK(recorder.calls[1] == "key "+std::to_string(keys::Backspace)+" up");
}
