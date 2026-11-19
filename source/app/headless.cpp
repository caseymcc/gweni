/*
 *  Gweni - a gweni canvas rendered headless (gweni/app/headless.h).
 *
 *  See license in Gweni.h
 */

#include <gweni/app/headless.h>

#include "instance.h"

#include <gweni/app/keys.h>
#include <gweni/controls/canvas.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/platform.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <iostream>

namespace gweni
{
namespace app
{

//! A press steps the clock a second on, as ControlTest's does, so two presses
//! are never a double-click by accident.
static const int64_t PressSteps=60;
static const double FrameStep=1.0/60.0;

Headless::Headless(const HeadlessOptions &options):
    m_options(options)
{
    open(options);
}

Headless::Headless(const Point &size, float scale, const std::string &resources)
{
    m_options.size=size;
    m_options.scale=scale;
    m_options.resources=resources;
    open(m_options);
}

void Headless::open(const HeadlessOptions &options)
{
    if(!internal::claimInstance("app::Headless", m_error))
        return;
    m_claimed=true;

    // its own clock, from 0, and nothing of an earlier canvas's pointer
    platform::setTimeSource([this]() { return seconds(); });
    input::resetPointerState();

    m_pixels=Point(renderer::Base::snap(float(options.size.x), options.scale),
        renderer::Base::snap(float(options.size.y), options.scale));

    ShellOptions shell;

    shell.size=m_pixels;
    shell.canvasSize=options.size;
    shell.scale=options.scale;
    shell.title="gweni headless";
    shell.backend=options.backend;
    shell.resources=options.resources;
    shell.capturing=true;
    shell.headless=true;
    // every glyph fallback face before the first layout, so what it draws
    // never waits on a lazy face
    shell.preloadGlyphFallbacks=options.preloadGlyphFallbacks;
    if(!m_shell.open(shell))
    {
        m_error="app::Headless: the headless device did not open";
        return;
    }

    m_background=skin::textured::tokyonight::Canvas.top;
    m_input.initialize(m_shell.canvas.get());
    m_measuresAtFrame=controls::measureTextCalls();
    m_invalidationsAtFrame=controls::invalidateCalls();
    m_ok=true;
}

Headless::~Headless()
{
    if(!m_claimed)
        return;
    // the canvas before the clock it reads goes
    m_shell=Shell();
    platform::setTimeSource(nullptr);
    input::resetPointerState();
    internal::releaseInstance();
}

controls::Canvas *Headless::canvas() const
{
    return m_shell.canvas.get();
}

renderer::SlotRenderer *Headless::renderer() const
{
    return m_shell.renderer.get();
}

skin::textured::TexturedSkin *Headless::skin() const
{
    return m_shell.skin.get();
}

double Headless::seconds() const
{
    return double(m_clockSteps)*FrameStep;
}

void Headless::frame(int count)
{
    if(!m_ok)
        return;

    controls::Canvas *canvas=m_shell.canvas.get();
    renderer::SlotRenderer *renderer=m_shell.renderer.get();

    for(int i=0; i < count; ++i)
    {
        controls::resetChangeStats();
        canvas->updateCanvas();
        renderer->clearTarget(m_background);
        canvas->renderCanvas();

        const size_t measures=controls::measureTextCalls()-m_measuresAtFrame;
        const size_t invalidations=controls::invalidateCalls()-m_invalidationsAtFrame;

        m_stats=frameStatFields(m_frames, m_options.label, controls::getChangeStats(),
            renderer->getFrameStats(), canvas->getLayoutStats(), canvas->getUpdateStats(),
            measures, invalidations);
        m_measuresAtFrame=controls::measureTextCalls();
        m_invalidationsAtFrame=controls::invalidateCalls();
        ++m_frames;
        ++m_clockSteps;
    }
}

void Headless::move(int x, int y)
{
    if(!m_ok)
        return;

    const Point delta=(m_mouse.x < 0 && m_mouse.y < 0) ? Point(0, 0) : Point(x-m_mouse.x, y-m_mouse.y);

    m_mouse=Point(x, y);
    m_shell.canvas->inputMouseMoved(x, y, delta.x, delta.y);
}

void Headless::press(int x, int y, int button)
{
    if(!m_ok)
        return;
    m_clockSteps+=PressSteps;
    move(x, y);
    m_input.processMouseButtons(button, GLFW_PRESS, 0);
}

void Headless::release(int x, int y, int button)
{
    if(!m_ok)
        return;
    move(x, y);
    m_input.processMouseButtons(button, GLFW_RELEASE, 0);
}

void Headless::click(int x, int y, int button)
{
    press(x, y, button);
    release(x, y, button);
}

bool Headless::wheel(int delta)
{
    return m_ok && m_shell.canvas->inputMouseWheel(delta);
}

bool Headless::wheelH(int delta)
{
    return m_ok && m_shell.canvas->inputMouseWheelH(delta);
}

bool Headless::key(const std::string &name)
{
    if(!m_ok)
        return false;

    Chord chord;
    std::string part;

    switch(parseChord(name, chord, part))
    {
    case ChordError::NotAModifier:
        std::cerr<<toolName()<<": app::Headless::key(\""<<name<<"\"): "<<part
                 <<" is not a modifier (ctrl, shift, alt, command)\n";
        return false;
    case ChordError::UnknownKey:
        std::cerr<<toolName()<<": app::Headless::key(\""<<name<<"\"): unknown key name\n";
        return false;
    case ChordError::None:
        break;
    }

    if(chord.isCharacter)
    {
        m_input.processCharEvent(chord.character);
        return true;
    }
    for(const KeyStroke &stroke : chord.strokes)
        m_input.processKeyEvent({stroke.key, 0, stroke.action, stroke.mods});
    return true;
}

void Headless::type(const std::string &text)
{
    if(!m_ok)
        return;
    for(unsigned int codepoint : decodeUtf8(text))
        m_input.processCharEvent(codepoint);
}

std::vector<uint8_t> Headless::pixels()
{
    std::vector<uint8_t> rows;

    if(m_ok)
        m_shell.renderer->readPixels(m_pixels.x, m_pixels.y, rows);
    return rows;
}

bool Headless::savePng(const std::string &path)
{
    if(!m_ok)
        return false;
    if(m_frames == 0)
    {
        std::cerr<<toolName()<<": app::Headless::savePng("<<path<<"): before any frame was rendered\n";
        return false;
    }
    if(!ensureDirectory(fs::path(path).parent_path()))
        return false;
    return writeRendererPng(*m_shell.renderer, m_pixels.x, m_pixels.y, path);
}

}//namespace app
}//namespace gweni
