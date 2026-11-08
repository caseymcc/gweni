#include "demo/dataTableVirtual.h"
#include "gweni/controls/dataTable.h"
#include "gweni/controls/label.h"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <string>

namespace gweni
{

namespace
{

// Every field of a record comes from its number, so 100 000 records hold no
// strings: a row's text is made when the row is bound.
uint32_t mix(uint32_t x)
{
    x*=2654435761u;
    x^=x>>16;
    x*=0x45d9f3bu;
    x^=x>>16;
    return x;
}

// Two-letter syllables in alphabetical order, so a name's order is the order of
// its syllable indices.
const char *const syllables[]={"ba", "da", "fe", "go", "ki", "lu", "mo", "na", "pe", "ri", "so", "tu"};
constexpr uint32_t SyllableCount=12;
const char *const groups[]={"Alpha", "Beta", "Delta", "Gamma"};
const char *const captions[]={"Id", "Name", "Score", "Group"};

uint32_t nameKey(uint32_t record)
{
    return mix(record)%(SyllableCount*SyllableCount*SyllableCount);
}

std::string name(uint32_t record)
{
    const uint32_t key=nameKey(record);
    std::string text=syllables[key/(SyllableCount*SyllableCount)];

    text+=syllables[(key/SyllableCount)%SyllableCount];
    text+=syllables[key%SyllableCount];
    text[0]=static_cast<char>(text[0]-'a'+'A');
    return text;
}

// Hundredths, 0.00 to 999.99.
uint32_t score(uint32_t record)
{
    return mix(record^0x9e3779b9u)%100000u;
}

uint32_t group(uint32_t record)
{
    return mix(record+77u)%4u;
}

}//namespace

// 100 000 records in a striped table with row and column rules, of which only
// the rows on screen exist: the binder makes each row's text from its record
// when the row comes into view. A header click sorts the records by that
// column (the table only shows the new order: rebind()), and the selection
// stays on its record.
GWENI_CONTROL_CONSTRUCTOR(DataTableVirtualDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText(std::to_string(RecordCount)+" records, a row for each one on screen: click a header to sort");
    title->setPos(10, 10);

    m_table=newChild<controls::DataTable>();
    m_table->setBounds(10, 34, 420, 400);
    m_table->setColumnCount(4);
    for(int i=0; i < 4; ++i)
        m_table->setColumnCaption(i, captions[i]);
    m_table->setColumnWidth(0, 80);
    m_table->setColumnWidth(1, 120);
    m_table->setColumnWidth(2, 90);
    m_table->setBorders(controls::DataTableBorders::All);
    m_table->setRowBinder([this](controls::DataTableRow *row, size_t item) { bind(row, item); });

    m_sorted=newChild<controls::Label>();
    m_sorted->setPos(444, 34);
    m_selected=newChild<controls::Label>();
    m_selected->setPos(444, 58);

    m_table->onSortChanged.add(this, &DataTableVirtualDemo::onSortChanged);
    m_table->onSelectionChange.add(this, &DataTableVirtualDemo::onSelectionChange);

    m_selectedRecord=5;
    setRecordCount(RecordCount);
    m_table->setSortColumn(0, controls::SortDirection::Ascending);
}

void DataTableVirtualDemo::setRecordCount(size_t count)
{
    count=std::min(count, RecordCount);
    m_order.resize(count);
    std::iota(m_order.begin(), m_order.end(), 0u);
    m_table->setItemCount(count);
    sortRecords();
}

void DataTableVirtualDemo::bind(controls::DataTableRow *row, size_t item)
{
    const uint32_t record=m_order[item];
    char text[16];

    std::snprintf(text, sizeof(text), "%u.%02u", score(record)/100, score(record)%100);
    row->setCellText(0, std::to_string(record));
    row->setCellText(1, name(record));
    row->setCellText(2, text);
    row->setCellText(3, groups[group(record)]);
}

// The records in the sorted column's order (ties by record number), the
// selected record's new item selected; descending is the reverse.
void DataTableVirtualDemo::sortRecords()
{
    const int column=m_table->getSortColumn();
    const bool descending=(m_table->getSortDirection() == controls::SortDirection::Descending);

    auto key=[column](uint32_t record) -> uint64_t
    {
        uint64_t field=record;

        if(column == 1)
            field=nameKey(record);
        else if(column == 2)
            field=score(record);
        else if(column == 3)
            field=group(record);
        return (field<<32)|record;
    };

    if(column >= 0)
        std::sort(m_order.begin(), m_order.end(), [&key, descending](uint32_t a, uint32_t b)
            {
                return descending?key(b) < key(a):key(a) < key(b);
            });

    m_table->rebind();

    auto it=std::find(m_order.begin(), m_order.end(), static_cast<uint32_t>(m_selectedRecord));
    const size_t item=(m_selectedRecord >= 0 && it != m_order.end())?
        static_cast<size_t>(it-m_order.begin()):controls::DataTable::NoItem;

    m_table->setSelectedItem(item, false);
    onSelectionChange(event::Info(m_table));
}

void DataTableVirtualDemo::onSortChanged(event::Info)
{
    const int column=m_table->getSortColumn();

    sortRecords();

    if(column < 0)
    {
        m_sorted->setText("Not sorted");
        return;
    }

    m_sorted->setText(String("Sorted by ")+captions[column]+
        (m_table->getSortDirection() == controls::SortDirection::Descending?", descending":", ascending"));
}

void DataTableVirtualDemo::onSelectionChange(event::Info)
{
    const size_t item=m_table->getSelectedItem();

    // none: the selected record is not among those shown (a row is never
    // deselected by a click), and is selected again if it comes back
    if(item == controls::DataTable::NoItem)
    {
        m_selected->setText("Selected: none");
        return;
    }

    m_selectedRecord=m_order[item];
    m_selected->setText("Selected: "+name(m_order[item])+" (Id "+std::to_string(m_order[item])+")");
}

}//namespace gweni
