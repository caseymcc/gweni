/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/dialogs/dialog.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/inputHandler.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace dialogs
{

int Extent::resolve(int canvas, int content) const
{
    int extent=content;

    if(kind == Kind::Units)
        extent=static_cast<int>(std::lround(value));
    else if(kind == Kind::Fraction)
        extent=static_cast<int>(std::lround(value*canvas));

    return std::max(0, std::min(extent, canvas));
}

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(DialogTitleBar)
{
    setHeight(Height);
    setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    setDock(DockPosition::Top);
}

GWENI_CONTROL_CONSTRUCTOR(DialogFooter)
{
    setDock(DockPosition::Bottom);
    setPadding(Padding(8, 8, 8, 8));
    setHidden(true);

    m_row=newChild<controls::Base>();
    m_row->setDock(DockPosition::Right);
}

}//namespace internal

GWENI_CONTROL_CONSTRUCTOR(Dialog)
{
    // the shadow is inside the bounds, below and right of the panel; the
    // panel's border is 1 unit
    setPadding(Padding(1, 1, 1+ShadowSize, 1+ShadowSize));
    // placed and sized by itself, on the canvas, not by the canvas
    setPlacement(controls::Placement::Owner);
    setMouseInputEnabled(true);
    setHidden(true);

    m_titleBar=newChild<internal::DialogTitleBar>();
    m_titleBar->onDragStart.add(this, &Dialog::onTitleDragStart);
    m_titleBar->onDragged.add(this, &Dialog::onTitleDragged);

    m_titleLabel=m_titleBar->newChild<controls::Label>();
    m_titleLabel->setDock(DockPosition::Center);
    m_titleLabel->setPadding(Padding(10, 0, 0, 0));
    m_titleLabel->setAlignment(Alignment::Left|Alignment::CenterV);
    m_titleLabel->setTextColor(getSkin()->Colors.Window.TitleActive);
    m_titleLabel->setMouseInputEnabled(false);

    m_closeButton=m_titleBar->newChild<controls::WindowCloseButton>();
    m_closeButton->setSize(30, internal::DialogTitleBar::Height);
    m_closeButton->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_closeButton->setDock(DockPosition::Right);
    m_closeButton->setTabable(false);
    m_closeButton->setName("closeButton");
    m_closeButton->setWindow(this);
    m_closeButton->onPressCaller.add(this, &Dialog::onCloseButton);

    m_footer=newChild<internal::DialogFooter>();

    // above the content, outside its ScrollControl: a wrapped text there is
    // measured at the width this pass gives it, in one pass
    m_message=newChild<controls::Label>();
    m_message->setDock(DockPosition::Top);
    m_message->setMargin(Margin(10, 8, 10, 8));
    m_message->setWrap(true);
    m_message->setName("message");
    m_message->setHidden(true);

    m_content=newChild<controls::ScrollControl>();
    m_content->setDock(DockPosition::Center);
    m_content->setScroll(false, true);
    m_content->setAutoHideBars(true);
}

Dialog::~Dialog()
{
    // a deleted dialog calls nothing; ~Base takes it off the modal stack
    m_onResult=nullptr;
}

void Dialog::setTitle(const String &title)
{
    m_titleLabel->setText(title);
}

String Dialog::getTitle() const
{
    return m_titleLabel->getText();
}

controls::Label *Dialog::setMessage(const String &text)
{
    m_message->setText(text);
    m_message->setHidden(text.empty());
    return m_message;
}

void Dialog::setDialogSize(Extent width, Extent height)
{
    m_width=width;
    m_height=height;
    invalidate();
}

controls::Button *Dialog::addButton(const String &label)
{
    controls::Button *button=m_footer->getRow()->newChild<controls::Button>();

    button->setText(label);
    button->setDock(DockPosition::Left);
    // wider than its text, as a dialog's buttons are
    button->setPadding(Padding(16, 3, 16, 3));
    // between the buttons, none left of the first
    if(m_footer->getRow()->getChildren().size() > 1)
        button->setMargin(Margin(6, 0, 0, 0));
    // it holds the focus (Base drops a focus on a control without keyboard
    // input) and Tab reaches it
    button->setKeyboardInputEnabled(true);
    button->setTabable(true);
    m_footer->show();
    return button;
}

controls::Button *Dialog::addButton(const String &label, Result result)
{
    controls::Button *button=addButton(label);

    button->onPressCaller.addCallback(this, [result](event::Handler &handler, event::Info)
    {
        static_cast<Dialog &>(handler).close(result);
    });
    return button;
}

