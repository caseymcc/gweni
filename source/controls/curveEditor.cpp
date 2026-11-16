/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/curveEditor.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/inputEvent.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace gweni
{
namespace controls
{

namespace
{

//! The view keeps its spans inside this, so a zoom far out stays finite.
constexpr double MaxViewSpan=1e12;

const std::vector<CurveEditorEdit> NoEdits;

//! Where index \p i goes when the key at \p from moves to \p to.
size_t movedIndex(size_t i, size_t from, size_t to)
{
    if(i == from)
        return to;
    if((from < to) && (i > from) && (i <= to))
        return i-1;
    if((to < from) && (i >= to) && (i < from))
        return i+1;
    return i;
}

//! The first key of \p curve at or after \p time.
size_t firstKeyFrom(const Curve &curve, double time)
{
    const std::vector<Curve::Key> &keys=curve.keys();

    return (size_t)(std::lower_bound(keys.begin(), keys.end(), time,
        [](const Curve::Key &key, double t) { return key.time < t; })-keys.begin());
}

float distance(const PointF &a, const PointF &b)
{
    return std::hypot(a.x-b.x, a.y-b.y);
}

}//namespace

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(CurvePlot)
{
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setTabable(true);
}

CurveEditor *CurvePlot::getEditor() const
{
    return gweni_cast<CurveEditor>(getParent());
}

void CurvePlot::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return;
    if(editor->m_gesture != CurveEditor::Gesture::None)
        editor->dragTo(Point(x, y));
    else
        editor->hoverAt(Point(x, y));
}

void CurvePlot::onMouseClickLeft(int x, int y, bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return;
    if(down)
        editor->pressAt(Point(x, y));
    else
        editor->release();
}

void CurvePlot::onMouseDoubleClickLeft(int x, int y)
{
    if(CurveEditor *editor=getEditor())
        editor->doubleClickAt(Point(x, y));
}

void CurvePlot::onMouseClickRight(int x, int y, bool down)
{
    CurveEditor *editor=getEditor();

    if(editor && down)
        editor->rightPressAt(Point(x, y));
}

void CurvePlot::onMouseLeave()
{
    ParentClass::onMouseLeave();
    if(CurveEditor *editor=getEditor())
        editor->clearHover();
}

bool CurvePlot::onMouseWheeled(int delta)
{
    CurveEditor *editor=getEditor();

    if(!editor || editor->isDisabled())
        return false;
    editor->wheel(delta, false);
    return true;
}

// Shift+wheel arrives as the horizontal wheel (the canvas turns it): it zooms
// the value axis. A real horizontal wheel is taken and zooms nothing, so
// nothing above the editor scrolls sideways instead.
bool CurvePlot::onMouseWheeledH(int delta)
{
    CurveEditor *editor=getEditor();

    if(!editor || editor->isDisabled())
        return false;
    editor->wheel(delta, true);
    return true;
}

bool CurvePlot::onKeyDelete(bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return false;
    if(down)
        editor->deleteSelection();
    return true;
}

bool CurvePlot::onKeyLeft(bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return false;
    if(down)
        editor->nudgeSelection(-(input::isShiftDown()?10.0:1.0)/editor->scaleX(), 0.0);
    return true;
}

bool CurvePlot::onKeyRight(bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return false;
    if(down)
        editor->nudgeSelection((input::isShiftDown()?10.0:1.0)/editor->scaleX(), 0.0);
    return true;
}

bool CurvePlot::onKeyUp(bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return false;
    if(down)
        editor->nudgeSelection(0.0, (input::isShiftDown()?10.0:1.0)/editor->scaleY());
    return true;
}

bool CurvePlot::onKeyDown(bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return false;
    if(down)
        editor->nudgeSelection(0.0, -(input::isShiftDown()?10.0:1.0)/editor->scaleY());
    return true;
}

// Only a gesture takes it: otherwise it goes on up (a dialog's cancel).
bool CurvePlot::onKeyEscape(bool down)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return false;
    if(!down)
        return false;
    return editor->cancelGesture();
}

bool CurvePlot::onChar(gweni::UnicodeChar c)
{
    CurveEditor *editor=getEditor();

    if(!editor || editor->isDisabled())
        return false;
    if((c == 'a') || (c == 'A'))
    {
        editor->frameAll();
        return true;
    }
    if((c == 'f') || (c == 'F'))
    {
        editor->frameSelection();
        return true;
    }
    return false;
}

InputCapture *CurvePlot::pressGesture(int button)
{
    CurveEditor *editor=getEditor();

    if(!editor)
        return nullptr;
    return editor->m_pan?editor->startPan(button):nullptr;
}

}//namespace internal


// A pan from a press until its release, with the canvas's input, as a
// ZoomPanel's: under the threshold it is only held, and let go, the press and
// the release go to the canvas then (a right-click's menu); past it the view
// follows the mouse.
class CurveEditor::PanGesture: public InputCapture
{
public:

    explicit PanGesture(CurveEditor *editor): m_editor(editor) {}

    ~PanGesture() override { end(); }

    void start(Canvas *canvas, int button)
    {
        m_canvas=canvas;
        m_button=button;
        m_press=input::getMousePosition();
        m_mouse=m_press;
        m_startX0=m_editor->m_x0;
        m_startY0=m_editor->m_y0;
        m_panning=false;
    }

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }
    bool panning() const { return active() && m_panning; }
    bool replaying() const { return m_replaying; }

    void captureMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/) override
    {
        m_mouse=Point(x, y);
        if(!m_editor->visible())
        {
            end();
            return;
        }
        if(!m_panning && (std::abs(x-m_press.x) < PanThreshold) && (std::abs(y-m_press.y) < PanThreshold))
            return;

        const PointF from=m_editor->plotPoint(m_press);
        const PointF to=m_editor->plotPoint(m_mouse);
        const double spanX=m_editor->m_x1-m_editor->m_x0;
        const double spanY=m_editor->m_y1-m_editor->m_y0;
        const double x0=m_startX0-(double(to.x)-from.x)/m_editor->scaleX();
        const double y0=m_startY0+(double(to.y)-from.y)/m_editor->scaleY();

        m_panning=true;
        m_editor->setView(x0, x0+spanX, y0, y0+spanY);
    }

    void captureMouseButton(int button, bool down) override
    {
        if(!down && (button == m_button))
            release();
    }

    // A zoom mid-drag goes on from where the mouse and the view are now.
    void captureMouseWheel(int delta, Dim dim) override
    {
        if(!m_editor->visible() || m_editor->isDisabled())
            return;

        m_editor->wheel(delta, dim == Dim::X);
        m_press=m_mouse;
        m_startX0=m_editor->m_x0;
        m_startY0=m_editor->m_y0;
    }

    void captureCanvasDeleted(Canvas * /*canvas*/) override { m_canvas=nullptr; }

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

    CurveEditor *m_editor;
    Canvas *m_canvas=nullptr;
    int m_button=0;
    Point m_press;
    Point m_mouse;
    double m_startX0=0.0;
    double m_startY0=0.0;
    bool m_panning=false;
    bool m_replaying=false;
};


