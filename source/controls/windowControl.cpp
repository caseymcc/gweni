/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/windowControl.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/controls/label.h>
#include <gweni/controls/modal.h>


namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(WindowControl)
{
    m_modal=nullptr;
    m_deleteOnClose=false;

    m_titleBar=new internal::Dragger(this);
    m_titleBar->setHeight(24);
    m_titleBar->setPadding(Padding(0, 0, 0, 0));
    m_titleBar->setMargin(Margin(0, 0, 0, 4));
    m_titleBar->setTarget(this);
    m_titleBar->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    m_titleBar->dock(Position::Top);

    m_title=new Label(m_titleBar);
    m_title->setAlignment(Position::Left|Position::CenterV);
    m_title->setText("Window");
    m_title->dock(Position::Fill);
    m_title->setPadding(Padding(8, 0, 0, 0));
    m_title->setTextColor(getSkin()->Colors.Window.TitleInactive);

    m_closeButton=new WindowCloseButton(m_titleBar);
    m_closeButton->setText("");
    m_closeButton->setSize(29, 24);
    m_closeButton->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_closeButton->dock(Position::Right);
    m_closeButton->onPressCaller.add(this, &WindowControl::closeButtonPressed);
    m_closeButton->setTabable(false);
    m_closeButton->setName("closeButton");
    m_closeButton->setWindow(this);

    // Create a blank content control, dock it to the top - Should this be a
    // ScrollControl?
    m_innerPanel=new Base(this);
    m_innerPanel->dock(Position::Fill);

    // hide the top resizer as we don't want it for window, we have a Dragger
    getResizer(3)->hide();  // TODO - hack

    bringToFront();
    setTabable(false);
    focus();
    setMinimumSize(Size(100, 40));
    setClampMovement(true);
    setKeyboardInputEnabled(false);
}


WindowControl::~WindowControl()
{
    destroyModal();
}

void WindowControl::makeModal(bool drawBackground)
{
    if(m_modal)
        return;

    m_modal=new internal::Modal(getCanvas());
    setParent(m_modal);
    m_modal->setShouldDrawBackground(drawBackground);
}

void WindowControl::destroyModal()
{
    if(!m_modal)
        return;

    // Really should be restoring our parent here.. but we don't know who it is.
    // Assume it's the canvas.
    setParent(getCanvas());
    m_modal->delayedDelete();
    m_modal=nullptr;
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

void WindowControl::render(skin::Base *skin)
{
    bool bIsFocussed=isOnTop();

    if(bIsFocussed)
        m_title->setTextColor(getSkin()->Colors.Window.TitleActive);
    else
        m_title->setTextColor(getSkin()->Colors.Window.TitleInactive);

//    skin->drawWindow(this, skin::Generate, m_titleBar->bottom(), bIsFocussed);
    skin->drawControl(this);
}

void WindowControl::renderUnder(skin::Base *skin)
{
    ParentClass::renderUnder(skin);
//    skin->drawShadow(this, skin::Generate);
    skin->drawControl(this);
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
    if(!hidden)
        bringToFront();

    ParentClass::setHidden(hidden);
}

void WindowControl::touch()
{
    ParentClass::touch();
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
