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
//! move enter no control, write no slot and upload nothing but the layer
//! table, as it opens. It takes the
//! canvas's input while open (Canvas::setInputCapture), so the UI gets none and
//! keeps its hover and keyboard focus, but goes on running: its animations,
//! a caret blink, a tooltip's delay.
//!
//! The camera, in the view's device px: left-drag orbits round the canvas's
//! centre on its back plane (z 0), right- or middle-drag pans, the wheel and
//! W/S dolly in and out, the arrows and A/D move the camera, P switches
//! perspective and orthographic, + and - space the layers further apart and
//! closer, L switches between shown layers and raw z labels, R resets and
//! Escape closes it. The layers come towards the camera from the back plane,
//! each shown layer zSpacing device px from the next, by default enough to put
//! the highest one half the view's width in front. A shown layer is a z label
//! some shown slot uses: open, the view holds the renderer's layer table and
//! remap on (SlotRenderer::setLayerTable, setLayerRemap), so a hidden page or
//! a gap between labels takes no depth; with raw labels every label is
//! zSpacing apart, as the canvas numbers them. At reset the camera looks
//! straight at the canvas from a distance at which that highest layer is
//! drawn exactly as in the front view and every layer behind it smaller about
//! the view's centre, so the whole canvas is on the view; the orthographic
//! projection draws every layer at the perspective's back-plane size, so P at
//! reset does not move the back plane. A camera at reset is fitted again in a
//! flush whose shown layers changed (the table's listener, before the draws),
//! so a view opened before the canvas's first render fits what it draws.
//!
//! The library binds no key: an app opens and closes it (open(), close(),
//! toggle()), hears of it through onOpenChangedCaller, and calls viewChanged()
//! when the renderer's view rect changes (a resize), since the camera is built
//! from it. One renderer draws one canvas's depth view at a time. It needs a
//! SlotRenderer (OpenGLCore, renderer::Vulkan); on another renderer open()
//! prints one line and opens nothing. It holds that renderer by pointer and
//! gives it the front view back as it closes, so close the view, or delete its
//! canvas (which closes it), before deleting the renderer.
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
        //! view's longer side): 1 would draw that plane at its size, 2 at half
        //! of it (a dolly; in the orthographic projection only that scale).
        //! resetCamera() sets it a little over 1.
        float distance=1.0f;
        //! The picture moved across the view, device px
        float panX=0.0f;
        float panY=0.0f;
        //! Device px between two shown layers (with rawLabels(), two z labels)
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
    //! The camera at reset: in front of the canvas, perspective, with
    //! defaultZSpacing() and at distance 1 + zSpacing*highest z/focal length,
    //! so the highest z is drawn at its front-view size and every lower one
    //! smaller (z 0 at 1/distance of it, in either projection)
    Camera resetCamera() const;
    //! The camera at resetCamera(), which follows the shown layers as they
    //! change until the next setCamera() (a key, a drag, the wheel)
    void reset();
    //! Device px a layer that puts the highest one, highestZ(), half the view's
    //! width in front of z 0
    float defaultZSpacing() const;
    //! What the camera spaces: the highest shown layer (the renderer's layer
    //! table, SlotRenderer::shownLayerCount() less 1), or with rawLabels()
    //! the canvas's highest z index; at least 1. apply(), resetCamera() and
    //! defaultZSpacing() all fit to it.
    float highestZ() const;
    //! Raw z labels (L): the renderer draws Vertex::z as it is (no remap),
    //! every label zSpacing apart, hidden pages and the gaps between labels
    //! included, rather than each shown layer zSpacing apart. Switching
    //! converts zSpacing so the stack keeps its depth and its front layer
    //! stays where it is.
    void setRawLabels(bool raw);
    bool rawLabels() const { return m_rawLabels; }
    //! The transform last handed to the renderer, column-major
    const float *transform() const { return m_transform; }

    //! The focal length, in the view's longer side: how far the camera is
    //! from the plane it draws at its size, so the perspective's strength
    static constexpr float FocalViews=4.0f;
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
    void captureMouseWheel(int delta, Dim dim=Dim::Y) override;
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
    //! renderer and asks the canvas for a redraw (not from inside a flush)
    void apply(bool redraw=true);
    //! The renderer's listener (SlotRenderer::setLayerTableListener): the
    //! shown layers changed in a flush, before its draws
    void layersChanged();
    //! The canvas scale: device px per logical unit, the mouse's
    float scale() const;
    //! FocalViews times the view's longer side (at least 1), device px
    float focalLength() const;
    void dolly(float steps);
    void move(float dx, float dy);

    controls::Canvas *m_canvas;
    renderer::SlotRenderer *m_renderer;
    Camera m_camera;
    float m_transform[16];
    bool m_orbiting=false;
    bool m_panning=false;
    bool m_rawLabels=false;
    //! the camera is resetCamera()'s, fitted again as the shown layers change
    bool m_atReset=true;
};

}//namespace tools
}//namespace gweni

#endif//_gweni_tools_depthView_h_