GWENI_CONTROL_CONSTRUCTOR(CurveEditor)
{
    setSize(480, 320);
    m_pan=std::make_unique<PanGesture>(this);

    m_plot=newChild<internal::CurvePlot>();
    m_plot->setPlacement(Placement::Owner);

    m_timeField=newChild<DragNumber>();
    m_timeField->setPlacement(Placement::Owner);
    m_timeField->setFormat("t %.3f");
    m_timeField->setWidth(FieldWidth);
    m_timeField->onValueChanged.add(this, &CurveEditor::onTimeFieldChanged);

    m_valueField=newChild<DragNumber>();
    m_valueField->setPlacement(Placement::Owner);
    m_valueField->setFormat("v %.3f");
    m_valueField->setWidth(FieldWidth);
    m_valueField->onValueChanged.add(this, &CurveEditor::onValueFieldChanged);

    syncFields();
}

CurveEditor::~CurveEditor()
{
    m_pan->end();
    if(gweni::MouseFocus == m_plot)
        gweni::MouseFocus=nullptr;

    Menu *menu=gweni_cast<Menu>(getControlById(m_menuId));

    if(!menu)
        return;

    m_menuId=0;
    m_plot->setContextMenu(nullptr);

    Canvas *canvas=menu->getCanvas();

    if(canvas && canvas->releasingChildren())
        return;

    const bool deletesItself=menu->deleteOnClose();

    menu->closeAll();
    menu->close();
    if(!deletesItself)
        menu->delayedDelete();
}

//------------------------------------------------------------------------------
// Curves
//------------------------------------------------------------------------------

size_t CurveEditor::addCurve(Curve *curve, const String &surface)
{
    CurveEntry entry;

    entry.curve=curve;
    entry.surface=surface.empty()?"CurveCurve"+std::to_string(m_curves.size()%4):surface;
    m_curves.push_back(std::move(entry));
    m_plot->addStateChange(StateChange_Background);
    return m_curves.size()-1;
}

void CurveEditor::clearCurves()
{
    endGesture();
    m_curves.clear();
    m_hovered=false;
    syncFields();
    m_plot->addStateChange(StateChange_Background);
}

Curve *CurveEditor::getCurve(size_t index) const
{
    return (index < m_curves.size())?m_curves[index].curve:nullptr;
}

const String &CurveEditor::getCurveSurface(size_t index) const
{
    static const String none;

    return (index < m_curves.size())?m_curves[index].surface:none;
}

void CurveEditor::curveChanged(size_t index)
{
    if(index >= m_curves.size())
        return;

    // a gesture's keys are not the curve's now
    endGesture();

    CurveEntry &entry=m_curves[index];
    const size_t count=entry.curve->keyCount();

    entry.selected.erase(std::lower_bound(entry.selected.begin(), entry.selected.end(), count),
        entry.selected.end());
    if(m_hovered && (m_hover.curve == index) && (m_hover.key >= count))
        m_hovered=false;
    logAll(index);
    syncFields();
}

//------------------------------------------------------------------------------
// The change log
//------------------------------------------------------------------------------

void CurveEditor::logEdit(size_t index, const CurveEditorEdit &edit)
{
    if(index >= m_curves.size())
        return;

    CurveEntry &entry=m_curves[index];

    m_plot->addStateChange(StateChange_Background);
    if(entry.all)
        return;
    if(entry.edits.size() >= MaxPendingEdits)
    {
        logAll(index);
        return;
    }
    entry.edits.push_back(edit);
}

void CurveEditor::logLook(size_t curve, size_t key)
{
    CurveEditorEdit edit;

    edit.kind=CurveEditorEdit::Kind::KeyLook;
    edit.first=key;
    logEdit(curve, edit);
}

void CurveEditor::logAll(size_t index)
{
    if(index >= m_curves.size())
        return;

    m_curves[index].all=true;
    m_curves[index].edits.clear();
    m_plot->addStateChange(StateChange_Background);
}

const std::vector<CurveEditorEdit> &CurveEditor::pendingEdits(size_t index) const
{
    return (index < m_curves.size())?m_curves[index].edits:NoEdits;
}

bool CurveEditor::pendingAll(size_t index) const
{
    return (index < m_curves.size()) && m_curves[index].all;
}

void CurveEditor::clearPendingEdits()
{
    for(CurveEntry &entry : m_curves)
    {
        entry.all=false;
        entry.edits.clear();
    }
}

//------------------------------------------------------------------------------
// The view
//------------------------------------------------------------------------------

void CurveEditor::setView(double x0, double x1, double y0, double y1)
{
    x0=Curve::clean(x0);
    x1=Curve::clean(x1);
    y0=Curve::clean(y0);
    y1=Curve::clean(y1);
    if(x1 < x0)
        std::swap(x0, x1);
    if(y1 < y0)
        std::swap(y0, y1);

    // a span kept between the least and the most, about its middle
    auto keep=[](double &lo, double &hi)
    {
        const double span=hi-lo;

        if((span >= MinViewSpan) && (span <= MaxViewSpan))
            return;

        const double middle=lo*0.5+hi*0.5;
        const double half=std::min(std::max(span, MinViewSpan), MaxViewSpan)*0.5;

        lo=middle-half;
        hi=middle+half;
    };

    keep(x0, x1);
    keep(y0, y1);
    if((x0 == m_x0) && (x1 == m_x1) && (y0 == m_y0) && (y1 == m_y1))
        return;

    m_x0=x0;
    m_x1=x1;
    m_y0=y0;
    m_y1=y1;
    viewChanged();
}

