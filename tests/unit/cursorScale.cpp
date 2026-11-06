/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The GLFW translator hands the canvas the cursor in logical units: GLFW
 *  reports it in window coordinates, which input::windowToLogical maps through
 *  the framebuffer/window ratio and the canvas scale. On X11 and Windows the
 *  window is in pixels (window == framebuffer), on macOS and scaled Wayland in
 *  points (the framebuffer is larger); a manual zoom multiplies the scale on
 *  top. The mapping happens before the truncation to whole units, and the
 *  deltas are in logical units too.
 */
#include <gweni/platforms/input/glfw3.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

struct Move
{
    int x, y, dx, dy;
};

// Records the moves; nothing else is asked of it.
struct MoveRecorder: public IInputEvent
{
    std::vector<Move> moves;

    bool inputMouseMoved(int x, int y, int deltaX, int deltaY) override
    {
        moves.push_back({x, y, deltaX, deltaY});
        return true;
    }
    bool inputMouseButton(int, bool) override { return false; }
    bool inputModifierKey(int, bool) override { return false; }
    bool inputCharacter(UnicodeChar) override { return false; }
    bool inputMouseWheel(int) override { return false; }
    bool inputQuit() override { return false; }
};

}//namespace

TEST_CASE("windowToLogical: X11-like, window in pixels, scale 1.5")
{
    // a 1500x900 px window on a 150 % desktop is a 1000x600 canvas
    CHECK(input::windowToLogical(0.0, 1500, 1500, 1.5f) == doctest::Approx(0.0));
    CHECK(input::windowToLogical(1500.0, 1500, 1500, 1.5f) == doctest::Approx(1000.0));
    CHECK(input::windowToLogical(300.0, 900, 900, 1.5f) == doctest::Approx(200.0));
    CHECK(input::windowToLogical(301.0, 900, 900, 1.5f) == doctest::Approx(200.6666667));
}

TEST_CASE("windowToLogical: macOS-like, framebuffer twice the window, scale 2")
{
    // an 800x600 pt window has a 1600x1200 px framebuffer and an 800x600 canvas:
    // the point is the logical unit
    CHECK(input::windowToLogical(400.0, 800, 1600, 2.0f) == doctest::Approx(400.0));
    CHECK(input::windowToLogical(123.5, 600, 1200, 2.0f) == doctest::Approx(123.5));
    CHECK(input::windowToLogical(800.0, 800, 1600, 2.0f) == doctest::Approx(800.0));
}

TEST_CASE("windowToLogical: a 1.25 manual zoom on top of either")
{
    // macOS: content scale 2 times 1.25 is 2.5, so a point is 0.8 units
    CHECK(input::windowToLogical(400.0, 800, 1600, 2.0f*1.25f) == doctest::Approx(320.0));
    // X11 at 150 %: 1.5 times 1.25 is 1.875
    CHECK(input::windowToLogical(375.0, 1500, 1500, 1.5f*1.25f) == doctest::Approx(200.0));
    // and at 1x the zoom alone
    CHECK(input::windowToLogical(500.0, 1000, 1000, 1.25f) == doctest::Approx(400.0));
}

TEST_CASE("windowToLogical: an unsized window or no scale leaves the coordinate as it is")
{
    CHECK(input::windowToLogical(37.0, 0, 0, 2.0f) == doctest::Approx(37.0));
    CHECK(input::windowToLogical(37.0, 100, 0, 2.0f) == doctest::Approx(37.0));
    CHECK(input::windowToLogical(37.0, 100, 200, 0.0f) == doctest::Approx(37.0));
}

TEST_CASE("GLFW3::processMouseMove: logical position and delta, truncated after the mapping")
{
    MoveRecorder recorder;
    input::GLFW3 glfw;
    glfw.initialize(&recorder);

    SUBCASE("before setWindowMetrics the cursor is taken as logical")
    {
        glfw.processMouseMove(10.9, 20.2);
        REQUIRE(recorder.moves.size() == 1);
        CHECK(recorder.moves[0].x == 10);
        CHECK(recorder.moves[0].y == 20);
    }

    SUBCASE("X11-like at 1.5")
    {
        glfw.setWindowMetrics(Point(1500, 900), Point(1500, 900), 1.5f);

        glfw.processMouseMove(301.0, 151.0);    // 200.67, 100.67
        glfw.processMouseMove(302.0, 152.0);    // 201.33, 101.33: the pixel steps
                                                // land on the next unit
        glfw.processMouseMove(450.0, 0.0);      // 300, 0

        REQUIRE(recorder.moves.size() == 3);
        CHECK(recorder.moves[0].x == 200);
        CHECK(recorder.moves[0].y == 100);
        CHECK(recorder.moves[0].dx == 0);       // the first move has no previous one
        CHECK(recorder.moves[0].dy == 0);
        CHECK(recorder.moves[1].x == 201);
        CHECK(recorder.moves[1].y == 101);
        CHECK(recorder.moves[1].dx == 1);
        CHECK(recorder.moves[1].dy == 1);
        CHECK(recorder.moves[2].x == 300);
        CHECK(recorder.moves[2].y == 0);
        CHECK(recorder.moves[2].dx == 99);
        CHECK(recorder.moves[2].dy == -101);
    }

    SUBCASE("macOS-like at 2 with a 1.25 zoom")
    {
        glfw.setWindowMetrics(Point(800, 600), Point(1600, 1200), 2.5f);

        glfw.processMouseMove(100.0, 50.0);     // 80, 40
        glfw.processMouseMove(200.6, 100.6);    // 160.48, 80.48

        REQUIRE(recorder.moves.size() == 2);
        CHECK(recorder.moves[0].x == 80);
        CHECK(recorder.moves[0].y == 40);
        CHECK(recorder.moves[1].x == 160);
        CHECK(recorder.moves[1].y == 80);
        CHECK(recorder.moves[1].dx == 80);
        CHECK(recorder.moves[1].dy == 40);
    }

    SUBCASE("a cursor left of the window maps to a negative unit, not to 0")
    {
        glfw.setWindowMetrics(Point(1500, 900), Point(1500, 900), 1.5f);

        glfw.processMouseMove(-0.5, -1.0);
        REQUIRE(recorder.moves.size() == 1);
        CHECK(recorder.moves[0].x == -1);
        CHECK(recorder.moves[0].y == -1);
    }
}
