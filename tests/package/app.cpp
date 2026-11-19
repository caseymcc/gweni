/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  package_consumer_app: an app built against an installed gweni that links
 *  gweni::app alone (CMakeLists.txt beside it) and draws through its
 *  app::Headless, from the resources/ gweni_add_resources put beside the
 *  executable (the Headless's default resource directory). On each renderer
 *  the package was built with (GL; Vulkan with GWENI_HAS_VULKAN, validated when
 *  GWENI_VULKAN_VALIDATION=1, which the test sets) it puts a Label, a Button, a
 *  CheckBoxWithLabel and a TextBox on the Headless's canvas, draws, clicks the
 *  Button and draws again, and fails, with one line naming it, if
 *  - the Headless does not open;
 *  - the skin config or the skin's default font does not load from there
 *    (degraded(); the glyph fallback faces are not preloaded, since the
 *    package has no lazy CJK face);
 *  - a control type has no skin renderer;
 *  - the click does not press the Button once;
 *  - the frame is one flat colour, or savePng does not write a file.
 *
 *  On the web (package_consumer_web) the Headless's context is WebGL 2 on the
 *  page's #canvas and the resources are the ones gweni_add_resources preloaded
 *  at /resources.
 */
#include <gweni/app/headless.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace gweni;

bool fail(const char *backend, const std::string &message)
{
    std::fprintf(stderr, "package_consumer_app: %s: %s\n", backend, message.c_str());
    return false;
}

struct Presses: public event::Handler
{
    int count=0;
    void onPress(event::Info) { ++count; }
};

bool drawFrame(app::Backend backend, const char *name)
{
    app::HeadlessOptions options;

    options.size=Point(320, 200);
    options.backend=backend;
    options.preloadGlyphFallbacks=false;

    app::Headless headless(options);

    if(!headless.ok())
        return fail(name, "the Headless did not open: "+headless.error());
    if(headless.degraded())
        return fail(name, "the skin or its default font did not load from the resources/ beside the executable");

    controls::Canvas *canvas=headless.canvas();
    controls::Label *label=canvas->newChild<controls::Label>();
    label->setText("gweni::app from a package");
    label->setBounds(10, 10, 200, 20);
    controls::Button *button=canvas->newChild<controls::Button>();
    button->setText("Button");
    button->setBounds(10, 40, 100, 24);
    controls::CheckBoxWithLabel *check=canvas->newChild<controls::CheckBoxWithLabel>();
    check->label()->setText("Check");
    check->checkBox()->setChecked(true);
    check->setBounds(10, 74, 150, 20);
    controls::TextBox *text=canvas->newChild<controls::TextBox>();
    text->setText("text");
    text->setBounds(10, 104, 150, 24);

    Presses presses;

    button->onPressCaller.add(&presses, &Presses::onPress);
    headless.frame(2);

    // at its centre as laid out (a Button's size follows its text)
    const Point at=button->localPosToCanvas();

    headless.click(at.x+button->getWidth()/2, at.y+button->getHeight()/2);
    headless.frame(2);

    const std::vector<std::string> missing=controls::typesWithoutSkinRenderer();

    if(!missing.empty())
    {
        std::string names;
        for(const std::string &type : missing)
            names+=" "+type;
        return fail(name, "control types without a skin renderer:"+names);
    }
    if(presses.count != 1)
        return fail(name, "the click at the Button's centre pressed it "+std::to_string(presses.count)
            +" times, not once (its bounds "+std::to_string(button->getX())+","+std::to_string(button->getY())+" "
            +std::to_string(button->getWidth())+"x"+std::to_string(button->getHeight())+")");

    const std::vector<uint8_t> pixels=headless.pixels();
    std::set<uint32_t> colors;

    if(pixels.size() != size_t(headless.pixelSize().x)*headless.pixelSize().y*4)
        return fail(name, "pixels() is not the target's size");
    for(size_t i=0; i < pixels.size(); i+=4)
    {
        uint32_t color;
        std::memcpy(&color, &pixels[i], 4);
        colors.insert(color);
    }
    if(colors.size() < 2)
        return fail(name, "the frame is one flat colour: nothing was drawn");
    if(!headless.savePng(std::string("package_consumer_app_")+name+".png"))
        return fail(name, "savePng wrote no file");

    std::printf("package_consumer_app: %s: %dx%d frame, %zu colours\n", name,
        headless.pixelSize().x, headless.pixelSize().y, colors.size());
    return true;
}

}//namespace

int main()
{
    app::setToolName("package_consumer_app");

    bool ok=drawFrame(app::Backend::GL, "gl");

#ifdef GWENI_HAS_VULKAN
    ok=drawFrame(app::Backend::Vulkan, "vulkan") && ok;
#endif
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
