/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/depthView.h>

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
    if(DepthView *view=find(canvas))
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
    return camera;
}

float DepthView::defaultZSpacing() const
{
    const Rect &view=m_renderer->viewRect();
    const int high=std::max(m_canvas?m_canvas->highestZIndex():0, 1);

    return std::min(std::max(0.5f*(float)view.w/(float)high, MinZSpacing), MaxZSpacing);
}

void DepthView::setCamera(const Camera &camera)
{
    m_camera=camera;
    m_camera.pitch=std::min(std::max(m_camera.pitch, -90.0f), 90.0f);
    m_camera.distance=std::min(std::max(m_camera.distance, MinDistance), MaxDistance);
    m_camera.zSpacing=std::min(std::max(m_camera.zSpacing, MinZSpacing), MaxZSpacing);
    apply();
}

float DepthView::scale() const
{
    return m_canvas?m_canvas->scale():1.0f;
}

void DepthView::apply()
{
    const Rect &view=m_renderer->viewRect();
    const float cx=(float)view.x+0.5f*(float)view.w;
    const float cy=(float)view.y+0.5f*(float)view.h;
    const float s=m_camera.zSpacing;
    const float k=(float)renderer::SlotRenderer::kMaxZIndex;
    const float high=(float)std::max(m_canvas?m_canvas->highestZIndex():0, 1);
    const float focal=FocalViews*(float)std::max(std::max(view.w, view.h), 1);
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

    // Everything is within the radius of the view's half diagonal and its
    // highest z from the centre, so the depth range is fitted to that.
    const float halfW=0.5f*(float)view.w, halfH=0.5f*(float)view.h;
    const float radius=std::sqrt(halfW*halfW+halfH*halfH+high*s*high*s)+1.0f;
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
    if(m_canvas)
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

void DepthView::captureMouseWheel(int delta)
{
    // away from the user (positive) dollies in
    if(delta)
        dolly((float)delta/60.0f);
}

void DepthView::captureKey(int key, bool down)
{
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
    case '+': case '=':
        camera.zSpacing*=ZSpacingStep;
        setCamera(camera);
        break;
    case '-': case '_':
        camera.zSpacing/=ZSpacingStep;
        setCamera(camera);
        break;
    case 'r': case 'R':
        reset();
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
}

}//namespace tools
}//namespace gweni