void CurveEditor::viewChanged()
{
    ++m_viewSerial;
    m_plot->addStateChange(StateChange_Background);

    // the key under a resting mouse may be another one now
    if(m_gesture == Gesture::None)
    {
        if(HoveredControl == m_plot)
            hoverAt(input::getMousePosition());
        else
            clearHover();
    }
}

double CurveEditor::scaleX() const
{
    return double(std::max(1, m_plot->getWidth()))/(m_x1-m_x0);
}

double CurveEditor::scaleY() const
{
    return double(std::max(1, m_plot->getHeight()))/(m_y1-m_y0);
}

PointF CurveEditor::curveToLocal(const CurvePoint &point) const
{
    return PointF(float((point.x-m_x0)*scaleX()), float((m_y1-point.y)*scaleY()));
}

CurvePoint CurveEditor::localToCurve(const PointF &local) const
{
    return CurvePoint{m_x0+double(local.x)/scaleX(), m_y1-double(local.y)/scaleY()};
}

void CurveEditor::zoomAt(double factorX, double factorY, const PointF &at)
{
    if(!(factorX > 0.0) || !(factorY > 0.0))
        return;

    const CurvePoint fixed=localToCurve(at);
    const double spanX=std::min(std::max((m_x1-m_x0)/factorX, MinViewSpan), MaxViewSpan);
    const double spanY=std::min(std::max((m_y1-m_y0)/factorY, MinViewSpan), MaxViewSpan);
    const double width=std::max(1, m_plot->getWidth());
    const double height=std::max(1, m_plot->getHeight());
    const double x0=fixed.x-double(at.x)*spanX/width;
    const double y1=fixed.y+double(at.y)*spanY/height;

    setView(x0, x0+spanX, y1-spanY, y1);
}

void CurveEditor::panBy(const PointF &delta)
{
    const double dx=double(delta.x)/scaleX();
    const double dy=double(delta.y)/scaleY();

    setView(m_x0-dx, m_x1-dx, m_y0+dy, m_y1+dy);
}

void CurveEditor::frameAll()
{
    frameKeys(false);
}

void CurveEditor::frameSelection()
{
    frameKeys(selectionSize() > 0);
}

void CurveEditor::frameKeys(bool selectedOnly)
{
    double minX=std::numeric_limits<double>::max();
    double maxX=-minX;
    double minY=minX;
    double maxY=-minX;
    bool any=false;

    auto take=[&](const Curve::Key &key)
    {
        minX=std::min(minX, key.time);
        maxX=std::max(maxX, key.time);
        minY=std::min(minY, key.value);
        maxY=std::max(maxY, key.value);
        any=true;
    };

    for(const CurveEntry &entry : m_curves)
    {
        if(selectedOnly)
        {
            for(size_t key : entry.selected)
                take(entry.curve->key(key));
        }
        else if(entry.curve->keyCount() > 0)
        {
            // sorted by time: the ends give the time's extent
            take(entry.curve->keys().front());
            take(entry.curve->keys().back());
            for(const Curve::Key &key : entry.curve->keys())
            {
                minY=std::min(minY, key.value);
                maxY=std::max(maxY, key.value);
            }
        }
    }
    if(!any)
        return;

    auto pad=[](double &lo, double &hi)
    {
        if(hi-lo < MinViewSpan)
        {
            lo-=0.5;
            hi+=0.5;
            return;
        }

        const double margin=(hi-lo)*FramePadding;

        lo-=margin;
        hi+=margin;
    };

    pad(minX, maxX);
    pad(minY, maxY);
    setView(minX, maxX, minY, maxY);
}

//------------------------------------------------------------------------------
// Selection and hover
//------------------------------------------------------------------------------

bool CurveEditor::isSelected(size_t curve, size_t key) const
{
    if(curve >= m_curves.size())
        return false;

    const std::vector<size_t> &selected=m_curves[curve].selected;

    return std::binary_search(selected.begin(), selected.end(), key);
}

std::vector<CurveKeyRef> CurveEditor::getSelection() const
{
    std::vector<CurveKeyRef> keys;

    for(size_t curve=0; curve < m_curves.size(); ++curve)
    {
        for(size_t key : m_curves[curve].selected)
            keys.push_back(CurveKeyRef{curve, key});
    }
    return keys;
}

size_t CurveEditor::selectionSize() const
{
    size_t count=0;

    for(const CurveEntry &entry : m_curves)
        count+=entry.selected.size();
    return count;
}

void CurveEditor::setCurveSelection(size_t curve, std::vector<size_t> selected)
{
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());

    std::vector<size_t> &old=m_curves[curve].selected;

    if(old == selected)
        return;

    // the keys in one and not the other change their look
    std::vector<size_t> changed;

    std::set_symmetric_difference(old.begin(), old.end(), selected.begin(), selected.end(),
        std::back_inserter(changed));
    old=std::move(selected);
    for(size_t key : changed)
        logLook(curve, key);
}

void CurveEditor::setSelection(const std::vector<CurveKeyRef> &keys)
{
    std::vector<std::vector<size_t>> wanted(m_curves.size());

    for(const CurveKeyRef &ref : keys)
    {
        if((ref.curve < m_curves.size()) && (ref.key < m_curves[ref.curve].curve->keyCount()))
            wanted[ref.curve].push_back(ref.key);
    }
    for(size_t curve=0; curve < m_curves.size(); ++curve)
        setCurveSelection(curve, std::move(wanted[curve]));
    syncFields();
}

void CurveEditor::select(size_t curve, size_t key, bool selected)
{
    if((curve >= m_curves.size()) || (key >= m_curves[curve].curve->keyCount()))
        return;

    std::vector<size_t> keys=m_curves[curve].selected;

    if(selected)
        keys.push_back(key);
    else
        keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
    setCurveSelection(curve, std::move(keys));
    syncFields();
}

void CurveEditor::clearSelection()
{
    for(size_t curve=0; curve < m_curves.size(); ++curve)
        setCurveSelection(curve, {});
    syncFields();
}

