/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dockedtabcontrol_h_
#define _gweni_controls_dockedtabcontrol_h_

#include <gweni/controls/base.h>
#include <gweni/controls/tabControl.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT DockedTabControl: public TabControl
{
public:

    GWENI_CONTROL(DockedTabControl, TabControl);

public:
    void setShowTitleBar(bool bShow)
    {
        m_titleBar->setHidden(!bShow);
    }

    void layout(skin::Base *skin) override;
    void updateTitleBar();

    virtual void dragAndDrop_startDragging(gweni::draganddrop::Package *package,
        int x, int y) override;
    void dragAndDrop_endDragging(bool bSuccess, int x, int y) override;

    void moveTabsTo(DockedTabControl *target);

private:

    TabTitleBar *m_titleBar;
    Base *m_windowControl;

};


}
}
#endif//_gweni_controls_dockedtabcontrol_h_
