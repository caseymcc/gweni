/*
 *  Gweni - the parts an app draws with (gweni_app, gweni::app): the resource
 *  paths, a device of either backend, its renderer init()ed, the textured
 *  skin and a canvas, opened in the order every app needs and given back in
 *  the reverse one. app::Window shows them in a window; an app's own capture
 *  loop (the examples', the bench's) draws them offscreen.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_app_shell_h_
#define _gweni_app_shell_h_

#include <gweni/app/device.h>
#include <gweni/platforms/platformTypes.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace gweni
{
namespace controls
{
class Canvas;
}
namespace renderer
{
class SlotRenderer;
}
namespace skin
{
namespace textured
{
class TexturedSkin;
}
}

namespace app
{

struct ShellOptions
{
    //! the framebuffer, the window's or the offscreen target's, in device px
    Point size=Point(1024, 768);
    //! the canvas in logical units; logicalSize(size, scale) when not given
    std::optional<Point> canvasSize;
    //! the manual scale (an app's --scale): device px per logical unit; a
    //! window multiplies its OS content scale by it (fitCanvasToWindow)
    float scale=1.0f;
    //! the window's title
    std::string title="gweni";
    Backend backend=Backend::GL;
    //! the resource directory (relative to the working directory, or
    //! absolute); empty: the resources/ beside the executable (resourcePaths)
    std::string resources;
    //! the textured skin's config, a name in the resources
    std::string skinConfig="DefaultSkin.json";
    //! DeviceOptions' capturing, headless and systemClipboard
    bool capturing=false;
    bool headless=false;
    bool systemClipboard=true;
    //! read every glyph fallback face before the first layout
    //! (SlotRenderer::preloadGlyphFallbacks), as a capture does so that what it
    //! draws never waits on a lazy face
    bool preloadGlyphFallbacks=false;
    //! called once the device is open, its context current, before the
    //! renderer is made: an app's own GL objects. False fails open().
    std::function<bool(Device &)> afterDevice;
    //! called with the renderer before its init() (OpenGLCore's
    //! setPreserveAppState, say)
    std::function<void(renderer::SlotRenderer &)> beforeInit;
};

//! What an app draws with. Its members are its own and given back in the
//! reverse of their order (the canvas first, the paths last), so an app may
//! replace one (a canvas built again) through its unique_ptr, but not while an
//! app::Window or app::Headless holds the Shell: their input::GLFW3 is pointed
//! at the canvas once, as they open, and would be left on the deleted one.
class Shell
{
public:
    Shell();
    ~Shell();
    Shell(Shell &&);
    Shell &operator=(Shell &&);

    //! Opens every part: the resource paths, the device (openDevice), its
    //! renderer, init()ed, the textured skin init()ed from skinConfig, the
    //! skin's default font (loadDefaultFont) and the canvas at the canvas
    //! size and the scale. A missing resource directory, skin config or font
    //! has printed its line and sets degraded(), and the parts still open,
    //! drawing as the library degrades them. \return false only when the
    //! device cannot be made (after its line on stderr) or afterDevice
    //! returns false (which prints what it likes).
    bool open(const ShellOptions &options);

    std::unique_ptr<ResourcePaths> paths;
    std::unique_ptr<Device> device;
    std::unique_ptr<renderer::SlotRenderer> renderer;
    std::unique_ptr<skin::textured::TexturedSkin> skin;
    std::unique_ptr<controls::Canvas> canvas;

    //! the manual scale it was opened at
    float scale=1.0f;
    //! a resource it was given did not load (the run draws degraded)
    bool degraded=false;
};

}//namespace app
}//namespace gweni

#endif//_gweni_app_shell_h_