std::vector<std::vector<size_t>> CurveEditor::snapshotSelection() const
{
    std::vector<std::vector<size_t>> snapshot;

    for(const CurveEntry &entry : m_curves)
        snapshot.push_back(entry.selected);
    return snapshot;
}

void CurveEditor::restoreSelection(const std::vector<std::vector<size_t>> &snapshot)
{
    for(size_t curve=0; (curve < m_curves.size()) && (curve < snapshot.size()); ++curve)
        setCurveSelection(curve, snapshot[curve]);
    syncFields();
}

bool CurveEditor::getHoveredKey(CurveKeyRef &key) const
{
    if(m_hovered)
        key=m_hover;
    return m_hovered;
}

bool CurveEditor::showsHandles(size_t curve, size_t key) const
{
    return isSelected(curve, key) && (m_curves[curve].curve->key(key).mode == Curve::TangentMode::Free);
}

// Each handle HandleLength from its key along its tangent as drawn: the out
// handle to the right, the in handle to the left.
void CurveEditor::handlePositions(size_t curve, size_t key, PointF &in, PointF &out) const
{
    const Curve::Key &k=m_curves[curve].curve->key(key);
    const PointF at=curveToLocal(CurvePoint{k.time, k.value});

    auto along=[&](double slope, float side)
    {
        const double dx=scaleX();
        const double dy=-slope*scaleY();
        const double length=std::hypot(dx, dy);

        if(!(length > 0.0) || !std::isfinite(length))
            return PointF(at.x+side*HandleLength, at.y);
        return PointF(float(at.x+side*HandleLength*dx/length), float(at.y+side*HandleLength*dy/length));
    };

    in=along(k.inTangent, -1.0f);
    out=along(k.outTangent, 1.0f);
}

void CurveEditor::hoverAt(const Point &canvasPoint)
{
    CurveKeyRef key;
    const bool hovered=!isDisabled() && keyAt(plotPoint(canvasPoint), key);

    if((hovered == m_hovered) && (!hovered || (key == m_hover)))
        return;

    if(m_hovered)
        logLook(m_hover.curve, m_hover.key);
    m_hovered=hovered;
    m_hover=key;
    if(m_hovered)
        logLook(m_hover.curve, m_hover.key);
}

void CurveEditor::clearHover()
{
    if(!m_hovered || (m_gesture != Gesture::None))
        return;

    m_hovered=false;
    logLook(m_hover.curve, m_hover.key);
}

//------------------------------------------------------------------------------
// Hit tests
//------------------------------------------------------------------------------

bool CurveEditor::keyAt(const PointF &at, CurveKeyRef &key) const
{
    float best=KeyHitRadius;
    bool found=false;
    const double from=localToCurve(PointF(at.x-KeyHitRadius, 0.0f)).x;
    const double to=localToCurve(PointF(at.x+KeyHitRadius, 0.0f)).x;

    // the last curve first, so it wins a tie: it is drawn in front
    for(size_t curve=m_curves.size(); curve-- > 0;)
    {
        const Curve &c=*m_curves[curve].curve;

        // only the keys whose time is within the radius: a binary search
        for(size_t i=firstKeyFrom(c, from); (i < c.keyCount()) && (c.key(i).time <= to); ++i)
        {
            const float d=distance(curveToLocal(CurvePoint{c.key(i).time, c.key(i).value}), at);

            if(d <= best && (!found || (d < best)))
            {
                best=d;
                key=CurveKeyRef{curve, i};
                found=true;
            }
        }
    }
    return found;
}

bool CurveEditor::handleAt(const PointF &at, CurveKeyRef &key, bool &out) const
{
    float best=HandleHitRadius;
    bool found=false;

    for(size_t curve=m_curves.size(); curve-- > 0;)
    {
        for(size_t i : m_curves[curve].selected)
        {
            if(!showsHandles(curve, i))
                continue;

            PointF in, outPoint;

            handlePositions(curve, i, in, outPoint);
            for(int side=0; side < 2; ++side)
            {
                const float d=distance(side?outPoint:in, at);

                if(d <= best && (!found || (d < best)))
                {
                    best=d;
                    key=CurveKeyRef{curve, i};
                    out=(side == 1);
                    found=true;
                }
            }
        }
    }
    return found;
}

bool CurveEditor::curveAt(const PointF &at, size_t &curve) const
{
    float best=CurveHitDistance;
    bool found=false;
    const double time=localToCurve(at).x;

    for(size_t index=m_curves.size(); index-- > 0;)
    {
        const Curve &c=*m_curves[index].curve;

        if(c.keyCount() == 0)
            continue;

        const float y=curveToLocal(CurvePoint{time, c.evaluate(time)}).y;
        const float d=std::fabs(y-at.y);

        if(d <= best && (!found || (d < best)))
        {
            best=d;
            curve=index;
            found=true;
        }
    }
    return found;
}

void CurveEditor::keysIn(const PointF &min, const PointF &max, std::vector<std::vector<size_t>> &found) const
{
    const CurvePoint low=localToCurve(PointF(min.x, max.y));
    const CurvePoint high=localToCurve(PointF(max.x, min.y));

    found.assign(m_curves.size(), {});
    for(size_t curve=0; curve < m_curves.size(); ++curve)
    {
        const Curve &c=*m_curves[curve].curve;

        for(size_t i=firstKeyFrom(c, low.x); (i < c.keyCount()) && (c.key(i).time <= high.x); ++i)
        {
            if((c.key(i).value >= low.y) && (c.key(i).value <= high.y))
                found[curve].push_back(i);
        }
    }
}

//------------------------------------------------------------------------------
// Index bookkeeping
//------------------------------------------------------------------------------

void CurveEditor::keyMoved(size_t curve, size_t from, size_t to)
{
    if(from == to)
        return;

    std::vector<size_t> &selected=m_curves[curve].selected;

    for(size_t &key : selected)
        key=movedIndex(key, from, to);
    std::sort(selected.begin(), selected.end());

    auto follow=[&](CurveKeyRef &ref)
    {
        if(ref.curve == curve)
            ref.key=movedIndex(ref.key, from, to);
    };

    follow(m_hover);
    follow(m_pressedKey);
    follow(m_tangentKey);
    for(DraggedKey &dragged : m_dragged)
        follow(dragged.ref);
}

