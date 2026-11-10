/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_tools_depthView_h_
#define _gweni_tools_depthView_h_

#include <gweni/controls/canvas.h>
#include <gweni/events.h>
#include <gweni/export.h>

namespace gweni
{
namespace renderer
{
class Base;
class SlotRenderer;
}//namespace renderer

namespace tools
{

//! A debug view of a canvas's UI in 3D, its z indices as depth
//! (docs/architecture.md, "The depth view"): the canvas drawn through a camera
//! that orbits, pans and dollies round it, in perspective or orthographic, so
//! the stack of z layers the depth test draws in is seen from the side.
//!
//! It is not a control: it changes nothing in the tree. It hands its camera to
//! the renderer as a view transform (SlotRenderer::setViewTransform), which the
//! backend multiplies into its projection, so opening, closing and every camera
//! move enter no control, write no slot and upload nothing. It takes the
//! canvas's input while open (Canvas::setInputCapture), so the UI gets none and
//! keeps its hover and keyboard focus, but goes on running: its animations,
//! a caret blink, a tooltip's delay.
//!
//! The camera, in the view's device px: left-drag orbits round the canvas's
//! centre on its back plane (z 0), right- or middle-drag pans, the wheel and
//! W/S dolly in and out, the arrows and A/D move the camera, P switches
//! perspective and orthographic, + and - space the z labels further apart and
//! closer, R resets and Escape closes it. At reset the camera looks straight at
//! the canvas from a distance at which its back plane is drawn exactly as in
//! the front view, in either projection; the layers in front of it come towards
//! the camera, each z label zSpacing device px from the next, by default
//! enough to put the canvas's highest z half the view's width in front.
//!
//! The library binds no key: an app opens and closes it (open(), close(),
//! toggle()), hears of it through onOpenChangedCaller, and calls viewChanged()
//! when the renderer's view rect changes (a resize), since the camera is built
//! from it. One renderer draws one canvas's depth view at a time. It needs a
//! SlotRenderer (OpenGLCore, renderer::Vulkan); on another renderer open()
//! prints one line and opens nothing.
class GWENI_EXPORT DepthView: public controls::InputCapture
{
public:
    //! Where the camera is
    struct Camera
    {
        bool perspective=true;
        //! Degrees: yaw turns the canvas about its vertical axis (positive
        //! turns its front to the right), pitch about its horizontal one
        //! (positive turns it down), in that order
        float yaw=0.0f;
        float pitch=0.0f;
        //! From the canvas's back plane, in focal lengths (FocalViews times the
        //! view's longer side): 1 draws that plane at its size, 2 at half of
        //! it (a dolly; in the orthographic projection only that scale)
        float distance=1.0f;
        //! The picture moved across the view, device px
        float panX=0.0f;
        float panY=0.0f;
        //! Device px between two z labels
        float zSpacing=1.0f;

        bool operator==(const Camera &other) const;
        bool operator!=(const Camera &other) const { return !(*this == other); }
    };

    //! Opens the canvas's depth view on \p renderer (null: the canvas skin's),
    //! at reset, and returns it; the one open already if there is one. Null on
    //! a renderer that is no SlotRenderer, or one another canvas's view is on.
    static DepthView *open(controls::Canvas *canvas, renderer::Base *renderer=nullptr);
    //! Closes and deletes the canvas's view, if it has one: the renderer draws
    //! the front view again and the canvas gets its input again.
    static void close(controls::Canvas *canvas);
    //! Opens the view, or closes it if it is open. Returns whether it is open.
    static bool toggle(controls::Canvas *canvas, renderer::Base *renderer=nullptr);
    //! The canvas's view, or null
    static DepthView *find(controls::Canvas *canvas);

    //! Called after a canvas's view opens (open() making one) or closes
    //! (close(), or Escape in the view), when find() already gives the new
    //! answer; not for a view whose canvas is deleted under it. The caller and
    //! the info's control are the canvas, its integer 1 for an open, 0 for a
    //! close.
    static event::Caller onOpenChangedCaller;

    //! The renderer's view rect changed: builds the camera's transform from it
    //! again. Nothing when the canvas has no view.
    static void viewChanged(controls::Canvas *canvas);

    controls::Canvas *canvas() const { return m_canvas; }
    renderer::SlotRenderer *renderer() const { return m_renderer; }

    const Camera &camera() const { return m_camera; }
    //! Puts the camera there (zSpacing clamped to [MinZSpacing, MaxZSpacing],
    //! distance to [MinDistance, MaxDistance], pitch to +-90) and redraws
    void setCamera(const Camera &camera);
    //! The camera at reset: in front of the canvas, perspective, at distance 1
    //! and defaultZSpacing()
    Camera resetCamera() const;
    void reset() { setCamera(resetCamera()); }
    //! Device px a z label that puts the canvas's highest z index
    //! (Canvas::highestZIndex()) half the view's width in front of z 0
    float defaultZSpacing() const;
    //! The transform last handed to the renderer, column-major
    const float *transform() const { return m_transform; }

    //! The focal length, in the view's longer side: how far the camera is
    //! from the back plane at reset, so the perspective's strength
    static constexpr float FocalViews=2.0f;
    //! What the mouse and the keys do (see the class comment)
    static constexpr float OrbitDegreesPerUnit=0.4f;
    static constexpr float KeyMoveUnits=24.0f;
    static constexpr float DollyStep=0.85f;
    static constexpr float ZSpacingStep=1.25f;
    static constexpr float MinZSpacing=0.01f;
    static constexpr float MaxZSpacing=64.0f;
    static constexpr float MinDistance=0.05f;
    static constexpr float MaxDistance=20.0f;

    void captureMouseMoved(int x, int y, int deltaX, int deltaY) override;
    void captureMouseButton(int button, bool down) override;
    void captureMouseWheel(int delta) override;
    void captureKey(int key, bool down) override;
    void captureCharacter(gweni::UnicodeChar chr) override;
    void captureMouseLeave() override;
    void captureCanvasDeleted(controls::Canvas *canvas) override;

private:
    DepthView(controls::Canvas *canvas, renderer::SlotRenderer *renderer);
    ~DepthView() override;

    DepthView(const DepthView &)=delete;
    DepthView &operator=(const DepthView &)=delete;

    //! Builds the transform from the camera and the view rect, hands it to the
    //! renderer and asks the canvas for a redraw
    void apply();
    //! The canvas scale: device px per logical unit, the mouse's
    float scale() const;
    void dolly(float steps);
    void move(float dx, float dy);

    controls::Canvas *m_canvas;
    renderer::SlotRenderer *m_renderer;
    Camera m_camera;
    float m_transform[16];
    bool m_orbiting=false;
    bool m_panning=false;
};

}//namespace tools
}//namespace gweni

#endif//_gweni_tools_depthView_h_