void Dialog::setClosable(bool closable)
{
    m_closeButton->setHidden(!closable);
}

bool Dialog::isClosable() const
{
    return !m_closeButton->hidden();
}

void Dialog::open()
{
    controls::Canvas *canvas=getCanvas();

    if(m_open || !canvas)
        return;

    m_open=true;
    if(getParent() != canvas)
        setParent(canvas);
    ParentClass::setHidden(false);
    canvas->pushModal(this);
    invalidate();

    if(controls::Base *focus=m_initialFocus ? m_initialFocus : m_defaultButton)
        focus->focus();
}

void Dialog::close(Result result)
{
    if(!m_open || m_closing)
        return;

    m_open=false;
    m_closing=true;

    controls::Canvas *canvas=getCanvas();

    // hidden first, so the pop puts nothing over it again; the pop gives the
    // focus back
    ParentClass::setHidden(true);
    if(canvas)
        canvas->popModal(this);

    // the handler goes before it runs: it is called once, whatever it does
    ResultHandler handler=std::move(m_onResult);
    m_onResult=nullptr;
    const size_t id=getControlId();

    if(handler)
        handler(result, resultText(result));
    // it may have deleted this dialog outright
    if(!Base::getControlById(id))
        return;
    onClose.call(this);

    m_closing=false;
    // a handler that opened it again keeps it
    if(m_deleteOnClose && !m_open)
        delayedDelete();
}

void Dialog::setHidden(bool hidden)
{
    if(hidden && m_open)
    {
        close();
        return;
    }

    ParentClass::setHidden(hidden);
}

bool Dialog::onKeyEscape(bool down)
{
    if(!m_open || !isClosable())
        return false;

    if(down)
        close();
    return true;
}

bool Dialog::onKeyReturn(bool down)
{
    if(!m_open)
        return false;

    if(!down)
        return true;

    // a focused footer button is the one pressed, else the default
    controls::Button *button=m_defaultButton;

    if(KeyboardFocus && (KeyboardFocus->getParent() == m_footer->getRow()))
    {
        if(controls::Button *focused=gweni_cast<controls::Button>(KeyboardFocus))
            button=focused;
    }

    if(!button || !button->visible() || button->isDisabled())
        return false;

    button->onPress(event::Info(button));
    return true;
}

void Dialog::onCloseButton(event::Info)
{
    close();
}

void Dialog::onTitleDragStart(event::Info)
{
    const Point mouse=input::getMousePosition();

    m_dragHold=Point(mouse.x-getX(), mouse.y-getY());
}

void Dialog::onTitleDragged(event::Info)
{
    const Point mouse=input::getMousePosition();

    dragTo(Point(mouse.x-m_dragHold.x, mouse.y-m_dragHold.y));
}

namespace
{
int clampTo(int pos, int size, int area)
{
    return std::max(0, std::min(pos, area-size));
}
}

bool Dialog::dragTo(const Point &pos)
{
    controls::Canvas *canvas=getCanvas();

    if(!canvas)
        return false;

    m_userPlaced=true;
    m_userPos=pos;

    // as the next arrange puts it; a pure move, so nothing is laid out
    const int x=clampTo(pos.x, getWidth(), canvas->getWidth());
    const int y=clampTo(pos.y, getHeight(), canvas->getHeight());

    if((x == getX()) && (y == getY()))
        return false;

    placeChildPos(this, x, y);
    return true;
}

// The canvas's arrange skips it (Placement::Owner), so it takes its size from
// its Extents and its position, centred or the dragged one, before the
// subtree is arranged. Each axis needs only that axis's size, final when its
// arrange runs: its preferred size was measured in this pass.
void Dialog::arrange(skin::Base *skin, Dim dim)
{
    controls::Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas))
    {
        const Size preferred=getPreferredSize();

        if(dim == Dim::X)
        {
            const int area=canvas->getWidth();
            const int w=m_width.resolve(area, preferred.width);
            const int x=m_userPlaced ? clampTo(m_userPos.x, w, area) : (area-w)/2;

            setArrangedBounds(x, getY(), w, getHeight());
        }
        else
        {
            const int area=canvas->getHeight();
            const int h=m_height.resolve(area, preferred.height);
            const int y=m_userPlaced ? clampTo(m_userPos.y, h, area) : (area-h)/2;

            setArrangedBounds(getX(), y, getWidth(), h);
        }
    }

    ParentClass::arrange(skin, dim);
}

}//namespace dialogs
}//namespace gweni
