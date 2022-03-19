/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/windowCanvas.h>
#include <gweni/skin.h>
#include <gweni/controls/menu.h>
#include <gweni/dragAndDrop.h>
#include <gweni/toolTip.h>

#if GWENI_ANIMATE
#include <gweni/anim.h>
#endif

namespace gweni
{
namespace controls
{

WindowCanvas::WindowCanvas(int x, int y, int w, int h, skin::Base *skin,
    const String &strWindowTitle)
    : ParentClass(nullptr)
{
//    enlargePrimitiveIds(this, m_primitiveIds, 1);

    m_quit=false;
    m_canMaximize=true;
    m_isMaximized=false;
    setPadding(Padding(1, 0, 1, 1));

    // Centering the window on the desktop
    {
        int dw, dh;
        platform::getDesktopSize(dw, dh);

        if(x < 0)
            x=(dw-w)/2;

        if(y < 0)
            y=(dh-h)/2;
    }

    m_osWindow=platform::createPlatformWindow(x, y, w, h, strWindowTitle);
    m_windowPos=Point(x, y);
    skin->getRenderer()->initializeContext(this);
    skin->getRenderer()->init();
    m_skinChange=skin;
    setSize(w, h);

    m_titleBar=newChild<internal::Dragger>();
    m_titleBar->setHeight(24);
    m_titleBar->setPadding(Padding(0, 0, 0, 0));
    m_titleBar->setMargin(Margin(0, 0, 0, 0));
    m_titleBar->setDock(DockPosition::Top);
    m_titleBar->setDoMove(false);
    m_titleBar->onDragged.add(this, &ThisClass::dragger_moved);
    m_titleBar->onDragStart.add(this, &ThisClass::dragger_start);
    m_titleBar->onDoubleClickLeft.add(this, &ThisClass::onTitleDoubleClicked);

    m_title=m_titleBar->newChild<Label>();
    m_title->setAlignment(Alignment::Left|Alignment::CenterV);
    m_title->setText(strWindowTitle);
    m_title->setDock(DockPosition::Center);
    m_title->setPadding(Padding(8, 0, 0, 0));
    m_title->setTextColor(getSkin()->Colors.Window.TitleInactive);

    // CLOSE
    {
        m_close=m_titleBar->newChild<WindowCloseButton>("Close");
        m_close->setDock(DockPosition::Right);
        m_close->setMargin(Margin(0, 0, 4, 0));
        m_close->onPressCaller.add(this, &WindowCanvas::closeButtonPressed);
        m_close->setTabable(false);
        m_close->setWindow(this);
    }
    // MAXIMIZE
    {
        m_maximize=m_titleBar->newChild<WindowMaximizeButton>("Maximize");
        m_maximize->setDock(DockPosition::Right);
        m_maximize->onPressCaller.add(this, &WindowCanvas::maximizeButtonPressed);
        m_maximize->setTabable(false);
        m_maximize->setWindow(this);
    }
    // MINIMiZE
    {
        m_minimize=m_titleBar->newChild<WindowMinimizeButton>("Minimize");
        m_minimize->setDock(DockPosition::Right);
        m_minimize->onPressCaller.add(this, &WindowCanvas::minimizeButtonPressed);
        m_minimize->setTabable(false);
        m_minimize->setWindow(this);
    }
    // Bottom Right Corner Sizer
    {
        m_sizer=newChild<internal::Dragger>();
        m_sizer->setSize(16, 16);
        m_sizer->setDoMove(false);
        m_sizer->onDragged.add(this, &WindowCanvas::sizer_moved);
        m_sizer->onDragStart.add(this, &WindowCanvas::dragger_start);
        m_sizer->setCursor(cursor::SizeNWSE);
    }
}

WindowCanvas::~WindowCanvas()
{
    destroyWindow();
}

void *WindowCanvas::getWindow()
{
    return m_osWindow;
}

void WindowCanvas::layout(skin::Base *skin)
{
    m_sizer->bringToFront();
    m_sizer->setPosition(Position::Right|Position::Bottom);
    ParentClass::layout(skin);
}

void WindowCanvas::doThink()
{
    if(platform::messagePump(m_osWindow))
    {
        redraw();
    }
    ParentClass::doThink();
    renderCanvas();
}

void WindowCanvas::renderCanvas()
{
    //
    // If there isn't anything going on we sleep the thread for a few ms
    // This gives some cpu time back to the os. If you're using a rendering
    // method that needs continual updates, just call canvas->redraw every
    // frame.
    //
    if(!needsRedraw())
    {
        platform::sleep(10);
        return;
    }

    m_needsRedraw=false;
    renderer::Base *render=m_skin->getRenderer();

    if(render->beginContext(this))
    {
        render->begin();
        recurseLayout(m_skin);
        render->setClipRegion(getRenderBounds());
        render->setRenderOffset(Point(-getX(), -getY()));
        render->setScale(scale());

        if(m_drawBackground)
        {
            render->setDrawColor(m_backgroundColor);
//            render->drawFilledRect(m_primitiveIds[0], getRenderBounds(), getZIndex());
        }

        doRender(m_skin);
        draganddrop::renderOverlay(this, m_skin);
        tooltip::renderToolTip(m_skin);
        render->end();
    }

    render->endContext(this);
    render->presentContext(this);
}

//void WindowCanvas::render(skin::Base *skin)
//{
//    bool bIsFocussed=isOnTop();
//
//    if(bIsFocussed)
//        m_title->setTextColor(getSkin()->Colors.Window.TitleActive);
//    else
//        m_title->setTextColor(getSkin()->Colors.Window.TitleInactive);
//
////    skin->drawWindow(this, skin::Generate, m_titleBar->bottom(), bIsFocussed);
//    skin->drawControl(this);
//}

void WindowCanvas::destroyWindow()
{
    if(m_osWindow)
    {
        getSkin()->getRenderer()->shutdownContext(this);
        platform::destroyPlatformWindow(m_osWindow);
        m_osWindow=nullptr;
    }
}

bool WindowCanvas::inputQuit()
{
    m_quit=true;
    return true;
}

skin::Base *WindowCanvas::getSkin(void)
{
    if(m_skinChange)
    {
        setSkin(m_skinChange);
        m_skinChange=nullptr;
    }

    return ParentClass::getSkin();
}

void WindowCanvas::dragger_start(event::Info)
{
    platform::getCursorPos(m_holdPos);
    m_holdPos.x-=m_windowPos.x;
    m_holdPos.y-=m_windowPos.y;
}

void WindowCanvas::dragger_moved(event::Info)
{
    Point p;
    platform::getCursorPos(p);

    // Dragged out of maximized
    if(m_isMaximized)
    {
        float fOldWidth=getWidth();
        setMaximize(false);
        // Change the hold pos to be the same distance across the titlebar of
        // the resized window
        m_holdPos.x=static_cast<float>(m_holdPos.x)  *getWidth() / fOldWidth;
        m_holdPos.y=10;
    }

    setPos(p.x-m_holdPos.x, p.y-m_holdPos.y);
}

void WindowCanvas::setPos(int x, int y)
{
    int w, h;
    platform::getDesktopSize(w, h);
    y=clamp(y, 0, h);
    m_windowPos.x=x;
    m_windowPos.y=y;
    platform::setBoundsPlatformWindow(m_osWindow, x, y, getWidth(), getHeight());
}

void WindowCanvas::closeButtonPressed(event::Info)
{
    inputQuit();
}

bool WindowCanvas::isOnTop()
{
    return platform::isFocussedPlatformWindow(m_osWindow);
}

void WindowCanvas::sizer_moved(event::Info)
{
    Point p;
    platform::getCursorPos(p);
    int w=(p.x)-m_windowPos.x;
    int h=(p.y)-m_windowPos.y;
    w=clamp(w, 100, 9999);
    h=clamp(h, 100, 9999);
    platform::setBoundsPlatformWindow(m_osWindow, m_windowPos.x, m_windowPos.y, w, h);
    getSkin()->getRenderer()->resizedContext(this, w, h);
    this->setSize(w, h);
    ParentClass::doThink();
    renderCanvas();
}

void WindowCanvas::onTitleDoubleClicked(event::Info)
{
    if(!canMaximize())
        return;

    setMaximize(!m_isMaximized);
}

void WindowCanvas::setMaximize(bool b)
{
    m_isMaximized=b;
    m_maximize->setMaximized(m_isMaximized);
    Point size, pos;
    platform::setWindowMaximized(m_osWindow, m_isMaximized, pos, size);
    setSize(size.x, size.y);
    m_windowPos=pos;
    getSkin()->getRenderer()->resizedContext(this, size.x, size.y);
    ParentClass::doThink();
    renderCanvas();
}

void WindowCanvas::maximizeButtonPressed(event::Info)
{
    if(!canMaximize())
        return;

    setMaximize(!m_isMaximized);
}

void WindowCanvas::minimizeButtonPressed(event::Info)
{
    platform::setWindowMinimized(m_osWindow, true);
}

void WindowCanvas::setCanMaximize(bool b)
{
    if(m_canMaximize == b)
        return;

    m_canMaximize=b;
    m_maximize->setDisabled(!b);
}

}//namespace controls
}//namespace gweni