void CurveEditor::keyRemoved(size_t curve, size_t index)
{
    std::vector<size_t> &selected=m_curves[curve].selected;

    selected.erase(std::remove(selected.begin(), selected.end(), index), selected.end());
    for(size_t &key : selected)
    {
        if(key > index)
            --key;
    }
    if(m_hovered && (m_hover.curve == curve))
    {
        if(m_hover.key == index)
            m_hovered=false;
        else if(m_hover.key > index)
            --m_hover.key;
    }
    m_dragged.erase(std::remove_if(m_dragged.begin(), m_dragged.end(), [&](const DraggedKey &dragged)
        { return (dragged.ref.curve == curve) && (dragged.ref.key == index); }), m_dragged.end());
    for(DraggedKey &dragged : m_dragged)
    {
        if((dragged.ref.curve == curve) && (dragged.ref.key > index))
            --dragged.ref.key;
    }
}

void CurveEditor::keyInserted(size_t curve, size_t index)
{
    for(size_t &key : m_curves[curve].selected)
    {
        if(key >= index)
            ++key;
    }

    auto follow=[&](CurveKeyRef &ref)
    {
        if((ref.curve == curve) && (ref.key >= index))
            ++ref.key;
    };

    follow(m_hover);
    follow(m_pressedKey);
    follow(m_tangentKey);
    for(DraggedKey &dragged : m_dragged)
        follow(dragged.ref);
}

//------------------------------------------------------------------------------
// The user's edits
//------------------------------------------------------------------------------

void CurveEditor::fireChanged(size_t curve, size_t key)
{
    event::Info info(this);

    info.point=Point(int(curve), int(key));
    onChanged.call(this, info);
}

void CurveEditor::fireSelectionChanged()
{
    onSelectionChanged.call(this);
}

size_t CurveEditor::moveKey(size_t curve, size_t index, double time, double value, bool &changed)
{
    Curve *c=m_curves[curve].curve;
    const Curve::Key &key=c->key(index);

    time=Curve::clean(time);
    value=Curve::clean(value);
    changed=false;
    if((key.time == time) && (key.value == value))
        return index;

    const size_t to=c->moveKey(index, time, value);
    CurveEditorEdit spans;
    CurveEditorEdit keys;

    spans.kind=CurveEditorEdit::Kind::Spans;
    spans.first=c->lastChange().first;
    spans.removed=c->lastChange().removed;
    spans.inserted=c->lastChange().inserted;
    keys.kind=CurveEditorEdit::Kind::Keys;
    keys.first=std::min(index, to);
    keys.removed=keys.inserted=(std::max(index, to)-keys.first)+1;
    logEdit(curve, spans);
    logEdit(curve, keys);
    keyMoved(curve, index, to);
    changed=true;
    return to;
}

// Every dragged key to its press position plus (dt, dv): towards the motion's
// side first, so keys moving together pass one another in order.
static void sortForMotion(std::vector<size_t> &order, const std::vector<CurveKeyRef> &refs, bool forward)
{
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
    {
        if(refs[a].curve != refs[b].curve)
            return refs[a].curve < refs[b].curve;
        return forward?(refs[a].key > refs[b].key):(refs[a].key < refs[b].key);
    });
}

void CurveEditor::nudgeSelection(double dt, double dv)
{
    if(isDisabled() || (m_gesture != Gesture::None) || (selectionSize() == 0))
        return;

    std::vector<CurveKeyRef> refs=getSelection();
    std::vector<size_t> order(refs.size());

    for(size_t i=0; i < order.size(); ++i)
        order[i]=i;
    sortForMotion(order, refs, dt > 0.0);

    // the refs are the selection's; each move renumbers the selection, so
    // the next key is looked up by its place in the sorted selection
    for(size_t i : order)
    {
        // a move only reorders keys of its curve that sit between: the
        // selection's sorted order tells the key by its rank there
        const CurveKeyRef ref=refs[i];
        size_t rank=0;

        for(size_t j=0; j < refs.size(); ++j)
        {
            if((refs[j].curve == ref.curve) && (refs[j].key < ref.key))
                ++rank;
        }

        const size_t key=m_curves[ref.curve].selected[rank];
        const Curve::Key &k=m_curves[ref.curve].curve->key(key);
        bool changed=false;
        const size_t to=moveKey(ref.curve, key, k.time+dt, k.value+dv, changed);

        if(changed)
            fireChanged(ref.curve, to);
    }
    syncFields();
}

size_t CurveEditor::addKeyAt(size_t curve, double time)
{
    if(curve >= m_curves.size())
        return 0;

    endGesture();

    Curve *c=m_curves[curve].curve;
    const size_t index=c->addKey(time);
    CurveEditorEdit spans;
    CurveEditorEdit keys;

    spans.kind=CurveEditorEdit::Kind::Spans;
    spans.first=c->lastChange().first;
    spans.removed=c->lastChange().removed;
    spans.inserted=c->lastChange().inserted;
    keys.kind=CurveEditorEdit::Kind::Keys;
    keys.first=index;
    keys.inserted=1;
    keyInserted(curve, index);
    logEdit(curve, spans);
    logEdit(curve, keys);

    const std::vector<std::vector<size_t>> before=snapshotSelection();

    setSelection({CurveKeyRef{curve, index}});
    fireChanged(curve, index);
    if(snapshotSelection() != before)
        fireSelectionChanged();
    return index;
}

bool CurveEditor::deleteSelection()
{
    if(isDisabled() || (m_gesture != Gesture::None) || (selectionSize() == 0))
        return false;

    for(size_t curve=0; curve < m_curves.size(); ++curve)
    {
        Curve *c=m_curves[curve].curve;
        // the highest first, so the lower indices stay
        const std::vector<size_t> selected=m_curves[curve].selected;

        for(size_t i=selected.size(); i-- > 0;)
        {
            const size_t key=selected[i];

            c->removeKey(key);

            CurveEditorEdit spans;
            CurveEditorEdit keys;

            spans.kind=CurveEditorEdit::Kind::Spans;
            spans.first=c->lastChange().first;
            spans.removed=c->lastChange().removed;
            spans.inserted=c->lastChange().inserted;
            keys.kind=CurveEditorEdit::Kind::Keys;
            keys.first=key;
            keys.removed=1;
            keyRemoved(curve, key);
            logEdit(curve, spans);
            logEdit(curve, keys);
            fireChanged(curve, key);
        }
    }
    syncFields();
    fireSelectionChanged();
    return true;
}

