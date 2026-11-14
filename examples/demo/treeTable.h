#ifndef _gweni_treeTable_example_h_
#define _gweni_treeTable_example_h_

#include <gweni/controls/base.h>

#include <cstddef>

namespace gweni
{
namespace controls
{
class TreeTable;
class DataTableRow;
class Label;
}

//! A generated file-system-like tree in a TreeTable: RootCount top-level
//! folders, each folder holding Fanout files and folders, numbered breadth
//! first (node n's parent is (n-RootCount)/Fanout), so the folders near the
//! top are the same whatever the node count and only the rows on screen
//! exist.
class TreeTableDemo: public controls::Base
{
public:
    GWENI_CONTROL(TreeTableDemo, controls::Base);

public:
    static constexpr size_t RootCount=8;
    static constexpr size_t Fanout=10;
    static constexpr size_t NodeCount=100000;

    //! Builds the tree again with `count` nodes (a new table, the old one
    //! deleted with the frame's delayed deletes), the first root and its first
    //! folder expanded and the first file of that folder selected.
    void setNodeCount(size_t count);
    size_t getNodeCount() const { return m_count; }

    //! Whether the label follows the selection (on by default): a scenario
    //! turns it off to count a selection's own work.
    void setShowSelection(bool show) { m_showSelection=show; }

    controls::TreeTable *m_table=nullptr;
    controls::Label *m_selected=nullptr;

private:
    void build();
    void bind(controls::DataTableRow *row, size_t id);
    void onSelectionChange(event::Info info);
    void onNodeActivated(event::Info info);

    size_t m_count=NodeCount;
    bool m_showSelection=true;
};

}//namespace gweni

#endif//_gweni_treeTable_example_h_
