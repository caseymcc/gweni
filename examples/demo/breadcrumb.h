/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_breadcrumb_example_h_
#define _gweni_breadcrumb_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Breadcrumb;
class Button;
class HorizontalSlider;
class Label;
}

//! Breadcrumbs: a deep path (Home › Projects › gweni › source › controls ›
//! base.cpp) in a container a slider resizes, so its middle collapses into
//! the ellipsis; a "/"-separated path; one whose clicks go back to the
//! clicked item (popTo) with a button pushing a new folder; and a label
//! echoing the clicks.
class BreadcrumbDemo: public controls::Base
{
public:
    GWENI_CONTROL(BreadcrumbDemo, controls::Base);

public:
    //! The widest the deep path's container gets, the slider's maximum.
    static constexpr int MaxPathWidth=500;
    //! Its width at the start: narrow enough that the middle is collapsed.
    static constexpr int StartPathWidth=250;

    //! The controls a test drives.
    controls::Breadcrumb *deepPath() const { return m_deep; }
    controls::Breadcrumb *slashPath() const { return m_slash; }
    controls::Breadcrumb *navigationPath() const { return m_navigation; }
    controls::Button *pushButton() const { return m_push; }
    controls::Label *clickLabel() const { return m_clicked; }
    //! The deep path's container: its width is the path's.
    controls::Base *pathBox() const { return m_box; }

    //! The deep path's container \p width units wide, as the slider sets it.
    void setPathWidth(int width);
    //! Whether a click sets the label's text (on by default): a test weighing
    //! a path's own cost turns it off, since the label's new text is laid out.
    void setLabelUpdates(bool updates);

private:
    void onClicked(event::Info info);
    void onNavigate(event::Info info);
    void onPush(event::Info info);
    void onWidth(event::Info info);

    controls::Breadcrumb *m_deep=nullptr;
    controls::Breadcrumb *m_slash=nullptr;
    controls::Breadcrumb *m_navigation=nullptr;
    controls::Base *m_box=nullptr;
    controls::HorizontalSlider *m_width=nullptr;
    controls::Button *m_push=nullptr;
    controls::Label *m_clicked=nullptr;
    int m_pushed=0;
    bool m_labelUpdates=true;
};

}//namespace gweni

#endif//_gweni_breadcrumb_example_h_