void CurveEditor::setSelectionMode(Curve::TangentMode mode)
{
    for(const CurveKeyRef &ref : getSelection())
    {
        Curve *c=m_curves[ref.curve].curve;
        const Curve::Key before=c->key(ref.key);

        c->setMode(ref.key, mode);
        if(c->key(ref.key) == before)
            continue;

        CurveEditorEdit spans;

        spans.kind=CurveEditorEdit::Kind::Spans;
        spans.first=c->lastChange().first;
        spans.removed=c->lastChange().removed;
        spans.inserted=c->lastChange().inserted;
        logEdit(ref.curve, spans);
        // the handles come or go with Free
        logLook(ref.curve, ref.key);
        fireChanged(ref.curve, ref.key);
    }
}

void CurveEditor::setSelectionBroken(bool broken)
{
    for(const CurveKeyRef &ref : getSelection())
    {
        Curve *c=m_curves[ref.curve].curve;
        const Curve::Key before=c->key(ref.key);

        c->setBroken(ref.key, broken);
        if(c->key(ref.key) == before)
            continue;

        CurveEditorEdit spans;

        spans.kind=CurveEditorEdit::Kind::Spans;
        spans.first=c->lastChange().first;
        spans.removed=c->lastChange().removed;
        spans.inserted=c->lastChange().inserted;
        logEdit(ref.curve, spans);
        logLook(ref.curve, ref.key);
        fireChanged(ref.curve, ref.key);
    }
}

void CurveEditor::setSelectedTime(double time)
{
    if(selectionSize() != 1)
        return;

    const CurveKeyRef ref=getSelection().front();
    bool changed=false;
    const size_t to=moveKey(ref.curve, ref.key, time, m_curves[ref.curve].curve->key(ref.key).value, changed);

    if(changed)
    {
        syncFields();
        fireChanged(ref.curve, to);
    }
}

void CurveEditor::setSelectedValue(double value)
{
    if(selectionSize() != 1)
        return;

    const CurveKeyRef ref=getSelection().front();
    bool changed=false;
    const size_t to=moveKey(ref.curve, ref.key, m_curves[ref.curve].curve->key(ref.key).time, value, changed);

    if(changed)
    {
        syncFields();
        fireChanged(ref.curve, to);
    }
}

void CurveEditor::syncFields()
{
    const bool one=(selectionSize() == 1);

    m_syncing=true;
    if(one)
    {
        const CurveKeyRef ref=getSelection().front();
        const Curve::Key &key=m_curves[ref.curve].curve->key(ref.key);

        m_timeField->setNumber(key.time, false);
        m_valueField->setNumber(key.value, false);
    }
    m_timeField->setBlank(!one);
    m_valueField->setBlank(!one);
    m_timeField->setDisabled(!one || isDisabled());
    m_valueField->setDisabled(!one || isDisabled());
    m_syncing=false;
}

void CurveEditor::onTimeFieldChanged(event::Info /*info*/)
{
    if(!m_syncing)
        setSelectedTime(m_timeField->getNumber());
}

void CurveEditor::onValueFieldChanged(event::Info /*info*/)
{
    if(!m_syncing)
        setSelectedValue(m_valueField->getNumber());
}

//------------------------------------------------------------------------------
// Gestures
//------------------------------------------------------------------------------

PointF CurveEditor::plotPoint(const Point &canvasPoint) const
{
    return m_plot->canvasPosToLocalF(PointF(canvasPoint));
}

InputCapture *CurveEditor::startPan(int button)
{
    const bool pans=(button == 1) || (button == 2) || ((button == 0) && input::isKeyDown(keys::Space));
    Canvas *canvas=getCanvas();

    if(!pans || !canvas || isDisabled() || m_pan->replaying() || m_pan->active()
        || (m_gesture != Gesture::None))
        return nullptr;

    m_pan->start(canvas, button);
    return m_pan.get();
}

bool CurveEditor::panning() const
{
    return m_pan->panning();
}

void CurveEditor::pressAt(const Point &canvasPoint)
{
    if(isDisabled())
        return;
    endGesture();

    const PointF at=plotPoint(canvasPoint);
    const bool ctrl=input::isControlDown();
    const bool shift=input::isShiftDown();
    CurveKeyRef ref;
    bool out=false;

    m_press=at;
    m_mouse=at;
    m_moved=false;
    m_cancelled=false;
    m_snapshot=snapshotSelection();

    if(handleAt(at, ref, out))
    {
        const Curve::Key &key=m_curves[ref.curve].curve->key(ref.key);

        m_gesture=Gesture::Tangent;
        m_tangentKey=ref;
        m_tangentOut=out;
        m_tangentIn=key.inTangent;
        m_tangentOutStart=key.outTangent;
        gweni::MouseFocus=m_plot;
        return;
    }

    if(keyAt(at, ref))
    {
        const bool was=isSelected(ref.curve, ref.key);

        m_pressedKey=ref;
        m_pressedSelected=was && !ctrl && !shift;
        if(ctrl)
            select(ref.curve, ref.key, !was);
        else if(shift || was)
            select(ref.curve, ref.key, true);
        else
            setSelection({ref});
        if(snapshotSelection() != m_snapshot)
        {
            fireSelectionChanged();
            // the release fires again only for a change it makes itself
            m_snapshot=snapshotSelection();
        }
        // a key Ctrl takes off is not dragged
        if(!isSelected(ref.curve, ref.key))
            return;

        m_gesture=Gesture::Keys;
        m_dragged.clear();
        for(const CurveKeyRef &selected : getSelection())
        {
            const Curve::Key &key=m_curves[selected.curve].curve->key(selected.key);

            m_dragged.push_back(DraggedKey{selected, key.time, key.value});
        }
        m_dragDt=0.0;
        gweni::MouseFocus=m_plot;
        return;
    }

    m_gesture=Gesture::Marquee;
    m_additive=ctrl || shift;
    gweni::MouseFocus=m_plot;
}

