/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_windowcontrol_h_
#define _gweni_controls_windowcontrol_h_

#include <gweni/gweni.h>
#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dragger.h>
#include <gweni/controls/label.h>
#include <gweni/controls/resizableControl.h>
#include <gweni/controls/modal.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{

class CloseButton;

class GWENI_EXPORT WindowControl: public ResizableControl
{
public:

    GWENI_CONTROL(WindowControl, ResizableControl);

    virtual ~WindowControl();
    void render(skin::Base *skin) override;
    void renderUnder(skin::Base *skin) override;

    virtual void setTitle(String title);

    virtual void setClosable(bool closeable);

    void touch() override;
    bool isOnTop() override;

    void setHidden(bool hidden) override;

    void renderFocus(skin::Base *skin) override;
    void setDeleteOnClose(bool b)
    {
        m_deleteOnClose=b;
    }

    void makeModal(bool drawBackground=true);
    void destroyModal();

    event::Caller onWindowClosed;

protected:

    void closeButtonPressed(event::Info);

    internal::Dragger *m_titleBar;
    Label *m_title;
    WindowCloseButton *m_closeButton;

    bool m_deleteOnClose;

    internal::Modal *m_modal;
};


}//namespace controls
}//namespace gweni

#endif//_gweni_controls_windowcontrol_h_
