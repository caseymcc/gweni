/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  app_headless (and app_headless_vulkan): gweni_app's app::Headless, from an
 *  exe that links gweni_app alone, on GWENI_RENDERER's backend (gl, or vulkan
 *  validated). It fails, one line naming the check, unless:
 *    1. two Headless instances, one after the other, running the same script
 *       (a Button and a TextBox, a click on the Button, a click into the box,
 *       keys and typed text, a spot clicked first and last) press the Button
 *       once each, click the spot twice with no double-click, type the same text
 *       and give byte-identical pixels and stats for every frame: each
 *       starts its clock, the library's, at 0 (a frame a step of 1/60 s on)
 *       and forgets the pointer state of the one before (a press at the same
 *       point on the same clock is not a double-click);
 *    2. a settled frame writes no primitive or glyph slot and lays nothing
 *       out;
 *    3. a second Headless opened while one is alive is refused (ok() false,
 *       error() saying so) and the first draws on;
 *    4. a frame is more than one flat colour, savePng writes a file and one
 *       before any frame is refused;
 *  and prints the stats' keys (app_headless: keys=...), which
 *  tests/app/headless.cmake holds to ControlTest --stats's line.
 *
 *  -- <resources dir> <scratch dir>
 */
#include <gweni/app/headless.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/textBox.h>
#include <gweni/platforms/platform.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace gweni;

int g_failures=0;

