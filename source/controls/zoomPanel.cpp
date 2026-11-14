/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/zoomPanel.h>
#include <gweni/controls/canvas.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gweni
{
namespace controls
{

// A pan from a press until its release, with the canvas's input. Under the
// threshold it is only held: let go, the press and the release are given to
// the canvas then, where the mouse still is for the canvas (the capture kept
// input::'s position at the press), and a move to where the mouse went
// after. Past it the content follows the mouse, by the motion in the panel's
// own units, and the release ends it with a move to where the mouse is.
class ZoomPanel::PanGesture: public InputCapture
{
public:

    explicit PanGesture(ZoomPanel *panel): m_panel(panel) {}

    ~PanGesture() override { end(); }

    void start(Canvas *canvas, int button)
    {
        m_canvas=canvas;
        m_button=button;
        m_press=input::getMousePosition();
        m_mouse=m_press;
        m_startPan=m_panel->pan();
        m_panning=false;
    }

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }
    bool panning() const { return active() && m_panning; }
    //! giving a held press to the canvas: the panel takes no press then
    bool replaying() const { return m_replaying; }

    void captureMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/) override
    {
        m_mouse=Point(x, y);
        if(!m_panel->visible())
        {
            end();
            return;
        }

        if(!m_panning && (std::abs(x-m_press.x) < PanThreshold) && (std::abs(y-m_press.y) < PanThreshold))
            return;

        // the motion in the panel's units, which a zoom it is in scales
        const PointF from=m_panel->canvasPosToLocalF(PointF(m_press));
        const PointF to=m_panel->canvasPosToLocalF(PointF(m_mouse));

        m_panning=true;
        m_panel->setPan(PointF(m_startPan.x+(to.x-from.x), m_startPan.y+(to.y-from.y)));
    }

    void captureMouseButton(int button, bool down) override
    {
        if(!down && (button == m_button))
            release();
    }

    // A zoom mid-drag moves the pan about the cursor, so the drag goes on
    // from there: from where the mouse and the pan are now.
    void captureMouseWheel(int delta, Dim dim) override
    {
        // the horizontal wheel zooms nothing, mid-pan as outside one
        if((dim == Dim::X) || !m_panel->visible() || m_panel->isDisabled())
            return;

        m_panel->wheelAt(delta, m_mouse);
        m_press=m_mouse;
        m_startPan=m_panel->pan();
    }

    void captureCanvasDeleted(Canvas * /*canvas*/) override { m_canvas=nullptr; }

    //! Gives the canvas its input back, with nothing replayed.
    void end()
    {
        if(active())
            m_canvas->setInputCapture(nullptr);
        m_canvas=nullptr;
        m_panning=false;
    }

private:

    void release()
    {
        Canvas *canvas=m_canvas;
        const bool panned=m_panning;
        const int button=m_button;
        const Point mouse=m_mouse;

        end();
        if(!canvas)
            return;

        if(!panned)
        {
            m_replaying=true;
            canvas->inputMouseButton(button, true);
            canvas->inputMouseButton(button, false);
            m_replaying=false;
        }

        const Point at=input::getMousePosition();

        if((at.x != mouse.x) || (at.y != mouse.y))
            canvas->inputMouseMoved(mouse.x, mouse.y, mouse.x-at.x, mouse.y-at.y);
    }

    ZoomPanel *m_panel;
    Canvas *m_canvas=nullptr;
    int m_button=0;
    Point m_press;
    Point m_mouse;
    PointF m_startPan;
    bool m_panning=false;
    bool m_replaying=false;
};

GWENI_CONTROL_CONSTRUCTOR(ZoomPanel)
{
    m_transformsChildren=true;
    // it takes the wheel and the presses over it that its content leaves
    setMouseInputEnabled(true);
    m_gesture=std::make_unique<PanGesture>(this);

    m_innerPanel=newChild<Base>();
    m_innerPanel->setPlacement(Placement::Owner);
    m_innerPanel->setMouseInputEnabled(true);
}

void ZoomPanel::setZoom(float zoom)
{
    zoom=std::min(std::max(zoom, m_minZoom), m_maxZoom);
    if(zoom == m_zoom)
        return;

    m_zoom=zoom;
    spaceChanged(true);
}

void ZoomPanel::setPan(const PointF &pan)
{
    if((pan.x == m_pan.x) && (pan.y == m_pan.y))
        return;

    m_pan=pan;
    spaceChanged(false);
}

ZoomPanel::~ZoomPanel()
{
}

