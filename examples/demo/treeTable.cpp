/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TreeTableDemo: the tree, its columns and the selection label, the lazy
 *  folder, the sort comparators and the edit buttons.
 */
#include "demo/treeTable.h"
#include "gweni/controls/button.h"
#include "gweni/controls/canvas.h"
#include "gweni/controls/label.h"
#include "gweni/controls/treeTable.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace gweni
{

namespace
{

// Every field of a node comes from its number, so 100 000 nodes hold no
// strings: a row's text is made when the row is bound.
uint32_t mix(uint32_t x)
{
    x*=2654435761u;
    x^=x>>16;
    x*=0x45d9f3bu;
    x^=x>>16;
    return x;
}

const char *const roots[]={"home", "projects", "media", "system", "archive", "shared", "backup", "work"};
const char *const folders[]={"src", "docs", "assets", "build", "tests", "tools", "config", "lib", "notes", "data"};
const char *const stems[]={"main", "readme", "index", "photo", "report", "notes", "draft", "export", "schema",
    "theme"};
const char *const extensions[]={".cpp", ".md", ".png", ".json", ".txt", ".yaml", ".log", ".h"};
const char *const captions[]={"Name", "Size", "Modified"};

std::string nodeName(size_t id, bool folder)
{
    if(id < TreeTableDemo::RootCount)
        return roots[id];

    const uint32_t key=mix(static_cast<uint32_t>(id));

    if(folder)
        return std::string(folders[key%10])+"-"+std::to_string(id);
    return std::string(stems[key%10])+"-"+std::to_string(id)+extensions[(key>>8)%8];
}

uint32_t fileBytes(size_t id)
{
    return mix(static_cast<uint32_t>(id)^0x9e3779b9u)%50000000u;
}

std::string fileSize(size_t id)
{
    const uint32_t bytes=fileBytes(id);
    char text[32];

    if(bytes < 1024u)
        std::snprintf(text, sizeof(text), "%u B", bytes);
    else if(bytes < 1024u*1024u)
        std::snprintf(text, sizeof(text), "%.1f KB", bytes/1024.0);
    else
        std::snprintf(text, sizeof(text), "%.1f MB", bytes/(1024.0*1024.0));
    return text;
}

std::string modified(size_t id)
{
    const uint32_t key=mix(static_cast<uint32_t>(id)+77u);
    char text[32];

    std::snprintf(text, sizeof(text), "2026-%02u-%02u %02u:%02u", 1+key%12, 1+(key>>4)%28,
        (key>>9)%24, (key>>14)%60);
    return text;
}

}//namespace

// The table and its label; the tree is built by setNodeCount.
GWENI_CONTROL_CONSTRUCTOR(TreeTableDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText(std::to_string(NodeCount)+" files and folders, a row for each one on screen");
    title->setPos(10, 10);

    m_selected=newChild<controls::Label>();
    m_selected->setPos(10, 420);

    m_add=newChild<controls::Button>();
    m_add->setText("Add file");
    m_add->setBounds(10, 446, 90, 24);
    m_add->onPressCaller.add(this, &TreeTableDemo::onAddPressed);
    m_remove=newChild<controls::Button>();
    m_remove->setText("Remove");
    m_remove->setBounds(106, 446, 80, 24);
    m_remove->onPressCaller.add(this, &TreeTableDemo::onRemovePressed);
    m_rename=newChild<controls::Button>();
    m_rename->setText("Rename");
    m_rename->setBounds(192, 446, 80, 24);
    m_rename->onPressCaller.add(this, &TreeTableDemo::onRenamePressed);

    build();
}

void TreeTableDemo::setNodeCount(size_t count)
{
    count=std::max(count, RootCount);
    if(count == m_count)
        return;

    m_count=count;
    m_names.clear();
    m_nextAdded=FirstAdded;
    m_renames=0;
    m_lazyPending=false;
    m_table->setHidden(true);
    m_table->delayedDelete();
    m_table=nullptr;
    build();
}

// The two top levels are folders; below them two nodes in five are.
static bool isFolder(size_t id)
{
    return id < TreeTableDemo::RootCount*(TreeTableDemo::Fanout+1)
        || mix(static_cast<uint32_t>(id)^0x5bd1e995u)%5 < 2;
}

// Breadth first, so every parent is added before its children: node n's
// parent is (n-RootCount)/Fanout, and a number whose parent is a file is
// skipped. A node with children shows as a folder, the rest as files.
void TreeTableDemo::build()
{
    m_table=newChild<controls::TreeTable>();
    m_table->setBounds(10, 34, 600, 380);
    m_table->setColumnCount(3);
    for(int i=0; i < 3; ++i)
        m_table->setColumnCaption(i, captions[i]);
    m_table->setColumnWidth(0, 300);
    m_table->setColumnWidth(1, 100);

    // the lazy folder first, its children asked for when it is expanded
    m_names[LazyFolder]="remote";
    m_table->addNode(controls::TreeTable::Root, LazyFolder, true);

    std::vector<uint8_t> folder;

    for(size_t id=0, added=0; added < m_count; ++id)
    {
        const size_t parent=(id < RootCount)?controls::TreeTable::Root:(id-RootCount)/Fanout;

        folder.push_back(0);
        if(parent != controls::TreeTable::Root && !folder[parent])
            continue;
        m_table->addNode(parent, id);
        folder[id]=isFolder(id)?1:0;
        ++added;
    }

    m_table->setNodeBinder([this](controls::DataTableRow *row, size_t id, size_t) { bind(row, id); });
    m_table->onSelectionChange.add(this, &TreeTableDemo::onSelectionChange);
    m_table->onNodeActivated.add(this, &TreeTableDemo::onNodeActivated);
    m_table->onChildrenRequested.add(this, &TreeTableDemo::onChildrenRequested);

    // a header click sorts by the column, folders first
    for(int column=0; column < 3; ++column)
        m_table->setSortComparator(column, [this, column](size_t a, size_t b) { return less(column, a, b); });
    m_table->setFoldersFirst(true);

    // the first root and its first folder open, its first child selected
    const size_t first=RootCount;

    m_table->setExpanded(0, true);
    m_table->setExpanded(first, true);
    m_table->setSelectedNode(RootCount+first*Fanout);
    onSelectionChange(event::Info(m_table));
}

void TreeTableDemo::bind(controls::DataTableRow *row, size_t id)
{
    const controls::TreeTable::Node *node=m_table->getNode(id);
    const bool folder=node && node->hasChildren;

    row->setCellText(0, displayName(id));
    // a lazy folder not loaded yet has no count to show
    row->setCellText(1, !folder?fileSize(id):node->children.empty()?String("-")
        :std::to_string(node->children.size())+" items");
    row->setCellText(2, modified(id));
}

std::string TreeTableDemo::displayName(size_t id) const
{
    const auto named=m_names.find(id);

    if(named != m_names.end())
        return named->second;

    const controls::TreeTable::Node *node=m_table?m_table->getNode(id):nullptr;

    return nodeName(id, node && node->hasChildren);
}

// Name and Modified as their text sorts (the dates are written to sort so),
// Size by bytes, a folder by its item count; ties fall to the table, which
// keeps them in the tree's order.
bool TreeTableDemo::less(int column, size_t a, size_t b) const
{
    if(column == 0)
        return displayName(a) < displayName(b);
    if(column == 2)
        return modified(a) < modified(b);

    const auto size=[this](size_t id) -> uint64_t
        {
            const controls::TreeTable::Node *node=m_table->getNode(id);

            return node && node->hasChildren?node->children.size():fileBytes(id);
        };

    return size(a) < size(b);
}

void TreeTableDemo::onSelectionChange(event::Info)
{
    showSelection();
}

void TreeTableDemo::showSelection()
{
    if(!m_showSelection)
        return;

    const size_t id=m_table->getSelectedNode();

    if(!m_table->getNode(id))
    {
        m_selected->setText("Selected: none");
        return;
    }
    m_selected->setText("Selected: "+displayName(id));
}

// The children come later, on the canvas clock, as a fetch's would.
void TreeTableDemo::onChildrenRequested(event::Info info)
{
    if(info.integer != LazyFolder)
        return;

    m_lazyPending=true;
    if(controls::Canvas *canvas=getCanvas())
        canvas->after(LazyDelay, [this]() { answerPending(); }, this);
}

bool TreeTableDemo::answerPending()
{
    if(!m_lazyPending)
        return false;

    m_lazyPending=false;

    std::vector<size_t> ids;

    for(size_t i=1; i <= LazyChildren; ++i)
    {
        const uint32_t key=mix(static_cast<uint32_t>(i));

        ids.push_back(LazyFolder+i);
        m_names[LazyFolder+i]=std::string(stems[key%10])+"-remote-"+std::to_string(i)+extensions[(key>>8)%8];
    }
    return m_table->insertNodes(LazyFolder, 0, ids);
}

// A new file goes into the selected folder, or beside the selected file; it
// is named before it is inserted, since a sorted table asks where it goes.
void TreeTableDemo::addFile()
{
    const size_t selected=m_table->getSelectedNode();
    const controls::TreeTable::Node *node=m_table->getNode(selected);
    size_t parent=controls::TreeTable::Root;

    if(node)
        parent=node->hasChildren?selected:node->parent;

    const size_t id=m_nextAdded++;

    m_names[id]="new-file-"+std::to_string(id-FirstAdded+1)+".txt";
    if(m_table->insertNode(parent, static_cast<size_t>(-1), id) != id)
        return;
    if(parent != controls::TreeTable::Root)
        m_table->setExpanded(parent, true);
    m_table->setSelectedNode(id);
    m_table->scrollToItem(m_table->itemForNode(id));
    showSelection();
}

void TreeTableDemo::onAddPressed(event::Info)
{
    addFile();
}

void TreeTableDemo::onRemovePressed(event::Info)
{
    removeSelected();
}

void TreeTableDemo::onRenamePressed(event::Info)
{
    renameSelected();
}

void TreeTableDemo::removeSelected()
{
    const size_t selected=m_table->getSelectedNode();

    if(m_table->getNode(selected))
        m_table->removeNode(selected);
}

void TreeTableDemo::renameSelected()
{
    const size_t selected=m_table->getSelectedNode();
    const controls::TreeTable::Node *node=m_table->getNode(selected);

    if(!node)
        return;

    m_names[selected]="renamed-"+std::to_string(++m_renames)+(node->hasChildren?"":".txt");
    m_table->nodeChanged(selected);
    showSelection();
}

void TreeTableDemo::onNodeActivated(event::Info info)
{
    const size_t id=info.integer;
    const controls::TreeTable::Node *node=m_table->getNode(id);

    if(m_showSelection && node)
        m_selected->setText("Opened: "+displayName(id));
}

}//namespace gweni
