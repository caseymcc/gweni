/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/skin.h>
#include <gweni/platforms/platform.h>
#include <gweni/dragAndDrop.h>
#include <gweni/inputHandler.h>
#include <gweni/controls/toolTip.h>
#include <gweni/utility.h>
#include <gweni/objectIdBank.h>
#include <algorithm>
#include <limits>
#include <list>
#include <vector>

#if GWENI_ANIMATE
#   include <gweni/anim.h>
#endif

#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <iostream>

namespace gweni
{
namespace controls
{

size_t countParents(Base *base)
{
    Base *parent=base->getParent();
    size_t count=0;

    while(parent)
    {
        count++;
        parent=parent->getParent();
    }
    return count;
}

std::string spaceByParent(Base *base)
{
    size_t parents=countParents(base);
    std::string spaces;

    for(size_t i=0; i<parents; ++i)
        spaces+="  ";
    
    return spaces;
}

size_t getControlType(std::string name)
{
    static std::vector<std::string> controlTypes;

    for(size_t i=0; i<controlTypes.size(); ++i)
    {
        if(controlTypes[i] == name)
            return i;
    }

    controlTypes.push_back(name);
    return controlTypes.size()-1;
}

std::string getFullName(Base *base)
{
    std::string name=(!base->getName().empty())?base->getName():base->getTypeName();
    controls::Base *parent=base->getParent();

    while(parent)
    {
        std::string parentName=(!parent->getName().empty())?parent->getName():parent->getTypeName();

        parentName+="/"+name;
        name=parentName;
        parent=parent->getParent();
    }

    return name;
}

ObjectIdBank<Base *, nullptr> g_controls;

namespace
{
// Open RenderWalkScopes, and the control whose hooks the drain is running in
// the innermost one (the render phase is single-threaded).
int g_renderWalkDepth=0;
Base *g_renderingControl=nullptr;
// Tree changes made inside a RenderWalkScope: a drain that sees this move
// rebuilds its ancestor stack, which the change may have made stale.
unsigned g_renderWalkTreeChanges=0;

// The pending-render queue: every control with a state change the render
// phase has not rendered yet, in the order they were queued, each at its
// m_renderQueueIndex. A deleted control leaves a null slot, which the next
// drain compacts away. One UI thread, as the id banks.
std::vector<Base *> g_renderQueue;
// Slots ~Base nulled since the last drain compacted the queue.
size_t g_renderQueueHoles=0;
// A drain is running: a canvas rendered from a hook drains nothing.
bool g_draining=false;

// The (operation, type) pairs reported, each with its record entry.
std::map<std::pair<std::string, std::string>, std::string> &renderWalkMutationRecord()
{
    static std::map<std::pair<std::string, std::string>, std::string> record;
    return record;
}

void reportRenderWalkMutation(Base *control, const char *operation)
{
    const char *type=control->getTypeName();
    std::string &entry=renderWalkMutationRecord()[{operation, type}];
    if(!entry.empty())
        return;

    const char *renderingType=g_renderingControl?g_renderingControl->getTypeName():"none";
    entry=std::string(type)+" "+operation+" (rendering "+renderingType+")";

    std::cerr<<"gweni: "<<type<<"#"<<control->getControlId()<<" "<<operation
        <<" during the render walk (rendering "<<renderingType;
    if(g_renderingControl)
        std::cerr<<"#"<<g_renderingControl->getControlId();
    std::cerr<<")\n";
}

// At the top of every tree change: one branch outside the walk.
inline void noteRenderWalkMutation(Base *control, const char *operation)
{
    if(g_renderWalkDepth != 0)
    {
        ++g_renderWalkTreeChanges;
        reportRenderWalkMutation(control, operation);
    }
}
}//namespace

RenderWalkScope::RenderWalkScope()
{
    ++g_renderWalkDepth;
}

RenderWalkScope::~RenderWalkScope()
{
    --g_renderWalkDepth;
}

std::vector<std::string> renderWalkMutations()
{
    std::set<std::string> entries;
    for(const auto &reported : renderWalkMutationRecord())
        entries.insert(reported.second);
    return std::vector<std::string>(entries.begin(), entries.end());
}

void resetRenderWalkMutations()
{
    renderWalkMutationRecord().clear();
}

size_t queuedRenderCount()
{
    return g_renderQueue.size()-std::count(g_renderQueue.begin(), g_renderQueue.end(), nullptr);
}

void Base::appendToRenderQueue()
{
    m_renderQueueIndex=(int)g_renderQueue.size();
    g_renderQueue.push_back(this);
}

Base::Base(const String &name)
{

    m_id=g_controls.getNextId(this);

    m_parent=nullptr;
    m_actualParent=nullptr;
    m_ancestorHidden=false;
    m_skin=nullptr;

    m_alignment=Alignment::Absolute;

    init(name);

    addStateChange(StateChange_Created);
}

void Base::init(const String &name)
{
    m_updatePrimitives=true;

    m_stateChange=StateChange_Nothing;
    m_sizeFlags={SizeFlag::Expand, SizeFlag::Expand};
    m_minimumSize={0, 0};
    m_maximumSize={std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    m_layoutItem=nullptr;
//    m_parent=nullptr;
//    m_actualParent=nullptr;
    m_innerPanel=nullptr;
//    m_skin=nullptr;
    setName(name);
//    setParent(parent);
    m_hidden=false;
    // not numbered yet: renumberZRecursive() gives it its z before it is drawn
    m_zIndex=0;
    m_bounds=Rect(0, 0, 10, 10);
    m_requestedBounds=m_bounds;
    m_widthRequested=false;
    m_heightRequested=false;
    m_padding=Padding(0, 0, 0, 0);
    m_margin=Margin(0, 0, 0, 0);
    m_dock=DockPosition::None;
    m_dragAndDrop_Package=nullptr;
    restrictToParent(false);
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(false);
    invalidate();
    setCursor(cursor::Normal);
    setToolTip(nullptr);
    setTabable(false);
    setShouldDrawBackground(true);
    m_disabled=false;
    m_cacheTextureDirty=true;
    m_cacheToTexture=false;
    m_includeInSize=true;
    m_updateLayout=true;
}

namespace
{
// Set while a control of a setExcludedFromStats() subtree is deleted, around
// its children's deletion: they are the subtree's too, though the root has
// stopped being one as its ~Base began.
size_t g_excludedDeleting=0;
}//namespace

Base::~Base()
{
    noteRenderWalkMutation(this, "~Base");

    Canvas *canvas=getCanvas();
    // before the flag goes: its ids and its subtree's were a tool's, and the
    // canvas's tools leave their cleared slots out (Canvas::excludedSlots())
    const bool excludedRelease=canvas && ((g_excludedDeleting != 0) || statsExcluded());

    if(excludedRelease && m_skinControl)
        canvas->excludedIdsReleased(m_skinControl.get());
    // no longer a root statsExcluded() looks for
    setExcludedFromStats(false);

    {
        // Its setToolTip() control is not its child, so it is shown in a
        // canvas's ToolTip box: that canvas deletes it, whether or not this
        // control is still on it.
        Canvas *shownOn=nullptr;

        if(m_toolTip && (m_toolTip->getParent() != this))
        {
            shownOn=m_toolTip->getCanvas();
            m_toolTip=nullptr;
        }

        // off the queue of the canvas that queued it, whatever canvas (or
        // none) it is on now
        if(m_deleteQueuedOn)
            m_deleteQueuedOn->PreDeleteCanvas(this);

        if(canvas)
        {
            canvas->toolTipControlDeleted(this);
            canvas->popupControlDeleted(this);
            canvas->menuAnchorDeleted(this);
            canvas->overlayDeleted(this);
        }

        // after the canvas has closed it: a menu this control owns goes too
        dropContextMenu();
        // and so do its popups, on their own canvas, wherever this control is
        dropOwnedPopups();

        if(shownOn && (shownOn != canvas))
            shownOn->toolTipControlDeleted(this);
    }

    if(excludedRelease)
        ++g_excludedDeleting;

    Base::List::iterator iter=m_children.begin();

    while(iter != m_children.end())
    {
        Base *child=*iter;
        iter=m_children.erase(iter);
        delete child;
    }
    if(excludedRelease)
        --g_excludedDeleting;

    for(AccelMap::iterator accelIt=m_accelerators.begin();
        accelIt != m_accelerators.end();
        ++accelIt)
    {
        delete accelIt->second;
    }

    m_accelerators.clear();
    setParent(nullptr);

    if(HoveredControl == this)
        HoveredControl=nullptr;

    if(KeyboardFocus == this)
        KeyboardFocus=nullptr;

    if(MouseFocus == this)
        MouseFocus=nullptr;

    draganddrop::controlDeleted(this);
#if GWENI_ANIMATE
    anim::cancel(this);
#endif

    if(m_dragAndDrop_Package)
    {
        delete m_dragAndDrop_Package;
        m_dragAndDrop_Package=nullptr;
    }

    removeSkinControl();

    // Last, after everything above that may have queued it: the queue never
    // hands out a deleted control, and a tree change reported from here on
    // does not name it.
    if(m_renderQueueIndex >= 0)
    {
        g_renderQueue[m_renderQueueIndex]=nullptr;
        ++g_renderQueueHoles;
    }
    if(g_renderingControl == this)
        g_renderingControl=nullptr;
}

namespace
{
// Set while setBounds() invalidates for its own sake: the control's children,
// and the control itself once its size changed. Those are placement, which a
// running layout pass either has still to reach or is reported by setBounds.
bool g_placement=false;
// Set while invalidate() walks up to the canvas, so only its first call, the
// control that asked, is looked at.
bool g_propagating=false;
// Controls flagged with setExcludedFromStats(true): while there are none,
// statsExcluded() looks nothing up.
size_t g_statsExcludedRoots=0;
// Those controls (Base::statsExcludedRoots()).
std::vector<Base *> g_statsExcludedRootList;
// The invalidation now going up started outside every flagged subtree.
bool g_invalidationCounted=true;
// Open ExcludedFromStatsScopes: what is marked counts as a flagged subtree's.
size_t g_excludedScopes=0;
// The counted invalidate() calls since the process started (invalidateCalls()).
size_t g_invalidateCalls=0;
// Set while a walk invalidates a whole subtree (invalidateChildren(true),
// Base::scaleChangedSubtree), with the control it started from: an
// invalidate() the walk makes on a control under that root does not walk up
// to the canvas, since the walk itself enters every ancestor it has in the
// subtree; the way up out of the subtree is taken once, for the walk.
bool g_subtreeWalk=false;
Base *g_subtreeWalkRoot=nullptr;

struct SubtreeWalk
{
    explicit SubtreeWalk(Base *root): m_outer(g_subtreeWalk), m_outerRoot(g_subtreeWalkRoot)
    {
        g_subtreeWalk=true;
        g_subtreeWalkRoot=root;
    }
    ~SubtreeWalk()
    {
        g_subtreeWalk=m_outer;
        g_subtreeWalkRoot=m_outerRoot;
    }
    SubtreeWalk(const SubtreeWalk &)=delete;
    SubtreeWalk &operator=(const SubtreeWalk &)=delete;

    bool m_outer;
    Base *m_outerRoot;
};

// Whether this invalidate() of `control` is one a subtree walk makes under its
// root. Read where it is needed rather than kept across invalidate()'s calls:
// outside a walk it is one load and a branch.
inline bool walkedBy(const Base *control)
{
    return g_subtreeWalk && !g_propagating && (control != g_subtreeWalkRoot);
}

// Set while a part is made during a layout pass (Base::PartScope): its
// constructor's requests are not counted as layout code's.
bool g_makingPart=false;

// The parts of the bounds the next setBounds() call requests (see
// Base::setBounds): setPos/moveTo ask for the position, setSize for the size.
enum RequestPart
{
    RequestX=1,
    RequestY=2,
    RequestWidth=4,
    RequestHeight=8,
    RequestPosition=RequestX|RequestY,
    RequestSize=RequestWidth|RequestHeight,
    RequestAll=RequestPosition|RequestSize
};
int g_requestParts=RequestAll;

struct RequestScope
{
    explicit RequestScope(int parts): m_outer(g_requestParts) { g_requestParts=parts; }
    ~RequestScope() { g_requestParts=m_outer; }
    int m_outer;
};

}

Base::PlacementScope::PlacementScope(): m_outer(g_placement)
{
    g_placement=true;
}

Base::PlacementScope::~PlacementScope()
{
    g_placement=m_outer;
}

Base::PartScope::PartScope(): m_outer(g_makingPart)
{
    g_makingPart=true;
}

Base::PartScope::~PartScope()
{
    g_makingPart=m_outer;
}

bool Base::laidOutThisPass(Dim dim) const
{
    const unsigned pass=(dim == Dim::X)?m_layoutPassX:m_layoutPassY;

    return Canvas::s_layingOut && pass == Canvas::s_passSerial;
}

void Base::stampLayoutPass(Dim dim)
{
    if(!Canvas::s_layingOut)
        return;

    if(dim == Dim::X)
        m_layoutPassX=Canvas::s_passSerial;
    else
        m_layoutPassY=Canvas::s_passSerial;
}

void Base::stampLayoutPass()
{
    stampLayoutPass(Dim::X);
    stampLayoutPass(Dim::Y);
}

// The control stays dirty for the next pass, whatever this one still does to
// it (see doArrange), and the canvas runs that pass.
void Base::noteRedirty()
{
    m_redirtiedPass=Canvas::s_passSerial;
    Canvas::s_layingOut->layoutInvalidated(this);
}

bool Base::inLayoutPass()
{
    return Canvas::s_layingOut != nullptr;
}

// Out of line: the walks that mark controls dirty (invalidate() up to the
// canvas, setBounds) pay for a test, not for a push's registers, on a control
// already in its parent's list.
#if defined(__GNUC__) || defined(__clang__)
#define GWENI_LAYOUT_NOINLINE __attribute__((noinline))
#else
#define GWENI_LAYOUT_NOINLINE
#endif

// At one in ChangedShare of the children the walks take them all
// (walkChangedChildren),
// so the list stops there: the parent's next Y arrange looks at every child
// instead (doArrange), as a first frame or a resize does.
GWENI_LAYOUT_NOINLINE void Base::enlistLayoutChanged()
{
    Base *parent=m_actualParent;

    if(!parent || parent->m_changedOverflow || parent->m_children.size() < MinChangedWalk)
        return;

    if((parent->m_changedChildren.size()+1)*ChangedShare >= parent->m_children.size())
    {
        parent->m_changedOverflow=true;
        return;
    }

    m_changedIndex=static_cast<int>(parent->m_changedChildren.size());
    parent->m_changedChildren.push_back(this);
}

// Once per count: the term the parent holds is the one its inputs give now.
// A control measured since the count may hold another preferred size than
// the one counted, so then it is not known, and the next count is full.
GWENI_LAYOUT_NOINLINE void Base::noteSizeInputsChanging()
{
    Base *parent=m_actualParent;

    if(!parent)
        return;

    for(size_t axis=0; axis < 2; ++axis)
    {
        ChildSizes &sizes=parent->m_childSizes[axis];

        if(!sizes.valid || m_termSeq[axis] > parent->m_countSeq[axis])
            continue;

        if(m_measureSeq[axis] > parent->m_countSeq[axis])
            sizes.valid=false;
        else
        {
            m_sizeTerm[axis]=computeSizeTerm(parent, axis == 0 ? Dim::X : Dim::Y);
            m_termSeq[axis]=++s_measureSeq;
        }
    }
    noteLayoutChanged();
}

bool Base::countedSizeTerm(const Base *child, Dim dim, SizeTerm &term)
{
    const size_t axis=static_cast<size_t>(dim);

    if(child->m_termSeq[axis] > m_countSeq[axis])
    {
        term=child->m_sizeTerm[axis];
        return true;
    }
    if(child->m_measureSeq[axis] > m_countSeq[axis])
        return false;

    term=child->computeSizeTerm(this, dim);
    return true;
}

void Base::dropLayoutChanged()
{
    if(m_changedIndex < 0)
        return;

    if(m_actualParent)
        m_actualParent->m_changedChildren[static_cast<size_t>(m_changedIndex)]=nullptr;
    m_changedIndex=-1;
}

// Not counted by its new parent yet: its term there is none.
void Base::noteAdded()
{
    m_sizeTerm[0]=SizeTerm();
    m_sizeTerm[1]=SizeTerm();
    m_termSeq[0]=m_termSeq[1]=++s_measureSeq;
    m_parentKeepsChanged=(m_actualParent->m_children.size() >= MinChangedWalk);
    noteLayoutChanged();
}

void Base::noteChildLeaving(Base *child)
{
    child->dropLayoutChanged();
    child->m_parentKeepsChanged=false;

    for(size_t axis=0; axis < 2; ++axis)
    {
        const Dim dim=(axis == 0) ? Dim::X : Dim::Y;
        SizeTerm term;

        if(!m_childSizes[axis].valid)
            continue;

        if(countedSizeTerm(child, dim, term))
            removeSizeTerm(dim, term);
        else
            m_childSizes[axis].valid=false;
    }

    if(child->getDock() != DockPosition::None)
        m_arrangeAll[0]=m_arrangeAll[1]=true;
}

// Below MinChangedWalk children none was put in the list (noteLayoutChanged):
// reaching it, the list is not every changed child yet, so it is taken as
// stopped until the next Y arrange looks at them all.
void Base::notedChildCount()
{
    if(m_children.size() != MinChangedWalk)
        return;

    m_changedOverflow=true;
    for(Base *child:m_children)
        child->m_parentKeepsChanged=true;
}

void Base::noteReordered()
{
    if(m_actualParent && getDock() != DockPosition::None)
        m_actualParent->m_arrangeAll[0]=m_actualParent->m_arrangeAll[1]=true;
}

bool Base::changedChildDocked() const
{
    for(const Base *child:m_changedChildren)
    {
        if(child && child->getDock() != DockPosition::None)
            return true;
    }
    return false;
}

// The same branches, in the same order, as sizeOfChildren's loop.
Base::SizeTerm Base::computeSizeTerm(Base *parent, Dim dim) const
{
    SizeTerm term;

    if(hidden() || !shouldIncludeInSize())
        return term;

    const DockPosition dock=getDock();
    const Size preferred=getPreferredSize();
    const Margin &margin=getMargin();

    if(dim == Dim::X)
    {
        term.value=preferred.width+margin.left+margin.right;

        if((dock & DockPosition::Top) || (dock & DockPosition::Bottom))
            term.kind=MaxTerm;
        else if((dock & DockPosition::Left) || (dock & DockPosition::Right))
            term.kind=DockTerm;
        else if(dock & DockPosition::Center)
            term.kind=InnerTerm;
        else
        {
            // Where the child asked to be, not where the last pass put it.
            if(!parent->hasAlignment())
                term.value+=m_requestedBounds.x;
            term.kind=MaxTerm;
        }
    }
    else
    {
        term.value=preferred.height+margin.top+margin.bottom;

        if((dock & DockPosition::Top) || (dock & DockPosition::Bottom))
            term.kind=DockTerm;
        else if((dock & DockPosition::Left) || (dock & DockPosition::Right))
            term.kind=MaxTerm;
        else if(dock & DockPosition::Center)
            term.kind=InnerTerm;
        else
        {
            if(!parent->hasAlignment())
                term.value+=m_requestedBounds.y;
            term.kind=MaxTerm;
        }
    }
    return term;
}

void Base::addSizeTerm(Dim dim, const SizeTerm &term)
{
    ChildSizes &sizes=m_childSizes[static_cast<size_t>(dim)];

    switch(term.kind)
    {
    case MaxTerm:
        if(sizes.maxCount == 0 || term.value > sizes.max)
        {
            sizes.max=term.value;
            sizes.maxCount=1;
        }
        else if(term.value == sizes.max)
            ++sizes.maxCount;
        break;
    case DockTerm:
        sizes.dockSum+=term.value;
        break;
    case InnerTerm:
        sizes.innerSum+=term.value;
        break;
    case NoTerm:
        break;
    }
}

// Taking away the last child holding the largest term leaves the largest of
// the others unknown: the next count is a full one.
void Base::removeSizeTerm(Dim dim, const SizeTerm &term)
{
    ChildSizes &sizes=m_childSizes[static_cast<size_t>(dim)];

    switch(term.kind)
    {
    case MaxTerm:
        if(term.value > sizes.max || sizes.maxCount == 0)
            sizes.valid=false;
        else if(term.value == sizes.max && --sizes.maxCount == 0)
            sizes.valid=false;
        break;
    case DockTerm:
        sizes.dockSum-=term.value;
        break;
    case InnerTerm:
        sizes.innerSum-=term.value;
        break;
    case NoTerm:
        break;
    }
}

// The new term goes in before the old one comes out, so a holder of the
// largest term that keeps it, or grows it, leaves the count valid.
void Base::recountSizeTerm(Dim dim, const SizeTerm &counted, const SizeTerm &term)
{
    if(term.kind == counted.kind && term.value == counted.value)
        return;

    addSizeTerm(dim, term);
    removeSizeTerm(dim, counted);
}

inline int Base::childSizesContent(Dim dim) const
{
    const ChildSizes &sizes=m_childSizes[static_cast<size_t>(dim)];
    const int largest=(sizes.maxCount > 0) ? std::max(0, sizes.max) : 0;

    return std::max(largest, sizes.dockSum+sizes.innerSum);
}

// During a layout pass (Canvas::layoutPass) a layout invalidation either feeds
// the pass or re-dirties it. Placement feeds it: the pass reaches the control
// later and lays it out with its new bounds. Anything else raised during the
// pass (setHidden, setDock, an explicit invalidate() from a layout() or an
// onChildBoundsChanged()) may change what a control already laid out read, so
// it re-dirties and the canvas runs another pass. A resize re-dirties only when
// it hits a control this pass has already laid out; setBounds reports that.
void Base::invalidate(int state)
{
    // where the invalidation started decides, all the way up, whether the
    // entries it causes are counted (layoutEntryCounted)
    if(!g_propagating)
    {
        g_invalidationCounted=(g_statsExcludedRoots == 0) || ((g_excludedScopes == 0) && !statsExcluded());

        // the hidden controls it is under may be measured anyway; a shown
        // one has no width kept (m_hiddenWidthCurrent). In a walk each hidden
        // ancestor in the subtree clears its own, and the walk clears the
        // root's way up.
        if(!s_boundsInvalidation && !visible())
        {
            if(walkedBy(this))
                m_hiddenWidthCurrent=false;
            else
            {
                for(Base *control=this; control && !control->visible();
                    control=control->m_actualParent?control->m_actualParent:control->m_parent)
                    control->m_hiddenWidthCurrent=false;
            }
        }
    }

    // This control, then (unless a subtree walk entered it) its logical
    // parents up to the canvas, each marked as it is: a loop rather than each
    // calling invalidate() on its parent.
    const bool outer=g_propagating;
    const bool layout=(state & StateChange_Created) || (state & StateChange_Visibility) ||
        (state & StateChange_Geometry);

    for(Base *control=this; control; control=control->m_parent)
    {
        // a control with no parent that is not a canvas marks only itself:
        // one being made (Base's constructor), or the root of a detached
        // subtree
        if(g_invalidationCounted && (control->m_parent || (control->getCanvas() == control)))
            ++g_invalidateCalls;

        if(layout)
        {
            control->m_needsLayout=true;
            // its parent places it again; a change to what the parent counts
            // it by (its margin, dock, visibility, preferred size or request)
            // marks its term stale where it is made
            control->markLayoutDirty();
            if(g_invalidationCounted)
                control->m_layoutDirtyCounted=true;

            // A pass walks the children a control actually holds: a child of
            // a control with an inner panel is held by the panel, which the
            // invalidation of the logical parent does not reach.
            for(Base *holder=control->m_actualParent; holder && holder != control->m_parent;
                holder=holder->m_actualParent)
            {
                holder->markLayoutDirty();
                if(g_invalidationCounted)
                    holder->m_layoutDirtyCounted=true;
            }

            if(Canvas::s_layingOut && !g_propagating && !g_placement)
                control->noteRedirty();
        }

        control->m_cacheTextureDirty=true;

        if(control == this)
        {
            // entered by a subtree walk, under its root: the walk invalidates
            // the ancestors this control has in the subtree itself, and the
            // way up from the subtree once (see SubtreeWalk)
            if(walkedBy(this))
                break;
            g_propagating=true;
        }
    }
    g_propagating=outer;
}

void Base::delayedDelete()
{
    if(m_deleteQueuedOn)
        return;

    Canvas *canvas=getCanvas();

    if(!canvas)
    {
        delete this;
        return;
    }
    canvas->addDelayedDelete(this);
}

Canvas *Base::getCanvas()
{
    Base *canvas=m_parent;

    if(!canvas)
        return nullptr;

    return canvas->getCanvas();
}

void Base::redraw()
{
    updateColors();
    m_cacheTextureDirty=true;
    addStateChange(StateChange_Background);

    // One hop to the canvas instead of walking the ancestors: their colors do
    // not depend on this control's state, and re-running updateColors() on
    // each of them dirtied their text whenever a descendant was hovered.
    if(Canvas *canvas=getCanvas())
        canvas->redraw();
}

void Base::setParent(Base *parent)
{
    if(m_parent == parent)
        return;

    if(m_parent)
        m_parent->removeChild(this);

    m_parent=parent;
    m_actualParent=nullptr;

    if(m_parent)
    {
        m_parent->addChild(this);
    }
}

void Base::setDock(DockPosition dock)
{
    if(m_dock == dock)
        return;

    noteSizeInputsChanging();
    m_dock=dock;
    // docked or not, it moved the children after it, or did
    if(m_actualParent)
        m_actualParent->m_arrangeAll[0]=m_actualParent->m_arrangeAll[1]=true;
    invalidate();
    invalidateParent();
    invalidateParentReaders();
}

DockPosition Base::getDock() const
{
    return m_dock;
}

Alignment Base::getAlignment()
{
    return m_alignment;
}

void Base::setAlignment(Alignment alignment)
{
    if(alignment == m_alignment)
        return;

    m_alignment=alignment;
    invalidate();
}

SizeFlags Base::getSizeFlags()
{
    return m_sizeFlags;
}

void Base::setSizeFlags(SizeFlags sizeFlags)
{
    if((m_sizeFlags.horizontal==sizeFlags.horizontal)&&
        (m_sizeFlags.vertical==sizeFlags.vertical))
        return;

    m_sizeFlags=sizeFlags;
    invalidate();
    invalidateParent();
}

void Base::setLayout(LayoutItem *layoutItem)
{
    if(m_innerPanel)
    {
        m_innerPanel->setLayout(layoutItem);
        return;
    }

    //library owns items so we need to delete
    if(m_layoutItem!=nullptr)
        delete m_layoutItem;

    m_layoutItem=layoutItem;
    m_layoutItem->setParent(this);

    for(Base *child:m_children)
        m_layoutItem->addControl(child);
}

LayoutItem *Base::getLayout()
{
    if(m_innerPanel)
        return m_innerPanel->getLayout();
    return m_layoutItem;
}

bool Base::hidden() const
{
    return m_hidden;
}

bool Base::visible() const
{
    return !m_hidden && !m_ancestorHidden;
}

void Base::setHidden(bool hidden)
{
    if(m_hidden == hidden)
        return;

    noteSizeInputsChanging();
    m_hidden=hidden;
    if(visible())
        m_hiddenWidthCurrent=false;

    if(m_parent)
        m_parent->onChildHiddenChanged(this);

    // Under a hidden ancestor it is not shown either way: only its own flag
    // changes, and nothing below it.
    if(!m_ancestorHidden)
    {
        addStateChange(StateChange_Visibility);

        for(auto &&child : m_children)
            child->setAncestorHidden(hidden);
    }

    invalidate();
    redraw();
}

void Base::setAncestorHidden(bool ancestorHidden)
{
    if(m_ancestorHidden == ancestorHidden)
        return;

    m_ancestorHidden=ancestorHidden;

    // It hid itself: neither it nor anything under it changes.
    if(m_hidden)
        return;
    if(!ancestorHidden)
        m_hiddenWidthCurrent=false;

    addStateChange(StateChange_Visibility);

    for(auto &&child : m_children)
        child->setAncestorHidden(ancestorHidden);
}

void Base::invalidateChildren(bool recursive)
{
    if(!recursive)
    {
        for(auto &&child : m_children)
            child->invalidate();

        // the children it hands out (getChildren()) are the inner panel's
        if(m_innerPanel)
        {
            for(auto &&innerchild : m_innerPanel->getChildren())
                innerchild->invalidate();
        }
        return;
    }

    if(m_children.empty() && !(m_innerPanel && !m_innerPanel->getChildren().empty()))
        return;

    // One child with nothing under it (a Label's Text, resized with it): its
    // own invalidate() is the whole walk, the way up included.
    if(!m_innerPanel && (m_children.size() == 1))
    {
        Base *child=m_children.front();

        if(child->m_children.empty() && !child->m_innerPanel)
        {
            child->invalidate();
            return;
        }
    }

    // A hidden control under it clears its own kept width, and every hidden
    // control above it up to this one clears its own; past this one, the
    // walk clears the way up once.
    if(!s_boundsInvalidation && !visible())
    {
        for(Base *control=this; control && !control->visible();
            control=control->m_actualParent?control->m_actualParent:control->m_parent)
            control->m_hiddenWidthCurrent=false;
    }

    // Each control under it is entered once and does not walk up to the
    // canvas: the walk enters every ancestor a control has in the subtree.
    // What lies above is invalidated once, when the walk is done: the logical
    // parents outside the subtree that its controls have (this control, or
    // for an inner panel its owner), each walking up to the canvas as an
    // invalidation started by one of the controls would.
    WalkExits exits;

    exits.rootControl=this;

    {
        SubtreeWalk walk(this);

        invalidateSubtree(1, exits);
    }

    const bool outerCounted=g_invalidationCounted;
    const bool outerPropagating=g_propagating;

    g_propagating=true;
    if(exits.root)
    {
        g_invalidationCounted=exits.rootCounted;
        invalidate(StateChange_Geometry);
    }
    for(size_t i=0; i<exits.count; ++i)
    {
        g_invalidationCounted=exits.exits[i].counted;
        exits.exits[i].parent->invalidate(StateChange_Geometry);
    }
    g_propagating=outerPropagating;
    g_invalidationCounted=outerCounted;
}

void Base::WalkExits::add(Base *parent, bool counted)
{
    if(parent == rootControl)
    {
        root=true;
        rootCounted=rootCounted || counted;
        return;
    }

    for(size_t i=0; i<count; ++i)
    {
        if(exits[i].parent == parent)
        {
            exits[i].counted=exits[i].counted || counted;
            return;
        }
    }

    if(count < Capacity)
    {
        exits[count++]={parent, counted};
        return;
    }

    const bool outerCounted=g_invalidationCounted;
    const bool outerPropagating=g_propagating;

    g_invalidationCounted=counted;
    g_propagating=true;
    parent->invalidate(StateChange_Geometry);
    g_propagating=outerPropagating;
    g_invalidationCounted=outerCounted;
}

void Base::invalidateSubtree(int depth, WalkExits &exits)
{
    auto enter=[&](Base *control)
    {
        control->invalidate();

        // Its logical parent is outside the subtree when it is the root or
        // above: its actual parent at depth 1, or for a control an inner
        // panel holds, its holder's owner as far up as the root.
        Base *parent=control->m_parent;

        if(parent == this && depth == 1)
        {
            exits.root=true;
            exits.rootCounted=exits.rootCounted || g_invalidationCounted;
        }
        else if(parent && ((depth == 1) || (parent != control->m_actualParent)))
        {
            int up=1;
            for(Base *holder=control->m_actualParent; holder && holder != parent; holder=holder->m_actualParent)
                ++up;

            if(up >= depth)
                exits.add(parent, g_invalidationCounted);
        }

        if(!control->m_children.empty() || control->m_innerPanel)
            control->invalidateSubtree(depth+1, exits);
    };

    for(auto &&child : m_children)
        enter(child);

    // The children it hands out (getChildren()) are the inner panel's: the
    // loop above has been through them when the panel is one of its own
    // children, as every inner panel is today. Walking them again doubled the
    // walk at each level of nested panels.
    if(m_innerPanel && m_innerPanel->m_actualParent != this)
    {
        for(auto &&innerchild : m_innerPanel->getChildren())
            enter(innerchild);
    }
}

void Base::invalidateParentReaders()
{
    for(auto &&child : m_children)
    {
        if(child->readsParentLayout())
            child->invalidate();
    }
}

void Base::sendToBack()
{
    noteRenderWalkMutation(this, "sendToBack");

    if(!m_actualParent)
        return;

    if(m_actualParent->m_children.front() == this)
        return;

    m_actualParent->m_children.remove(this);
    m_actualParent->m_children.push_front(this);
    zOrderMoved(this);
    noteReordered();
    // the holder docks its children in this order: re-lay it, not only the
    // logical parent (a control with an inner panel holds them in the panel)
    m_actualParent->invalidate();
}

void Base::bringToFront()
{
    noteRenderWalkMutation(this, "bringToFront");

    if(!m_actualParent)
        return;

    if(m_actualParent->m_children.back() == this)
        return;

    m_actualParent->m_children.remove(this);
    m_actualParent->m_children.push_back(this);
    zOrderMoved(this);
    noteReordered();
    m_actualParent->invalidate();
    redraw();
}

controls::Base *Base::findChildByName(const String &name, bool recursive)
{
    for(auto &&child : m_children)
    {
        if(!child->getName().empty() && child->getName() == name)
            return child;

        if(recursive)
        {
            controls::Base *subChild=child->findChildByName(name, true);

            if(subChild)
                return subChild;
        }
    }

    return nullptr;
}

void Base::bringNextToControl(controls::Base *child, bool bBehind)
{
    noteRenderWalkMutation(this, "bringNextToControl");

    if(!m_actualParent)
        return;

    m_actualParent->m_children.remove(this);
    Base::List::iterator it=std::find(
        m_actualParent->m_children.begin(), m_actualParent->m_children.end(), child);

    if(it == m_actualParent->m_children.end())
        return bringToFront();

    if(bBehind)
    {
        ++it;

        if(it == m_actualParent->m_children.end())
            return bringToFront();
    }

    m_actualParent->m_children.insert(it, this);
    zOrderMoved(this);
    noteReordered();
    m_actualParent->invalidate();
}

void Base::addChild(Base *child)
{
    if(m_innerPanel)
    {
        m_innerPanel->addChild(child);
        return;
    }

    // the control whose list changes, not one forwarding to its inner panel
    noteRenderWalkMutation(this, "addChild");
    m_children.push_back(child);
    notedChildCount();

    if(m_layoutItem)
        m_layoutItem->addControl(child);

    onChildAdded(child);
    child->m_actualParent=this;
    child->noteAdded();
    // Not visible under a hidden parent, e.g. content built into a PageControl
    // page that is hidden before its content is added; its own flag is kept.
    child->setAncestorHidden(!visible());
    // new, or moved here from another place: its old labels mean nothing here
    child->m_zMoved=true;
    child->markZDirty();
    zOrderChanged();
}

void Base::addChildOfLayout(Base *child)
{
    noteRenderWalkMutation(this, "addChildOfLayout");
    m_children.push_back(child);
    notedChildCount();

    onChildAdded(child);
    child->m_actualParent=this;
    child->noteAdded();
    child->setAncestorHidden(!visible());
    // new, or moved here from another place: its old labels mean nothing here
    child->m_zMoved=true;
    child->markZDirty();
    zOrderChanged();
}

void Base::removeChild(Base *child)
{
    noteRenderWalkMutation(this, "removeChild");

    // If we removed our innerpanel, remove our pointer to it
    if(m_innerPanel == child)
        m_innerPanel=nullptr;

    if(m_innerPanel)
        m_innerPanel->removeChild(child);

    if(m_layoutItem)
        m_layoutItem->removeControl(child);

    m_children.remove(child);

    // not in our list any more, so its changes must not mark us (nor reach
    // through a stale pointer once we are deleted)
    if(child->m_actualParent == this)
    {
        noteChildLeaving(child);
        child->m_actualParent=nullptr;
        // On its own it is shown unless it hid itself; a new parent sets this
        // again as it is attached.
        child->setAncestorHidden(false);
    }

    zChildRemoved();
    onChildRemoved(child);
}

void Base::addStateChangeRecursive(int stateChange)
{
    for(Base *child:m_children)
    {
        child->addStateChangeRecursive(stateChange);
    }

    addStateChange(stateChange);
}

void Base::removeAllChildren()
{
    while(m_children.size() > 0)
    {
        removeChild(*m_children.begin());
    }
}

unsigned int Base::numChildren()
{
    // Include m_innerPanel's children here?
    return static_cast<unsigned int>(m_children.size());
}

controls::Base *Base::getChild(unsigned int i)
{
    if(i >= numChildren())
        return nullptr;

    for(auto &&child : m_children)
    {
        if(i == 0)
            return child;

        i--;
    }

    // Should never happen.
    return nullptr;
}

void Base::onChildAdded(Base * /*child*/)
{
    invalidate();
}

void Base::onChildRemoved(Base * /*child*/)
{
    invalidate();
}

skin::Base *Base::getSkin(void)
{
    if(m_skin)
        return m_skin;

    if(m_parent)
        return m_parent->getSkin();

    GWENI_ASSERT_MSG(false, "Base::GetSkin Returning null!");
    return nullptr;
}

void Base::moveBy(int x, int y)
{
    moveTo(getX()+x, getY()+y);
}

void Base::moveTo(int x, int y)
{
    const Point pos=restrictedPos(x, y);

    RequestScope request(RequestPosition);
    setBounds(pos.x, pos.y, getWidth(), getHeight());
}

Point Base::restrictedPos(int x, int y) const
{
    if(m_restrictToParent && getParent())
    {
        const Base *parent=getParent();

        if(x-getPadding().left < parent->getMargin().left)
            x=parent->getMargin().left+getPadding().left;

        if(y-getPadding().top < parent->getMargin().top)
            y=parent->getMargin().top+getPadding().top;

        if(x+getWidth()+getPadding().right > parent->getWidth()-parent->getMargin().right)
            x=parent->getWidth()-parent->getMargin().right-getWidth()-getPadding().right;

        if(y+getHeight()+getPadding().bottom > parent->getHeight()-parent->getMargin().bottom)
            y=parent->getHeight()-parent->getMargin().bottom-getHeight()-getPadding().bottom;
    }

    return Point(x, y);
}

void Base::setPos(int x, int y)
{
    RequestScope request(RequestPosition);
    setBounds(x, y, getWidth(), getHeight());
}

void Base::setWidth(int w)
{
    RequestScope request(RequestWidth);
    setBounds(getX(), getY(), w, getHeight());
}

void Base::setHeight(int h)
{
    RequestScope request(RequestHeight);
    setBounds(getX(), getY(), getWidth(), h);
}

bool Base::setSize(int w, int h)
{
    RequestScope request(RequestSize);
    return setBounds(getX(), getY(), w, h);
}

bool Base::setSize(const Point &p)
{
    return setSize(p.x, p.y);
}

bool Base::setBounds(const Rect &bounds)
{
    // Taken once: any setBounds() the code below makes is a call of its own.
    const int parts=g_requestParts;
    g_requestParts=RequestAll;

    if(parts)
    {
        // Only the app asks; layout code computes (setArrangedBounds,
        // placeChild). A request made while a pass runs is counted.
        if(Canvas::s_layingOut && !g_makingPart)
            Canvas::s_layingOut->layoutRequested(this);

        const Rect requested=m_requestedBounds;

        // the requested position is in the term its parent counts for it
        if(((parts & RequestX) && bounds.x != requested.x) || ((parts & RequestY) && bounds.y != requested.y))
            noteSizeInputsChanging();

        if(parts & RequestX)
            m_requestedBounds.x=bounds.x;
        if(parts & RequestY)
            m_requestedBounds.y=bounds.y;
        if(parts & RequestWidth)
            m_requestedBounds.w=bounds.w;
        if(parts & RequestHeight)
            m_requestedBounds.h=bounds.h;

        const bool newRequest=((parts & RequestWidth) && !m_widthRequested) ||
            ((parts & RequestHeight) && !m_heightRequested);

        m_widthRequested=m_widthRequested || (parts & RequestWidth);
        m_heightRequested=m_heightRequested || (parts & RequestHeight);

        // The request is what its parent places it by, in a pass or not.
        if(newRequest || !(requested == m_requestedBounds))
            noteLayoutChanged();

        // A new request is laid out again: the requested size feeds the rules
        // (dockNoneSize, layoutCellSize) and the position counts in the
        // parent's preferred size, whether or not the bounds already match.
        // From inside a pass it is layout code putting a control where it
        // already is (TabStrip sizing itself, say): no re-dirty.
        if(!inLayoutPass() && (newRequest || !(requested == m_requestedBounds)))
        {
            invalidate();

            if(newRequest || requested.w != m_requestedBounds.w || requested.h != m_requestedBounds.h)
                invalidateParentReaders();
        }
    }

    if(m_bounds == bounds)
        return false;

    // A width is read from the X arrange on, a height from the Y arrange on
    // (layout() runs again there if the height changed, see Base::arrange).
    if((m_bounds.w != bounds.w && laidOutThisPass(Dim::X)) ||
        (m_bounds.h != bounds.h && laidOutThisPass(Dim::Y)))
        noteRedirty();

    // A new size is what the subtree is laid out in. A move is not: children
    // are placed relative to it, and onBoundsChanged marks the subtree for
    // the render walk (absolute positions, clip boxes); the move is laid out
    // as a request of it or as its parent's placement, below.
    if(m_bounds.w != bounds.w || m_bounds.h != bounds.h)
    {
        PlacementScope placement;
        BoundsInvalidationScope boundsOnly;
        invalidateChildren(true);
        // its content box is another along that axis: every child is
        // placed again there
        if(m_bounds.w != bounds.w)
            m_arrangeAll[0]=true;
        if(m_bounds.h != bounds.h)
            m_arrangeAll[1]=true;
    }

    // The pass arranges it again at its new bounds, and its own layout() runs
    // again: one may correct where its parent put it (Menu::layout keeps the
    // menu inside the canvas). The way down to it is marked, not invalidated:
    // no layout() of an ancestor reads it, and the parent placing it in a
    // pass is on that way already.
    m_needsLayout=true;

    const bool counted=(g_statsExcludedRoots == 0) || !statsExcluded();

    for(Base *control=this; control;
        control=control->m_actualParent?control->m_actualParent:control->m_parent)
    {
        control->markLayoutDirty();
        if(counted)
            control->m_layoutDirtyCounted=true;
    }

    const Rect oldBounds=getBounds();
    m_bounds=bounds;
    onBoundsChanged(oldBounds);

    addStateChange(StateChange_Geometry);

    return true;
}

bool Base::setBounds(int x, int y, int w, int h)
{
    return setBounds(Rect(x, y, w, h));
}

bool Base::setArrangedBounds(int x, int y, int w, int h)
{
    RequestScope request(0);
    return setBounds(x, y, w, h);
}

bool Base::placeChild(Base *child, const Rect &bounds)
{
    RequestScope request(0);
    PlacementScope placement;
    return child->setBounds(bounds);
}

bool Base::placeChild(Base *child, int x, int y, int w, int h)
{
    return placeChild(child, Rect(x, y, w, h));
}

bool Base::placeChildPos(Base *child, int x, int y)
{
    return placeChild(child, Rect(x, y, child->getWidth(), child->getHeight()));
}

bool Base::placeChildSize(Base *child, int w, int h)
{
    return placeChild(child, Rect(child->getX(), child->getY(), w, h));
}

void Base::setPlacement(Placement placement)
{
    if(m_placement == placement)
        return;

    // The holder's size and arrange read it: the control holding this one,
    // which for a child of a control with an inner panel is the panel.
    noteSizeInputsChanging();
    m_placement=placement;
    if(m_actualParent && getDock() != DockPosition::None)
        m_actualParent->m_arrangeAll[0]=m_actualParent->m_arrangeAll[1]=true;
    if(m_actualParent)
        m_actualParent->invalidate();
}

Size Base::getMinimumSize()
{
    Size minimumSize=m_minimumSize;

    if(m_sizeFlags.horizontal==SizeFlag::Fixed)
        minimumSize.width=m_requestedBounds.w;
    else if((m_sizeFlags.horizontal == SizeFlag::Expand) || (m_sizeFlags.horizontal == SizeFlag::Bloat))
        minimumSize.width=std::max(minimumSize.width, m_preferredSize.width);

    if(m_sizeFlags.vertical==SizeFlag::Fixed)
        minimumSize.height=m_requestedBounds.h;
    else if((m_sizeFlags.vertical==SizeFlag::Expand)||(m_sizeFlags.vertical==SizeFlag::Bloat))
        minimumSize.height=std::max(minimumSize.height, m_preferredSize.height);

    return minimumSize;
}

Size Base::getMaximumSize()
{
    Size maximumSize=m_maximumSize;

    if(m_sizeFlags.horizontal==SizeFlag::Fixed)
        maximumSize.width=m_requestedBounds.w;
    else if(m_sizeFlags.horizontal == SizeFlag::Shrink)
        maximumSize.width=std::max(maximumSize.width, m_preferredSize.width);

    if(m_sizeFlags.vertical==SizeFlag::Fixed)
        maximumSize.height=m_requestedBounds.h;
    else if(m_sizeFlags.vertical == SizeFlag::Shrink)
        maximumSize.height=std::max(maximumSize.height, m_preferredSize.height);

    return maximumSize;
}

// The render drain's helpers, inlined into each of its loops (GCC and Clang
// leave a function or lambda called from several places out of line).
#if defined(__GNUC__) || defined(__clang__)
#define GWENI_RENDER_INLINE __attribute__((always_inline))
#else
#define GWENI_RENDER_INLINE
#endif

namespace
{
// renderer::Base::addClipRegion's intersection: \p clip narrowed to a w x h
// rect at \p at; no overlap gives an empty region at the overlap's corner.
GWENI_RENDER_INLINE inline
Rect clipRegionAt(const Rect &clip, const Point &at, int w, int h)
{
    const int x0=std::max(at.x, clip.x);
    const int y0=std::max(at.y, clip.y);
    const int x1=std::min(at.x+w, clip.x+clip.w);
    const int y1=std::min(at.y+h, clip.y+clip.h);

    return Rect(x0, y0, std::max(0, x1-x0), std::max(0, y1-y0));
}

// What the render drain sets on the renderer for a control: the render
// offset and clip region, in the units of the space the transform maps to
// device px.
struct RenderState
{
    Point offset;
    Rect clip;
    renderer::RenderTransform transform;
};

// The clip region of a space a control's children enter (a ZoomPanel's): as
// good as none, since the region they were entered from is the transform's
// device clip, and small enough that a corner of it in device px fits an int
// at any scale.
constexpr int SpaceClipReach=1<<24;

// \p inner, the state \p control's children render in so far, moved into the
// space of their own the control gives them (\p at: its origin in the space it
// renders in): the region they were entered from in device px, cut to the one
// that space was entered from, becomes the transform's device clip.
void enterChildSpace(const Base *control, const Point &at, RenderState &inner)
{
    const renderer::RenderTransform outer=inner.transform;
    const Rect &clip=inner.clip;
    int x0=outer.deviceX(float(clip.x));
    int y0=outer.deviceY(float(clip.y));
    int x1=outer.deviceX(float(clip.x+clip.w));
    int y1=outer.deviceY(float(clip.y+clip.h));

    if(outer.bounded)
    {
        x0=std::max(x0, outer.deviceClip.x);
        y0=std::max(y0, outer.deviceClip.y);
        x1=std::min(x1, outer.deviceClip.x+outer.deviceClip.w);
        y1=std::min(y1, outer.deviceClip.y+outer.deviceClip.h);
    }

    control->childRenderTransform(outer, at, inner.transform);
    inner.transform.bounded=true;
    inner.transform.deviceClip=Rect(x0, y0, std::max(0, x1-x0), std::max(0, y1-y0));
    inner.offset=Point(0, 0);
    inner.clip=Rect(-SpaceClipReach, -SpaceClipReach, 2*SpaceClipReach, 2*SpaceClipReach);
}

// The state \p control's children render in, \p control rendering in \p
// state: its bounds' origin added, its bounds clipped to if it clips, and the
// space it gives them if it has one of its own.
GWENI_RENDER_INLINE inline
void innerRenderState(Base *control, const RenderState &state, RenderState &inner)
{
    const Rect &bounds=control->getBounds();
    const Point at(state.offset.x+bounds.x, state.offset.y+bounds.y);

    inner.clip=control->shouldClip()?clipRegionAt(state.clip, at, bounds.w, bounds.h):state.clip;
    inner.offset=at;
    inner.transform=state.transform;
    if(control->transformsChildren())
        enterChildSpace(control, at, inner);
}

// The clip region \p child of \p parent renders in, \p parent's children's
// being (offset, clip): narrowed by the parent's getChildClipRect() for it.
GWENI_RENDER_INLINE inline
void childRenderState(const Base *parent, const Base *child, const Point &offset, Rect &clip)
{
    if(const Rect *childClip=parent->getChildClipRect(child))
        clip=clipRegionAt(clip, Point(offset.x+childClip->x, offset.y+childClip->y), childClip->w,
            childClip->h);
}

// One axis of the clip region a control's children are stamped with, worked
// out up its ancestor chain in the coordinates of the ancestor reached. Each
// clipRegionAt() of the drain keeps, on each axis, the largest lower edge and
// the smallest upper edge it was given (an empty region at the largest lower
// edge), so the order the ancestors narrow it in does not matter: \p low and
// \p high are those edges so far, \p at the control's own lower edge, and
// the control's two sizes give its two upper edges.
struct ClipAxis
{
    long long low;
    long long high;
    long long at;
    int oldSize;
    int newSize;

    void narrow(long long from, long long to)
    {
        low=std::max(low, from);
        high=std::min(high, to);
    }

    void shift(int by)
    {
        low+=by;
        high+=by;
        at+=by;
    }

    // Whether both sizes give the region the same extent on this axis, and
    // will whatever the ancestors further up narrow it to: the upper edge so
    // far is at or before the nearer of the control's two (it cuts both), or
    // the farther one, cut, is at or before the lower edge (both empty).
    bool same() const
    {
        if(oldSize == newSize)
            return true;

        const long long lower=std::max(low, at);
        const long long nearer=at+std::min(oldSize, newSize);
        const long long farther=at+std::max(oldSize, newSize);

        return (high <= nearer) || (std::min(high, farther) <= lower);
    }
};
}//namespace

bool Base::childRegionChanged(const Rect &oldBounds)
{
    // every absolute position below it moved
    if((m_bounds.x != oldBounds.x) || (m_bounds.y != oldBounds.y))
        return true;
    if((m_bounds.w == oldBounds.w) && (m_bounds.h == oldBounds.h))
        return false;
    // its children's clip rects follow its size
    if(hasChildClipRects())
        return true;
    // they render in its own region
    if(!shouldClip())
        return false;

    // The region it renders in narrowed as the render drain narrows it
    // (innerRenderState, childRenderState), from its parent up, stopping as
    // soon as both sizes give the same region: no ancestor further up can
    // make them differ again. Its canvas's children render in the canvas's
    // bounds; off any canvas, a region not settled by then is not known.
    const long long unbounded=std::numeric_limits<long long>::max()/4;
    ClipAxis x{-unbounded, unbounded, m_bounds.x, oldBounds.w, m_bounds.w};
    ClipAxis y{-unbounded, unbounded, m_bounds.y, oldBounds.h, m_bounds.h};
    const Base *child=this;

    for(Base *parent=m_actualParent; parent; child=parent, parent=parent->m_actualParent)
    {
        // its children's region is in device px from there up (enterChildSpace)
        if(parent->transformsChildren())
            return true;
        if(const Rect *childClip=parent->getChildClipRect(child))
        {
            x.narrow(childClip->x, static_cast<long long>(childClip->x)+childClip->w);
            y.narrow(childClip->y, static_cast<long long>(childClip->y)+childClip->h);
        }
        if(parent->shouldClip() || (!parent->m_actualParent && parent->getCanvas() == parent))
        {
            x.narrow(0, parent->m_bounds.w);
            y.narrow(0, parent->m_bounds.h);
        }
        if(x.same() && y.same())
            return false;
        x.shift(parent->m_bounds.x);
        y.shift(parent->m_bounds.y);
    }
    return true;
}

void Base::onBoundsChanged(Rect oldBounds)
{
    // Anything that needs to update on size changes
    // Iterate my children and tell them I've changed
    //
    if(getParent())
        getParent()->onChildBoundsChanged(oldBounds, this);

    if(m_bounds.w != oldBounds.w || m_bounds.h != oldBounds.h)
    {
        PlacementScope placement;
        BoundsInvalidationScope boundsOnly;
        invalidate();
    }

    // The subtree is rendered again where its absolute positions or the
    // region it is clipped to changed. A new size alone changes neither: what
    // the arrange moves or resizes below is marked by its own setBounds. In a
    // top-down arrange the region this control is in is already the new one:
    // an ancestor whose region changed has marked this subtree, or will.
    // A control with no children has nothing below it to mark, and no
    // region to work out.
    if(!m_children.empty() && childRegionChanged(oldBounds))
        addStateChangeRecursive(StateChange_Geometry);

//    redraw();
    updateRenderBounds();
}

void Base::scaleChangedSubtree()
{
    SubtreeWalk walk(this);

    onScaleChanged();
}

// A new canvas scale (Canvas::setScale): every control is laid out again and
// re-emits its primitives, and the clip boxes they carry, at the new scale, by
// one state change each. A subclass that overrides this calls it too.
void Base::onScaleChanged()
{
    invalidate();
    addStateChange(StateChange_Geometry);

    for(auto &&child : m_children)
    {
        child->onScaleChanged();
    }
}

void Base::onChildBoundsChanged(Rect /*oldChildBounds*/, Base * /*child*/)
{}

void Base::setPrimitiveIdsSize(size_t size)
{
    //enlargePrimitiveIds(this, m_primitiveIds, size);
}

static ChangeStats g_changeStats;

const ChangeStats &getChangeStats()
{
    return g_changeStats;
}

void resetChangeStats()
{
    g_changeStats=ChangeStats();
}

// The types reported by setSkin(), each printed once when it was added.
static std::set<std::string> &typesReportedWithoutRenderer()
{
    static std::set<std::string> types;
    return types;
}

std::vector<std::string> typesWithoutSkinRenderer()
{
    const std::set<std::string> &types=typesReportedWithoutRenderer();
    return std::vector<std::string>(types.begin(), types.end());
}

static size_t g_measureTextCalls=0;


ExcludedFromStatsScope::ExcludedFromStatsScope(bool active): m_active(active)
{
    if(m_active)
        ++g_excludedScopes;
}

ExcludedFromStatsScope::~ExcludedFromStatsScope()
{
    if(m_active)
        --g_excludedScopes;
}

void Base::setExcludedFromStats(bool exclude)
{
    if(m_excludeFromStats == exclude)
        return;
    m_excludeFromStats=exclude;
    if(exclude)
    {
        ++g_statsExcludedRoots;
        g_statsExcludedRootList.push_back(this);
    }
    else
    {
        --g_statsExcludedRoots;
        g_statsExcludedRootList.erase(std::find(g_statsExcludedRootList.begin(),
            g_statsExcludedRootList.end(), this));
    }
    // what a tool counts of the tree (StatsOverlay's controls) changed with
    // it, and which ids are the tools'
    if(Canvas *canvas=getCanvas())
    {
        ++canvas->m_treeSerial;
        canvas->m_excludedSlotsValid=false;
    }
}

const std::vector<Base *> &Base::statsExcludedRoots()
{
    return g_statsExcludedRootList;
}

bool Base::layoutEntryCounted() const
{
    return (g_statsExcludedRoots == 0) || (m_layoutDirtyCounted && !statsExcluded());
}

void Base::markZDirty()
{
    // a mark from a flagged subtree makes the way up dirty but not counted; a
    // counted one goes on up through what is dirty uncounted
    const bool counted=(g_statsExcludedRoots == 0) || ((g_excludedScopes == 0) && !statsExcluded());

    for(Base *control=this; control; control=control->m_actualParent)
    {
        if(control->m_zDirty && (!counted || control->m_zDirtyCounted))
            break;
        control->m_zDirty=true;
        if(counted)
            control->m_zDirtyCounted=true;
    }
}

bool Base::statsExcluded() const
{
    if(g_statsExcludedRoots == 0)
        return false;

    for(const Base *control=this; control; control=control->m_parent)
    {
        if(control->m_excludeFromStats)
            return true;
    }
    return false;
}

size_t measureTextCalls()
{
    return g_measureTextCalls;
}

size_t invalidateCalls()
{
    return g_invalidateCalls;
}

gweni::Point Base::measureText(const gweni::Font &font, const gweni::String &text)
{
    if(!statsExcluded())
        ++g_measureTextCalls;
    return getSkin()->getRenderer()->measureText(font, text);
}

int Base::measureLineHeight(const gweni::Font &font)
{
    if(!statsExcluded())
        ++g_measureTextCalls;
    return getSkin()->getRenderer()->lineHeight(font);
}

void Base::measureCarets(const gweni::Font &font, const gweni::String &text, std::vector<int> &xs)
{
    if(!statsExcluded())
        ++g_measureTextCalls;
    getSkin()->getRenderer()->measureCarets(font, text, xs);
}

void Base::render(skin::Base *skin)
{
//    update();
    const int stateChange=getStateChange();

    if(stateChange == StateChange_Nothing)
        return;

    //the one choke point every changed control passes through, so the whole
    //frame's change count is these few increments
    if(!statsExcluded())
    {
        ++g_changeStats.controls;
        if(stateChange & StateChange_Created) ++g_changeStats.created;
        if(stateChange & StateChange_Visibility) ++g_changeStats.visibility;
        if(stateChange & StateChange_Geometry) ++g_changeStats.geometry;
        if(stateChange & StateChange_Background) ++g_changeStats.background;
        if(stateChange & StateChange_Text) ++g_changeStats.text;
    }
    else
        ++g_changeStats.excluded;

    if(!m_skinControl)
        return;

    if(getStateChange() & StateChange_Created)
    {
        m_skinControl->generate(m_skin->getRenderer(), this);
#ifdef GWENI_DEBUG_LOGGING
        std::string name=getFullName(this);
        const std::vector<size_t> &primitives=m_skinControl->getPrimitives();
        std::string primitiveList;

        for(size_t i=0; i<primitives.size(); i++)
        {
            if(i == 0)
                primitiveList+=std::to_string(primitives[i]);
            else
                primitiveList+=", "+std::to_string(primitives[i]);
        }
        std::cout<<"Primitives: "<<name<<" ("<<primitiveList<<")\n";
#endif//GWENI_DEBUG_LOGGING
    }

#ifdef GWENI_DEBUG_LOGGING
    if(getStateChange() != StateChange_Text)
    {
        const Rect &bounds=getBounds();
        renderer::Base *baseRenderer=skin->getRenderer();
        gweni::Point clipPt=baseRenderer->getRenderOffset();
        std::string show=hidden()?"hide":"show";

        std::string name=getFullName(this);
        std::cout<<"Updating: "<<show<<" ("<<bounds.x+clipPt.x<<", "<<bounds.y+clipPt.y<<", w:"<<bounds.w<<", h:"<<bounds.h<<") "<<name<<"\n";
    }
#endif//GWENI_DEBUG_LOGGING

    m_skinControl->update(m_skin->getRenderer(), this);

}

void Base::removeSkinControl()
{
    if(m_skinControl)
    {
        // a new skin for a tool's control: its old ids were the tool's (a
        // deleted control reported them as its ~Base began)
        if((g_statsExcludedRoots != 0) && statsExcluded())
        {
            if(Canvas *canvas=getCanvas())
                canvas->excludedIdsReleased(m_skinControl.get());
        }
        m_skinControl->remove(m_skin->getRenderer(), this);
        m_skinControl=nullptr;
        zOrderChanged();
    }
}

bool Base::placeZRange(ZNumbering &numbering, int &z, int layers, bool take)
{
    int placed=z;
    int gap=0;

    if(take || numbering.full || (z < numbering.cursor))
    {
        placed=numbering.cursor;
        // Packed when a kept label may follow (a short cascade); past every
        // kept label nothing follows, so the gap costs nothing there but range.
        if(numbering.full)
            gap=numbering.gap;
        else if(numbering.cursor > numbering.high)
            gap=(z != 0)?numbering.movedTailGap:numbering.tailGap;

        if(placed+layers-1 > numbering.clampTo)
        {
            placed=std::max(1, numbering.clampTo-layers+1);
            numbering.clamped=true;
        }
    }
    numbering.cursor=placed+layers+gap;
    numbering.maxUsed=std::max(numbering.maxUsed, placed+layers-1);

    const bool changed=(placed != z);
    z=placed;
    return changed;
}

void Base::renumberZRecursive(ZNumbering &numbering, bool moved)
{
    // a setExcludedFromStats() subtree is numbered like any other, but counted
    // apart (Canvas::renumberZOrder)
    if(m_excludeFromStats)
        ++numbering.excludedDepth;
    // entered only on the way to a flagged subtree's change: not counted
    const bool uncountedPath=m_zDirty && !m_zDirtyCounted && !moved && !m_zMoved;
    if(numbering.excludedDepth || uncountedPath)
        ++numbering.excludedVisited;
    else
        ++numbering.stats->zVisited;
    m_zDirtyCounted=false;
    // a moved control's subtree is out of order wherever it kept its labels
    moved=moved || m_zMoved;
    m_zMoved=false;
    m_zDirty=false;

    const int layers=m_skinControl?(int)m_skinControl->depthLayers():1;
    bool changed=placeZRange(numbering, m_zIndex, layers, moved);

    for(auto &&child:m_children)
    {
        // Nothing changed in it since it was numbered, so its ranges are in
        // order and, its first still past the cursor, it keeps every label: a
        // pass that entered it would leave the cursor at its end.
        if(numbering.skipClean && !moved && !child->m_zDirty
            && (child->m_zIndex >= numbering.cursor))
        {
            numbering.cursor=child->m_zEnd;
            numbering.maxUsed=std::max(numbering.maxUsed, child->m_zEnd-1);
            continue;
        }
        child->renumberZRecursive(numbering, moved);
    }

    const int overlay=m_skinControl?(int)m_skinControl->overlayLayers():0;

    if(overlay)
        changed|=placeZRange(numbering, m_overlayZIndex, overlay, moved);
    else if(m_overlayZIndex != 0)
    {
        m_overlayZIndex=0;
        changed=true;
    }

    m_zEnd=zSubtreeEnd();

    if(changed)
    {
        if(!numbering.excludedDepth)
            ++numbering.stats->zChanged;
        addStateChange(StateChange_Geometry);
    }

    if(m_excludeFromStats)
        --numbering.excludedDepth;
}

int Base::zSubtreeEnd() const
{
    const int overlay=m_skinControl?(int)m_skinControl->overlayLayers():0;

    if(overlay)
        return m_overlayZIndex+overlay;
    if(!m_children.empty())
        return m_children.back()->m_zEnd;
    return m_zIndex+(m_skinControl?(int)m_skinControl->depthLayers():1);
}

void Base::countZRanges(size_t &layers, size_t &ranges) const
{
    layers+=m_skinControl?m_skinControl->depthLayers():1;
    ++ranges;

    for(const Base *child:m_children)
        child->countZRanges(layers, ranges);

    if(const size_t overlay=m_skinControl?m_skinControl->overlayLayers():0)
    {
        layers+=overlay;
        ++ranges;
    }
}

void Base::zOrderMoved(Base *control)
{
    control->m_zMoved=true;
    control->zOrderChanged();
}

void Base::zOrderChanged()
{
    markZDirty();

    // A control not on a canvas yet is numbered once it is: attaching it is an
    // addChild() on the canvas's tree, which reports there.
    if(Canvas *canvas=getCanvas())
    {
        canvas->m_zOrderDirty=true;
        // a control (or a skin Control) taken into, out of or onto a flagged
        // subtree: the tools' held ids are walked again at the next read
        canvas->m_excludedSlotsValid=false;
        if(!statsExcluded())
            ++canvas->m_treeSerial;
    }
}

void Base::zChildRemoved()
{
    // A dirty control is entered by the next renumber, which sets its end, and
    // its ancestors are dirty too.
    for(Base *control=this; control && !control->m_zDirty; control=control->m_actualParent)
    {
        const int end=control->zSubtreeEnd();

        if(end == control->m_zEnd)
            break;
        control->m_zEnd=end;
    }

    if(Canvas *canvas=getCanvas())
    {
        canvas->m_zOrderDirty=true;
        // a control (or a skin Control) taken into, out of or onto a flagged
        // subtree: the tools' held ids are walked again at the next read
        canvas->m_excludedSlotsValid=false;
        if(!statsExcluded())
            ++canvas->m_treeSerial;
    }
}

namespace
{
// One level of the drain's ancestor stack: a control of the drained tree, and
// the render offset and clip region its children render in (before a
// getChildClipRect() of its own narrows one child's). The control is only
// compared, never dereferenced: a hook may have deleted it.
struct RenderLevel
{
    Base *control;
    RenderState state;
};

// A queue slot to render and its z, for the sort.
struct RenderOrder
{
    int z;
    unsigned slot;

    bool operator<(const RenderOrder &other) const
    {
        return (z != other.z)?(z < other.z):(slot < other.slot);
    }
};

// Kept from frame to frame, so the render phase allocates nothing once they
// have grown.
std::vector<RenderLevel> g_renderStack;
std::vector<Base *> g_renderChain;
std::vector<RenderOrder> g_renderOrder;
std::vector<RenderOrder> g_renderOrderScratch;

// Entries queued during a drain (by a hook, on another control) that it
// renders after the sorted ones, at least: the rest waits for the next frame,
// so a hook that keeps queueing cannot keep the drain going.
constexpr size_t MinQueuedDuringDrain=256;

// Sorts \p order, built in slot order, by z and then slot: nothing to do if it
// is in z order already (a whole tree queued by a layout pass mostly is), else
// a stable radix sort on the bytes z uses, which keeps the slot order of ties.
void sortRenderOrder(std::vector<RenderOrder> &order)
{
    bool sorted=true;
    bool negative=false;
    unsigned highest=0;

    for(size_t i=0; i < order.size(); ++i)
    {
        const int z=order[i].z;

        if(z < 0)
            negative=true;
        else
            highest=std::max(highest, (unsigned)z);
        if((i > 0) && (z < order[i-1].z))
            sorted=false;
    }

    if(sorted)
        return;
    if(negative)
    {
        std::sort(order.begin(), order.end());
        return;
    }

    std::vector<RenderOrder> &scratch=g_renderOrderScratch;
    scratch.resize(order.size());

    for(unsigned shift=0; (shift < 32) && ((highest>>shift) != 0); shift+=8)
    {
        size_t counts[256]={};

        for(const RenderOrder &entry : order)
            ++counts[((unsigned)entry.z>>shift)&0xff];

        size_t at=0;
        for(size_t &count : counts)
        {
            const size_t n=count;
            count=at;
            at+=n;
        }

        for(const RenderOrder &entry : order)
            scratch[counts[((unsigned)entry.z>>shift)&0xff]++]=entry;
        order.swap(scratch);
    }
}
}//namespace

void Base::drainRenderQueue(skin::Base *skin, bool zTied, size_t treeSize)
{
    if(g_draining)
        return;
    g_draining=true;
    // a tree change from a hook is reported, and seen by this drain
    RenderWalkScope walk;

    renderer::Base *baseRenderer=skin->getRenderer();
    std::vector<Base *> &queue=g_renderQueue;

    // Drops the slots of deleted (and rendered) controls; the indices move.
    const auto compact=[&queue]()
        {
            size_t kept=0;

            for(Base *control : queue)
            {
                if(!control)
                    continue;
                control->m_renderQueueIndex=(int)kept;
                queue[kept++]=control;
            }
            queue.resize(kept);
            g_renderQueueHoles=0;
        };
    if(g_renderQueueHoles != 0)
        compact();

    const size_t queuedBefore=queue.size();
    const size_t appendCap=std::max(MinQueuedDuringDrain, queuedBefore);

    // This root's own state, as the canvas set it on the renderer.
    const RenderState root{baseRenderer->getRenderOffset(), baseRenderer->clipRegion(),
        baseRenderer->renderTransform()};
    unsigned treeChanges=g_renderWalkTreeChanges;

    // Runs the hooks of \p control, queued in \p slot, in \p state, and
    // clears its state; each hook but the first only if the ones before did not
    // delete the control (~Base nulls its slot). Returns whether it is still
    // there.
    const auto renderQueued=[&](Base *control, size_t slot, const RenderState &state)
        GWENI_RENDER_INLINE
        {
            const Point &offset=state.offset;
            const Rect &clip=state.clip;

            if(!control->statsExcluded())
                ++g_changeStats.visited;
            g_renderingControl=control;

            baseRenderer->setRenderTransform(state.transform);
            baseRenderer->setRenderOffset(offset);
            baseRenderer->setClipRegion(clip);
            control->renderUnder(skin);
            if(queue[slot] == control)
                control->render(skin);

            if(queue[slot] == control)
            {
                // its own offset, its parent's region
                baseRenderer->setRenderOffset(offset);
                baseRenderer->addRenderOffset(control->getBounds());
                baseRenderer->setClipRegion(clip);
                baseRenderer->startClip();
                control->renderOver(skin);
                if(queue[slot] == control)
                    control->renderFocus(skin);
                baseRenderer->endClip();
            }

            g_renderingControl=nullptr;

            if(queue[slot] != control)
                return false;

            // after the hooks: a change it raised on itself is cleared too
            control->m_stateChange=StateChange_Nothing;
            control->m_renderQueueIndex=-1;
            queue[slot]=nullptr;
            return true;
        };

    // A change over much of the tree (the first frame, a resize, a scale) is
    // rendered by one walk of it, in preorder, as the recursive walk did: no
    // sort, no ancestor chains. So is every frame of a tree whose z ties. It
    // stops at a tree change made by a hook, without touching the lists it
    // was in, and leaves the rest to the queue's order below.
    // the queue's slots below queuedBefore the walk emptied
    size_t walked=0;

    if(zTied || ((treeSize != 0) && (queuedBefore*4 >= treeSize)))
    {
        bool stopped=false;
        const auto walkTree=[&](const auto &self, Base *control, const RenderState &state) -> void
            {
                const int slot=control->m_renderQueueIndex;

                if(slot >= 0)
                {
                    if((size_t)slot < queuedBefore)
                        ++walked;
                    if(control->m_stateChange == StateChange_Nothing)
                    {
                        control->m_renderQueueIndex=-1;
                        queue[slot]=nullptr;
                    }
                    else if(!renderQueued(control, (size_t)slot, state)
                        || (treeChanges != g_renderWalkTreeChanges))
                    {
                        stopped=true;
                        return;
                    }
                }

                if(control->m_children.empty())
                    return;

                RenderState inner;

                innerRenderState(control, state, inner);
                for(Base *child : control->m_children)
                {
                    // a copy only for a child clipped to a rect of its own
                    // (childRenderState)
                    if(const Rect *childClip=control->getChildClipRect(child))
                    {
                        RenderState forChild=inner;

                        forChild.clip=clipRegionAt(inner.clip, Point(inner.offset.x+childClip->x,
                            inner.offset.y+childClip->y), childClip->w, childClip->h);
                        self(self, child, forChild);
                    }
                    else
                        self(self, child, inner);
                    // before the loop steps on through a list a hook changed
                    if(stopped)
                        return;
                }
            };
        walkTree(walkTree, this, root);
        treeChanges=g_renderWalkTreeChanges;
        // stopped: what it had not reached yet goes through the queue's order
        if(stopped)
            walked=0;
    }

    // The queue's order for what is left: z is the tree's preorder (a parent
    // before its children, a child before the next one's subtree), strictly
    // increasing unless the renumber clamped. Entries off this tree sort in
    // among them and are passed over below.
    std::vector<RenderOrder> &order=g_renderOrder;
    order.clear();
    if(walked < queuedBefore)
    {
        for(size_t slot=0; slot < queuedBefore; ++slot)
        {
            if(Base *control=queue[slot])
                order.push_back({control->getZIndex(), (unsigned)slot});
        }
        sortRenderOrder(order);
    }

    // The stack's levels are stack[0, depth): the vector only grows.
    std::vector<RenderLevel> &stack=g_renderStack;
    std::vector<Base *> &chain=g_renderChain;
    size_t depth=0;

    // Pushes \p control, rendering in \p state, with the state its children
    // render in.
    const auto pushLevel=[&](Base *control, const RenderState &state)
        {
            if(depth == stack.size())
                stack.emplace_back();

            RenderLevel &level=stack[depth];

            level.control=control;
            innerRenderState(control, state, level.state);
            control->m_renderStackLevel=(int)depth;
            ++depth;
        };
    // The state \p control renders in, its parent at the top of the stack.
    const auto stateUnderTop=[&](Base *control, RenderState &state)
        {
            const RenderLevel &top=stack[depth-1];

            state=top.state;
            childRenderState(control->m_actualParent, control, state.offset, state.clip);
        };
    // The stack back to this root alone.
    const auto resetStack=[&]()
        {
            depth=0;
            pushLevel(this, root);
        };
    resetStack();

    const size_t sorted=order.size();
    size_t next=0;
    size_t appended=queuedBefore;
    // entries passed over, still queued
    size_t carried=0;
    // The control rendered last and the state it rendered in, pushed on the
    // stack only if the next entry is its child: a leaf costs no level.
    Base *last=nullptr;
    RenderState lastState;

    for(;;)
    {
        // the sorted entries, then those queued since, in the order they came
        size_t slot;

        if(next < sorted)
            slot=order[next++].slot;
        else if((appended < queue.size()) && (appended-queuedBefore < appendCap))
            slot=appended++;
        else
            break;

        Base *control=queue[slot];

        if(!control)
            continue;

        if(control->m_stateChange == StateChange_Nothing)
        {
            control->m_renderQueueIndex=-1;
            queue[slot]=nullptr;
            continue;
        }

        // a hook moved or deleted controls: the stack may name them
        if(treeChanges != g_renderWalkTreeChanges)
        {
            treeChanges=g_renderWalkTreeChanges;
            last=nullptr;
            resetStack();
        }

        RenderState state=root;

        if(control != this)
        {
            Base *ancestor=control->m_actualParent;

            // a child of the last one: its level goes on the stack
            if(last && (ancestor == last))
                pushLevel(last, lastState);
            last=nullptr;

            // up to the nearest ancestor on the stack, which this root always is
            chain.clear();
            while(ancestor)
            {
                const int level=ancestor->m_renderStackLevel;

                if((level >= 0) && ((size_t)level < depth) && (stack[level].control == ancestor))
                    break;
                chain.push_back(ancestor);
                ancestor=ancestor->m_actualParent;
            }

            // detached, or on another canvas: rendered by a later drain
            if(!ancestor)
            {
                ++carried;
                continue;
            }

            depth=ancestor->m_renderStackLevel+1;

            for(auto it=chain.rbegin(); it != chain.rend(); ++it)
            {
                stateUnderTop(*it, state);
                pushLevel(*it, state);
            }
            stateUnderTop(control, state);
        }

        if(!renderQueued(control, slot, state)
            || (treeChanges != g_renderWalkTreeChanges))
            continue;

        if(control == this)
        {
            resetStack();
            last=nullptr;
        }
        else
        {
            // the next entry is often a child
            last=control;
            lastState=state;
        }
    }

    // as the canvas left them, for whatever it draws after
    baseRenderer->setRenderTransform(root.transform);
    baseRenderer->setRenderOffset(root.offset);
    baseRenderer->setClipRegion(root.clip);

    // every slot is null unless something was carried or left past the cap
    if((carried == 0) && (appended == queue.size()))
    {
        queue.clear();
        g_renderQueueHoles=0;
    }
    else
        compact();
    g_changeStats.renderCarried=queue.size();
    g_draining=false;
}

void Base::doHide(skin::Base *skin)
{
}

void Base::hideRecursive(skin::Base *skin)
{
    for(auto &&child : m_children)
    {
        child->doHide(skin);
    }
}

void Base::setSkin(skin::Base *skin, bool doChildren)
{
    if(m_skin == skin)
        return;

    // give the old skin's Control's ids back while m_skin is still the skin
    // whose renderer they came from
    removeSkinControl();

    m_skin=skin;
    m_skinControl=skin->getControlRenderer(getTypeName());

    if(!m_skinControl)
    {
        const char *parentName=getParentTypeName();
        size_t index=0;

        while(parentName)
        {
            m_skinControl=skin->getControlRenderer(parentName);

            if(m_skinControl)
                break;

            parentName=getParentTypeName(index);
            index++;
        }

        if(!m_skinControl)
        {
            std::cerr<<"Failed to get skin for: "<<getTypeName()<<"\n";
        }
#ifdef GWENI_DEBUG_LOGGING
        else
        {
            std::cout<<"Using:"<<parentName<<" skin for: "<<getTypeName()<<"\n";
        }
#endif//GWENI_DEBUG_LOGGING

        // Falling back to an intermediate ancestor's renderer is by design
        // (TextBoxNumeric draws as a TextBox); ending at the Base entry, which
        // draws nothing, means this type has no renderer of its own.
        if(m_skinControl && std::strcmp(parentName, Base::getTypeNameStatic()) == 0
            && !drawsNothing() && typesReportedWithoutRenderer().insert(getTypeName()).second)
        {
            std::cerr<<"gweni: no skin renderer for "<<getTypeName()<<"\n";
        }
    }

    // the new Control's depthLayers() may differ from the old one's
    zOrderChanged();
    invalidate();
    redraw();
    onSkinChanged(skin);

    m_stateChange=StateChange_Created;
    queueForRender();

    if(doChildren)
    {
        for(auto &&child : m_children)
        {
            child->setSkin(skin, true);
        }
    }

//    //need to set prefered size
//    calculateSize(skin, Dim::X);
//    calculateSize(skin, Dim::Y);
}

void Base::onSkinChanged(skin::Base * /*skin*/)
{
    // Do something
}

bool Base::onMouseWheeled(int iDelta)
{
    if(m_actualParent)
        return m_actualParent->onMouseWheeled(iDelta);

    return false;
}

void Base::onMouseMoved(int /*x*/, int /*y*/, int /*deltaX*/, int /*deltaY*/)
{}

void Base::onMouseEnter()
{
    onHoverEnter.call(this);
    redraw();
    notifyAncestorsOfInteraction();
}

void Base::onMouseLeave()
{
    onHoverLeave.call(this);
    redraw();
    notifyAncestorsOfInteraction();
}

void Base::notifyAncestorsOfInteraction()
{
    for(Base *ancestor=getParent(); ancestor; ancestor=ancestor->getParent())
        ancestor->onDescendantInteraction(this);
}

bool Base::isHovered() const
{
    return HoveredControl == this;
}

bool Base::shouldDrawHover() const
{
    return MouseFocus == this || MouseFocus == nullptr;
}

bool Base::isFocussed()
{
    return KeyboardFocus == this;
}

void Base::focus()
{
    if(KeyboardFocus == this)
        return;

    Base *previous=KeyboardFocus;
    if(previous)
        previous->onLostKeyboardFocus();

    KeyboardFocus=this;

    // The outgoing holder repaints too, once KeyboardFocus has moved on, so its
    // skin stops picking the focused look.
    if(previous)
    {
        previous->redraw();
        previous->notifyAncestorsOfInteraction();
    }

    onKeyboardFocus();
    redraw();
    notifyAncestorsOfInteraction();
}

void Base::unfocus()
{
    if(KeyboardFocus != this)
        return;

    KeyboardFocus=nullptr;
    onLostKeyboardFocus();
    redraw();
    notifyAncestorsOfInteraction();
}

bool Base::isOnTop()
{
    if(!getParent())
        return false;

    Base::List::iterator iter=getParent()->m_children.begin();
    Base *child=*iter;

    if(child == this)
        return true;

    return false;
}

void Base::touch()
{
    if(getParent())
        getParent()->onChildTouched(this);
}

void Base::onChildTouched(controls::Base * /*child*/)
{
    touch();
}

// input::hitTestVisits(), defined at the end of this file.
static size_t g_hitTestVisits=0;

void Base::countHitTestVisit()
{
    ++g_hitTestVisits;
}

Base *Base::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    countHitTestVisit();
    return getControlAtWalk(x, y, bOnlyIfMouseEnabled);
}

Base *Base::getControlAtWalk(int x, int y, bool bOnlyIfMouseEnabled)
{
    if(hidden())
        return nullptr;

    if(x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
        return nullptr;

    // the children's coordinates: the unit the point's unit starts in, in the
    // space they are placed in (canvasPosToLocal()'s rule)
    int childX=x;
    int childY=y;

    if(m_transformsChildren)
    {
        const PointF inChildSpace=toChildSpace(PointF(float(x), float(y)));

        childX=static_cast<int>(std::floor(inChildSpace.x));
        childY=static_cast<int>(std::floor(inChildSpace.y));
    }

    for(Base::List::reverse_iterator iter=m_children.rbegin(); iter != m_children.rend(); ++iter)
    {
        Base *child=*iter;

        // outside the rect this control clips the child to, it is not there
        if(const Rect *childClip=getChildClipRect(child))
        {
            if(childX < childClip->x || childY < childClip->y ||
               childX >= childClip->x+childClip->w || childY >= childClip->y+childClip->h)
                continue;
        }

        Base *found=child->getControlAt(childX-child->getX(), childY-child->getY(), bOnlyIfMouseEnabled);

        if(found)
            return found;
    }

    if(bOnlyIfMouseEnabled && !getMouseInputEnabled())
        return nullptr;

    return this;
}

void Base::layout(skin::Base *skin)
{
//    if(skin->getRenderer()->getCTT() && isCachedToTexture())
//        skin->getRenderer()->getCTT()->createControlCacheTexture(this, this->getBounds().getSize());
}

std::string info(Base *control, Dim dim)
{
    const Rect &bounds=control->getBounds();
    const Size &preferredSize=control->getPreferredSize();
    const Margin &margin=control->getMargin();
    const Padding &padding=control->getPadding();
    const SizeFlags &sizeFlags=control->getSizeFlags();

    std::ostringstream output;

    output<<control->getTypeName()<<" : "<<control->getName()<<"\n";
    if(dim==Dim::X)
        output<<"    X dim\n";
    else
        output<<"    Y dim\n";

    output<<"    Bounds: "<<bounds.x<<", "<<bounds.y<<", "<<bounds.w<<", "<<bounds.h<<"\n";
    output<<"    Preferred: "<<preferredSize.width<<", "<<preferredSize.height<<"\n";
    output<<"    Margin: "<<margin.top<<", "<<margin.bottom<<", "<<margin.left<<", "<<margin.right<<"\n";
    output<<"    Padding: "<<padding.top<<", "<<padding.bottom<<", "<<padding.left<<", "<<padding.right<<"\n";
    output<<"    Size flags: "<<sizeFlagName(sizeFlags.horizontal)<<", "<<sizeFlagName(sizeFlags.horizontal)<<"\n";

    return output.str();
}

bool Base::processLayout(skin::Base *skin, Dim dim)
{
    if(m_layoutItem)
    {
        m_layoutItem->calculateSize(skin, dim);

        const Size preferredSize=m_layoutItem->getPreferredSize();

        if(dim==Dim::X)
        {
            m_preferredSize.width=preferredSize.width+m_padding.left+m_padding.right;
        }
        else
        {
            m_preferredSize.height=preferredSize.height+m_padding.top+m_padding.bottom;
        }
        return true;
    }
    return false;
}

// Each shown child is measured, and what it adds to the content size is its
// term (computeSizeTerm): the largest dock-None child at its requested
// position, or the docks' sum and the Center children's if larger. Every
// count keeps the result in ChildSizes, so in a pass in which few children
// changed (walkChangedChildren), once they have been counted in full, only
// the changed children are measured and counted again, which is the same as
// counting them all, since every child whose term may have changed is among
// them. The term a count holds for a child is the one its inputs give until
// they change, so it is kept only from then (noteSizeInputsChanging) and
// nothing is stored per child by a count. Taking the largest term from its
// last holder needs the largest of the rest, and so does a full count.
Size Base::sizeOfChildren(skin::Base *skin, Dim dim)
{
    if(m_childSizes[static_cast<size_t>(dim)].valid && walkChangedChildren())
        return sizeOfChangedChildren(skin, dim);

    Size preferredSize=m_preferredSize;

    // computeSizeTerm's branches, counted as they come: the largest term (at
    // least 0, as the content is) and how many children hold it, and the
    // sums, kept for the walks of the changed children from MinChangedWalk
    // children on.
    countLayoutChildrenScanned(m_children);
    if(dim==Dim::X)
    {
        int width=0;
        int holders=0;
        int dockWidth=0;
        int innerWidth=0;

        for(auto &&child:m_children)
        {
            if(child->hidden())
                continue;

            child->doCalculateSize(skin, dim);

            // Placed by this control (Placement::Owner) or asked not to
            // count: measured for its own subtree, but not counted.
            if(!child->shouldIncludeInSize())
                continue;

            DockPosition dock=child->getDock();
            const Size &preferred=child->getPreferredSize();
            const Margin &margin=child->getMargin();
            int childWidth=preferred.width+margin.left+margin.right;

            if((dock & DockPosition::Top) || (dock & DockPosition::Bottom))
            {
                if(childWidth>width)
                {
                    width=childWidth;
                    holders=1;
                }
                else if(childWidth == width)
                    ++holders;
            }
            else if((dock & DockPosition::Left) || (dock & DockPosition::Right))
            {
                dockWidth+=childWidth;
            }
            else if(dock & DockPosition::Center)
            {
                innerWidth+=childWidth;
            }
            else
            {
                // Where the child asked to be, not where the last pass put it.
                if(!hasAlignment())
                    childWidth+=child->getRequestedBounds().x;

                if(childWidth>width)
                {
                    width=childWidth;
                    holders=1;
                }
                else if(childWidth == width)
                    ++holders;
            }
        }
        if(m_children.size() >= MinChangedWalk)
            keepChildSizes(dim, width, holders, dockWidth, innerWidth);

        dockWidth+=innerWidth;
        width=std::max(width, dockWidth);
        width+=m_padding.left+m_padding.right;

        preferredSize.width=preferredFromContent(width, Dim::X);
    }
    else
    {
        int height=0;
        int holders=0;
        int dockHeight=0;
        int innerHeight=0;

        for(auto &&child:m_children)
        {
            if(child->hidden())
                continue;

            child->doCalculateSize(skin, dim);

            // Placed by this control (Placement::Owner) or asked not to
            // count: measured for its own subtree, but not counted.
            if(!child->shouldIncludeInSize())
                continue;

            DockPosition dock=child->getDock();
            const Size &preferred=child->getPreferredSize();
            const Margin &margin=child->getMargin();

            int childHeight=preferred.height+margin.top+margin.bottom;

            if((dock & DockPosition::Top)||(dock & DockPosition::Bottom))
            {
                dockHeight+=childHeight;
            }
            else if((dock & DockPosition::Left)||(dock & DockPosition::Right))
            {
                if(childHeight>height)
                {
                    height=childHeight;
                    holders=1;
                }
                else if(childHeight == height)
                    ++holders;
            }
            else if(dock & DockPosition::Center)
            {
                innerHeight+=childHeight;
            }
            else
            {
                if(!hasAlignment())
                    childHeight+=child->getRequestedBounds().y;

                if(childHeight>height)
                {
                    height=childHeight;
                    holders=1;
                }
                else if(childHeight == height)
                    ++holders;
            }
        }
        if(m_children.size() >= MinChangedWalk)
            keepChildSizes(dim, height, holders, dockHeight, innerHeight);

        dockHeight+=innerHeight;
        height=std::max(height, dockHeight);
        height+=m_padding.top+m_padding.bottom;

        preferredSize.height=preferredFromContent(height, Dim::Y);
    }

    return preferredSize;
}

// The largest is at least 0, as the content is: with every term below 0,
// none holds it, and taking any term away makes the next count a full one.
void Base::keepChildSizes(Dim dim, int largest, int holders, int dockSum, int innerSum)
{
    const size_t axis=static_cast<size_t>(dim);
    ChildSizes &sizes=m_childSizes[axis];

    sizes.max=largest;
    sizes.maxCount=holders;
    sizes.dockSum=dockSum;
    sizes.innerSum=innerSum;
    sizes.valid=true;
    m_countSeq[axis]=s_measureSeq;
}

// The changed children are measured and counted again, which is the same as
// counting them all (see sizeOfChildren).
Size Base::sizeOfChangedChildren(skin::Base *skin, Dim dim)
{
    Size preferredSize=m_preferredSize;
    const size_t axis=static_cast<size_t>(dim);
    ChildSizes &sizes=m_childSizes[axis];

    countLayoutChildrenScanned(m_changedChildren);
    for(size_t i=0; i < m_changedChildren.size(); ++i)
    {
        Base *child=m_changedChildren[i];
        SizeTerm counted;

        if(!child)
            continue;

        // what the count holds, read before the measure changes it
        const bool known=sizes.valid && countedSizeTerm(child, dim, counted);

        if(!child->hidden())
            child->doCalculateSize(skin, dim);

        if(known)
            recountSizeTerm(dim, counted, child->computeSizeTerm(this, dim));
        else
            sizes.valid=false;
    }

    if(!sizes.valid)
    {
        // the children are measured: only their terms are counted again
        sizes=ChildSizes();
        countLayoutChildrenScanned(m_children);
        for(auto &&child:m_children)
            addSizeTerm(dim, child->computeSizeTerm(this, dim));
        sizes.valid=true;
    }
    m_countSeq[axis]=s_measureSeq;

    const int content=childSizesContent(dim);

    if(dim==Dim::X)
        preferredSize.width=preferredFromContent(content+m_padding.left+m_padding.right, Dim::X);
    else
        preferredSize.height=preferredFromContent(content+m_padding.top+m_padding.bottom, Dim::Y);

    return preferredSize;
}

int Base::preferredFromContent(int contentSize, Dim dim) const
{
    const bool x=(dim == Dim::X);
    const SizeFlag flag=x?m_sizeFlags.horizontal:m_sizeFlags.vertical;
    const int requested=x?m_requestedBounds.w:m_requestedBounds.h;
    const int minimum=x?m_minimumSize.width:m_minimumSize.height;
    const int maximum=x?m_maximumSize.width:m_maximumSize.height;

    switch(flag)
    {
    case SizeFlag::Fixed:
        return requested;
    case SizeFlag::Shrink:
        return std::min(contentSize, maximum);
    case SizeFlag::Expand:
        return std::max(contentSize, minimum);
    case SizeFlag::Elastic:
        return contentSize;
    case SizeFlag::Bloat:
        return std::max(contentSize, minimum);
    }

    return contentSize;
}

void Base::calculateSize(skin::Base *skin, Dim dim)
{
    if(processLayout(skin, dim))
        return;

    m_preferredSize=sizeOfChildren(skin, dim);

    // Its content is the size it was asked for: the children were measured,
    // for their own subtrees, but do not count.
    if(sizedByRequest(dim))
    {
        if(dim == Dim::X)
            m_preferredSize.width=preferredFromContent(m_requestedBounds.w, Dim::X);
        else
            m_preferredSize.height=preferredFromContent(m_requestedBounds.h, Dim::Y);
    }
}

int Base::dockNoneSize(Dim dim, int preferred) const
{
    const bool x=(dim == Dim::X);
    const SizeFlag flag=x?m_sizeFlags.horizontal:m_sizeFlags.vertical;
    const int requested=x?m_requestedBounds.w:m_requestedBounds.h;

    if(flag == SizeFlag::Fixed)
        return requested;

    if(!hasRequestedSize(dim))
        return preferred;

    switch(flag)
    {
    case SizeFlag::Expand:
        return std::max(requested, preferred);
    case SizeFlag::Shrink:
        return std::min(requested, preferred);
    default: // Elastic and Bloat both grow and shrink to the preferred size
        return preferred;
    }
}

int Base::layoutCellSize(Dim dim, int cell) const
{
    const bool x=(dim == Dim::X);
    const SizeFlag flag=x?m_sizeFlags.horizontal:m_sizeFlags.vertical;
    const int requested=x?m_requestedBounds.w:m_requestedBounds.h;
    const int preferred=x?m_preferredSize.width:m_preferredSize.height;

    if(flag == SizeFlag::Fixed)
        return requested;

    if(!hasRequestedSize(dim))
        return cell;

    if(flag == SizeFlag::Shrink)
        return std::min(requested, cell);

    return std::min(std::max(requested, preferred), cell);
}

void Base::policyResize(const Size &preferred, Dim dim)
{
    const Rect &bounds=getBounds();

    if(dim==Dim::X)
        setArrangedBounds(bounds.x, bounds.y, dockNoneSize(dim, preferred.width), bounds.h);
    else
        setArrangedBounds(bounds.x, bounds.y, bounds.w, dockNoneSize(dim, preferred.height));
}

namespace
{
size_t g_childrenScannedSink=0;
}

size_t *Base::s_childrenScanned=&g_childrenScannedSink;

namespace
{
// The entries of `children` that are not a flagged root: a child of a control
// whose entry is counted is in no flagged subtree unless it is one's root.
template<typename Children>
size_t childrenCounted(const Children &children)
{
    size_t counted=0;

    for(const Base *child:children)
    {
        if(!child || !child->isExcludedFromStats())
            ++counted;
    }
    return counted;
}
}

// Whether a shown dirty child among `children` was dirtied from outside every
// setExcludedFromStats() subtree.
template<typename Children>
bool Base::dirtyChildCounted(const Children &children)
{
    for(const Base *child:children)
    {
        if(child && !child->hidden() && child->m_updateLayout && child->m_layoutDirtyCounted &&
            !child->m_excludeFromStats)
            return true;
    }
    return false;
}

void Base::countLayoutChildrenScanned(size_t children) const
{
    if(layoutEntryCounted())
        *s_childrenScanned+=children;
}

void Base::countLayoutChildrenScanned(const List &children) const
{
    if(g_statsExcludedRoots == 0)
        *s_childrenScanned+=children.size();
    else if(layoutEntryCounted())
        *s_childrenScanned+=childrenCounted(children);
}

void Base::countLayoutChildrenScanned(const std::vector<Base *> &children) const
{
    if(g_statsExcludedRoots == 0)
        *s_childrenScanned+=children.size();
    else if(layoutEntryCounted())
        *s_childrenScanned+=childrenCounted(children);
}
bool Base::s_walkChanged=false;
bool Base::s_boundsInvalidation=false;
size_t Base::s_measureSeq=0;

// Counted only inside a pass: a calculateSize() made outside one (a
// sizeToContents(), say) is no entry of a pass, and is never skipped.
void Base::doCalculateSize(skin::Base *skin, Dim dim)
{
    if(Canvas::s_layingOut)
    {
        if(!m_updateLayout && !Canvas::s_verifyingLayout)
            return;
        // dirty only for want of an arrange (see m_hiddenWidthCurrent)
        if(dim == Dim::X && m_hiddenWidthCurrent && !visible() && !Canvas::s_verifyingLayout)
            return;

        Canvas::s_layingOut->countLayoutEntry(this);
    }

    // a measure after the parent counted this control may have changed its
    // term (see doArrange)
    m_measureSeq[static_cast<size_t>(dim)]=++s_measureSeq;
    calculateSize(skin, dim);
    // what the measure changed below it, it read after the change
    if(dim == Dim::X && Canvas::s_layingOut && !visible())
        m_hiddenWidthCurrent=true;
}

// A control is dirty from its invalidation (or a new bounds) until the end of
// its Y arrange, the last step a pass enters it in, so every step of the pass
// in between enters it: a width the X arrange gave it is measured in Y. What
// is dirtied under it after its children's own Y arrange (a child placed in a
// postLayout(), not in arrange() before its ParentClass::arrange) keeps it
// dirty, and so every control up to the canvas, whose own Y arrange ends the
// pass: Canvas::layoutPass then runs another in the same frame, which enters
// only that way down.
void Base::doArrange(skin::Base *skin, Dim dim)
{
    if(!Canvas::s_layingOut)
    {
        arrange(skin, dim);
        return;
    }

    if(!m_updateLayout && !Canvas::s_verifyingLayout)
        return;

    Canvas::s_layingOut->countLayoutEntry(this);

    arrange(skin, dim);

    if(dim == Dim::Y)
    {
        // Re-dirtied in this pass, or a shown child still is: the next pass
        // enters it again. Every shown dirty child is among the changed ones
        // (noteLayoutChanged), so they are all this looks at; the clean and
        // hidden ones leave the list. One that leaves it with its term
        // possibly out of date (an input changed, or it was measured, after
        // sizeOfChildren last counted it, or never counted: a calculateSize
        // that does not count its children) makes the next count a full one.
        m_updateLayout=(m_redirtiedPass == Canvas::s_passSerial);

        if(m_children.size() < MinChangedWalk)
        {
            // a few children keep no list: they are looked at
            if(!m_updateLayout)
            {
                size_t scanned=0;

                for(const Base *child:m_children)
                {
                    if(!child->m_excludeFromStats)
                        ++scanned;
                    if(!child->hidden() && child->m_updateLayout)
                    {
                        m_updateLayout=true;
                        break;
                    }
                }
                countLayoutChildrenScanned(scanned);
            }
        }
        else if(m_changedOverflow)
        {
            // the list stopped: every child is looked at
            const bool countedX=m_childSizes[0].valid;
            const bool countedY=m_childSizes[1].valid;

            for(Base *child:m_changedChildren)
            {
                if(child)
                    child->m_changedIndex=-1;
            }
            m_changedChildren.clear();
            m_changedOverflow=false;

            countLayoutChildrenScanned(m_children);
            for(Base *child:m_children)
            {
                if(countedX && std::max(child->m_termSeq[0], child->m_measureSeq[0]) > m_countSeq[0])
                    m_childSizes[0].valid=false;
                if(countedY && std::max(child->m_termSeq[1], child->m_measureSeq[1]) > m_countSeq[1])
                    m_childSizes[1].valid=false;

                if(child->m_updateLayout && !child->hidden())
                {
                    m_updateLayout=true;
                    child->noteLayoutChanged();
                }
            }
        }
        else
        {
            size_t kept=0;

            countLayoutChildrenScanned(m_changedChildren);
            for(Base *child:m_changedChildren)
            {
                if(!child)
                    continue;

                if(child->m_updateLayout && !child->hidden())
                {
                    child->m_changedIndex=static_cast<int>(kept);
                    m_changedChildren[kept++]=child;
                    m_updateLayout=true;
                    continue;
                }

                for(size_t axis=0; axis < 2; ++axis)
                {
                    if(std::max(child->m_termSeq[axis], child->m_measureSeq[axis]) > m_countSeq[axis])
                        m_childSizes[axis].valid=false;
                }
                child->m_changedIndex=-1;
            }
            m_changedChildren.resize(kept);
        }

        // still dirty: its parent's walk finds it again, and the next pass
        // enters it as counted only for a dirty child that is (an excluded
        // subtree's leftover alone makes a pass passOnlyExcluded)
        if(m_updateLayout)
        {
            noteLayoutChanged();
            if((g_statsExcludedRoots != 0) && (m_redirtiedPass != Canvas::s_passSerial))
                m_layoutDirtyCounted=((m_children.size() < MinChangedWalk) || m_changedOverflow)?
                    dirtyChildCounted(m_children):dirtyChildCounted(m_changedChildren);
        }
        else
            m_layoutDirtyCounted=false;
    }
}

void Base::arrange(skin::Base *skin, Dim dim)
{
    // Stamped before layout(), so what it does to this control counts as
    // done after this control was laid out.
    stampLayoutPass(dim);

    // layout() runs at the start of the X arrange, when the width is final
    // but the height is the previous pass's. The Y pass gives the control its
    // height before arranging it, and a new one invalidates it again (as
    // placement, see onBoundsChanged), so layout() runs a second time here
    // with both final. What it computes in X must depend on the width only,
    // so the second run writes the same X geometry (docs/layout.md).
    if(needsLayout())
    {
        m_needsLayout=false;
        layout(skin);
    }

    if(m_layoutItem)
    {
        Rect bounds=m_bounds;

        if(dim==Dim::X)
            bounds.x=0;
        else
            bounds.y=0;
        m_layoutItem->setBounds(bounds);
        m_layoutItem->arrange(skin, dim);
        return;
    }

    // A dock-None child is placed from its own inputs and the padded content
    // box alone, so with no docked child changed and the box the same, the
    // children that did not change are where a full walk would put them, and
    // a clean one is not entered: the changed ones are all there is to do.
    // m_arrangeAll is read after layout(), which may change what it says.
    if(m_children.size() >= MinChangedWalk && !takeArrangeAll(dim) && walkChangedChildren())
    {
        countLayoutChildrenScanned(m_changedChildren);
        if(!changedChildDocked())
        {
            const Rect &bounds=getBounds();
            const int contentPos=(dim == Dim::X) ? m_padding.left : m_padding.top;
            const int contentLength=(dim == Dim::X) ? bounds.w-m_padding.left-m_padding.right :
                bounds.h-m_padding.top-m_padding.bottom;

            countLayoutChildrenScanned(m_changedChildren);
            for(size_t i=0; i < m_changedChildren.size(); ++i)
            {
                Base *child=m_changedChildren[i];

                if(!child || child->hidden() || child->getPlacement() == Placement::Owner)
                    continue;

                placeDockNoneChild(child, dim, contentPos, contentLength,
                    child->getPreferredSize(), child->getMargin());
            }

            countLayoutChildrenScanned(m_changedChildren);
            for(size_t i=0; i < m_changedChildren.size(); ++i)
            {
                Base *child=m_changedChildren[i];

                if(!child || child->hidden())
                    continue;

                child->doArrange(skin, dim);
            }

            if(dim == Dim::Y)
                postLayout(skin);
            return;
        }
    }

    if(dim==Dim::X)
        arrangeHorizontal(skin);
    else
        arrangeVertical(skin);

    countLayoutChildrenScanned(m_children);
    for(auto &&child:m_children)
    {
        if(child->hidden())
            continue;

        child->doArrange(skin, dim);
    }

    if(dim == Dim::Y)
        postLayout(skin);
}

bool Base::takeArrangeAll(Dim dim)
{
    const size_t axis=static_cast<size_t>(dim);
    const bool arrangeAll=m_arrangeAll[axis];

    m_arrangeAll[axis]=false;
    return arrangeAll;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline)) inline
#endif
void Base::placeDockNoneChild(Base *child, Dim dim, int contentPos, int contentLength,
    const Size &preferred, const Margin &margin)
{
    const Rect &bounds=child->getBounds();

    if(dim == Dim::X)
    {
        const int contentX=contentPos;
        const int contentW=contentLength;

        child->policyResize(preferred, Dim::X);

        Alignment alignment=child->getAlignment();

        // Alignment is opt-in: an aligned child is placed in the padded
        // content box, whatever the docked siblings took; an unaligned one
        // stays at the position it asked for (setPos, setBounds, moveTo).
        if(alignment & Alignment::Left)
            child->setArrangedBounds(contentX+margin.left, bounds.y, bounds.w, bounds.h);
        else if(alignment & Alignment::Right)
            child->setArrangedBounds(contentX+contentW-bounds.w-margin.right, bounds.y, bounds.w, bounds.h);
        else if(alignment & Alignment::CenterH)
        {
            int start=contentX+margin.left+(contentW-bounds.w-margin.left-margin.right)/2;
            child->setArrangedBounds(start, bounds.y, bounds.w, bounds.h);
        }
        else //none/Absolute
            child->setArrangedBounds(child->getRequestedBounds().x, bounds.y, bounds.w, bounds.h);
    }
    else
    {
        const int contentY=contentPos;
        const int contentH=contentLength;

        child->policyResize(preferred, Dim::Y);

        Alignment alignment=child->getAlignment();

        // As in X: the padded content box, or the requested position.
        if(alignment & Alignment::Top)
            child->setArrangedBounds(bounds.x, contentY+margin.top, bounds.w, bounds.h);
        else if(alignment & Alignment::Bottom)
            child->setArrangedBounds(bounds.x, contentY+contentH-bounds.h-margin.bottom, bounds.w, bounds.h);
        else if(alignment & Alignment::CenterV)
        {
            int start=contentY+margin.top+(contentH-bounds.h-margin.top-margin.bottom)/2;
            child->setArrangedBounds(bounds.x, start, bounds.w, bounds.h);
        }
        else //none/Absolute
            child->setArrangedBounds(bounds.x, child->getRequestedBounds().y, bounds.w, bounds.h);
    }
}

void Base::arrangeHorizontal(skin::Base *skin)
{
    Rect innerBounds=getBounds();

    innerBounds.x=m_padding.left;
    innerBounds.w-=m_padding.left+m_padding.right;

    // The padded content box an aligned dock-None child is placed in; unlike
    // innerBounds it does not shrink by the docked children.
    const int contentX=innerBounds.x;
    const int contentW=innerBounds.w;

    countLayoutChildrenScanned(m_children);
    for(auto &&child:m_children)
    {
        if(child->hidden() || child->getPlacement() == Placement::Owner)
            continue;

        DockPosition dock=child->getDock();
        const Margin &margin=child->getMargin();
        Size preferred=child->getPreferredSize();
        const Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        if(dock & DockPosition::Top)
        {
            if(sizeFlags.horizontal==SizeFlag::Fixed)
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock & DockPosition::Left)
        {
            child->setArrangedBounds(innerBounds.x+margin.left,
                bounds.y,
                preferred.width,
                bounds.h);
            int iWidth=margin.left+margin.right+preferred.width;
            innerBounds.x+=iWidth;
            innerBounds.w-=iWidth;
        }
        else if(dock & DockPosition::Right)
        {
            child->setArrangedBounds((innerBounds.x+innerBounds.w)-preferred.width-margin.right,
                bounds.y,
                preferred.width,
                bounds.h);
            int iWidth=margin.left+margin.right+preferred.width;
            innerBounds.w-=iWidth;
        }
        else if(dock & DockPosition::Bottom)
        {
            if(sizeFlags.horizontal==SizeFlag::Fixed)
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    bounds.w,
                    bounds.h);
            else
                child->setArrangedBounds(innerBounds.x+margin.left,
                    bounds.y,
                    innerBounds.w-margin.left-margin.right,
                    bounds.h);
        }
        else if(dock == DockPosition::None)
            placeDockNoneChild(child, Dim::X, contentX, contentW, preferred, margin);

#ifdef DEBUG_LAYOUT
        if(!(dock&DockPosition::Center))
        {
            Alignment alignment=child->getAlignment();

            logUpdates("%zd %s%s - arrangeH - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
                getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
        }
#endif//DEBUG_LAYOUT
    }

    m_innerBounds.x=innerBounds.x;
    m_innerBounds.w=innerBounds.w;

    //now fill
    countLayoutChildrenScanned(m_children);
    for(auto &&child:m_children)
    {
        DockPosition dock=child->getDock();

        if(!(dock & DockPosition::Center) || child->getPlacement() == Placement::Owner)
            continue;

        const Margin &margin=child->getMargin();
        const Rect &bounds=child->getBounds();
        //            const Rect &preferred=child->getPreferredSize();

        child->setArrangedBounds(innerBounds.x+margin.left, bounds.y,
            innerBounds.w-margin.left-margin.right, bounds.h);

#ifdef DEBUG_LAYOUT
        Alignment alignment=child->getAlignment();
        Size preferred=child->getPreferredSize();

        logUpdates("%zd %s%s - arrangeH - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
            getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
#endif//DEBUG_LAYOUT
    }
}

void Base::arrangeVertical(skin::Base *skin)
{
    Rect innerBounds=getBounds();

    innerBounds.y=m_padding.top;
    innerBounds.h-=m_padding.top+m_padding.bottom;

    const int contentY=innerBounds.y;
    const int contentH=innerBounds.h;

    int centerCount=0;

    countLayoutChildrenScanned(m_children);
    for(auto &&child:m_children)
    {
        if(child->hidden() || child->getPlacement() == Placement::Owner)
            continue;

        DockPosition dock=child->getDock();
        const Margin &margin=child->getMargin();
        const Size &preferred=child->getPreferredSize();
        const Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        if(dock & DockPosition::Top)
        {
            child->setArrangedBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                preferred.height);

            int iHeight=margin.top+margin.bottom+preferred.height;
            innerBounds.y+=iHeight;
            innerBounds.h-=iHeight;
        }
        else if(dock & DockPosition::Left)
        {
            int height;

            if(sizeFlags.vertical==SizeFlag::Fixed)
                height=bounds.h;
            else
                height=innerBounds.h-margin.top-margin.bottom;

            child->setArrangedBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                height);
        }
        else if(dock & DockPosition::Right)
        {
            int height;

            if(sizeFlags.vertical==SizeFlag::Fixed)
                height=bounds.h;
            else
                height=innerBounds.h-margin.top-margin.bottom;

            child->setArrangedBounds(bounds.x,
                innerBounds.y+margin.top,
                bounds.w,
                height);
        }
        else if(dock & DockPosition::Bottom)
        {
            child->setArrangedBounds(bounds.x,
                (innerBounds.y+innerBounds.h)-preferred.height-margin.bottom,
                bounds.w,
                preferred.height);

            innerBounds.h-=preferred.height+margin.bottom+margin.top;
        }
        else if(dock == DockPosition::None)
            placeDockNoneChild(child, Dim::Y, contentY, contentH, preferred, margin);
        else if(dock==DockPosition::Center)
            centerCount++;

#ifdef DEBUG_LAYOUT
        if(!(dock&DockPosition::Center))
        {
            Alignment alignment=child->getAlignment();

            logUpdates("%zd %s%s - arrangeV - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
                getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
        }
#endif//DEBUG_LAYOUT
    }

    m_innerBounds.y=innerBounds.y;
    m_innerBounds.h=innerBounds.h;

    if(centerCount > 0)
    {
        int centerH=innerBounds.h/centerCount;

        centerCount=0;
        countLayoutChildrenScanned(m_children);
        for(auto &&child:m_children)
        {
            // Skip hidden children: they are excluded from centerCount above, so
            // including them here would push the visible Center children down by
            // one slot each (e.g. PageControl's hidden pages shoving the current
            // page off-screen).
            if(child->hidden() || child->getPlacement() == Placement::Owner)
                continue;

            DockPosition dock=child->getDock();

            if(!(dock & DockPosition::Center))
                continue;

            const Margin &margin=child->getMargin();
            const Rect &bounds=child->getBounds();

            child->setArrangedBounds(bounds.x, innerBounds.y+(centerCount*centerH)+margin.top,
                bounds.w, centerH-margin.top-margin.bottom);

            centerCount++;
#ifdef DEBUG_LAYOUT
            Alignment alignment=child->getAlignment();
            Size preferred=child->getPreferredSize();

            logUpdates("%zd %s%s - arrangeV - dock: %s, align: %s (%d, %d, %d, %d) p(%d, %d)\n", child->getControlId(), spaceByParent(child).c_str(), child->getTypeName(), getDockPositionName(dock),
                getAlignmentName(alignment), child->getX(), child->getY(), child->getWidth(), child->getHeight(), preferred.width, preferred.height);
#endif//DEBUG_LAYOUT
        }
    }
}

void Base::recurseLayout(skin::Base *skin)
{
    if(m_skin)
        skin=m_skin;

    if(hidden())
        return;

    if(!m_updateLayout)
        return;

    // The root is entered like any dirty control; its Y arrange leaves it
    // dirty only while a child is, and Canvas::layoutPass runs another while
    // the pass re-dirtied layout or left it so.
    doCalculateSize(skin, Dim::X);
    doArrange(skin, Dim::X);
    doCalculateSize(skin, Dim::Y);
    doArrange(skin, Dim::Y);

    //postLayout(skin);
}

bool Base::isChild(controls::Base *possiblechild)
{
    for(auto &&child : m_children)
    {
        if(possiblechild == child)
            return true;
    }

    return false;
}

// Up the controls this one is actually placed in, each at its own offset: a
// child added to a control with an inner panel is the inner panel's, which
// may itself be another control's inner panel (a TreeNode is in its
// TreeControl's ScrollControl's panel), so the logical parent chain would
// miss an offset.
// A control that transformsChildren() maps between its own units and the
// space its children are placed in on the way.
PointF Base::localPosToCanvasF(const PointF &pnt) const
{
    const Base *placedIn=m_actualParent?m_actualParent:m_parent;

    if(!placedIn)
        return pnt;

    PointF inParent(pnt.x+getX(), pnt.y+getY());

    if(placedIn->transformsChildren())
        inParent=placedIn->fromChildSpace(inParent);
    return placedIn->localPosToCanvasF(inParent);
}

PointF Base::canvasPosToLocalF(const PointF &pnt) const
{
    const Base *placedIn=m_actualParent?m_actualParent:m_parent;

    if(!placedIn)
        return pnt;

    PointF local=placedIn->canvasPosToLocalF(pnt);

    if(placedIn->transformsChildren())
        local=placedIn->toChildSpace(local);
    return PointF(local.x-getX(), local.y-getY());
}

Point Base::localPosToCanvas(const Point &pnt)
{
    const PointF onCanvas=localPosToCanvasF(PointF(pnt));

    return Point(static_cast<int>(std::floor(onCanvas.x+0.5f)), static_cast<int>(std::floor(onCanvas.y+0.5f)));
}

Point Base::canvasPosToLocal(const Point &pnt)
{
    const PointF local=canvasPosToLocalF(PointF(pnt));

    return Point(static_cast<int>(std::floor(local.x)), static_cast<int>(std::floor(local.y)));
}

Rect Base::getCanvasBounds() const
{
    const PointF topLeft=localPosToCanvasF(PointF(0.0f, 0.0f));
    const PointF bottomRight=localPosToCanvasF(PointF(float(getWidth()), float(getHeight())));
    const int left=static_cast<int>(std::floor(topLeft.x+0.5f));
    const int top=static_cast<int>(std::floor(topLeft.y+0.5f));

    return Rect(left, top, static_cast<int>(std::floor(bottomRight.x+0.5f))-left,
        static_cast<int>(std::floor(bottomRight.y+0.5f))-top);
}

bool Base::isMenuComponent()
{
    if(!m_parent)
        return false;

    return m_parent->isMenuComponent();
}

void Base::closeMenus()
{
    for(auto &&child : m_children)
    {
        child->closeMenus();
    }
}

void Base::updateRenderBounds()
{
    m_renderBounds.x=m_bounds.x;
    m_renderBounds.y=m_bounds.y;
    m_renderBounds.w=m_bounds.w;
    m_renderBounds.h=m_bounds.h;
}

void Base::updateCursor()
{
    platform::setCursor(m_cursor);
}

draganddrop::Package *Base::dragAndDrop_getPackage(int /*x*/, int /*y*/)
{
    return m_dragAndDrop_Package;
}

bool Base::dragAndDrop_handleDrop(draganddrop::Package * /*package*/, int /*x*/, int /*y*/)
{
    draganddrop::SourceControl->setParent(this);
    return true;
}

bool Base::dragAndDrop_draggable()
{
    if(!m_dragAndDrop_Package)
        return false;

    return m_dragAndDrop_Package->draggable;
}

void Base::dragAndDrop_setPackage(bool draggable, const String &name, void *userData)
{
    if(!m_dragAndDrop_Package)
        m_dragAndDrop_Package=new draganddrop::Package();

    m_dragAndDrop_Package->draggable=draggable;
    m_dragAndDrop_Package->name=name;
    m_dragAndDrop_Package->userdata=userData;
}

bool Base::sizeToChildren(bool w, bool h)
{
    Point size=childrenSize();
    size.y+=getPadding().bottom;
    size.x+=getPadding().right;
    return setSize(w?size.x:getWidth(), h?size.y:getHeight());
}

Point Base::childrenSize()
{
    Point size;

    for(auto &&child : m_children)
    {
        if(child->hidden())
            continue;

        if(!child->shouldIncludeInSize())
            continue;

        size.x=std::max(size.x, child->right());
        size.y=std::max(size.y, child->bottom());
    }

    return size;
}

void Base::setPadding(const Padding &padding)
{
    if(m_padding.left == padding.left &&
        m_padding.top == padding.top &&
        m_padding.right == padding.right &&
        m_padding.bottom == padding.bottom)
        return;

    m_padding=padding;
    m_arrangeAll[0]=m_arrangeAll[1]=true;
    invalidate();
    invalidateParent();
    invalidateParentReaders();
}

void Base::setMargin(const Margin &margin)
{
    if(m_margin.top == margin.top &&
        m_margin.left == margin.left &&
        m_margin.bottom == margin.bottom &&
        m_margin.right == margin.right)
        return;

    noteSizeInputsChanging();
    m_margin=margin;
    invalidate();
    invalidateParent();
}

bool Base::handleAccelerator(String &accelerator)
{
    if(KeyboardFocus == this || !AccelOnlyFocus())
    {
        AccelMap::iterator iter=m_accelerators.find(accelerator);

        if(iter != m_accelerators.end())
        {
            iter->second->call(this);
            return true;
        }
    }

    for(auto &&child : m_children)
    {
        if(child->handleAccelerator(accelerator))
            return true;
    }

    return false;
}

bool Base::onKeyPress(int key, bool bPress)
{
    bool handled=false;

    switch(key)
    {
    case keys::Tab:
        handled=onKeyTab(bPress);
        break;

    case keys::Space:
        handled=onKeySpace(bPress);
        break;

    case keys::Home:
        handled=onKeyHome(bPress);
        break;

    case keys::End:
        handled=onKeyend(bPress);
        break;

    case keys::Return:
        handled=onKeyReturn(bPress);
        break;

    case keys::Backspace:
        handled=onKeyBackspace(bPress);
        break;

    case keys::Delete:
        handled=onKeyDelete(bPress);
        break;

    case keys::Right:
        handled=onKeyRight(bPress);
        break;

    case keys::Left:
        handled=onKeyLeft(bPress);
        break;

    case keys::Up:
        handled=onKeyUp(bPress);
        break;

    case keys::Down:
        handled=onKeyDown(bPress);
        break;

    case keys::Escape:
        handled=onKeyEscape(bPress);
        break;

    default:
        break;
    }

    if(!handled && getParent())
        getParent()->onKeyPress(key, bPress);

    return handled;
}

bool Base::onKeyRelease(int key)
{
    return onKeyPress(key, false);
}

bool Base::onKeyTab(bool down)
{
    if(!down)
        return true;

    // focus() repaints both the old and the new focus holder.
    if(Base *next=nextTabable(input::isShiftDown()))
        next->focus();

    return true;
}

Base *Base::nextTabable(bool backwards)
{
    Base *root=getCanvas();

    if(!root)
        return nullptr;

    // One pre-order walk of the canvas tree. Forward, the answer is the first
    // candidate after this one, else (wrapping) the first before it; backwards,
    // the last candidate before it, else the last after it. The whole tree is
    // walked, so this is found even inside a hidden or disabled subtree, whose
    // controls are never candidates.
    bool passed=false;
    Base *before=nullptr;   // forward: the first candidate before this; backwards: the last
    Base *after=nullptr;    // forward: the first candidate after this; backwards: the last

    auto walk=[&](auto &self, Base *control, bool excluded)->bool
    {
        excluded=excluded || control->hidden() || control->isDisabled();

        if(control == this)
        {
            passed=true;

            if(backwards && before)
                return true;
        }
        else if(!excluded && control->isTabable())
        {
            if(!passed)
            {
                if(backwards || !before)
                    before=control;
            }
            else
            {
                after=control;

                if(!backwards)
                    return true;
            }
        }

        for(auto &&child : control->m_children)
        {
            if(self(self, child, excluded))
                return true;
        }
        return false;
    };
    walk(walk, root, false);

    if(backwards)
        return before ? before : after;
    return after ? after : before;
}

void Base::renderFocus(skin::Base *skin)
{
    if(KeyboardFocus != this)
        return;

    if(!isTabable())
        return;

//    skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 3);
}

// The canvas reads the tooltip when it shows it (Canvas::updateToolTip), so
// neither setter creates anything; one shown now is filled in again.
void Base::setToolTipText(const String &text)
{
    if(m_toolTipText == text)
        return;

    m_toolTipText=text;

    if(Canvas *canvas=getCanvas())
        canvas->toolTipChanged(this);
}

void Base::setToolTip(Base *tooltip)
{
    if(m_toolTip == tooltip)
        return;

    m_toolTip=tooltip;

    if(m_toolTip)
    {
        m_toolTip->setParent(this);
        m_toolTip->setHidden(true);
    }

    if(Canvas *canvas=getCanvas())
        canvas->toolTipChanged(this);
}

void Base::setContextMenu(Menu *menu)
{
    if(menu == m_contextMenu)
        return;

    dropContextMenu();
    m_contextMenu=menu;

    if(menu)
        menu->m_contextOwners.push_back(this);
}

Menu *Base::createContextMenu()
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return nullptr;

