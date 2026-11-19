/*
 *  Gweni - the parts an app draws with (gweni/app/shell.h).
 *
 *  See license in Gweni.h
 */

#include <gweni/app/shell.h>

#include <gweni/controls/canvas.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/skins/textured/texturedSkin.h>

namespace gweni
{
namespace app
{

Shell::Shell()=default;

Shell::~Shell()
{
    // the members' own order, written out: what uses a part goes before it
    canvas.reset();
    skin.reset();
    renderer.reset();
    device.reset();
    paths.reset();
}

Shell::Shell(Shell &&other)=default;

Shell &Shell::operator=(Shell &&other)
{
    if(this != &other)
    {
        canvas.reset();
        skin.reset();
        renderer.reset();
        device.reset();
        paths.reset();
        paths=std::move(other.paths);
        device=std::move(other.device);
        renderer=std::move(other.renderer);
        skin=std::move(other.skin);
        canvas=std::move(other.canvas);
        scale=other.scale;
        degraded=other.degraded;
    }
    return *this;
}

bool Shell::open(const ShellOptions &options)
{
    scale=options.scale;

    // Before the renderer, which keeps a reference to it. A missing directory
    // is reported there and the run carries on: what loads from it degrades.
    bool found=true;

    paths=resourcePaths(options.resources, found);
    degraded=!found;

    DeviceOptions deviceOptions;

    deviceOptions.size=options.size;
    deviceOptions.title=options.title;
    deviceOptions.capturing=options.capturing;
    deviceOptions.headless=options.headless;
    deviceOptions.systemClipboard=options.systemClipboard;
    device=openDevice(options.backend, deviceOptions);
    if(!device)
        return false;
    if(options.afterDevice && !options.afterDevice(*device))
        return false;

    renderer.reset(device->newRenderer(*paths, Rect(Point(0, 0), options.size)));
    if(options.beforeInit)
        options.beforeInit(*renderer);
    renderer->init();

    // A skin config or default font that does not load is reported by the
    // library and drawn degraded (every surface magenta; the built-in face, or
    // no text if that fails too).
    skin.reset(new skin::textured::TexturedSkin(renderer.get()));
    if(!skin->init(options.skinConfig))
        degraded=true;
    if(options.preloadGlyphFallbacks && !renderer->preloadGlyphFallbacks())
        degraded=true;
    if(loadDefaultFont(*renderer, skin->getDefaultFont()) != Font::Status::Loaded)
        degraded=true;

    const Point canvasSize=options.canvasSize ? *options.canvasSize : logicalSize(options.size, scale);

    canvas.reset(new controls::Canvas(skin.get()));
    canvas->setScale(scale);
    canvas->setSize(canvasSize.x, canvasSize.y);
    return true;
}

}//namespace app
}//namespace gweni
