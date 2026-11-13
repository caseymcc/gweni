#include "demo/chipBadge.h"
#include "gweni/controls/badge.h"
#include "gweni/controls/button.h"
#include "gweni/controls/chip.h"
#include "gweni/controls/label.h"
#include "gweni/controls/layout/layout.h"
#include "gweni/controls/scrollControl.h"

#include <algorithm>
#include <string>

namespace gweni
{

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 6, 0, 2));
}

// A row of controls side by side, docked Top in the column. A plain control
// clips its children, so a row whose children carry badges keeps \p padding
// clear around them for the overhang.
controls::Base *line(controls::Base *column, const Padding &padding=Padding(0, 0, 0, 0), int spacing=4)
{
    controls::Base *row=column->newChild<controls::Base>();
    controls::HorizontalLayout *layout=new controls::HorizontalLayout();

    layout->setSpacing(spacing);
    layout->setPadding(padding);
    row->setLayout(layout);
    row->setDock(DockPosition::Top);
    return row;
}

// The rest of a row's width, after its controls: a layout cell of a control
// that may grow takes a share of what is left, so the chips stay packed.
void endLine(controls::Base *row)
{
    controls::Base *rest=row->newChild<controls::Base>();

    rest->setSizeFlags({SizeFlag::Bloat, SizeFlag::Fixed});
    rest->setHeight(1);
}

controls::Chip *chip(controls::Base *row, const char *text)
{
    controls::Chip *chip=row->newChild<controls::Chip>();

    chip->setText(text);
    return chip;
}

controls::Badge *badge(controls::Base *anchor, int count, Corner corner=Corner::TopRight,
    const Point &offset=Point(0, 0))
{
    controls::Badge *badge=anchor->newChild<controls::Badge>();

    badge->attach(anchor, corner, offset);
    badge->setCount(count);
    return badge;
}

controls::Button *button(controls::Base *parent, const char *text, const Rect &bounds)
{
    controls::Button *button=parent->newChild<controls::Button>();

    button->setText(text);
    button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    button->setBounds(bounds);
    return button;
}

}//namespace

