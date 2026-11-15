/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TreeTableDemo: a generated file-system-like tree of 100 000 nodes, a
 *  folder whose children load lazily, sortable columns and live edits.
 */
#ifndef _gweni_treeTable_example_h_
#define _gweni_treeTable_example_h_

#include <gweni/controls/base.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace gweni
{
namespace controls
{
class TreeTable;
class DataTableRow;
class Label;
class Button;
}

//! A generated file-system-like tree in a TreeTable: RootCount top-level
//! folders, each folder holding Fanout files and folders, numbered breadth
//! first (node n's parent is (n-RootCount)/Fanout), so the folders near the
//! top are the same whatever the node count and only the rows on screen
//! exist. A first root, "remote", loads its children lazily: its first
//! expand asks for them and LazyDelay seconds of the canvas clock later
//! (answerPending()) they arrive in place of its "Loading…" row. The Name,
//! Size and Modified columns sort with folders first, and three buttons add a
//! file to the selected folder (or beside the selected file), remove the
//! selection and rename it.
class TreeTableDemo: public controls::Base
{
public:
    GWENI_CONTROL(TreeTableDemo, controls::Base);

public:
    static constexpr size_t RootCount=8;
    static constexpr size_t Fanout=10;
    static constexpr size_t NodeCount=100000;
    //! The lazy folder's id, past every generated one, and its children's
    //! (LazyFolder+1 ...), and the ids the Add button hands out.
    static constexpr size_t LazyFolder=900000000;
    static constexpr size_t LazyChildren=12;
    static constexpr size_t FirstAdded=900001000;
    static constexpr double LazyDelay=0.4;

    //! Builds the tree again with `count` nodes (a new table, the old one
    //! deleted with the frame's delayed deletes), the first root and its first
    //! folder expanded and the first file of that folder selected.
    void setNodeCount(size_t count);
    size_t getNodeCount() const { return m_count; }

    //! Whether the label follows the selection (on by default): a scenario
    //! turns it off to count a selection's own work.
    void setShowSelection(bool show) { m_showSelection=show; }

    //! The lazy folder's answer, if it was asked for and has not come: its
    //! children inserted at once. False when nothing is pending.
    bool answerPending();
    //! The order a column sorts in (0 Name, 1 Size, 2 Modified): whether a
    //! comes before b, ascending, folders first left to the table.
    bool less(int column, size_t a, size_t b) const;
    //! The name a row shows.
    std::string displayName(size_t id) const;
    //! Whether the node shows as a folder: it has children, or Remove took
    //! its last one (the table makes that a leaf; the demo keeps its name).
    bool showsAsFolder(size_t id) const;

    //! The buttons' actions: add a file to the selected folder (beside a
    //! selected file), remove the selection, rename it.
    void addFile();
    void removeSelected();
    void renameSelected();

    controls::TreeTable *m_table=nullptr;
    controls::Label *m_selected=nullptr;
    controls::Button *m_add=nullptr;
    controls::Button *m_remove=nullptr;
    controls::Button *m_rename=nullptr;

private:
    void build();
    void bind(controls::DataTableRow *row, size_t id);
    void onSelectionChange(event::Info info);
    void onNodeActivated(event::Info info);
    void onChildrenRequested(event::Info info);
    void onAddPressed(event::Info info);
    void onRemovePressed(event::Info info);
    void onRenamePressed(event::Info info);
    void showSelection();

    size_t m_count=NodeCount;
    bool m_showSelection=true;
    //! Names the app gave (added and renamed nodes), the rest generated.
    std::unordered_map<size_t, std::string> m_names;
    size_t m_nextAdded=FirstAdded;
    size_t m_renames=0;
    //! Folders Remove emptied.
    std::unordered_set<size_t> m_emptiedFolders;
    bool m_lazyPending=false;
};

}//namespace gweni

#endif//_gweni_treeTable_example_h_
