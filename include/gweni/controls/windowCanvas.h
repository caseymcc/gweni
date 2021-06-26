/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_windowcanvas_h_
#define _gweni_controls_windowcanvas_h_

#include <gweni/controls/base.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/windowProvider.h>
#include <gweni/controls/dragger.h>
#include <gweni/controls/label.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/controls/canvas.h>

namespace gweni
{
namespace controls
{

class GWENI_EXPORT WindowCanvas: public Canvas, public WindowProvider
{
public:

    GWENI_CLASS(WindowCanvas, controls::Canvas);

    WindowCanvas(int x, int y, int w, int h, gweni::skin::Base *renderer,
        const gweni::String &strWindowTitle="");
    ~WindowCanvas();

    void doThink() override;

    virtual bool wantsQuit()
    {
        return m_quit;
    }

    // gweni::WindowProvider
    void *getWindow() override;

    bool inputQuit() override;

    skin::Base *getSkin(void) override;

    void render(skin::Base *skin) override;

    void setPos(int x, int y) override;
    bool isOnTop() override;

    void layout(skin::Base *skin) override;

    virtual bool canMaximize()
    {
        return m_canMaximize;
    }

    virtual void setCanMaximize(bool b);
    virtual void setMaximize(bool b);

    virtual void setSizable(bool b)
    {
        m_sizer->setHidden(!b);
    }

    virtual bool getSizable()
    {
        return m_sizer->visible();
    }

protected:

    void renderCanvas() override;
    virtual void destroyWindow();

    virtual void closeButtonPressed(event::Info);
    virtual void maximizeButtonPressed(event::Info);
    virtual void minimizeButtonPressed(event::Info);

    virtual void dragger_start(event::Info);
    virtual void dragger_moved(event::Info);
    virtual void sizer_moved(event::Info);
    virtual void onTitleDoubleClicked(event::Info);

    void *m_osWindow;
    bool m_quit;

    gweni::skin::Base *m_skinChange;

    internal::Dragger *m_titleBar;
    internal::Dragger *m_sizer;
    gweni::controls::Label *m_title;


    gweni::Point m_windowPos;
    gweni::Point m_holdPos;

    bool m_canMaximize;
    bool m_isMaximized;

    gweni::controls::WindowCloseButton *m_close;
    gweni::controls::WindowMaximizeButton *m_maximize;
    gweni::controls::WindowMinimizeButton *m_minimize;

};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_windowcanvas_h_
