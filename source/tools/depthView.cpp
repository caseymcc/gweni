/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/depthView.h>

#include <gweni/inputHandler.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/skin.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace gweni
{
namespace tools
{

namespace
{

//! Every open view; a handful at most, one per canvas
std::vector<DepthView *> &openViews()
{
    static std::vector<DepthView *> views;

    return views;
}

float radians(float degrees)
{
    return degrees*3.14159265358979f/180.0f;
}

}//namespace

event::Caller DepthView::onOpenChangedCaller;
event::Caller DepthView::onSpacingChangedCaller;

bool DepthView::Camera::operator==(const Camera &other) const
{
    return perspective == other.perspective && yaw == other.yaw && pitch == other.pitch
        && distance == other.distance && panX == other.panX && panY == other.panY
        && zSpacing == other.zSpacing;
}

DepthView *DepthView::open(controls::Canvas *canvas, renderer::Base *render)
{
    if(DepthView *view=find(canvas))
        return view;
    if(!canvas)
        return nullptr;

    if(!render && canvas->getSkin())
        render=canvas->getSkin()->getRenderer();

    renderer::SlotRenderer *slots=dynamic_cast<renderer::SlotRenderer *>(render);

    if(!slots)
    {
        std::fprintf(stderr, "gweni: the depth view needs a SlotRenderer (OpenGLCore,"
            " renderer::Vulkan) to draw through; not opened\n");
        return nullptr;
    }
    for(DepthView *view:openViews())
    {
        if(view->m_renderer == slots)
        {
            std::fprintf(stderr, "gweni: the renderer draws another canvas's depth view;"
                " not opened\n");
            return nullptr;
        }
    }

    DepthView *view=new DepthView(canvas, slots);

    openViews().push_back(view);
    canvas->setInputCapture(view);
    // the layers the shown slots use, counted now from what the renderer last
    // drew (nothing yet, when the view opens before the canvas's first frame)
    // and kept by each flush, which tells the view before its draws
    slots->setLayerTable(true);
    slots->setLayerRemap(true);
    slots->setLayerTableListener([view]{ view->layersChanged(); });
    // an Alt held as it opens goes up through the capture too
    view->m_alt=input::isKeyDown(keys::Alt);
    view->reset();

    event::Info info(canvas);

    info.integer=1;
    onOpenChangedCaller.call(canvas, info);
    return view;
}

void DepthView::close(controls::Canvas *canvas)
{
    DepthView *view=find(canvas);

    if(!view)
        return;

    delete view;

    event::Info info(canvas);

    info.integer=0;
    onOpenChangedCaller.call(canvas, info);
}

bool DepthView::toggle(controls::Canvas *canvas, renderer::Base *render)
{
    if(find(canvas))
    {
        close(canvas);
        return false;
    }
    return open(canvas, render) != nullptr;
}

DepthView *DepthView::find(controls::Canvas *canvas)
{
    for(DepthView *view:openViews())
    {
        if(view->m_canvas == canvas)
            return view;
    }
    return nullptr;
}

void DepthView::viewChanged(controls::Canvas *canvas)
{
    DepthView *view=find(canvas);

    if(!view)
        return;
    // the reset camera is fitted to the new view, any other keeps its spacing
    // factor (so the stack's depth, half the new width times it)
    if(view->m_atReset)
        view->m_camera=view->resetCamera();
    else
        view->keepSpacingFactor();
    view->apply();
}

DepthView::DepthView(controls::Canvas *canvas, renderer::SlotRenderer *renderer)
    : m_canvas(canvas), m_renderer(renderer)
{
    std::fill(m_transform, m_transform+16, 0.0f);
}

DepthView::~DepthView()
{
    std::vector<DepthView *> &views=openViews();

    views.erase(std::remove(views.begin(), views.end(), this), views.end());
    m_renderer->setLayerTableListener(nullptr);
    m_renderer->setLayerTable(false);
    m_renderer->clearViewTransform();
    if(m_canvas)
    {
        if(m_canvas->inputCapture() == this)
            m_canvas->setInputCapture(nullptr);
        m_canvas->redraw();
    }
}

void DepthView::captureCanvasDeleted(controls::Canvas *canvas)
{
    m_canvas=nullptr;
    delete this;
}

DepthView::Camera DepthView::resetCamera() const
{
    Camera camera;

    camera.zSpacing=defaultZSpacing();
    // backed off by the highest z's depth, so that layer is drawn at its size
    // and every one behind it smaller, about the view's centre
    camera.distance=1.0f+camera.zSpacing*highestZ()/focalLength();
    return camera;
}

float DepthView::defaultZSpacing() const
{
    const Rect &view=m_renderer->viewRect();

    return 0.5f*(float)std::max(view.w, 1)/highestZ();
}

float DepthView::stackDepth(float zSpacing) const
{
    return zSpacing*highestZ()/focalLength();
}

float DepthView::highestZ() const
{
    if(m_rawLabels)
        return (float)std::max(m_canvas?m_canvas->highestZIndex():0, 1);
    // layer 0 is label 0's; the shown labels are layers 1 up
    return (float)std::max(m_renderer->shownLayerCount(), size_t(2))-1.0f;
}

void DepthView::reset()
{
    place(resetCamera(), 1.0f);
    m_atReset=true;
    spacingChanged();
}

void DepthView::setSpacingFactor(float factor)
{
    // The front layer stays where it is, the end of the stack nearer the
    // camera. Seen from the front that is the highest layer: its centre,
    // (0, 0, depth) in the model turned by yaw and pitch, is held in place in
    // the camera's space, the pan taking up what the depth's change moves
    // across the view and, in perspective, the distance what it moves along
    // it, so the back plane goes back by what the stack gained (straight on,
    // distance alone). In orthographic the distance is only the zoom, which
    // stays. Seen from behind (the turned z axis pointing away from the
    // camera) it is z 0, the orbit's pivot, which the spacing does not move,
    // so the spacing alone changes and the stack deepens away from the camera
    // there too.
    followRawHighestZ();

    Camera camera=m_camera;
    const float clamped=clampedFactor(factor);
    const float spacing=clamped*defaultZSpacing();
    const float gained=(spacing-m_camera.zSpacing)*highestZ();
    const float yaw=radians(m_camera.yaw);
    const float pitch=radians(m_camera.pitch);

    if(std::cos(yaw)*std::cos(pitch) >= 0.0f)
    {
        camera.panX-=gained*std::sin(yaw);
        camera.panY-=gained*std::cos(yaw)*std::sin(pitch);
        if(camera.perspective)
            camera.distance+=gained*std::cos(yaw)*std::cos(pitch)/focalLength();
    }
    camera.zSpacing=spacing;
    if((camera != m_camera) && place(camera, clamped))
        spacingChanged();
}

void DepthView::stepSpacing(float steps)
{
    setSpacingFactor(m_spacingFactor*std::pow(ZSpacingStep, steps));
}

float DepthView::clampedFactor(float factor)
{
    return std::min(std::max(factor, MinSpacingFactor), MaxSpacingFactor);
}

void DepthView::keepSpacingFactor()
{
    m_camera.zSpacing=m_spacingFactor*defaultZSpacing();
    m_placedSpacing=m_camera.zSpacing;
    m_placedHighZ=highestZ();
}

bool DepthView::followRawHighestZ()
{
    // With raw labels the stack's depth follows the canvas's highest z, which
    // a renumber moves with no change of the spacing; the factor is kept, so
    // the stack keeps its depth (a camera at reset is fitted again)
    if(!m_rawLabels || highestZ() == m_placedHighZ)
        return false;
    if(m_atReset)
    {
        m_camera=resetCamera();
        m_placedSpacing=m_camera.zSpacing;
        m_placedHighZ=highestZ();
        m_spacingFactor=1.0f;
    }
    else
        keepSpacingFactor();
    return true;
}

void DepthView::spacingChanged()
{
    if(!m_canvas)
        return;

    event::Info info(m_canvas);

    onSpacingChangedCaller.call(m_canvas, info);
}

void DepthView::setRawLabels(bool raw)
{
    if(raw == m_rawLabels)
        return;

    // the stack keeps its depth (the spacing factor times half the view's
    // width), so the front layer stays where it is
    m_rawLabels=raw;
    m_renderer->setLayerRemap(!raw);
    keepSpacingFactor();
    apply();
}

void DepthView::layersChanged()
{
    // In flush(), before its draws: the shown layers are new, so a camera at
    // reset is fitted to them again and any other gets a depth range that
    // covers the stack; no redraw, this frame draws with it already. Raw
    // labels do not depend on the table, only on the canvas's highest z,
    // which followRawHighestZ() follows. A turned camera keeps its spacing
    // factor, so the stack its depth and the front layer its place.
    if(m_rawLabels)
    {
        if(followRawHighestZ())
            apply(false);
        return;
    }
    if(m_atReset)
        m_camera=resetCamera();
    else
        keepSpacingFactor();
    apply(false);
}

float DepthView::focalLength() const
{
    const Rect &view=m_renderer->viewRect();

    return FocalViews*(float)std::max(std::max(view.w, view.h), 1);
}

void DepthView::setCamera(const Camera &camera)
{
    if(place(camera))
        spacingChanged();
}

bool DepthView::place(const Camera &camera, float factor)
{
    const float before=m_spacingFactor;

    m_atReset=false;
    m_camera=camera;
    m_camera.pitch=std::min(std::max(m_camera.pitch, -90.0f), 90.0f);
    // the spacing as a factor of the reset one, clamped there, so its ends are
    // exact; an unchanged spacing keeps its factor as it is, and one given
    // with its factor takes that
    if(factor > 0.0f)
        m_spacingFactor=factor;
    else if(m_camera.zSpacing != m_placedSpacing)
    {
        const float reset=defaultZSpacing();

        m_spacingFactor=clampedFactor(m_camera.zSpacing/reset);
        if(m_spacingFactor != m_camera.zSpacing/reset)
            m_camera.zSpacing=m_spacingFactor*reset;
    }
    // MaxDistance from the front layer, so a deeper stack backs the camera
    // off past it
    m_camera.distance=std::min(std::max(m_camera.distance, MinDistance),
        MaxDistance+stackDepth(m_camera.zSpacing));
    m_placedSpacing=m_camera.zSpacing;
    m_placedHighZ=highestZ();
    apply();
    return m_spacingFactor != before;
}

float DepthView::scale() const
{
    return m_canvas?m_canvas->scale():1.0f;
}

void DepthView::apply(bool redraw)
{
    const Rect &view=m_renderer->viewRect();
    const float cx=(float)view.x+0.5f*(float)view.w;
    const float cy=(float)view.y+0.5f*(float)view.h;
    const float s=m_camera.zSpacing;
    const float k=(float)renderer::SlotRenderer::kMaxZIndex;
    const float high=highestZ();
    const float focal=focalLength();
    const float distance=focal*m_camera.distance;

    // The model: the canvas's centre on z 0 at the origin, a z index s px
    // towards the camera per label (x right, y down, z towards the camera),
    // turned (yaw, then pitch) and panned across the view.
    const float yaw=radians(m_camera.yaw);
    const float pitch=radians(m_camera.pitch);
    const float cyaw=std::cos(yaw), syaw=std::sin(yaw);
    const float cpitch=std::cos(pitch), spitch=std::sin(pitch);
    // pitch * yaw: x' = x cos(yaw) + z sin(yaw), z' = -x sin(yaw) + z cos(yaw);
    // then y'' = y cos(pitch) + z' sin(pitch), z'' = -y sin(pitch) + z' cos(pitch)
    glm::mat4 turn(1.0f);

    turn[0]=glm::vec4(cyaw, -syaw*spitch, -syaw*cpitch, 0.0f);
    turn[1]=glm::vec4(0.0f, cpitch, -spitch, 0.0f);
    turn[2]=glm::vec4(syaw, cyaw*spitch, cyaw*cpitch, 0.0f);

    glm::mat4 model(1.0f);

    model[3]=glm::vec4(-cx, -cy, 0.0f, 1.0f);
    glm::mat4 spacing(1.0f);

    spacing[2][2]=s;
    glm::mat4 pan(1.0f);

    pan[3]=glm::vec4(m_camera.panX, m_camera.panY, 0.0f, 1.0f);

    const glm::mat4 camera=pan*turn*spacing*model;

    // Everything is within the radius of the view's half diagonal and one
    // layer past its highest z from the centre (the top run's glyphs sit up to
    // a layer above it), so the depth range is fitted to that.
    const float halfW=0.5f*(float)view.w, halfH=0.5f*(float)view.h;
    const float top=(high+1.0f)*s;
    const float radius=std::sqrt(halfW*halfW+halfH*halfH+top*top)+1.0f;
    glm::mat4 projection(0.0f);

    if(m_camera.perspective)
    {
        // d = distance - z, from the camera; w = d/focal, so z 0 at distance
        // focal is drawn at its size, and a point is at c + (x, y)/w. Its z
        // index out is a + b/d, kMaxZIndex at the nearest d and 0 at the
        // farthest, and as Z = w*(a + b/d) it is linear in the point.
        const float farD=distance+radius;
        const float nearD=std::max(distance-radius, 0.01f*distance);
        const float b=k/(1.0f/nearD-1.0f/farD);
        const float a=-b/farD;

        projection[0]=glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        projection[1]=glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        projection[2]=glm::vec4(-cx/focal, -cy/focal, -a/focal, -1.0f/focal);
        projection[3]=glm::vec4(cx*distance/focal, cy*distance/focal, (a*distance+b)/focal,
            distance/focal);
    }
    else
    {
        // the same size as the perspective's at z 0, the z range fitted linearly
        const float zoom=focal/distance;

        projection[0]=glm::vec4(zoom, 0.0f, 0.0f, 0.0f);
        projection[1]=glm::vec4(0.0f, zoom, 0.0f, 0.0f);
        projection[2]=glm::vec4(0.0f, 0.0f, k/(2.0f*radius), 0.0f);
        projection[3]=glm::vec4(cx, cy, 0.5f*k, 1.0f);
    }

    const glm::mat4 transform=projection*camera;
    const float *values=glm::value_ptr(transform);

    std::copy(values, values+16, m_transform);
    m_renderer->setViewTransform(m_transform);
    if(m_canvas && redraw)
        m_canvas->redraw();
}

void DepthView::dolly(float steps)
{
    Camera camera=m_camera;

    camera.distance*=std::pow(DollyStep, steps);
    setCamera(camera);
}

void DepthView::move(float dx, float dy)
{
    // the camera moves, so the picture the other way
    Camera camera=m_camera;

    camera.panX-=dx*KeyMoveUnits*scale();
    camera.panY-=dy*KeyMoveUnits*scale();
    setCamera(camera);
}

void DepthView::captureMouseMoved(int x, int y, int deltaX, int deltaY)
{
    if(!m_orbiting && !m_panning)
        return;

    Camera camera=m_camera;

    if(m_orbiting)
    {
        camera.yaw+=(float)deltaX*OrbitDegreesPerUnit;
        camera.pitch+=(float)deltaY*OrbitDegreesPerUnit;
    }
    else
    {
        camera.panX+=(float)deltaX*scale();
        camera.panY+=(float)deltaY*scale();
    }
    if(camera != m_camera)
        setCamera(camera);
}

void DepthView::captureMouseButton(int button, bool down)
{
    if(button == 0)
        m_orbiting=down;
    else if(button == 1 || button == 2)
        m_panning=down;
}

void DepthView::captureMouseWheel(int delta, Dim dim)
{
    // a notch is 60; away from the user (positive) dollies in, or with Alt
    // spaces the layers further apart (the vertical wheel alone, so a
    // trackpad's sideways part of a swipe does not)
    if(!delta)
        return;
    if(m_alt)
    {
        if(dim == Dim::Y)
            stepSpacing((float)delta/60.0f);
    }
    else
        dolly((float)delta/60.0f);
}

void DepthView::captureKey(int key, bool down)
{
    if(key == keys::Alt)
        m_alt=down;
    if(!down)
        return;

    switch(key)
    {
    case keys::Escape:
        // deletes this view
        close(m_canvas);
        return;
    case keys::Left: move(-1.0f, 0.0f); break;
    case keys::Right: move(1.0f, 0.0f); break;
    case keys::Up: move(0.0f, -1.0f); break;
    case keys::Down: move(0.0f, 1.0f); break;
    default: break;
    }
}

void DepthView::captureCharacter(gweni::UnicodeChar chr)
{
    Camera camera=m_camera;

    switch(chr)
    {
    case 'p': case 'P':
        camera.perspective=!camera.perspective;
        setCamera(camera);
        break;
    case '+': case '=': stepSpacing(1.0f); break;
    case '-': case '_': stepSpacing(-1.0f); break;
    case 'r': case 'R':
        reset();
        break;
    case 'l': case 'L':
        setRawLabels(!m_rawLabels);
        break;
    case 'w': case 'W': dolly(1.0f); break;
    case 's': case 'S': dolly(-1.0f); break;
    case 'a': case 'A': move(-1.0f, 0.0f); break;
    case 'd': case 'D': move(1.0f, 0.0f); break;
    default: break;
    }
}

void DepthView::captureMouseLeave()
{
    m_orbiting=false;
    m_panning=false;
    m_alt=false;
}

}//namespace tools
}//namespace gweni
