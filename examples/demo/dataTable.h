#ifndef _gweni_dataTable_example_h_
#define _gweni_dataTable_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class DataTable;
class Label;
}

class DataTableDemo: public controls::Base
{
public:
    GWENI_CONTROL(DataTableDemo, controls::Base);

public:
    //! The rows, one per element.
    static constexpr int RowCount=40;

    //! A header was clicked: the rows are put in that column's order.
    void onSortChanged(event::Info info);
    void onSelectionChange(event::Info info);

    controls::DataTable *m_table=nullptr;
    controls::Label *m_sorted=nullptr;
    controls::Label *m_selected=nullptr;
};

}//namespace gweni

#endif//_gweni_dataTable_example_h_