    Menu *menu=canvas->newChild<Menu>();

    menu->setHidden(true);
    setContextMenu(menu);
    m_ownsContextMenu=true;
    return menu;
}

Popup *Base::createPopup()
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return nullptr;

    Popup *popup=canvas->newChild<Popup>();

    popup->setOwner(this);
    return popup;
}

void Base::dropOwnedPopups()
{
    while(Popup *popup=m_ownedPopups)
    {
        m_ownedPopups=popup->m_nextOwned;
        popup->m_owner=nullptr;
        popup->m_nextOwned=nullptr;

        // The popup's canvas, not this control's: this control may have been
        // taken off the canvas the popup is on.
        Canvas *canvas=popup->getCanvas();

        // A canvas tearing down deletes it with its other children, and what
        // an onClose handler reads may already be gone.
        if(canvas && canvas->releasingChildren())
            continue;

        if(canvas && (canvas->shownPopup() == popup))
            canvas->closePopup();

        // Delayed, as a context menu is: this may run from an event of one of
        // the popup's own controls (at once off any canvas).
        popup->delayedDelete();
    }
}

void Base::dropContextMenu()
{
    Menu *menu=m_contextMenu;

    if(!menu)
        return;

    const bool owned=m_ownsContextMenu;

    m_contextMenu=nullptr;
    m_ownsContextMenu=false;

    std::vector<Base *> &owners=menu->m_contextOwners;
    owners.erase(std::remove(owners.begin(), owners.end(), this), owners.end());

    Canvas *canvas=menu->getCanvas();

    if(canvas && (canvas->contextMenuOwner() == this))
        canvas->closeContextMenu();

    if(!owned)
        return;

    // Delayed, as a menu is deleted: this may run from one of its own items'
    // events (at once off any canvas). A canvas tearing down deletes it with
    // its other children, or after them if it is off the canvas's tree.
    menu->delayedDelete();
}

