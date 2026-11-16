/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/breadcrumb.h"
#include "gweni/controls/breadcrumb.h"
#include "gweni/controls/button.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"

namespace gweni
{

//! The deep path's container: a plain box that clips nothing. The path cuts
//! its parts at its own edge, and a clipping container would re-stamp every
//! part's clip box on each resize.
class BreadcrumbDemoBox: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(BreadcrumbDemoBox, controls::Base)
    {}
    GWENI_DRAWS_NOTHING;

    bool shouldClip() override { return false; }
};

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 8, 0, 3));
}

// A row docked Top in the column, its parts docked Left in it.
controls::Base *row(controls::Base *column)
{
    controls::Base *row=column->newChild<controls::Base>();

    row->setDock(DockPosition::Top);
    row->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    return row;
}

}//namespace

// A column of captioned rows, then the label.
GWENI_CONTROL_CONSTRUCTOR(BreadcrumbDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(MaxPathWidth+20);

    caption(column, "A deep path in a container the slider resizes");
    controls::Base *pathRow=row(column);

    m_box=pathRow->newChild<BreadcrumbDemoBox>();
    m_box->setDock(DockPosition::Left);
    m_box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    m_box->setWidth(StartPathWidth);
    m_deep=m_box->newChild<controls::Breadcrumb>();
    m_deep->setItems({"Home", "Projects", "gweni", "source", "controls", "base.cpp"});
    m_deep->onItemClicked.add(this, &BreadcrumbDemo::onClicked);

    controls::Base *widthRow=row(column);
    controls::Label *widthLabel=widthRow->newChild<controls::Label>();

    widthLabel->setText("Width");
    widthLabel->setDock(DockPosition::Left);
    widthLabel->setMargin(Margin(0, 0, 8, 0));
    m_width=widthRow->newChild<controls::HorizontalSlider>();
    m_width->setDock(DockPosition::Left);
    m_width->setSize(240, 20);
    m_width->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_width->setRange(80, MaxPathWidth);
    m_width->setFloatValue(StartPathWidth);
    m_width->onValueChanged.add(this, &BreadcrumbDemo::onWidth);

    caption(column, "Text separators");
    m_slash=column->newChild<controls::Breadcrumb>();
    m_slash->setSeparatorText("/");
    m_slash->setItems({"usr", "local", "share", "gweni", "resources"});
    m_slash->onItemClicked.add(this, &BreadcrumbDemo::onClicked);

    caption(column, "A click goes back to the item");
    m_navigation=column->newChild<controls::Breadcrumb>();
    m_navigation->setItems({"Home", "Documents", "Reports"});
    m_navigation->onItemClicked.add(this, &BreadcrumbDemo::onClicked);
    m_navigation->onItemClicked.add(this, &BreadcrumbDemo::onNavigate);

    controls::Base *pushRow=row(column);

    m_push=pushRow->newChild<controls::Button>();
    m_push->setDock(DockPosition::Left);
    m_push->setText("Push folder");
    m_push->setHeight(24);
    m_push->setTextPadding(Padding(8, 0, 8, 0));
    m_push->setSizeFlags({SizeFlag::Shrink, SizeFlag::Fixed});
    m_push->onPressCaller.add(this, &BreadcrumbDemo::onPush);

    m_clicked=column->newChild<controls::Label>();
    m_clicked->setDock(DockPosition::Top);
    m_clicked->setMargin(Margin(0, 12, 0, 0));
    m_clicked->setText("Clicked: -");
}

void BreadcrumbDemo::setPathWidth(int width)
{
    m_box->setWidth(width);
}

void BreadcrumbDemo::setLabelUpdates(bool updates)
{
    m_labelUpdates=updates;
}

void BreadcrumbDemo::onClicked(event::Info info)
{
    auto *path=gweni_cast<controls::Breadcrumb>(info.control);

    if(!m_labelUpdates || !path || info.integer >= path->getItemCount())
        return;
    m_clicked->setText("Clicked: "+path->getItem(info.integer).label+" ("+std::to_string(info.integer)+")");
}

void BreadcrumbDemo::onNavigate(event::Info info)
{
    m_navigation->popTo(info.integer);
}

void BreadcrumbDemo::onPush(event::Info)
{
    m_navigation->push("Folder "+std::to_string(++m_pushed));
}

void BreadcrumbDemo::onWidth(event::Info)
{
    setPathWidth(static_cast<int>(m_width->getFloatValue()+0.5f));
}

}//namespace gweni