void CurveEditor::dragTo(const Point &canvasPoint)
{
    // a plot the mouse was taken from drags nothing more
    if(gweni::MouseFocus != m_plot)
    {
        endGesture();
        return;
    }

    m_mouse=plotPoint(canvasPoint);

    switch(m_gesture)
    {
    case Gesture::None:
        return;
    case Gesture::Marquee:
        if(!m_moved && (std::fabs(m_mouse.x-m_press.x) < PanThreshold)
            && (std::fabs(m_mouse.y-m_press.y) < PanThreshold))
            return;
        m_moved=true;
        updateMarquee();
        return;
    case Gesture::Keys:
    {
        float dx=m_mouse.x-m_press.x;
        float dy=m_mouse.y-m_press.y;

        // Shift keeps to the axis the mouse moved most along
        if(input::isShiftDown())
        {
            if(std::fabs(dx) >= std::fabs(dy))
                dy=0.0f;
            else
                dx=0.0f;
        }
        applyDrag(double(dx)/scaleX(), -double(dy)/scaleY());
        return;
    }
    case Gesture::Tangent:
    {
        Curve *c=m_curves[m_tangentKey.curve].curve;
        const Curve::Key &key=c->key(m_tangentKey.key);
        const PointF at=curveToLocal(CurvePoint{key.time, key.value});
        // the handle stays on its side of the key: straight up is steep
        const float dx=m_tangentOut?std::max(m_mouse.x-at.x, 0.5f):std::min(m_mouse.x-at.x, -0.5f);
        const double slope=(-double(m_mouse.y-at.y)/scaleY())/(double(dx)/scaleX());

        setTangent(m_tangentKey, m_tangentOut, slope);
        return;
    }
    }
}

void CurveEditor::setTangent(const CurveKeyRef &ref, bool out, double slope)
{
    Curve *c=m_curves[ref.curve].curve;
    const Curve::Key before=c->key(ref.key);

    if(out)
        c->setOutTangent(ref.key, slope);
    else
        c->setInTangent(ref.key, slope);
    if(c->key(ref.key) == before)
        return;

    CurveEditorEdit spans;

    spans.kind=CurveEditorEdit::Kind::Spans;
    spans.first=c->lastChange().first;
    spans.removed=c->lastChange().removed;
    spans.inserted=c->lastChange().inserted;
    logEdit(ref.curve, spans);
    // its handles moved
    logLook(ref.curve, ref.key);
    m_moved=true;
    fireChanged(ref.curve, ref.key);
}

// Every dragged key to where it was at the press plus (dt, dv), the keys
// moving the way the drag's step goes first, so a key never passes one of
// its own selection.
void CurveEditor::applyDrag(double dt, double dv)
{
    std::vector<CurveKeyRef> refs;

    for(const DraggedKey &dragged : m_dragged)
        refs.push_back(dragged.ref);

    std::vector<size_t> order(refs.size());

    for(size_t i=0; i < order.size(); ++i)
        order[i]=i;
    sortForMotion(order, refs, dt > m_dragDt);
    m_dragDt=dt;

    bool any=false;

    for(size_t i : order)
    {
        // read now: a move before it may have renumbered it
        const DraggedKey dragged=m_dragged[i];
        bool changed=false;
        const size_t to=moveKey(dragged.ref.curve, dragged.ref.key, dragged.time+dt, dragged.value+dv, changed);

        if(changed)
        {
            any=true;
            fireChanged(dragged.ref.curve, to);
        }
    }
    if(any)
    {
        m_moved=true;
        syncFields();
    }
}

void CurveEditor::updateMarquee()
{
    const PointF min(std::min(m_press.x, m_mouse.x), std::min(m_press.y, m_mouse.y));
    const PointF max(std::max(m_press.x, m_mouse.x), std::max(m_press.y, m_mouse.y));
    std::vector<std::vector<size_t>> found;

    keysIn(min, max, found);
    for(size_t curve=0; curve < m_curves.size(); ++curve)
    {
        if(m_additive && (curve < m_snapshot.size()))
            found[curve].insert(found[curve].end(), m_snapshot[curve].begin(), m_snapshot[curve].end());
        setCurveSelection(curve, std::move(found[curve]));
    }
    syncFields();
    // the marquee itself is drawn again
    m_plot->addStateChange(StateChange_Background);
}

Rect CurveEditor::getMarqueeRect() const
{
    if((m_gesture != Gesture::Marquee) || !m_moved)
        return Rect();

    const int x0=int(std::floor(std::min(m_press.x, m_mouse.x)));
    const int y0=int(std::floor(std::min(m_press.y, m_mouse.y)));
    const int x1=int(std::ceil(std::max(m_press.x, m_mouse.x)));
    const int y1=int(std::ceil(std::max(m_press.y, m_mouse.y)));

    return Rect(x0, y0, x1-x0, y1-y0);
}

void CurveEditor::release()
{
    const Gesture gesture=m_gesture;
    const bool moved=m_moved;

    endGesture();

    switch(gesture)
    {
    case Gesture::Marquee:
        // a click on empty space
        if(!moved && !m_additive)
            clearSelection();
        break;
    case Gesture::Keys:
        // a plain click on a key of a selection makes it the selection
        if(!moved && m_pressedSelected)
            setSelection({m_pressedKey});
        break;
    default:
        break;
    }
    if((gesture != Gesture::None) && (snapshotSelection() != m_snapshot))
        fireSelectionChanged();
}

bool CurveEditor::cancelGesture()
{
    switch(m_gesture)
    {
    case Gesture::None:
        return false;
    case Gesture::Marquee:
        restoreSelection(m_snapshot);
        break;
    case Gesture::Keys:
        applyDrag(0.0, 0.0);
        break;
    case Gesture::Tangent:
    {
        // a joined key's tangents were one: the in tangent sets both
        Curve *c=m_curves[m_tangentKey.curve].curve;

        if(c->key(m_tangentKey.key).broken)
            setTangent(m_tangentKey, m_tangentOut, m_tangentOut?m_tangentOutStart:m_tangentIn);
        else
            setTangent(m_tangentKey, false, m_tangentIn);
        break;
    }
    }

    // the release that comes next ends nothing more
    endGesture();
    m_snapshot=snapshotSelection();
    return true;
}