void check(bool ok, const std::string &what)
{
    if(!ok)
    {
        std::fprintf(stderr, "app_headless: FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

app::HeadlessOptions options(const std::string &resources)
{
    app::HeadlessOptions headless;
    const char *renderer=std::getenv("GWENI_RENDERER");

    headless.size=Point(320, 200);
    headless.resources=resources;
    headless.backend=(renderer && std::strcmp(renderer, "vulkan") == 0) ? app::Backend::Vulkan
        : app::Backend::GL;
    return headless;
}

struct Presses: public event::Handler
{
    int count=0;
    void onPress(event::Info) { ++count; }
};

//! A spot that counts the clicks and double-clicks it gets.
class ClickSpot: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(ClickSpot, controls::Base)
    {
        setMouseInputEnabled(true);
    }
    GWENI_DRAWS_NOTHING

    void onMouseClickLeft(int, int, bool down) override
    {
        if(down)
            ++clicks;
    }

    void onMouseDoubleClickLeft(int, int) override
    {
        ++doubleClicks;
    }

    int clicks=0;
    int doubleClicks=0;
};

std::string field(const app::StatFields &fields, const std::string &key)
{
    for(const app::StatField &f : fields)
        if(f.key == key)
            return f.value;
    return "?";
}

//! What one run of the script saw.
struct Run
{
    bool ok=false;
    int presses=0;
    int spotClicks=0;
    int spotDoubleClicks=0;
    std::string text;
    std::vector<std::string> stats;     //!< every frame's line
    std::vector<uint8_t> pixels;        //!< the last frame's
    app::StatFields settled;            //!< a frame with no input, at the end
};

Run runScript(const std::string &resources)
{
    Run run;
    app::Headless headless(options(resources));

    check(headless.ok(), "the Headless did not open: "+headless.error());
    if(!headless.ok())
        return run;
    check(!headless.degraded(), "the resources did not load");
    // its own clock, from 0, which the library reads
    check(headless.seconds() == 0.0, "a Headless's clock starts at "+std::to_string(headless.seconds())+", not 0");
    headless.frame();
    check(platform::getTimeInSeconds() == 1.0/60, "after a frame the library's clock reads "
        +std::to_string(platform::getTimeInSeconds())+", not a step of 1/60 s");

    controls::Canvas *canvas=headless.canvas();
    controls::Button *button=canvas->newChild<controls::Button>();
    controls::TextBox *box=canvas->newChild<controls::TextBox>();
    ClickSpot *spot=canvas->newChild<ClickSpot>();
    Presses presses;

    spot->setBounds(260, 20, 40, 40);
    button->setText("Press me");
    button->setPos(20, 20);
    button->onPressCaller.add(&presses, &Presses::onPress);
    box->setPos(20, 80);
    box->setWidth(200);

    auto frames=[&](int count)
    {
        for(int i=0; i < count; ++i)
        {
            headless.frame();
            run.stats.push_back(app::formatStatFields(headless.stats()));
        }
    };

    frames(3);
    // the spot first and last: the next run's first click is at the point
    // and the clock of this run's last
    headless.click(280, 40);
    frames(1);

    const Point at=button->localPosToCanvas();

    headless.click(at.x+button->getWidth()/2, at.y+button->getHeight()/2);
    frames(1);
    headless.click(box->getX()+10, box->getY()+box->getHeight()/2);
    frames(1);
    headless.type("hé");
    headless.key("left");
    headless.key("x");
    headless.key("ctrl+a");
    headless.key("end");
    headless.key("!");
    frames(2);
    headless.move(300, 190);
    frames(3);
    headless.click(280, 40);
    frames(1);

    run.presses=presses.count;
    run.spotClicks=spot->clicks;
    run.spotDoubleClicks=spot->doubleClicks;
    run.text=box->getText();
    run.pixels=headless.pixels();
    headless.frame();
    run.settled=headless.stats();
    run.ok=true;
    return run;
}

}//namespace

int main(int argc, char *argv[])
{
    app::setToolName("app_headless");

    const std::string resources=(argc > 1) ? argv[1] : "resources";
    const std::string scratch=(argc > 2) ? argv[2] : "/tmp/app_headless";

    // 1. the same script twice, one instance after the other
    const Run first=runScript(resources);
    const Run second=runScript(resources);

    if(!first.ok || !second.ok)
        return EXIT_FAILURE;
    check(first.presses == 1, "the first run pressed the Button "+std::to_string(first.presses)+" times, not once");
    check(second.presses == 1, "the second run pressed the Button "+std::to_string(second.presses)
        +" times, not once (its click the second half of a double-click?)");
    for(const Run *run : {&first, &second})
        check(run->spotClicks == 2 && run->spotDoubleClicks == 0, std::string(run == &first ? "the first" : "the second")
            +" run's spot got "+std::to_string(run->spotClicks)+" clicks and "+std::to_string(run->spotDoubleClicks)
            +" double-clicks, not 2 and 0");
    check(first.text == "hxé!", "the first run typed \""+first.text+"\", not \"hxé!\"");
    check(second.text == first.text, "the second run typed \""+second.text+"\", the first \""+first.text+"\"");
    check(first.stats.size() == second.stats.size(), "the runs drew different frame counts");
    for(size_t i=0; i < first.stats.size() && i < second.stats.size(); ++i)
        check(first.stats[i] == second.stats[i], "frame "+std::to_string(i)+"'s stats differ:\n  "
            +first.stats[i]+"\n  "+second.stats[i]);
    check(first.pixels == second.pixels, "the runs' last frames differ in their pixels");

    // 2. a settled frame
    check(field(first.settled, "prim_slots_written") == "0" && field(first.settled, "text_slots_written") == "0",
        "a settled frame wrote slots: "+app::formatStatFields(first.settled));
    check(field(first.settled, "layout_passes") == "0", "a settled frame laid out: "
        +app::formatStatFields(first.settled));

    // 3. one at a time, and 4. what a frame and a PNG are
    {
        app::Headless headless(options(resources));

        check(headless.ok(), "a Headless after the first two was refused: "+headless.error());
        check(!headless.savePng(scratch+"/before.png"), "savePng before any frame wrote a file");
        {
            app::Headless second(options(resources));

            check(!second.ok(), "a second Headless opened while one was alive");
            check(second.error().find("refused") != std::string::npos,
                "the second Headless's error does not say it was refused: "+second.error());
        }
        headless.canvas()->newChild<controls::Button>()->setText("still drawing");
        headless.frame(2);

        const std::vector<uint8_t> pixels=headless.pixels();
        std::set<uint32_t> colors;

        check(pixels.size() == size_t(headless.pixelSize().x)*headless.pixelSize().y*4,
            "pixels() is not the target's size");
        for(size_t i=0; i+3 < pixels.size() && colors.size() < 2; i+=4)
            colors.insert(uint32_t(pixels[i]) | uint32_t(pixels[i+1]) << 8 | uint32_t(pixels[i+2]) << 16);
        check(colors.size() > 1, "a frame after a refused second Headless is one flat colour");
        check(headless.savePng(scratch+"/frame.png"), "savePng did not write "+scratch+"/frame.png");

        std::string keys;

        for(const app::StatField &f : headless.stats())
            keys+=(keys.empty() ? "" : ",")+f.key;
        std::printf("app_headless: keys=%s\n", keys.c_str());
    }

    if(g_failures)
        return EXIT_FAILURE;
    std::printf("app_headless: passed\n");
    return EXIT_SUCCESS;
}
