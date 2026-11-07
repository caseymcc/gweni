/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_crosssplitter_h_
#define _gweni_controls_crosssplitter_h_

#include <gweni/gweni.h>
#include <gweni/controls/base.h>
#include <gweni/controls/splitterBar.h>

namespace gweni
{
namespace controls
{

class GWENI_EXPORT CrossSplitter: public controls::Base
{
public:

    GWENI_CONTROL(CrossSplitter, controls::Base);

public:
    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;
    //! layout() places the bars and the panels; the generic dock and size
    //! rules would resize the panels it sized.
    void arrangeHorizontal(skin::Base * /*skin*/) override {}
    void arrangeVertical(skin::Base * /*skin*/) override {}

    virtual float calculateValueVertical();
    virtual float calculateValueHorizontal();
    virtual void  calculateValueCenter();
    virtual void  updateHSplitter();
    virtual void  updateVSplitter();
    virtual void  updateCSplitter();

    virtual void            setPanel(int i, controls::Base *panel);
    virtual controls::Base *getPanel(int i);

    virtual bool isZoomed()
    {
        return m_zoomedSection != -1;
    }

    virtual void zoom(int section);
    virtual void unZoom();
    virtual void zoomChanged();
    virtual void centerPanels()
    {
        m_horizontal=0.5f;
        m_vertical=0.5f;
        invalidate();
    }

    virtual void setSplitterSize(int size)
    {
        m_barSize=size;
        invalidate();
    }

    //! Where the horizontal-position bar sits, as a fraction of the width it
    //! can move in (0 left, 1 right).
    float getHorizontalFraction() const { return m_horizontal; }
    //! Where the vertical-position bar sits, as a fraction of the height it
    //! can move in (0 top, 1 bottom).
    float getVerticalFraction() const { return m_vertical; }

protected:

    virtual void  onVerticalMoved(event::Info);
    virtual void  onHorizontalMoved(event::Info);
    virtual void  onCenterMoved(event::Info);

    //! The bars' positions from the fractions: round((W-b)*h) and
    //! round((H-b)*v), 0 when the splitter is no bigger than a bar.
    int barX() const;
    int barY() const;
    void setBarsHidden(bool hidden);

private:

    SplitterBar *m_verticalSplitter;
    SplitterBar *m_horizontalSplitter;
    SplitterBar *m_centerSplitter;

    controls::Base *m_sections[4];

    float m_horizontal;
    float m_vertical;
    int m_barSize;

    int m_zoomedSection;

    gweni::event::Caller onZoomed;
    gweni::event::Caller onUnZoomed;
    gweni::event::Caller onZoomChange;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_crosssplitter_h_