void CurveEditor::endGesture()
{
    const bool marquee=(m_gesture == Gesture::Marquee) && m_moved;

    m_gesture=Gesture::None;
    m_dragged.clear();
    if(gweni::MouseFocus == m_plot)
        gweni::MouseFocus=nullptr;
    if(marquee)
        m_plot->addStateChange(StateChange_Background);
}

void CurveEditor::doubleClickAt(const Point &canvasPoint)
{
    if(isDisabled())
        return;

    const PointF at=plotPoint(canvasPoint);
    CurveKeyRef ref;
    bool out=false;
    size_t curve=0;

    // on a key or a handle it is a press; near a curve it adds a key there
    if(!handleAt(at, ref, out) && !keyAt(at, ref) && curveAt(at, curve))
    {
        endGesture();
        addKeyAt(curve, localToCurve(at).x);
        return;
    }
    pressAt(canvasPoint);
}

Menu *CurveEditor::getKeyMenu() const
{
    return gweni_cast<Menu>(getControlById(m_menuId));
}

void CurveEditor::rightPressAt(const Point &canvasPoint)
{
    CurveKeyRef ref;

    if(isDisabled() || !keyAt(plotPoint(canvasPoint), ref))
    {
        m_plot->setContextMenu(nullptr);
        return;
    }

    // a right-click on a key not selected makes it the selection (Unity)
    if(!isSelected(ref.curve, ref.key))
    {
        setSelection({ref});
        fireSelectionChanged();
    }

    Menu *menu=getKeyMenu();

    if(!menu)
    {
        Canvas *canvas=getCanvas();

        if(!canvas)
            return;

        menu=canvas->newChild<Menu>();
        menu->setHidden(true);
        m_menuId=menu->getControlId();

        const char *names[]={"Smooth", "Linear", "Constant", "Free"};
        const Curve::TangentMode modes[]={Curve::TangentMode::Smooth, Curve::TangentMode::Linear,
            Curve::TangentMode::Constant, Curve::TangentMode::Free};

        for(int i=0; i < 4; ++i)
        {
            MenuItem *item=menu->addItem(names[i]);

            item->setName(names[i]);
            item->onMenuItemSelected.add(this, &CurveEditor::onMenuMode, event::Packet(int(modes[i])));
        }
        menu->addDivider();

        MenuItem *broken=menu->addItem("Broken");

        broken->setCheckable(true);
        broken->onMenuItemSelected.add(this, &CurveEditor::onMenuBroken);
        m_brokenItemId=broken->getControlId();
        menu->addDivider();
        menu->addItem("Delete")->onMenuItemSelected.add(this, &CurveEditor::onMenuDelete);
    }

    // Broken shows the clicked key's
    if(MenuItem *broken=gweni_cast<MenuItem>(getControlById(m_brokenItemId)))
    {
        m_syncing=true;
        broken->setChecked(m_curves[ref.curve].curve->key(ref.key).broken);
        m_syncing=false;
    }
    m_plot->setContextMenu(menu);
}

void CurveEditor::onMenuMode(event::Info info)
{
    if(info.packet && info.packet->isInt())
        setSelectionMode(Curve::TangentMode(info.packet->get<int>()));
}

// The checkable item has toggled itself by now: its check is the new state.
void CurveEditor::onMenuBroken(event::Info info)
{
    MenuItem *item=gweni_cast<MenuItem>(info.controlCaller);

    if(item)
        setSelectionBroken(item->isChecked());
}

void CurveEditor::onMenuDelete(event::Info /*info*/)
{
    deleteSelection();
}

void CurveEditor::wheel(int delta, bool horizontal)
{
    if(isDisabled())
        return;

    const double factor=std::pow(WheelStep, delta/60.0);
    const PointF at=plotPoint(input::getMousePosition());

    if(horizontal)
    {
        // Shift+wheel: the value axis alone; a real horizontal wheel nothing
        if(input::isShiftDown())
            zoomAt(1.0, factor, at);
        return;
    }
    if(input::isControlDown())
        zoomAt(factor, 1.0, at);
    else
        zoomAt(factor, factor, at);
}

bool CurveEditor::draggingKeys() const
{
    return (m_gesture == Gesture::Keys) && (gweni::MouseFocus == m_plot);
}

bool CurveEditor::draggingTangent() const
{
    return (m_gesture == Gesture::Tangent) && (gweni::MouseFocus == m_plot);
}

bool CurveEditor::marqueeActive() const
{
    return (m_gesture == Gesture::Marquee) && (gweni::MouseFocus == m_plot);
}

//------------------------------------------------------------------------------
// Base
//------------------------------------------------------------------------------

void CurveEditor::setDisabled(bool active)
{
    if(active == isDisabled())
        return;

    if(active)
    {
        endGesture();
        m_pan->end();
    }
    ParentClass::setDisabled(active);
    m_plot->setKeyboardInputEnabled(!active);
    m_plot->setTabable(!active);
    if(active && m_plot->isFocussed())
        m_plot->unfocus();
    syncFields();
    for(size_t curve=0; curve < m_curves.size(); ++curve)
        logAll(curve);
}

void CurveEditor::onVisibleChanged()
{
    if(visible())
        return;

    endGesture();
    m_pan->end();
}

// The plot over the footer row, which holds the time field and the value
// field; an axis at a time, before the pass arranges the children.
void CurveEditor::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();
    const int row=std::max(m_timeField->getHeight(), m_valueField->getHeight());

    if(dim == Dim::X)
    {
        const int width=std::max(0, getWidth()-padding.left-padding.right);

        placeChild(m_plot, padding.left, m_plot->getY(), width, m_plot->getHeight());
        placeChild(m_timeField, padding.left, m_timeField->getY(), FieldWidth, m_timeField->getHeight());
        placeChild(m_valueField, padding.left+FieldWidth+Gap, m_valueField->getY(), FieldWidth,
            m_valueField->getHeight());
    }
    else
    {
        const int height=std::max(0, getHeight()-padding.top-padding.bottom-row-Gap);
        const int rowY=padding.top+height+Gap;

        placeChild(m_plot, m_plot->getX(), padding.top, m_plot->getWidth(), height);
        placeChild(m_timeField, m_timeField->getX(), rowY, m_timeField->getWidth(), m_timeField->getHeight());
        placeChild(m_valueField, m_valueField->getX(), rowY, m_valueField->getWidth(),
            m_valueField->getHeight());
    }

    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
