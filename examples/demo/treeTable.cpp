/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  TreeTableDemo: the tree, its columns and the selection label.
 */
#include "demo/treeTable.h"
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

std::string fileSize(size_t id)
{
    const uint32_t bytes=mix(static_cast<uint32_t>(id)^0x9e3779b9u)%50000000u;
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

    build();
}

void TreeTableDemo::setNodeCount(size_t count)
{
    count=std::max(count, RootCount);
    if(count == m_count)
        return;

    m_count=count;
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

    row->setCellText(0, nodeName(id, folder));
    row->setCellText(1, folder?std::to_string(node->children.size())+" items":fileSize(id));
    row->setCellText(2, modified(id));
}

void TreeTableDemo::onSelectionChange(event::Info)
{
    if(!m_showSelection)
        return;

    const size_t id=m_table->getSelectedNode();
    const controls::TreeTable::Node *node=m_table->getNode(id);

    if(!node)
    {
        m_selected->setText("Selected: none");
        return;
    }
    m_selected->setText("Selected: "+nodeName(id, node->hasChildren));
}

void TreeTableDemo::onNodeActivated(event::Info info)
{
    const size_t id=info.integer;
    const controls::TreeTable::Node *node=m_table->getNode(id);

    if(m_showSelection && node)
        m_selected->setText("Opened: "+nodeName(id, node->hasChildren));
}

}//namespace gweni