// A column of sections: the tones, the palette (index i mod 8, so 8 and 9 are
// 0 and 1 again), clickable and removable chips with what the last click
// said, a row of buttons each with a badge over the next one's corner, and
// below them a button with a badge at every corner, a ScrollControl whose
// first button is flush with the viewport's top (its badge cut there) and
// whose second keeps an 8-unit margin (its badge whole), and a navigation
// list whose counts "New message" raises and "Clear" clears.
GWENI_CONTROL_CONSTRUCTOR(ChipBadgeDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(600);

    caption(column, "Tones");
    {
        controls::Base *row=line(column);
        const struct
        {
            const char *text;
            controls::ChipTone tone;
        } tones[]=
        {
            {"Neutral", controls::ChipTone::Neutral}, {"Accent", controls::ChipTone::Accent},
            {"Success", controls::ChipTone::Success}, {"Warning", controls::ChipTone::Warning},
            {"Danger", controls::ChipTone::Danger}
        };

        for(const auto &tone : tones)
            chip(row, tone.text)->setTone(tone.tone);
        endLine(row);
    }

    caption(column, "Palette, by index (mod 8)");
    {
        controls::Base *row=line(column);

        for(int i=0; i < 10; ++i)
            chip(row, ("area "+std::to_string(i)).c_str())->setPaletteIndex(i);
        endLine(row);
    }

    caption(column, "Clickable and removable");
    {
        controls::Base *row=line(column);

        m_clickable=chip(row, "project: gweni");
        m_clickable->setTone(controls::ChipTone::Accent);
        m_clickable->setClickable(true);
        m_clickable->onPressCaller.add(this, &ChipBadgeDemo::onChipPressed);

        controls::Chip *agent=chip(row, "agent: coder");

        agent->setPaletteIndex(6);
        agent->setClickable(true);
        agent->onPressCaller.add(this, &ChipBadgeDemo::onChipPressed);

        const char *const removable[]={"ui", "layout", "render", "docs"};

        for(int i=0; i < 4; ++i)
        {
            controls::Chip *tag=chip(row, removable[i]);

            tag->setPaletteIndex(i+1);
            tag->setRemovable(true);
            tag->onRemove.add(this, &ChipBadgeDemo::onChipRemoved);
            m_removable.push_back(tag);
        }
        // the last one is clickable too
        m_removable.back()->setClickable(true);
        m_removable.back()->onPressCaller.add(this, &ChipBadgeDemo::onChipPressed);
        m_removableRow=row;

        m_pressed=row->newChild<controls::Label>();
        m_pressed->setText("pressed: -");
        endLine(row);
    }

    caption(column, "Badges on a row of buttons, each over the next");
    {
        // room above and to the right for the badges' overhang
        controls::Base *row=line(column, Padding(0, 8, 10, 0), 2);
        const struct
        {
            const char *text;
            int count;
        } buttons[]={{"Inbox", 3}, {"Alerts", 12}, {"Mentions", 120}, {"Status", -1}};

        for(const auto &entry : buttons)
        {
            controls::Button *anchor=row->newChild<controls::Button>();

            anchor->setText(entry.text);
            anchor->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
            anchor->setSize(76, 22);

            controls::Badge *count=badge(anchor, std::max(0, entry.count));

            if(entry.count < 0)
                count->setDot(true);
            m_rowButtons.push_back(anchor);
            m_rowBadges.push_back(count);
        }
    }

    // the corners, the ScrollControl and the navigation side by side
    controls::Base *lower=column->newChild<controls::Base>();

    lower->setDock(DockPosition::Top);
    lower->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    lower->setHeight(124);
    lower->setMargin(Margin(0, 6, 0, 0));
    {
        controls::Label *label=lower->newChild<controls::Label>();

        label->setText("At every corner");
        label->setPos(0, 0);

        m_corner=button(lower, "Corners", Rect(16, 30, 120, 40));

        const Corner corners[]={Corner::TopLeft, Corner::TopRight, Corner::BottomLeft, Corner::BottomRight};

        for(int i=0; i < 4; ++i)
            m_cornerBadges.push_back(badge(m_corner, i+1, corners[i]));
    }
    {
        controls::Label *label=lower->newChild<controls::Label>();

        label->setText("In a ScrollControl: flush, cut / a margin");
        label->setPos(170, 0);

        controls::ScrollControl *scroll=lower->newChild<controls::ScrollControl>();

        scroll->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        scroll->setBounds(170, 20, 220, 96);
        scroll->setScroll(false, true);

        // flush with the viewport's top: its badge's upper half is cut there
        badge(button(scroll, "Flush top", Rect(6, 0, 80, 22)), 5);
        // kept 8 units down: its badge shows whole
        badge(button(scroll, "8 units down", Rect(104, 8, 86, 22)), 42);
        button(scroll, "Below", Rect(6, 120, 80, 22));
    }
    {
        controls::Label *label=lower->newChild<controls::Label>();

        label->setText("Navigation");
        label->setPos(410, 0);

        const int x=410;
        const struct
        {
            const char *text;
            int count;
        } entries[]={{"Inbox", 4}, {"Tasks", 0}, {"Reviews", 27}};

        for(int i=0; i < 3; ++i)
        {
            controls::Button *entry=button(lower, entries[i].text, Rect(x, 20+i*24, 160, 22));

            entry->setTextAlignment(Alignment::Left|Alignment::CenterV);
            entry->setTextPadding(Padding(8, 0, 0, 0));

            // inside the entry, at its right end, centred down it
            controls::Badge *count=badge(entry, entries[i].count, Corner::TopRight, Point(-16, 11));

            if(i == 0)
            {
                m_navInbox=entry;
                m_navInboxBadge=count;
            }
            else if(i == 2)
                m_navReviewsBadge=count;
        }

        m_newMessage=button(lower, "New message", Rect(x, 94, 86, 22));
        m_newMessage->onPressCaller.add(this, &ChipBadgeDemo::onNewMessage);
        button(lower, "Clear", Rect(x+92, 94, 68, 22))->onPressCaller.add(this, &ChipBadgeDemo::onClear);
    }
}

controls::Chip *ChipBadgeDemo::removableChip() const
{
    return m_removable.empty()?nullptr:m_removable.front();
}

const String &ChipBadgeDemo::pressedText() const
{
    return m_pressed->getText();
}

void ChipBadgeDemo::onChipPressed(event::Info info)
{
    if(auto *pressed=gweni_cast<controls::Chip>(info.controlCaller))
        m_pressed->setText("pressed: "+pressed->getText());
}

// The chip never deletes itself: the app takes it away.
void ChipBadgeDemo::onChipRemoved(event::Info info)
{
    auto *removed=gweni_cast<controls::Chip>(info.controlCaller);

    if(!removed)
        return;
    m_removable.erase(std::remove(m_removable.begin(), m_removable.end(), removed), m_removable.end());
    removed->delayedDelete();
}

// A new message: one more in the inbox, thirty more reviews (99+ after three).
void ChipBadgeDemo::onNewMessage(event::Info)
{
    m_navInboxBadge->setCount(m_navInboxBadge->getCount()+1);
    m_navReviewsBadge->setCount(m_navReviewsBadge->getCount()+30);
}

void ChipBadgeDemo::onClear(event::Info)
{
    m_navInboxBadge->setCount(0);
    m_navReviewsBadge->setCount(0);
}

}//namespace gweni