String Base::getChildValue(const String &name)
{
    Base *child=findChildByName(name, true);

    if(!child)
        return "";

    return child->getValue();
}

String Base::getValue()
{
    // Generic value accessor should be filled in if we have a value to give.
    return "";
}

void Base::setValue(const String &value)
{}

int Base::getNamedChildren(ControlList &list, const String &name, bool bDeep)
{
    int found=0;

    for(auto &&child : m_children)
    {
        if(!child->getName().empty() && child->getName() == name)
        {
            list.add(child);
            found++;
        }

        if(!bDeep)
            continue;

        found+=child->getNamedChildren(list, name, bDeep);
    }

    return found;
}

ControlList Base::getNamedChildren(const String &name, bool bDeep)
{
    ControlList list;
    getNamedChildren(list, name, bDeep);
    return list;
}

#if GWENI_ANIMATE

void Base::anim_widthIn(float length, float delay, float ease)
{
    anim::add(this, new anim::size::Width(0, getWidth(), length, false, delay, ease));
    setWidth(0);
}

void Base::anim_heightIn(float length, float delay, float ease)
{
    anim::add(this, new anim::size::Height(0, getHeight(), length, false, delay, ease));
    setHeight(0);
}

void Base::anim_widthOut(float length, bool hide, float delay, float ease)
{
    anim::add(this, new anim::size::Width(getWidth(), 0, length, hide, delay, ease));
}

void Base::anim_heightOut(float length, bool hide, float delay, float ease)
{
    anim::add(this, new anim::size::Height(getHeight(), 0, length, hide, delay, ease));
}

#endif // if GWENI_ANIMATE


} // namespace gweni
} // namespace controls

namespace gweni
{
namespace input
{

size_t hitTestVisits()
{
    return controls::g_hitTestVisits;
}

}//namespace input
}//namespace gweni
