/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/inspector.h>
#include <gweni/tools/toolSupport.h>

#include <gweni/anim.h>
#include <gweni/gweni.h>
#include <gweni/controls/separator.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/skin.h>
#include <gweni/utility.h>

#include <algorithm>

namespace gweni
{
namespace tools
{

namespace
{

//! A node's caption: the control's type, its name if it has one, its id.
String caption(controls::Base *control)
{
    const String &name=control->getName();

    if(name.empty())
        return format("%s #%zu", control->getTypeName(), control->getControlId());
    return format("%s '%s' #%zu", control->getTypeName(), name.c_str(), control->getControlId());
}

String idList(const char *what, const std::vector<size_t> &ids)
{
    // a long list ends in its count, so the line stays one line
    constexpr size_t Shown=12;
    String line=what;

    if(ids.empty())
        return line+" none";
    for(size_t i=0; i < ids.size() && i < Shown; ++i)
        line+=format(" %zu", ids[i]);
    if(ids.size() > Shown)
        line+=format(" ... (%zu)", ids.size());
    return line;
}

const char *yesNo(bool b)
{
    return b?"yes":"no";
}

//! The InspectorNodes directly under node (a placeholder is none).
std::vector<InspectorNode *> childNodes(controls::TreeNode *node)
{
    std::vector<InspectorNode *> nodes;

    for(controls::Base *child : node->GetChildNodes())
    {
        if(InspectorNode *inspectorNode=gweni_cast<InspectorNode>(child))
            nodes.push_back(inspectorNode);
    }
    return nodes;
}

}//namespace

///////////////////////////////////////////////////////////////////////////
// InspectorNode

GWENI_CONTROL_CONSTRUCTOR(InspectorNode)
{}

void InspectorNode::bind(Inspector *inspector, controls::Base *target)
{
    m_inspector=inspector;
    m_target=target;
    setText(caption(target));

    std::vector<controls::Base *> children;

    inspector->childrenOf(target, children);
    // a placeholder node shows the toggle until the node is first opened
    if(!children.empty())
        addNode("...");
}

void InspectorNode::open()
{
    if(!m_populated && m_inspector && m_inspector->onCanvas(m_target))
    {
        m_populated=true;
        m_inspector->deleteChildNodes(this);
        m_inspector->populate(this, m_target, {});
    }
    ParentClass::open();
}

void InspectorNode::setSelected(bool b, bool fireEvents)
{
    ParentClass::setSelected(b, fireEvents);
    if(b && m_inspector && (m_inspector->m_selected != m_target) && m_inspector->onCanvas(m_target))
        m_inspector->select(m_target);
}

///////////////////////////////////////////////////////////////////////////
// Inspector

GWENI_CONTROL_CONSTRUCTOR(Inspector)
{
    setExcludedFromStats(true);
    setTitle("Inspector");
    setSize(560, 400);
    setDeleteOnClose(true);

    m_tree=newChild<controls::TreeControl>();
    m_tree->setDock(DockPosition::Left);
    m_tree->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    m_tree->setWidth(250);
    m_tree->setMargin(Margin(0, 0, 4, 0));

    m_panel=newChild<controls::Base>();
    m_panel->setDock(DockPosition::Center);

    for(int i=0; i < CounterLines; ++i)
    {
        controls::Label *label=m_panel->newChild<controls::Label>();

        label->setDock(DockPosition::Top);
        label->setText(" ");
        m_counters.push_back(label);
    }

    controls::Separator *separator=m_panel->newChild<controls::Separator>();

    separator->setDock(DockPosition::Top);
    separator->setMargin(Margin(0, 4, 0, 4));

    for(int i=0; i < DetailLines; ++i)
    {
        controls::Label *label=m_panel->newChild<controls::Label>();

        label->setDock(DockPosition::Top);
        label->setText(" ");
        m_details.push_back(label);
    }

    // next to the inspector, so it goes wherever the inspector is shown; on the
    // canvas, above everything
    if(controls::Base *parent=getParent())
    {
        m_highlight=parent->newChild<InspectorHighlight>();
        m_highlight->m_inspector=this;
        if(controls::Canvas *canvas=dynamic_cast<controls::Canvas *>(parent))
            canvas->addOverlay(m_highlight);
    }

#if GWENI_ANIMATE
    anim::add(this, new RefreshAnimation<Inspector>());
#endif
}

Inspector::~Inspector()
{
    // a sibling: its parent may be deleting its children, so it goes later
    if(m_highlight)
    {
        m_highlight->m_inspector=nullptr;
        m_highlight->hide();
        m_highlight->delayedDelete();
    }
}

///////////////////////////////////////////////////////////////////////////
// InspectorHighlight

GWENI_CONTROL_CONSTRUCTOR(InspectorHighlight)
{
    setExcludedFromStats(true);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    setTabable(false);
    hide();
}

InspectorHighlight::~InspectorHighlight()
{
    if(m_inspector)
    {
        m_inspector->m_highlight=nullptr;
        m_inspector->m_highlightTarget=nullptr;
    }
}

Inspector *Inspector::find(controls::Canvas *canvas)
{
    if(!canvas)
        return nullptr;
    for(controls::Base *child : canvas->getActualChildren())
    {
        Inspector *inspector=gweni_cast<Inspector>(child);

        // one closed (close() or its button) and left for its delayed delete
        // is gone already
        if(inspector && !canvas->isPendingDelete(inspector))
            return inspector;
    }
    return nullptr;
}

Inspector *Inspector::open(controls::Canvas *canvas)
{
    if(Inspector *inspector=find(canvas))
        return inspector;
    if(!canvas)
        return nullptr;

    Inspector *inspector=canvas->newChild<Inspector>();

    inspector->setPos(std::max(0, canvas->getWidth()-inspector->getWidth()-10), 10);
    inspector->refresh();
    return inspector;
}

void Inspector::close(controls::Canvas *canvas)
{
    if(Inspector *inspector=find(canvas))
    {
        inspector->hide();
        inspector->delayedDelete();
    }
}

bool Inspector::toggle(controls::Canvas *canvas)
{
    if(find(canvas))
    {
        close(canvas);
        return false;
    }
    return open(canvas) != nullptr;
}

void Inspector::setLine(controls::Label *label, const String &text)
{
    if(setLabelText(label, text))
        ++m_textSets;
}

void Inspector::refresh()
{
    if(!m_canvas)
        m_canvas=getCanvas();
    if(!m_canvas)
        return;

    if(!m_treeBuilt || (m_canvas->treeSerial() != m_treeSerial))
    {
        m_treeSerial=m_canvas->treeSerial();
        if(m_selected && !onCanvas(m_selected))
            m_selected=nullptr;
        syncTree();
        m_treeBuilt=true;
    }

    refreshCounters();
    refreshDetails();
    refreshHighlight();
}

void Inspector::refreshHighlight()
{
    if(!m_highlight)
        return;

    // the row under the mouse, if it is one of this inspector's
    controls::Base *target=nullptr;

    for(controls::Base *at=HoveredControl; at; at=at->getParent())
    {
        InspectorNode *node=gweni_cast<InspectorNode>(at);

        if(node && (node->m_inspector == this))
        {
            target=node->target();
            break;
        }
    }
    if(!target)
        target=m_selected;
    // a node's target was on the canvas when the tree was last brought in
    // line; one deleted since moved treeSerial(), which refresh() checked
    if((target != m_highlightTarget) && target && !onCanvas(target))
        target=nullptr;
    m_highlightTarget=target;

    if(!target || !target->visible() || hidden())
    {
        if(!m_highlight->hidden())
            m_highlight->hide();
        return;
    }

    controls::Base *parent=m_highlight->getParent();
    const Point at=parent?parent->canvasPosToLocal(target->localPosToCanvas()):target->localPosToCanvas();
    const int edge=InspectorHighlight::Edge;
    const Rect rect(at.x-edge, at.y-edge, target->getWidth()+2*edge, target->getHeight()+2*edge);
    const Rect &bounds=m_highlight->getBounds();

    if((bounds.x != rect.x) || (bounds.y != rect.y) || (bounds.w != rect.w) || (bounds.h != rect.h))
        m_highlight->setBounds(rect);
    if(m_highlight->hidden())
        m_highlight->show();
}

void Inspector::ownIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const
{
    appendSubtreeIds(this, primitiveIds, textIds);
    if(m_highlight)
        appendSubtreeIds(m_highlight, primitiveIds, textIds);
}

void Inspector::refreshCounters()
{
    const controls::CanvasFrameStats &frame=m_canvas->previousFrameStats();

    setLine(m_counters[UpdateLine], format("update: %zu z visited, %zu z changed, %zu animations",
        frame.update.zVisited, frame.update.zChanged, frame.update.animsRun));
    setLine(m_counters[LayoutLine], format("layout: %d passes%s, %zu entered, %zu measures",
        frame.layout.passes, frame.layout.renderLayout?" (+render phase)":"",
        frame.layout.visited, frame.layout.measureCalls));
    if(frame.layout.redirtiedType)
        setLine(m_counters[RedirtiedLine], format("re-dirtied by %s #%zu", frame.layout.redirtiedType,
            frame.layout.redirtiedId));
    else
        setLine(m_counters[RedirtiedLine], "re-dirtied by none");
    setLine(m_counters[RenderLine], format("render: %zu rendered, %zu changed",
        frame.render.visited, frame.render.controls));
    setLine(m_counters[ChangesLine], format("created %zu, visibility %zu, geometry %zu, background %zu, text %zu",
        frame.render.created, frame.render.visibility, frame.render.geometry, frame.render.background,
        frame.render.text));

    renderer::Base *render=getSkin()?getSkin()->getRenderer():nullptr;
    const AppFrameWrites writes=appFrameWrites(m_canvas, render);

    if(!writes.counted)
    {
        setLine(m_counters[SlotsLine], "slots written: not counted");
        setLine(m_counters[UploadsLine], "uploads: not counted");
        return;
    }
    setLine(m_counters[SlotsLine], format("slots written: %zu primitive, %zu text", writes.primitiveSlots,
        writes.glyphSlots));
    // a frame only the inspector wrote in uploaded nothing of the app's
    setLine(m_counters[UploadsLine], format("uploads: %zu calls, %zu bytes", writes.uploadCalls,
        writes.uploadBytes));
}

void Inspector::refreshDetails()
{
    controls::Base *control=m_selected;

    if(!control)
    {
        setLine(m_details[NameLine], "no control selected");
        for(int i=NameLine+1; i < DetailLines; ++i)
            setLine(m_details[i], " ");
        return;
    }

    const Rect &bounds=control->getBounds();
    const Point onCanvas=control->localPosToCanvas();
    // depthLayers() and overlayLayers() are not const; reading them changes nothing
    skin::Control *skinControl=const_cast<skin::Control *>(control->getSkinControl());
    const size_t layers=skinControl?skinControl->depthLayers():1;
    const size_t overlays=skinControl?skinControl->overlayLayers():0;
    std::vector<size_t> primitives;
    std::vector<size_t> texts;

    if(skinControl)
        skinControl->appendIds(primitives, texts);

    const int pending=control->getStateChange();
    String names;
    const struct { int flag; const char *name; } flags[]=
    {
        {controls::StateChange_Created, "Created"},
        {controls::StateChange_Visibility, "Visibility"},
        {controls::StateChange_Geometry, "Geometry"},
        {controls::StateChange_Background, "Background"},
        {controls::StateChange_Text, "Text"},
        {controls::StateChange_Caret, "Caret"},
    };

    for(const auto &flag : flags)
    {
        if(pending & flag.flag)
            names+=String(names.empty()?"":" ")+flag.name;
    }

    setLine(m_details[NameLine], caption(control));
    setLine(m_details[BoundsLine], format("bounds %d,%d %dx%d, on canvas %d,%d", bounds.x, bounds.y,
        bounds.w, bounds.h, onCanvas.x, onCanvas.y));
    setLine(m_details[ZLine], format("z %d (%zu layers), overlay z %d (%zu layers)", control->getZIndex(),
        layers, control->getOverlayZIndex(), overlays));
    setLine(m_details[PrimitivesLine], idList("primitive ids:", primitives));
    setLine(m_details[TextIdsLine], idList("text ids:", texts));
    setLine(m_details[PendingLine], String("pending: ")+(names.empty()?"none":names));
    setLine(m_details[StateLine], format("hidden %s, visible %s, disabled %s, clips %s", yesNo(control->hidden()),
        yesNo(control->visible()), yesNo(control->isDisabled()), yesNo(control->shouldClip())));
}

void Inspector::select(controls::Base *control)
{
    if(m_selected == control)
        return;
    m_selected=control;
    if(!m_canvas)
        m_canvas=getCanvas();

    InspectorNode *node=control?nodeFor(control):nullptr;

    if(control && !node && m_canvas && onCanvas(control))
    {
        // open the way down to it: its ancestors from the canvas's child on
        std::vector<controls::Base *> path;

        for(controls::Base *at=control->getActualParent(); at && (at != m_canvas); at=at->getActualParent())
            path.push_back(at);
        if(!m_treeBuilt)
        {
            syncTree();
            m_treeSerial=m_canvas->treeSerial();
            m_treeBuilt=true;
        }
        for(auto at=path.rbegin(); at != path.rend(); ++at)
        {
            InspectorNode *ancestor=nodeFor(*at);

            if(!ancestor)
                break;
            if(!ancestor->isOpen())
                ancestor->open();
        }
        node=nodeFor(control);
    }

    if(!node || !node->isSelected())
    {
        m_tree->deselectAll();
        if(node)
            node->setSelected(true);
    }
    if(m_canvas)
        refreshDetails();
}

InspectorNode *Inspector::nodeFor(const controls::Base *control) const
{
    std::vector<controls::TreeNode *> stack={m_tree};

    while(!stack.empty())
    {
        controls::TreeNode *node=stack.back();

        stack.pop_back();
        for(InspectorNode *child : childNodes(node))
        {
            if(child->target() == control)
                return child;
            stack.push_back(child);
        }
    }
    return nullptr;
}

std::vector<String> Inspector::counterLines() const
{
    std::vector<String> lines;

    for(controls::Label *label : m_counters)
        lines.push_back(label->getText());
    return lines;
}

std::vector<String> Inspector::detailLines() const
{
    std::vector<String> lines;

    for(controls::Label *label : m_details)
        lines.push_back(label->getText());
    return lines;
}

void Inspector::childrenOf(const controls::Base *parent, std::vector<controls::Base *> &out) const
{
    out.clear();
    for(controls::Base *child : parent->getActualChildren())
    {
        if((child != this) && (child != m_highlight))
            out.push_back(child);
    }
}

bool Inspector::onCanvas(const controls::Base *control) const
{
    if(!control || !m_canvas)
        return false;
    if(control == m_canvas)
        return true;

    std::vector<const controls::Base *> stack={m_canvas};

    while(!stack.empty())
    {
        const controls::Base *parent=stack.back();

        stack.pop_back();
        for(const controls::Base *child : parent->getActualChildren())
        {
            if(child == control)
                return true;
            stack.push_back(child);
        }
    }
    return false;
}

void Inspector::collectOpen(controls::TreeNode *node, std::vector<const controls::Base *> &out) const
{
    for(InspectorNode *child : childNodes(node))
    {
        if(child->populated() && child->isOpen())
            out.push_back(child->target());
        collectOpen(child, out);
    }
}

void Inspector::populate(controls::TreeNode *node, controls::Base *parent,
    const std::vector<const controls::Base *> &open)
{
    controls::TreeNode *under=node?node:m_tree;
    std::vector<controls::Base *> children;

    childrenOf(parent, children);
    for(controls::Base *child : children)
    {
        // as TreeNode::addNode makes a node, of this type
        InspectorNode *childNode=under->newChild<InspectorNode>();

        childNode->setDock(DockPosition::Top);
        childNode->setRoot(under == m_tree);
        childNode->setTreeControl(m_tree);
        m_tree->onNodeAdded(childNode);
        childNode->bind(this, child);
        if(child == m_selected)
            childNode->ParentClass::setSelected(true);
        if(std::find(open.begin(), open.end(), child) != open.end())
        {
            childNode->m_populated=true;
            deleteChildNodes(childNode);
            populate(childNode, child, open);
            childNode->ParentClass::open();
        }
    }
}

void Inspector::deleteChildNodes(controls::TreeNode *node)
{
    const std::vector<controls::Base *> children(node->GetChildNodes().begin(), node->GetChildNodes().end());

    for(controls::Base *child : children)
    {
        // its slots are cleared in the next flush, under ids it no longer
        // holds: the canvas keeps them as a tool's (Canvas::excludedSlots())
        delete child;
    }
}

void Inspector::syncTree()
{
    std::vector<controls::Base *> wanted;

    childrenOf(m_canvas, wanted);

    std::vector<InspectorNode *> nodes=childNodes(m_tree);
    bool same=(nodes.size() == wanted.size());

    for(size_t i=0; same && i < nodes.size(); ++i)
        same=(nodes[i]->target() == wanted[i]);

    if(same)
    {
        // each target is one of the canvas's children: followed safely
        for(InspectorNode *node : nodes)
            syncChildren(node);
        return;
    }

    std::vector<const controls::Base *> open;

    collectOpen(m_tree, open);
    deleteChildNodes(m_tree);
    populate(nullptr, m_canvas, open);
}

void Inspector::syncChildren(InspectorNode *node)
{
    std::vector<controls::Base *> wanted;

    childrenOf(node->target(), wanted);

    if(!node->populated())
    {
        const bool hasPlaceholder=!node->GetChildNodes().empty();

        if(hasPlaceholder == !wanted.empty())
            return;
        if(wanted.empty())
            deleteChildNodes(node);
        else
            node->addNode("...");
        return;
    }

    std::vector<InspectorNode *> nodes=childNodes(node);
    bool same=(nodes.size() == wanted.size());

    for(size_t i=0; same && i < nodes.size(); ++i)
        same=(nodes[i]->target() == wanted[i]);

    if(same)
    {
        for(InspectorNode *child : nodes)
            syncChildren(child);
        return;
    }

    std::vector<const controls::Base *> open;

    collectOpen(node, open);
    deleteChildNodes(node);
    populate(node, node->target(), open);
}

}//namespace tools
}//namespace gweni
