/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/searchCombo.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/layout/layout.h"
#include "gweni/controls/searchComboBox.h"

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
    label->setMargin(Margin(0, 8, 0, 2));
}

// A box docked Top in the column, the column's width.
controls::SearchComboBox *box(controls::Base *column)
{
    controls::SearchComboBox *box=column->newChild<controls::SearchComboBox>();

    box->setDock(DockPosition::Top);
    return box;
}

const char *const g_sizes[]={"Small", "Medium", "Large", "Extra large"};

}//namespace

String SearchComboDemo::referenceLabel(size_t index)
{
    static const char *const kinds[]={"feature", "fix", "docs", "release", "task"};
    static const char *const words[]={"search", "layout", "chips", "render", "vulkan", "theme",
        "popup", "toast", "editor", "timeline", "chart", "badge"};
    // four digits, so a query of digits picks the same few at any count
    const std::string number=std::to_string(10000+index).substr(1);

    return String(kinds[index%5])+"/"+words[(index*7)%12]+"-"+number;
}

bool SearchComboDemo::validHost(const String &text)
{
    if(text.empty() || (text.front() == '.') || (text.back() == '.') || (text.find('.') == String::npos))
        return false;

    for(char c : text)
    {
        if(!(((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '-') || (c == '.')))
            return false;
    }
    return true;
}

// A column of sections, each a caption and its boxes; the event label last.
GWENI_CONTROL_CONSTRUCTOR(SearchComboDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(420);

    caption(column, "ComboBox and a plain SearchComboBox");
    {
        controls::Base *row=column->newChild<controls::Base>();
        controls::HorizontalLayout *layout=new controls::HorizontalLayout();

        layout->setSpacing(8);
        row->setLayout(layout);
        row->setDock(DockPosition::Top);

        m_combo=row->newChild<controls::ComboBox>();
        m_plain=row->newChild<controls::SearchComboBox>();

        for(const char *size : g_sizes)
        {
            m_combo->addItem(size);
            m_plain->addOption(size);
        }
        m_combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        m_combo->setSize(150, 20);
        m_plain->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        m_plain->setSize(150, 20);
        m_plain->onSelection.add(this, &SearchComboDemo::onPicked);
    }

    caption(column, "Searchable: 300 references (type to filter)");
    m_references=box(column);
    m_references->setFilterable(true);
    setReferenceCount(ReferenceCount);
    m_references->onSelection.add(this, &SearchComboDemo::onPicked);

    caption(column, "Free text: a host name (a-z, 0-9, '-', '.' with a dot)");
    m_hosts=box(column);
    m_hosts->setOptions({{"web-1.example.org", ""}, {"web-2.example.org", ""}, {"db.example.org", ""}});
    m_hosts->setAllowCustomValue(true);
    m_hosts->onSelection.add(this, &SearchComboDemo::onPicked);
    m_hosts->onCustomValue.add(this, &SearchComboDemo::onHost);

    caption(column, "Multi-select");
    m_tags=box(column);
    m_tags->setMultiSelect(true);
    m_tags->setOptions({{"bug", ""}, {"enhancement", ""}, {"question", ""}, {"good first issue", ""},
        {"documentation", ""}});
    m_tags->setSelectedIndices({0, 3});
    m_tags->onSelectionChange.add(this, &SearchComboDemo::onPicksChanged);

    caption(column, "Multi-select with search and free text");
    m_areas=box(column);
    m_areas->setMultiSelect(true);
    m_areas->setAllowCustomValue(true);
    m_areas->setOptions({{"controls", ""}, {"layout", ""}, {"skin-textured", ""}, {"renderer-vulkan", ""},
        {"renderer-gl", ""}, {"text", ""}, {"tests", ""}, {"docs", ""}, {"controltest", ""}});
    m_areas->setSelectedIndices({0, 2, 5, 6});
    m_areas->onSelectionChange.add(this, &SearchComboDemo::onPicksChanged);

    m_event=column->newChild<controls::Label>();
    m_event->setDock(DockPosition::Top);
    m_event->setMargin(Margin(0, 10, 0, 0));
    m_event->setText("Pick, type or add something.");
}

void SearchComboDemo::setReferenceCount(size_t count)
{
    std::vector<controls::SearchComboBox::Option> options;

    options.reserve(count);
    for(size_t i=0; i < count; ++i)
        options.push_back({referenceLabel(i), ""});
    m_references->setOptions(std::move(options));
}

const String &SearchComboDemo::lastEvent() const
{
    return m_event->getText();
}

void SearchComboDemo::say(const String &text)
{
    m_event->setText(text);
}

void SearchComboDemo::onPicked(event::Info info)
{
    auto *picked=gweni_cast<controls::SearchComboBox>(info.controlCaller);

    if(picked)
        say("Picked: "+picked->getValue());
}

// The validator: a host it refuses stays in the field in the refused colour;
// one it takes is added as an option, for the next time.
void SearchComboDemo::onHost(event::Info info)
{
    if(!validHost(info.string))
    {
        m_hosts->refuseCustomValue();
        say("Refused: \""+info.string+"\" is no host name");
        return;
    }

    m_hosts->addOption(info.string);
    say("Added host: "+info.string);
}

void SearchComboDemo::onPicksChanged(event::Info info)
{
    auto *changed=gweni_cast<controls::SearchComboBox>(info.controlCaller);

    if(!changed)
        return;

    String text;

    for(const String &value : changed->getSelectedValues())
        text+=(text.empty()?"":", ")+value;
    say("Picks: "+(text.empty()?String("none"):text));
}

}//namespace gweni
