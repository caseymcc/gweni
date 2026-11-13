/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/toast.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/windowButtons.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(Toast)
{
    // its own width, its content's height; the accent strip and a margin on
    // the left
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    setWidth(Width);
    setPadding(Padding(AccentWidth+8, 6, 4, 8));
    setDock(DockPosition::Bottom);
    setMargin(Margin(0, Gap, 0, 0));
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(false);

    m_close=newChild<WindowCloseButton>();
    m_close->setSize(22, 20);
    m_close->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_close->setDock(DockPosition::Right);
    m_close->setAlignment(Alignment::Top);
    m_close->setTabable(false);
    m_close->setName("closeButton");
    m_close->setWindow(this);
    m_close->onPressCaller.add(this, &Toast::onClosePressed);

    m_title=newChild<Label>();
    m_title->setDock(DockPosition::Top);
    m_title->setMargin(Margin(0, 2, 0, 2));
    m_title->setMouseInputEnabled(false);
    m_title->setHidden(true);

    m_text=newChild<Label>();
    m_text->setDock(DockPosition::Top);
    m_text->setMargin(Margin(0, 2, 0, 0));
    m_text->setWrap(true);
    m_text->setMouseInputEnabled(false);
}

void Toast::setup(ToastKind kind, const String &text, const String &title, double timeout, double deadline)
{
    m_kind=kind;
    m_timeout=timeout;
    m_deadline=deadline;
    m_text->setText(text);
    m_title->setText(title);
    m_title->setHidden(title.empty());
    updateColors();
}

String Toast::getText() const
{
    return m_text->getText();
}

String Toast::getTitle() const
{
    return m_title->getText();
}

void Toast::updateColors()
{
    ParentClass::updateColors();

    const auto &colors=getSkin()->Colors.Toast;
    const Color *title=&colors.Info;

    switch(m_kind)
    {
    case ToastKind::Info: title=&colors.Info; break;
    case ToastKind::Positive: title=&colors.Positive; break;
    case ToastKind::Warning: title=&colors.Warning; break;
    case ToastKind::Negative: title=&colors.Negative; break;
    }
    m_title->setTextColor(*title);
    m_text->setTextColor(colors.Text);
}

void Toast::dismiss()
{
    if(m_dismissed)
        return;

    m_dismissed=true;
    setHidden(true);
    delayedDelete();

    if(Canvas *canvas=getCanvas())
        canvas->toastRemoved(this);
}

void Toast::onClosePressed(event::Info)
{
    dismiss();
}

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(ToastHost)
{
    // placed and sized by itself, in the canvas's top-right corner
    setPlacement(Placement::Owner);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
}

// The canvas's arrange skips it (Placement::Owner): Toast::Width wide, as
// high as its toasts (its preferred height, measured in this pass), Gap in
// from the canvas's top and right edges. The toasts are docked Bottom in the
// order they came, so the newest is at the top and a removed one's gap
// closes towards the corner.
void ToastHost::arrange(skin::Base *skin, Dim dim)
{
    Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas))
    {
        if(dim == Dim::X)
            setArrangedBounds(canvas->getWidth()-Toast::Width-Toast::Gap, getY(), Toast::Width, getHeight());
        else
            setArrangedBounds(getX(), 0, getWidth(), getPreferredSize().height);
    }

    ParentClass::arrange(skin, dim);
}

}//namespace internal

}//namespace controls
}//namespace gweni