void ZoomPanel::zoomAt(float zoom, const PointF &at)
{
    zoom=std::min(std::max(zoom, m_minZoom), m_maxZoom);
    if(zoom == m_zoom)
        return;

    // the content point under at, (at-pan)/zoom, is at at again
    const double contentX=(double(at.x)-m_pan.x)/m_zoom;
    const double contentY=(double(at.y)-m_pan.y)/m_zoom;

    m_zoom=zoom;
    m_pan=PointF(static_cast<float>(at.x-contentX*zoom), static_cast<float>(at.y-contentY*zoom));
    spaceChanged(true);
}

bool ZoomPanel::panning() const
{
    return m_gesture->panning();
}

void ZoomPanel::wheelAt(int delta, const Point &mouse)
{
    zoomAt(m_zoom*std::pow(WheelStep, delta/60.0f), canvasPosToLocalF(PointF(mouse)));
}

// Whatever under the cursor did not take the wheel, the panel does: at a limit
// of the range too, so nothing above it scrolls instead.
bool ZoomPanel::onMouseWheeled(int delta)
{
    if(isDisabled())
        return false;

    wheelAt(delta, input::getMousePosition());
    return true;
}

// The horizontal wheel (and Shift+wheel) zooms nothing, but the panel takes it
// as it takes the vertical one, so nothing above it scrolls sideways instead.
bool ZoomPanel::onMouseWheeledH(int /*delta*/)
{
    return !isDisabled();
}

InputCapture *ZoomPanel::pressGesture(int button)
{
    const bool pans=(button == 1) || (button == 2) || ((button == 0) && input::isKeyDown(keys::Space));
    Canvas *canvas=getCanvas();

    if(!pans || !canvas || isDisabled() || m_gesture->replaying() || m_gesture->active())
        return nullptr;

    m_gesture->start(canvas, button);
    return m_gesture.get();
}

void ZoomPanel::setZoomRange(float lowest, float highest)
{
    // a content point is (p-pan)/zoom: no zoom of 0
    lowest=std::max(lowest, MinZoomLimit);
    m_minZoom=lowest;
    m_maxZoom=std::max(lowest, highest);
    setZoom(m_zoom);
}

void ZoomPanel::setContentSize(const Point &size)
{
    if((size.x == m_contentSize.x) && (size.y == m_contentSize.y))
        return;

    m_contentSize=size;
    invalidate();
}

Point ZoomPanel::contentSize() const
{
    return Point((m_contentSize.x < 0)?getWidth():m_contentSize.x,
        (m_contentSize.y < 0)?getHeight():m_contentSize.y);
}

void ZoomPanel::spaceChanged(bool zoomed)
{
    // every rect moves, and on a zoom every text run is drawn at a new size
    if(m_innerPanel)
        m_innerPanel->addStateChangeRecursive(StateChange_Geometry|(zoomed?StateChange_Text:0));

    // what is under a resting mouse may be another control now
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
}

// The panel's origin on the whole device px translate() puts it at, then the
// pan, which may leave a fraction: the content's origin.
void ZoomPanel::childRenderTransform(const renderer::RenderTransform &outer, const Point &at,
    renderer::RenderTransform &inner) const
{
    const double x=outer.deviceX(float(at.x))+double(m_pan.x)*outer.scale;
    const double y=outer.deviceY(float(at.y))+double(m_pan.y)*outer.scale;
    const double wholeX=std::floor(x);
    const double wholeY=std::floor(y);

    inner.originX=static_cast<int>(wholeX);
    inner.originY=static_cast<int>(wholeY);
    inner.fractionX=static_cast<float>(x-wholeX);
    inner.fractionY=static_cast<float>(y-wholeY);
    inner.scale=outer.scale*m_zoom;
}

PointF ZoomPanel::toChildSpace(const PointF &local) const
{
    return PointF(static_cast<float>((double(local.x)-m_pan.x)/m_zoom),
        static_cast<float>((double(local.y)-m_pan.y)/m_zoom));
}

PointF ZoomPanel::fromChildSpace(const PointF &child) const
{
    return PointF(static_cast<float>(m_pan.x+double(child.x)*m_zoom),
        static_cast<float>(m_pan.y+double(child.y)*m_zoom));
}

// The inner panel is placed at the origin at contentSize(), an axis at a time
// before the pass arranges it along that axis.
void ZoomPanel::arrange(skin::Base *skin, Dim dim)
{
    if(m_innerPanel)
    {
        const Point size=contentSize();

        if(dim == Dim::X)
            placeChild(m_innerPanel, 0, 0, size.x, m_innerPanel->getHeight());
        else
            placeChild(m_innerPanel, 0, 0, m_innerPanel->getWidth(), size.y);
    }

    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
