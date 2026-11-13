/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/windowControl.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/controls/label.h>
#include <gweni/controls/canvas.h>


namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(WindowControl)
{
    m_deleteOnClose=false;

    m_titleBar=newChild<internal::Dragger>();
    m_titleBar->setHeight(24);
    m_titleBar->setPadding(Padding(0, 0, 0, 0));
    m_titleBar->setMargin(Margin(0, 0, 0, 4));
    m_titleBar->setTarget(this);
    m_titleBar->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    m_titleBar->setDock(DockPosition::Top);

    m_title=m_titleBar->newChild<Label>();
    m_title->setText("Window");
    m_title->setDock(DockPosition::Center);
    m_title->setPadding(Padding(8, 0, 0, 0));
    m_title->setTextColor(getSkin()->Colors.Window.TitleInactive);

    m_closeButton=m_titleBar->newChild<WindowCloseButton>();
    m_closeButton->setText("");
    m_closeButton->setSize(29, 24);
    m_closeButton->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_closeButton->setDock(DockPosition::Right);
    m_closeButton->onPressCaller.add(this, &WindowControl::closeButtonPressed);
    m_closeButton->setTabable(false);
    m_closeButton->setName("closeButton");
    m_closeButton->setWindow(this);

    // Create a blank content control, dock it to the top - Should this be a
    // ScrollControl?
    m_innerPanel=newChild<Base>();
    m_innerPanel->setDock(DockPosition::Center);

    // hide the top resizer as we don't want it for window, we have a Dragger
    getResizer(3)->hide();  // TODO - hack

    bringToFront();
    setTabable(false);
    if(KeyboardFocus && (KeyboardFocus != this))
        m_focusBefore=KeyboardFocus->getControlId();
    focus();
    setMinimumSize(Size(100, 40));
    setClampMovement(true);
    setKeyboardInputEnabled(false);
}


// ~Base takes it off the modal stack (Canvas::modalDeleted).
WindowControl::~WindowControl()
{}

void WindowControl::makeModal()
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    // the focus it took as it was made goes back where it was on the pop,
    // also once the app has moved it on into the window (a field focused)
    Base *before=nullptr;

    for(Base *at=KeyboardFocus; at; at=at->getParent())
    {
        if(at == this)
        {
            before=Base::getControlById(m_focusBefore);
            break;
        }
    }

    canvas->pushModal(this, before);
}

void WindowControl::destroyModal()
{
    if(Canvas *canvas=getCanvas())
        canvas->popModal(this);
}

bool WindowControl::isModal()
{
    Canvas *canvas=getCanvas();
    return canvas && canvas->isModal(this);
}

bool WindowControl::onKeyEscape(bool down)
{
    if(!isModal() || m_closeButton->hidden())
        return false;

    if(down)
        closeButtonPressed(event::Info(this));
    return true;
}

bool WindowControl::isOnTop()
{
    for(Base::List::reverse_iterator iter=getParent()->getChildren().rbegin();
        iter != getParent()->getChildren().rend();
        ++iter)
    {
        WindowControl *window=gweni_cast<WindowControl>(*iter);

        if(!window)
            continue;

        if(window == this)
            return true;

        return false;
    }

    return false;
}

//void WindowControl::render(skin::Base *skin)
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

void WindowControl::renderUnder(skin::Base *skin)
{
//    ParentClass::renderUnder(skin);
////    skin->drawShadow(this, skin::Generate);
//    skin->drawControl(this);
}

void WindowControl::setTitle(gweni::String title)
{
    m_title->setText(title);
}

void WindowControl::setClosable(bool closeable)
{
    m_closeButton->setHidden(!closeable);
}

void WindowControl::setHidden(bool hidden)
{
    // a hidden modal blocks nothing
    if(hidden)
        destroyModal();
    else if(!isModal())
        bringToFront();

    ParentClass::setHidden(hidden);
}

void WindowControl::touch()
{
    ParentClass::touch();

    // a modal is above the app already (Canvas::keepOverlaysLast)
    if(!isModal())
        bringToFront();
}

void WindowControl::closeButtonPressed(event::Info)
{
    destroyModal();
    onWindowClosed.call(this);
    setHidden(true);

    if(m_deleteOnClose)
        delayedDelete();
}

void WindowControl::renderFocus(gweni::skin::Base * /*skin*/)
{}

}//namespace controls
}//namespace gweni
