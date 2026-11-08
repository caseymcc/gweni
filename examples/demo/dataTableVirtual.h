#ifndef _gweni_dataTableVirtual_example_h_
#define _gweni_dataTableVirtual_example_h_

#include <gweni/controls/base.h>

#include <cstdint>
#include <vector>

namespace gweni
{
namespace controls
{
class DataTable;
class DataTableRow;
class Label;
}

class DataTableVirtualDemo: public controls::Base
{
public:
    GWENI_CONTROL(DataTableVirtualDemo, controls::Base);

public:
    //! The records, as many as the table is given.
    static constexpr size_t RecordCount=100000;

    //! Shows the first `count` records (at most RecordCount), in the current
    //! sort order; the selected record stays selected if it is among them.
    void setRecordCount(size_t count);
    size_t getRecordCount() const { return m_order.size(); }
    //! The record the table's item shows.
    uint32_t recordAt(size_t item) const { return m_order[item]; }

    //! A header was clicked: the records are put in that column's order.
    void onSortChanged(event::Info info);
    void onSelectionChange(event::Info info);

    controls::DataTable *m_table=nullptr;
    controls::Label *m_sorted=nullptr;
    controls::Label *m_selected=nullptr;

private:
    void bind(controls::DataTableRow *row, size_t item);
    //! Puts m_order in the table's sort order and the selection on its record.
    void sortRecords();

    //! The record each item shows: item i is record m_order[i].
    std::vector<uint32_t> m_order;
    //! The selected record, or -1.
    int64_t m_selectedRecord=-1;
};

}//namespace gweni

#endif//_gweni_dataTableVirtual_example_h_
