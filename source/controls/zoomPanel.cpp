/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/zoomPanel.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ZoomPanel)
{
    m_transformsChildren=true;

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
    spaceChanged();
}

void ZoomPanel::setPan(const PointF &pan)
{
    if((pan.x == m_pan.x) && (pan.y == m_pan.y))
        return;

    m_pan=pan;
    spaceChanged();
}

void ZoomPanel::setZoomRange(float lowest, float highest)
{
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

void ZoomPanel::spaceChanged()
{
    if(m_innerPanel)
        m_innerPanel->addStateChangeRecursive(StateChange_Geometry);
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
