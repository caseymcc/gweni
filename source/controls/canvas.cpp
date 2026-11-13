/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/canvas.h>
#include <gweni/skin.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/popup.h>
#include <gweni/dragAndDrop.h>
#include <gweni/controls/toolTip.h>
#include <gweni/controls/toast.h>
#include <gweni/platforms/platform.h>
#include <gweni/primitiveIds.h>

#if GWENI_ANIMATE
#include <gweni/anim.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace gweni
{
namespace controls
{

//! Every canvas alive, for ~Base to cancel an owner's timers on each.
static std::vector<Canvas *> &liveCanvases()
{
    static std::vector<Canvas *> canvases;
    return canvases;
}

Canvas::Canvas(skin::Base *skin): ParentClass()
{
    liveCanvases().push_back(this);

//    enlargePrimitiveIds(this, m_primitiveIds, 1);

    setBounds(0, 0, 10000, 10000);
    setScale(1.0f);
    setBackgroundColor(Color(255, 255, 255, 255));
    setDrawBackground(false);

    if(skin)
        setSkin(skin);
    // a new tree is laid out and rendered whole anyway
    if(skin && skin->getRenderer())
        m_textLayoutSerial=skin->getRenderer()->textLayoutSerial();
}

Canvas::~Canvas()
{
    // a worker's post() through postHandle() is dropped from here on, and what
    // is queued is destroyed before the tree it may point into
    m_postQueue->close();
    // its timers go unrun, before the tree their functions may point into
    {
        std::vector<Canvas *> &canvases=liveCanvases();
        canvases.erase(std::remove(canvases.begin(), canvases.end(), this), canvases.end());
        std::unordered_map<uint64_t, Timer> timers;
        timers.swap(m_timers);
        m_timerHeap.clear();
    }
    if(InputCapture *capture=m_inputCapture)
    {
        m_inputCapture=nullptr;
        capture->captureCanvasDeleted(this);
    }
    releaseChildren();
    // What is still queued is off this tree (detached, or moved to another
    // canvas) or was queued by the teardown: it goes now, with whatever its
    // destructors queue, so no control is left pointing at this canvas.
    Canvas::processDelayedDeletes();
}

void Canvas::updateCanvas()
{
    const uint64_t start=platform::profileTimeMicros();

    // the frame before this one is whole: its update and render phases
    m_previousFrame.layout=m_layoutStats;
    m_previousFrame.update=m_updateStats;
    m_previousFrame.render=m_renderChange;
    m_previousFrame.times=m_frameTimes;
    m_renderChange=ChangeStats();
    m_updateStats=UpdateStats();
    m_frameTimes=FrameTimes();
    m_frameTimes.startMicros=start;

    // What other threads (or this one) posted since the last update, after the
    // input the loop delivered; a post made while these run waits for the next.
    m_postQueue->run();
    // then the timers due now, before doThink()'s animations
    runTimers();

    // The renderer's text draws or measures otherwise now (a new text AA mode):
    // every control is laid out and re-emitted again, as for a new scale.
    if(m_skin && m_skin->getRenderer()
        && (m_skin->getRenderer()->textLayoutSerial() != m_textLayoutSerial))
    {
        m_textLayoutSerial=m_skin->getRenderer()->textLayoutSerial();
        scaleChangedSubtree();
        redraw();
    }

    doThink();
    renumberZOrder();
    m_frameTimes.updateMicros=platform::profileTimeMicros()-start;
}

namespace
{

//! The heap's order: the earliest deadline on top, then the first pushed.
struct TimerLater
{
    template<typename Entry>
    bool operator()(const Entry &a, const Entry &b) const
    {
        if(a.deadline != b.deadline)
            return a.deadline > b.deadline;
        return a.serial > b.serial;
    }
};

uint64_t g_nextTimerId=1;

}//namespace

TimerHandle Canvas::after(double seconds, std::function<void()> function, Base *owner)
{
    return addTimer(seconds, std::move(function), owner, false);
}

TimerHandle Canvas::every(double seconds, std::function<void()> function, Base *owner)
{
    return addTimer(seconds, std::move(function), owner, true);
}

TimerHandle Canvas::addTimer(double seconds, std::function<void()> function, Base *owner, bool repeating)
{
    if(!function)
        return TimerHandle();

    const double now=platform::getTimeInSeconds();
    const double delay=(seconds > 0.0)?seconds:0.0;
    const uint64_t id=g_nextTimerId++;

    Timer &timer=m_timers[id];
    timer.function=std::move(function);
    timer.origin=now+delay;
    timer.period=delay;
    timer.repeating=repeating;
    timer.owner=owner;
    if(owner)
        owner->m_ownsTimers=true;

    pruneTimers();
    // earlier than every pending one: an idle loop waiting for that one
    // wakes (an update running the timers needs none)
    const bool earliest=m_timerHeap.empty() || (timer.origin < m_timerHeap.front().deadline);

    pushTimer(timer.origin, id);
    if(earliest && !m_runningTimers)
        m_postQueue->wake();
    return TimerHandle(id);
}

void Canvas::pushTimer(double deadline, uint64_t id)
{
    // cancelled entries are dropped as they come up; past twice the pending
    // count the heap is rebuilt without them
    if(m_timerHeap.size() >= 2*m_timers.size()+16)
    {
        m_timerHeap.erase(std::remove_if(m_timerHeap.begin(), m_timerHeap.end(),
            [this](const TimerEntry &entry) { return m_timers.count(entry.id) == 0; }),
            m_timerHeap.end());
        std::make_heap(m_timerHeap.begin(), m_timerHeap.end(), TimerLater());
    }
    m_timerHeap.push_back({deadline, m_timerSerial++, id});
    std::push_heap(m_timerHeap.begin(), m_timerHeap.end(), TimerLater());
}

void Canvas::pruneTimers()
{
    while(!m_timerHeap.empty() && (m_timers.count(m_timerHeap.front().id) == 0))
    {
        std::pop_heap(m_timerHeap.begin(), m_timerHeap.end(), TimerLater());
        m_timerHeap.pop_back();
    }
}

bool Canvas::cancel(TimerHandle timer)
{
    // the heap entry stays until it comes up; the function goes now, unless
    // it is running (runTimers() holds it then)
    return m_timers.erase(timer.id()) != 0;
}

void Canvas::cancelTimersOwnedBy(Base *owner)
{
    for(Canvas *canvas:liveCanvases())
    {
        for(auto it=canvas->m_timers.begin(); it != canvas->m_timers.end();)
        {
            if(it->second.owner == owner)
                it=canvas->m_timers.erase(it);
            else
                ++it;
        }
    }
}

double Canvas::nextWorkTime()
{
    renderer::Base *render=m_skin?m_skin->getRenderer():nullptr;

    // a stale hover is taken by input::onCanvasThink on a shown canvas with no
    // input capture set, so until then it waits for the capture to be cleared
    // or the canvas shown (and is work again once it is)
    const bool hoverToTake=m_hoverStale && !hidden() && !m_inputCapture;
    // a hidden canvas's update phase stops before the steps that take these,
    // so they wait for it to be shown, as layout does
    const bool modalsOrToasts=!hidden() && (m_modalFocusPending || m_toastDeadlineDirty);

    if(m_needsRedraw || m_zOrderDirty || hoverToTake || !m_deleteSet.empty()
        || modalsOrToasts
        || m_postQueue->pending()
        || (!hidden() && layoutDirty())
        || (render && (render->textLayoutSerial() != m_textLayoutSerial))
        || (queuedRenderCount(this) != 0))
        return WorkNow;

    pruneTimers();

    double next=m_timerHeap.empty()?NoWork:m_timerHeap.front().deadline;

    if(hidden())
        return next;

#if GWENI_ANIMATE
    next=std::min(next, anim::nextThinkTime(this));
    if(next == WorkNow)
        return next;
#endif

    // the tooltip: a new owner is taken (or let go) by the next update; one
    // waiting shows once now-m_toolTipSince >= m_toolTipDelay, the first time
    // for which that holds as updateToolTip() computes it
    Base *owner=toolTipOwnerFor(HoveredControl);

    if(m_toolTipSuppressed && (owner == m_toolTipSuppressed))
        owner=nullptr;
    if(owner != m_toolTipOwner)
        return WorkNow;
    if(owner && !m_toolTipShown)
    {
        double due=m_toolTipSince+m_toolTipDelay;

        while(due-m_toolTipSince < m_toolTipDelay)
            due=std::nextafter(due, NoWork);
        next=std::min(next, due);
    }
    else if(owner && m_toolTipDirty)
        return WorkNow;

    // a toast due: the update compares the toast clock with its deadline, so
    // the wake is the first time at which that holds
    if(m_toastHost && !m_toastPaused && (m_toastDeadline != NoWork))
    {
        double due=m_toastDeadline+m_toastPausedTotal;

        while(due-m_toastPausedTotal < m_toastDeadline)
            due=std::nextafter(due, NoWork);
        next=std::min(next, due);
    }

    return std::min(next, input::nextKeyRepeatTime(this));
}

void Canvas::runTimers()
{
    pruneTimers();
    if(m_timerHeap.empty())
        return;

    const double now=platform::getTimeInSeconds();
    // what this update's callbacks add or reschedule waits for the next one
    const uint64_t serialLimit=m_timerSerial;

    m_runningTimers=true;
    while(!m_timerHeap.empty())
    {
        const TimerEntry top=m_timerHeap.front();
        auto it=m_timers.find(top.id);

        if(it == m_timers.end())
        {
            std::pop_heap(m_timerHeap.begin(), m_timerHeap.end(), TimerLater());
            m_timerHeap.pop_back();
            continue;
        }
        if((top.deadline > now) || (top.serial >= serialLimit))
            break;

        std::pop_heap(m_timerHeap.begin(), m_timerHeap.end(), TimerLater());
        m_timerHeap.pop_back();

        // the callback may cancel this timer or any other, or add some, so
        // it runs from a local, found again by id after it returns
        std::function<void()> function=std::move(it->second.function);
        const uint64_t id=top.id;

        if(it->second.repeating)
        {
            Timer &timer=it->second;

            if(timer.period > 0.0)
            {
                // the next multiple of the period past now: after a gap it
                // fires once, and the deadlines stay origin+k*period
                uint64_t next=timer.index+1;
                const double missed=std::floor((now-timer.origin)/timer.period);

                if(missed+1.0 > double(next))
                    next=uint64_t(missed)+1;
                while(timer.origin+double(next)*timer.period <= now)
                    ++next;
                timer.index=next;
                pushTimer(timer.origin+double(next)*timer.period, id);
            }
            else
                pushTimer(now, id);
        }
        else
            m_timers.erase(it);

        function();

        auto again=m_timers.find(id);

        if(again != m_timers.end() && !again->second.function)
            again->second.function=std::move(function);
    }
    m_runningTimers=false;
}

void Canvas::renumberZOrder()
{
    // Only after the order changed: otherwise every control keeps its number.
    if(!m_zOrderDirty)
        return;

    keepOverlaysLast();
    // after it: what it moved is numbered in this pass
    m_zOrderDirty=false;

    renderer::Base *render=m_skin?m_skin->getRenderer():nullptr;
    // with nothing to draw into, nothing limits the range
    const int maxZ=render?render->maxZIndex():std::numeric_limits<int>::max();
    // a clamped tree's ties are out of order wherever they are
    const bool skipClean=m_zNumbered && !m_zWalkAll && !m_zClamped;
    m_zWalkAll=false;

    // Nothing to enter: only removals since the last pass. Each left a gap and
    // every label is still in order, so a walk would keep them all and end
    // where the canvas's subtree now ends.
    if(skipClean && !m_zDirty && (m_zEnd-1 <= maxZ))
    {
        m_zHigh=m_zEnd-1;
        return;
    }

    // The first numbering spreads the tree out; after that a control keeps
    // its z unless the ones before it now reach past it.
    ZNumbering numbering;
    numbering.full=!m_zNumbered;
    numbering.gap=m_zGap;
    numbering.high=m_zHigh;
    numbering.tailGap=m_zGap;
    numbering.movedTailGap=std::min(m_zGap, MovedZGap);
    numbering.skipClean=skipClean;
    numbering.stats=&m_updateStats;
    renumberZRecursive(numbering);

    if(numbering.maxUsed > maxZ)
    {
        // Out of z: one full pass spreads the tree out again, over at most
        // half of the range, so what is added or moved to the end after it
        // (which takes new z at the top) has the other half before the next
        // compaction; packed for a tree that barely fits at all.
        size_t layers=0;
        size_t ranges=0;
        countZRanges(layers, ranges);
        const size_t spare=(layers < (size_t)maxZ)?(size_t)maxZ-layers:0;
        m_zGap=(int)std::min<size_t>(ZGap, ranges?spare/(2*ranges):0);

        if(layers > (size_t)maxZ)
        {
            static bool warned=false;

            if(!warned)
            {
                warned=true;
                std::fprintf(stderr, "gweni: %zu z layers do not fit the renderer's %d;"
                    " the last controls share z values (warned once)\n", layers, maxZ);
            }
        }

        ZNumbering compaction;
        compaction.full=true;
        compaction.gap=m_zGap;
        compaction.clampTo=maxZ;
        compaction.stats=&m_updateStats;
        renumberZRecursive(compaction);
        numbering=compaction;
    }
    m_zNumbered=true;
    m_zClamped=numbering.clamped;
    m_zHigh=numbering.maxUsed;
}

void Canvas::renderCanvas()
{
    const uint64_t start=platform::profileTimeMicros();
    renderer::Base *render=m_skin->getRenderer();
    render->begin();
    renderPhaseLayout();
    // what that layout added is numbered before the walk draws it
    renumberZOrder();
    render->setClipRegion(getBounds());
    render->setRenderOffset(Point(0, 0));
    render->setScale(scale());
    // here rather than in render(): the canvas is not rendered on a frame
    // nothing on it changed
    m_needsRedraw=false;

    if(m_drawBackground && (m_stateChange != StateChange_Nothing))
    {
        render->setDrawColor(m_backgroundColor);
//        render->drawFilledRect(m_primitiveIds[0], getRenderBounds(), getZIndex());
    }

    const ChangeStats before=getChangeStats();

    {
        // the walk only draws: a tree change from here on is reported
        RenderWalkScope walk;
        renderPendingControls();
    }

    // this canvas's share, whoever resets the process-wide counts
    const ChangeStats &after=getChangeStats();

    m_renderChange.visited+=after.visited-before.visited;
    m_renderChange.controls+=after.controls-before.controls;
    m_renderChange.created+=after.created-before.created;
    m_renderChange.visibility+=after.visibility-before.visibility;
    m_renderChange.geometry+=after.geometry-before.geometry;
    m_renderChange.background+=after.background-before.background;
    m_renderChange.text+=after.text-before.text;
    m_renderChange.excluded+=after.excluded-before.excluded;
    m_renderChange.renderCarried=after.renderCarried;
    render->end();
    // the renderer's flush timed itself, inside this phase's time
    m_frameTimes.flushMicros=render->frameStats().flushMicros;
    m_frameTimes.renderMicros=platform::profileTimeMicros()-start;
    snapshotExcludedSlots();
}

void Canvas::renderPendingControls()
{
    // about one control per gapped range of the last numbering
    drainRenderQueue(m_skin, m_zClamped, size_t(m_zHigh)/size_t(1+m_zGap));
}

void Canvas::render(skin::Base * /*render*/)
{
}

void Canvas::doThink()
{
    m_layoutStats=LayoutStats();
    m_measureCallsAtThink=measureTextCalls();
    processDelayedDeletes();

    if(hidden())
        return;

#if GWENI_ANIMATE
    // only this canvas's controls: another canvas's run in its own update
    // phase, before its own layout
    m_updateStats.animsRun+=anim::think(this);
#endif
    processDelayedDeletes();
    updateModals();
    updatePopup();
    updateAnchoredMenus();
    updateToolTip();
    updateToasts();
    // A focus hidden or turned off before the frame is dropped before its
    // layout, so what its onLostKeyboardFocus() changes (a DragNumber ending
    // its edit) is laid out in this pass; one the layout hid is dropped after
    // it, and laid out again here only if that dirtied layout, rather than in
    // the render phase.
    input::dropLostKeyboardFocus();
    // The overlays are last in the layout's walk too, not only in z, so what
    // the app's layout sets for one (StatsOverlay::setBottomReserve) is read
    // when the pass reaches it, a control added after one included.
    if(m_zOrderDirty)
        keepOverlaysLast();
    layoutCanvas();
    if(input::dropLostKeyboardFocus() && m_updateLayout)
        layoutCanvas();

    input::onCanvasThink(this);
    noteMeasureCalls();
}

void Canvas::noteMeasureCalls()
{
    m_layoutStats.measureCalls=measureTextCalls()-m_measureCallsAtThink;
}

Canvas *Canvas::s_layingOut=nullptr;
unsigned Canvas::s_passSerial=0;

void Canvas::layoutPass()
{
    m_passRedirtied=false;
    m_passCulpritType=nullptr;
    m_passCulpritId=0;
    m_passExcludedEntries=0;
    m_passCountedEntries=0;

    Canvas *outer=s_layingOut;
    size_t *outerScanned=s_childrenScanned;
    const bool outerWalk=s_walkChanged;
    // Layout verification's full pass counts nothing and walks everything.
    static size_t verifySink=0;

    s_layingOut=this;
    s_childrenScanned=s_verifyingLayout?&verifySink:&m_layoutStats.childrenScanned;
    s_walkChanged=!s_verifyingLayout;
    ++s_passSerial;
    m_inLayout=true;
    recurseLayout(m_skin);
    m_inLayout=false;
    s_layingOut=outer;
    s_childrenScanned=outerScanned;
    s_walkChanged=outerWalk;

    // The canvas's own Y arrange, the pass's last step, left it dirty exactly
    // while a shown child still is (Base::doArrange): something placed after
    // its arrange (a child moved in a postLayout()) or re-dirtied. Either
    // asks for another pass, which the caller runs in the same frame and
    // which enters only that way down. An invalidation the pass consumed
    // after it was raised (see Base::invalidate) left nothing dirty.
    const bool leftDirty=m_updateLayout;

    if(leftDirty && !m_passRedirtied)
        nameLeftDirty();
    m_updateLayout=m_passRedirtied || leftDirty;
}

// The way down the pass left dirty is short: the first shown dirty child of
// each control, to the deepest. Its parent is named, the control whose arrange
// ended with it dirty (it placed the child after the child's own arrange, or
// never entered it).
void Canvas::nameLeftDirty()
{
    Base *deepest=this;

    for(bool deeper=true; deeper;)
    {
        deeper=false;
        for(Base *child:deepest->getActualChildren())
        {
            if(!child->hidden() && child->layoutDirty())
            {
                deepest=child;
                deeper=true;
                break;
            }
        }
    }

    Base *culprit=deepest->getActualParent()?deepest->getActualParent():deepest;

    m_passCulpritType=culprit->getTypeName();
    m_passCulpritId=culprit->getControlId();
}

namespace
{

// -1 until GWENI_LAYOUT_VERIFY has been read or setLayoutVerify() called.
int g_layoutVerify=-1;
size_t g_layoutVerifyFailures=0;

// What verifyLayout() compares, per control.
struct LayoutSnapshot
{
    Base *control;
    size_t id;
    const char *type;
    Rect bounds;
    Size preferred;
};

void snapshotLayout(Base *control, std::vector<LayoutSnapshot> &out)
{
    out.push_back({control, control->getControlId(), control->getTypeName(),
        control->getBounds(), control->getPreferredSize()});

    for(Base *child : control->getActualChildren())
        snapshotLayout(child, out);
}

// Prints at most this many differences per verification; every one counts.
constexpr size_t MaxVerifyLines=20;

}//namespace

bool layoutVerifyEnabled()
{
    if(g_layoutVerify < 0)
    {
        const char *env=std::getenv("GWENI_LAYOUT_VERIFY");
        g_layoutVerify=(env && *env && std::strcmp(env, "0") != 0)?1:0;
    }
    return g_layoutVerify == 1;
}

void setLayoutVerify(bool verify)
{
    g_layoutVerify=verify?1:0;
}

size_t layoutVerifyFailures()
{
    return g_layoutVerifyFailures;
}

bool Canvas::s_verifyingLayout=false;

void Canvas::verifyLayout()
{
    std::vector<LayoutSnapshot> before;
    snapshotLayout(this, before);

    // The full pass is no part of the frame's layout: its stats and its
    // measures are put back or left out.
    const LayoutStats stats=m_layoutStats;
    const size_t measures=measureTextCalls();

    s_verifyingLayout=true;
    m_updateLayout=true;
    layoutPass();
    s_verifyingLayout=false;

    m_measureCallsAtThink+=measureTextCalls()-measures;
    m_layoutStats=stats;

    std::vector<LayoutSnapshot> after;
    snapshotLayout(this, after);

    std::unordered_map<const Base *, const LayoutSnapshot *> now;
    for(const LayoutSnapshot &entry : after)
        now.emplace(entry.control, &entry);

    size_t failures=0;
    auto report=[&](const char *type, size_t id, const char *what)
    {
        if(++failures <= MaxVerifyLines)
            std::fprintf(stderr, "gweni: layout verify: %s#%zu %s\n", type, id, what);
    };
    auto compare=[&](const LayoutSnapshot &a, const char *axis, const char *field,
        int pass, int full)
    {
        if(pass == full)
            return;

        char what[128];
        std::snprintf(what, sizeof(what), "%s %s: pass %d, full pass %d", axis, field, pass, full);
        report(a.type, a.id, what);
    };

    for(const LayoutSnapshot &a : before)
    {
        // An id is handed out again once released, so a control the full pass
        // deleted may have a successor at its address under another id.
        auto found=now.find(a.control);

        if(found == now.end() || found->second->id != a.id)
        {
            report(a.type, a.id, "was deleted by the full pass");
            continue;
        }

        const LayoutSnapshot &b=*found->second;

        compare(a, "X", "bounds.x", a.bounds.x, b.bounds.x);
        compare(a, "X", "bounds.w", a.bounds.w, b.bounds.w);
        compare(a, "X", "preferred.width", a.preferred.width, b.preferred.width);
        compare(a, "Y", "bounds.y", a.bounds.y, b.bounds.y);
        compare(a, "Y", "bounds.h", a.bounds.h, b.bounds.h);
        compare(a, "Y", "preferred.height", a.preferred.height, b.preferred.height);
        now.erase(found);
    }

    for(const LayoutSnapshot &b : after)
    {
        if(now.count(b.control))
            report(b.type, b.id, "was created by the full pass");
    }

    std::vector<std::string> problems;

    for(const LayoutSnapshot &b : after)
    {
        problems.clear();
        b.control->verifyOwnLayout(problems);
        for(const std::string &problem : problems)
            report(b.type, b.id, problem.c_str());
    }

    if(m_updateLayout)
    {
        char what[160];
        std::snprintf(what, sizeof(what), "re-dirtied layout during the full pass (by %s#%zu)",
            m_passCulpritType, m_passCulpritId);
        report(getTypeName(), getControlId(), what);
    }

    if(failures > MaxVerifyLines)
        std::fprintf(stderr, "gweni: layout verify: ... and %zu more\n", failures-MaxVerifyLines);

    g_layoutVerifyFailures+=failures;
}

void Canvas::countLayoutEntry(controls::Base *control)
{
    if(!control->layoutEntryCounted())
    {
        ++m_passExcludedEntries;
        return;
    }
    ++m_layoutStats.visited;
    ++m_passCountedEntries;
}

void Canvas::layoutCanvas()
{
    layoutUntilClean(true);
}

// The update phase's passes and the render phase's run the same loop, so a
// pass the render phase leaves dirty is laid out in that frame too rather
// than left to the next (counted as LayoutStats::renderLayout, not passes).
void Canvas::layoutUntilClean(bool updatePhase)
{
    static std::set<std::string> warnedCapped;
    // every pass run, counted or not (passOnlyExcluded), for the cap
    int passesRun=0;

    while(m_updateLayout && !hidden())
    {
        if(passesRun == MaxLayoutPasses)
        {
            m_layoutStats.capped=true;
            if(warnedCapped.insert(m_passCulpritType).second)
                std::fprintf(stderr, "gweni: layout still dirty after %d passes in one frame,"
                    " re-dirtied by %s#%zu; the rest is left to the next frame"
                    " (warned once per control type)\n",
                    MaxLayoutPasses, m_passCulpritType, m_passCulpritId);
            break;
        }

        const size_t visitedBefore=m_layoutStats.visited;

        enterFollowers();
        layoutPass();
        ++passesRun;

        // a pass that ran for an excluded subtree alone is none of the app's:
        // its entries of the canvas are taken back too
        if(passOnlyExcluded())
        {
            m_layoutStats.visited=visitedBefore;
            continue;
        }
        if(updatePhase)
            ++m_layoutStats.passes;
        else
            m_layoutStats.renderLayout=true;

        // re-dirtied, or left dirty under the culprit (layoutPass)
        if(m_updateLayout && !m_layoutStats.redirtiedType)
        {
            m_layoutStats.redirtiedType=m_passCulpritType;
            m_layoutStats.redirtiedId=m_passCulpritId;
        }
    }

    if(!hidden() && !m_updateLayout && layoutVerifyEnabled())
        verifyLayout();
}

void Canvas::renderPhaseLayout()
{
    if(!m_updateLayout || hidden())
        return;

    layoutUntilClean(false);
    noteMeasureCalls();
}

void Canvas::layoutInvalidated(controls::Base *control)
{
    if(m_passRedirtied)
        return;

    m_passRedirtied=true;
    m_passCulpritType=control->getTypeName();
    m_passCulpritId=control->getControlId();

#ifndef NDEBUG
    // Once per type, so an app author sees which of their controls lean on the
    // loop without a line per frame.
    static std::set<std::string> warned;
    if(warned.insert(m_passCulpritType).second)
        std::fprintf(stderr, "gweni: %s#%zu re-dirtied layout during a layout pass,"
            " so the canvas lays out again (warned once per control type)\n",
            m_passCulpritType, m_passCulpritId);
#endif
}

void Canvas::layoutRequested(controls::Base *control)
{
    ++m_layoutStats.requests;

    if(!m_layoutStats.requestedType)
    {
        m_layoutStats.requestedType=control->getTypeName();
        m_layoutStats.requestedId=control->getControlId();
    }

#ifndef NDEBUG
    // Once per type, like the re-dirty warning: what layout code writes is
    // computed, and a request it records makes a preferred size depend on
    // what an earlier pass did.
    static std::set<std::string> warned;
    if(warned.insert(control->getTypeName()).second)
        std::fprintf(stderr, "gweni: %s#%zu recorded a requested size or position during a"
            " layout pass; layout code should write through setArrangedBounds or"
            " placeChild (warned once per control type)\n",
            control->getTypeName(), control->getControlId());
#endif
}

void Canvas::setScale(float f)
{
    if(m_scale == f)
        return;

    m_scale=f;

    if(m_skin && m_skin->getRenderer())
        m_skin->getRenderer()->setScale(m_scale);

    scaleChangedSubtree();
    redraw();
}

bool ExcludedSlots::primitive(size_t id) const
{
    return std::binary_search(m_heldPrimitives.begin(), m_heldPrimitives.end(), id)
        || std::binary_search(m_gonePrimitives.begin(), m_gonePrimitives.end(), id);
}

bool ExcludedSlots::text(size_t id) const
{
    return std::binary_search(m_heldTexts.begin(), m_heldTexts.end(), id)
        || std::binary_search(m_goneTexts.begin(), m_goneTexts.end(), id);
}

namespace
{

void appendHeldIds(const Base *control, std::vector<size_t> &primitives, std::vector<size_t> &texts)
{
    if(const skin::Control *skinControl=control->getSkinControl())
        skinControl->appendIds(primitives, texts);
    for(const Base *child : control->getActualChildren())
        appendHeldIds(child, primitives, texts);
}

void sortUnique(std::vector<size_t> &ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

}//namespace

void Canvas::snapshotExcludedSlots()
{
    ExcludedSlots &slots=m_excludedSlots;
    const std::vector<Base *> &roots=statsExcludedRoots();
    const bool held=std::any_of(roots.begin(), roots.end(),
        [this](Base *root) { return root->getCanvas() == this; });

    // taken at every flush, so a tool opened over a settled app reads the
    // app's ids at the flush before it (two counter reads, no walk)
    slots.m_livePrimitives=primitiveIdCount()-releasedPrimitiveIdCount();
    slots.m_liveTexts=textIdCount()-releasedTextIdCount();

    // with no tool on the canvas, once the last snapshot held and cleared
    // nothing there is nothing to keep
    if(!held && slots.m_heldPrimitives.empty() && slots.m_heldTexts.empty()
        && slots.m_gonePrimitives.empty() && slots.m_goneTexts.empty())
        return;

    // the last flush's held ids nobody has now: the slots this flush cleared
    // (an id taken again is its new holder's), sorted as the held ids are
    slots.m_gonePrimitives.clear();
    slots.m_goneTexts.clear();
    for(size_t id : slots.m_heldPrimitives)
    {
        if(!isPrimitiveIdLive(id))
            slots.m_gonePrimitives.push_back(id);
    }
    for(size_t id : slots.m_heldTexts)
    {
        if(!isTextIdLive(id))
            slots.m_goneTexts.push_back(id);
    }

    slots.m_heldPrimitives.clear();
    slots.m_heldTexts.clear();
    // a flagged subtree inside another is walked twice; sortUnique drops the
    // repeats
    for(Base *root : roots)
    {
        if(root->getCanvas() == this)
            appendHeldIds(root, slots.m_heldPrimitives, slots.m_heldTexts);
    }
    sortUnique(slots.m_heldPrimitives);
    sortUnique(slots.m_heldTexts);
}

void Canvas::addDelayedDelete(controls::Base *control)
{
    if(m_deleteSet.insert(control).second)
    {
        m_deleteList.push_back(control);
        control->m_deleteQueuedOn=this;
    }
}

// A control deleted before its turn (by an ancestor, a destructor or the app),
// on this canvas or off it, leaves the set only, through its m_deleteQueuedOn;
// its list entry is skipped when it comes up.
void Canvas::PreDeleteCanvas(controls::Base *control)
{
    m_deleteSet.erase(control);
    control->m_deleteQueuedOn=nullptr;
}

// The list front first, until it is empty: what a destructor queues is
// appended and goes in this call too. An entry is deleted only if the set
// still holds it, so a control deleted with an ancestor queued before it, or
// by another queued control's destructor, is skipped, and a stale entry (a
// control already freed) is never dereferenced. If a freed control's address
// is reused by a control queued later, the stale entry deletes that control
// at the stale position, still once and in this batch, and its own entry is
// skipped.
void Canvas::processDelayedDeletes()
{
    while(!m_deleteList.empty())
    {
        controls::Base *control=m_deleteList.front();

        m_deleteList.pop_front();

        if(m_deleteSet.erase(control) == 0)
            continue;

        control->m_deleteQueuedOn=nullptr;
        control->preDelete(getSkin());
        delete control;
        redraw();
    }
}

void Canvas::releaseChildren()
{
    m_releasingChildren=true;

    Base::List::iterator iter=m_children.begin();

    while(iter != m_children.end())
    {
        Base *child=*iter;
        iter=m_children.erase(iter);
        delete child;
    }

    // the menus left are off the canvas, and outlive it
    for(Menu *menu : m_anchoredMenus)
        menu->m_followedBy=nullptr;
    m_anchoredMenus.clear();
    // the modals left are off the canvas
    m_modals.clear();
    m_modalFocusPending=false;

    m_releasingChildren=false;
}

bool Canvas::inputMouseMoved(int x, int y, int deltaX, int deltaY)
{
    if(m_inputCapture)
    {
        m_inputCapture->captureMouseMoved(x, y, deltaX, deltaY);
        return true;
    }

    if(hidden())
        return false;

    // x and y are logical units already: the platform input layer maps window
    // coordinates through the scale (input::GLFW3::setWindowMetrics).
    input::onMouseMoved(this, x, y, deltaX, deltaY);

    if(!HoveredControl)
        return false;

    if(HoveredControl == this)
        return false;

    if(HoveredControl->getCanvas() != this)
        return false;

    // a stale hover under a modal; a capture's holder still gets its drag
    if((HoveredControl != MouseFocus) && !modalAllows(HoveredControl))
        return false;

    HoveredControl->onMouseMoved(x, y, deltaX, deltaY);
    HoveredControl->updateCursor();
    draganddrop::onMouseMoved(HoveredControl, x, y);
    return true;
}

// A button the canvas pressed before its input capture was set and has not
// let go yet (input:: tracks the left and right ones).
static bool heldBeforeCapture(int button)
{
    if(button == 0)
        return input::isLeftMouseDown();

    if(button == 1)
        return input::isRightMouseDown();

    return false;
}

bool Canvas::inputMouseButton(int button, bool down)
{
    if(m_inputCapture)
    {
        // A press the canvas took before the capture was set ends on the
        // canvas, where the canvas has the mouse, so nothing stays held there.
        InputCapture *capture=m_inputCapture;

        if(!down && heldBeforeCapture(button))
            input::onMouseClicked(this, button, false);

        // The release's handler may have cleared or replaced the capture (a
        // button that closes the depth view deletes it).
        if(m_inputCapture == capture)
            capture->captureMouseButton(button, down);
        return true;
    }

    if(hidden())
        return false;

    // A control that takes the press as a gesture of its own (a ZoomPanel's
    // pan) gets it, and the canvas's input with it, before anything under it.
    if(down && !input::isLeftMouseDown() && !input::isRightMouseDown()
        && (!MouseFocus || (MouseFocus->getCanvas() != this))
        && HoveredControl && (HoveredControl->getCanvas() == this)
        && modalAllows(HoveredControl))
    {
        for(Base *control=HoveredControl; control; control=control->getParent())
        {
            if(InputCapture *gesture=control->pressGesture(button))
            {
                // a press all the same: it closes what one there closes
                closeMenus();
                popupPressed(HoveredControl);
                setInputCapture(gesture);
                gesture->captureMouseButton(button, true);
                return true;
            }
        }
    }

    // The next update phase hides it, and shows none for this control again
    // until the mouse has been on another one. The control is the one under
    // the mouse now (input::onMouseMoved keeps HoveredControl current), not
    // m_toolTipOwner, which a move in this same frame has not updated yet.
    if(down)
    {
        if(Base *pressed=toolTipOwnerFor(HoveredControl))
            m_toolTipSuppressed=pressed;
    }

    return input::onMouseClicked(this, button, down);
}

//------------------------------------------------------------------------------
// Tooltip
//------------------------------------------------------------------------------

Base *Canvas::toolTipOwnerFor(Base *hovered)
{
    if(!hovered || (hovered == this) || (hovered->getCanvas() != this))
        return nullptr;

    Base *owner=nullptr;

    if(hovered->hasToolTip())
        owner=hovered;
    else if(hovered->getParent() && hovered->getParent()->hasToolTip())
        owner=hovered->getParent();

    if(!owner || !owner->visible() || owner->isDisabled() || !modalAllows(owner))
        return nullptr;

    return owner;
}

void Canvas::updateToolTip()
{
    Base *owner=toolTipOwnerFor(HoveredControl);

    if(m_toolTipSuppressed)
    {
        if(owner == m_toolTipSuppressed)
            owner=nullptr;
        else
            m_toolTipSuppressed=nullptr;
    }

    const double now=platform::getTimeInSeconds();

    if(owner != m_toolTipOwner)
    {
        // straight from one tooltip to the next: no second wait
        const bool reshow=m_toolTipShown && owner;

        hideToolTip();
        m_toolTipOwner=owner;
        m_toolTipSince=now;
        m_toolTipDirty=false;

        if(reshow)
            showToolTipFor(owner);
        return;
    }

    if(!owner)
        return;

    if(!m_toolTipShown)
    {
        if(now-m_toolTipSince >= m_toolTipDelay)
            showToolTipFor(owner);
    }
    else if(m_toolTipDirty)
        showToolTipFor(owner);
    else
        // the control moved (a scroll, a layout change): the box follows
        m_toolTip->placeFor(owner, Rect(0, 0, getWidth(), getHeight()));

    m_toolTipDirty=false;
}

void Canvas::showToolTipFor(Base *owner)
{
    // A tool's tooltip (its owner in a setExcludedFromStats() subtree) is the
    // tool's work: the box is flagged before it changes, from its creation on,
    // and stays so through the hide, until it shows an app control's.
    const bool excluded=owner->statsExcluded();

    if(!m_toolTip)
    {
        // as newChild(), but flagged before it is attached, and for a tool's
        // tooltip with this canvas's share of the attach (its own invalidate
        // and z mark) counted as the tool's too
        m_toolTip=newControl<ToolTip>();
        m_toolTip->setExcludedFromStats(excluded);

        const ExcludedFromStatsScope scope(excluded);

        m_toolTip->setParent(this);
        m_toolTip->recursiveInit("");
        m_toolTip->setSkin(m_skin);
    }
    else
        m_toolTip->setExcludedFromStats(excluded);
    m_toolTipShownForId=owner->getControlId();

    m_toolTip->setContentFor(owner, Rect(0, 0, getWidth(), getHeight()));
    m_toolTip->setHidden(false);
    m_toolTipShown=true;
}

void Canvas::hideToolTip()
{
    if(!m_toolTipShown)
        return;

    m_toolTipShown=false;
    m_toolTip->setHidden(true);
    m_toolTip->releaseContent(false);
}

void Canvas::toolTipChanged(Base *control)
{
    if(control == m_toolTipOwner)
        m_toolTipDirty=true;
}

void Canvas::toolTipControlDeleted(Base *control)
{
    if(control == m_toolTip)
    {
        // ~ToolTip has given back what it held
        m_toolTip=nullptr;
        m_toolTipShown=false;
    }

    if(control == m_toolTipOwner)
    {
        if(m_toolTipShown)
        {
            m_toolTipShown=false;
            m_toolTip->setHidden(true);
            // the owner's setToolTip() control goes with it
            m_toolTip->releaseContent(true);
        }
        m_toolTipOwner=nullptr;
    }

    if(control == m_toolTipSuppressed)
        m_toolTipSuppressed=nullptr;
}

//------------------------------------------------------------------------------
// Popup
//------------------------------------------------------------------------------

namespace
{
//! \p control is \p root or inside it.
bool within(Base *control, Base *root)
{
    for(; control; control=control->getParent())
    {
        if(control == root)
            return true;
    }
    return false;
}

//! A Menu that floats on the canvas (a ComboBox's list, a submenu) and closes
//! with closeMenus(); not a MenuStrip, a bar laid out with the rest that never
//! closes.
bool floatingMenu(Base *control)
{
    return gweni_cast<Menu>(control) && !gweni_cast<MenuStrip>(control);
}
}

bool Canvas::overPopup(Base *control)
{
    if(!m_popup || m_popupIsMenu)
        return false;

    // the canvas's child it is in
    while(control && (control->getParent() != this))
        control=control->getParent();

    if(!control || !floatingMenu(control) || control->hidden())
        return false;

    // opened after the popup (bringToFront), so over it
    Base::List::iterator popup=std::find(m_children.begin(), m_children.end(), m_popup);
    return std::find(popup, m_children.end(), control) != m_children.end();
}

void Canvas::showPopup(Popup *popup, Base *anchor, PopupPlacement side)
{
    closePopup();

    if(popup->getParent() != this)
        popup->setParent(this);

    m_popup=popup;
    m_popupAnchor=anchor;
    m_popupIsMenu=false;
    popup->setAnchor(anchor, side);
    popup->setHidden(false);
    // last among the children, before the ToolTip box (keepOverlaysLast)
    popup->bringToFront();
    popup->invalidate();
    popup->onOpen.call(popup);
}

void Canvas::closePopup()
{
    if(!m_popup)
        return;

    if(m_popupIsMenu)
    {
        closeContextMenu();
        return;
    }

    Popup *popup=static_cast<Popup *>(m_popup);

    m_popup=nullptr;
    m_popupAnchor=nullptr;
    popup->setAnchor(nullptr, popup->getPlacement());
    // a ComboBox's list it opened goes with it
    closeMenus();
    popup->setHidden(true);
    popup->onClose.call(popup);
}

bool Canvas::escapePopup()
{
    if(!m_popup)
        return false;

    bool menusOnly=false;

    if(!m_popupIsMenu)
    {
        for(Base *child : m_children)
        {
            if(overPopup(child))
            {
                menusOnly=true;
                break;
            }
        }
    }

    if(menusOnly)
        closeMenus();
    else
        closePopup();

    return true;
}

void Canvas::popupPressed(Base *hovered)
{
    if(!m_popup || m_popupIsMenu)
        return;

    // A press on the anchor is the anchor's (a button toggling its popup); one
    // on a Menu opened over the popup is on a ComboBox's list in it, say. A
    // menu bar is outside it like any other control.
    if(hovered && (within(hovered, m_popup) || within(hovered, m_popupAnchor)
        || overPopup(hovered)))
        return;

    closePopup();
}

void Canvas::updatePopup()
{
    if(!m_popup)
        return;

    if(m_popupIsMenu && m_popup->hidden())
    {
        // closed as menus are (a click outside, an item chosen)
        m_popup=nullptr;
        m_popupAnchor=nullptr;
        m_popupIsMenu=false;
        m_contextMenu=nullptr;
    }
    else if(!m_popupAnchor->visible() || m_popupAnchor->isDisabled())
        closePopup();
    else if(!m_popupIsMenu)
        static_cast<Popup *>(m_popup)->followAnchor();
}

void Canvas::followAnchoredMenu(Menu *menu)
{
    if(menu->m_followedBy == this)
        return;

    if(menu->m_followedBy)
        menu->m_followedBy->anchoredMenuDeleted(menu);

    menu->m_followedBy=this;
    m_anchoredMenus.push_back(menu);
}

void Canvas::anchoredMenuDeleted(Menu *menu)
{
    std::vector<Menu *>::iterator iter=std::find(m_anchoredMenus.begin(), m_anchoredMenus.end(), menu);

    if(iter != m_anchoredMenus.end())
        m_anchoredMenus.erase(iter);
    menu->m_followedBy=nullptr;
}

void Canvas::menuAnchorDeleted(Base *control)
{
    // Closing changes no entry: a closed menu leaves in the next update phase.
    // A canvas tearing down only forgets the anchor: what a close redraws (the
    // item a submenu belongs to) may be gone.
    for(Menu *menu : m_anchoredMenus)
    {
        if(menu->m_anchor != control)
            continue;

        menu->m_anchor=nullptr;

        if(!m_releasingChildren)
        {
            menu->close();
            menu->closeAll();
        }
    }
}

// Between passes, so not a re-dirty. A pass may move an anchor without
// laying it out or any ancestor of it below the canvas (the canvas's docking
// moves a top-level panel), so a follower is entered in every pass the app
// runs, placed from where that pass puts its anchor: it comes after the
// anchor's top-level ancestor among the canvas's children, and a submenu
// after the menu holding its item. A pass run for setExcludedFromStats()
// subtrees alone (an Inspector's or a StatsOverlay's refresh) moves no
// anchor, as those are placed after the app and hold none, and enters none.
void Canvas::enterFollowers()
{
    if(!layoutEntryCounted())
        return;

    if(m_popup && !m_popupIsMenu && !m_popup->hidden())
        m_popup->invalidate();

    for(Menu *menu : m_anchoredMenus)
    {
        if(menu->m_anchor && !menu->hidden() && (menu->getParent() == this))
            menu->invalidate();
    }
}

// A menu closed or placed some other way since the last frame leaves the
// list here; Menu::followAnchor closes one whose anchor is hidden.
void Canvas::updateAnchoredMenus()
{
    for(size_t i=0; i < m_anchoredMenus.size();)
    {
        Menu *menu=m_anchoredMenus[i];

        if(menu->followAnchor(this))
        {
            ++i;
            continue;
        }

        menu->m_followedBy=nullptr;
        m_anchoredMenus.erase(m_anchoredMenus.begin()+i);
    }
}

void Canvas::keepOverlaysLast()
{
    keepModalsInPlace();

    if(m_popup && !m_popupIsMenu && (m_popup->getParent() == this))
    {
        // the Menus after it were opened over it (a ComboBox's list in it)
        Base::List after;
        bool stays=false;
        Base::List::iterator iter=std::find(m_children.begin(), m_children.end(), m_popup);

        for(++iter; iter != m_children.end();)
        {
            if((*iter != m_toolTip) && !floatingMenu(*iter))
            {
                stays=true;
                ++iter;
                continue;
            }
            after.push_back(*iter);
            iter=m_children.erase(iter);
        }

        // only what passed a control that stays gives up its z labels
        if(stays)
        {
            zOrderMoved(m_popup);

            for(Base *moved : after)
                zOrderMoved(moved);
        }
        m_children.remove(m_popup);
        m_children.push_back(m_popup);
        m_children.splice(m_children.end(), after);
    }

    if(!m_overlays.empty())
    {
        // in place when the children end with the overlays, in order, and the
        // ToolTip box (if any) after them
        Base::List::reverse_iterator tail=m_children.rbegin();

        if((tail != m_children.rend()) && (*tail == m_toolTip))
            ++tail;

        bool inPlace=true;

        for(auto overlay=m_overlays.rbegin(); overlay != m_overlays.rend(); ++overlay)
        {
            if((*overlay)->getParent() != this)
                continue;
            if((tail == m_children.rend()) || (*tail != *overlay))
            {
                inPlace=false;
                break;
            }
            ++tail;
        }

        if(!inPlace)
        {
            for(Base *overlay : m_overlays)
            {
                if(overlay->getParent() != this)
                    continue;
                m_children.remove(overlay);
                m_children.push_back(overlay);
                zOrderMoved(overlay);
            }
        }
    }

    if(!m_toolTip || (m_children.back() == m_toolTip))
        return;

    m_children.remove(m_toolTip);
    m_children.push_back(m_toolTip);
    zOrderMoved(m_toolTip);
}

void Canvas::addOverlay(Base *control)
{
    if(std::find(m_overlays.begin(), m_overlays.end(), control) != m_overlays.end())
        return;
    m_overlays.push_back(control);
    // moved into place by the next renumber
    m_zOrderDirty=true;
}

void Canvas::removeOverlay(Base *control)
{
    m_overlays.erase(std::remove(m_overlays.begin(), m_overlays.end(), control), m_overlays.end());
}

void Canvas::overlayDeleted(controls::Base *control)
{
    if(!m_overlays.empty())
        removeOverlay(control);
}

void Canvas::popupControlDeleted(Base *control)
{
    if(!m_popup)
        return;

    if(control == m_popup)
    {
        // a deleted popup fires no onClose
        m_popup=nullptr;
        m_popupAnchor=nullptr;
        m_popupIsMenu=false;
        m_contextMenu=nullptr;
        return;
    }

    if(control != m_popupAnchor)
        return;

    if(m_releasingChildren)
    {
        // A canvas tearing down only hides it: the submenus closeAll() reaches,
        // or what an onClose handler reads, may already be gone. The owner
        // deletes a menu it owns itself.
        m_popup->setHidden(true);
        m_popup=nullptr;
        m_popupAnchor=nullptr;
        m_popupIsMenu=false;
        m_contextMenu=nullptr;
    }
    else
        closePopup();
}

//------------------------------------------------------------------------------
// Context menu
//------------------------------------------------------------------------------

Base *Canvas::contextMenuOwnerFor(Base *hovered)
{
    if(!hovered || (hovered->getCanvas() != this) || hovered->isMenuComponent()
        || !modalAllows(hovered))
        return nullptr;

    for(Base *control=hovered; control; control=control->getParent())
    {
        if(!control->getContextMenu())
            continue;

        if(!control->visible() || control->isDisabled())
            return nullptr;
        return control;
    }
    return nullptr;
}

void Canvas::showContextMenu(Base *owner, const Point &at)
{
    closePopup();

    // the handler may fill the menu in, or replace or clear it
    owner->onContextMenu.call(owner);

    Menu *menu=owner->getContextMenu();

    if(!menu)
        return;

    if(menu->getParent() != this)
        menu->setParent(this);

    menu->openAt(at);
    m_popup=menu;
    m_popupAnchor=owner;
    m_popupIsMenu=true;
    m_contextMenu=menu;
}

void Canvas::closeContextMenu()
{
    if(!m_popupIsMenu)
        return;

    Menu *menu=m_contextMenu;

    m_popup=nullptr;
    m_popupAnchor=nullptr;
    m_popupIsMenu=false;
    m_contextMenu=nullptr;
    menu->closeAll();
    menu->close();
}

//------------------------------------------------------------------------------
// Modal stack
//------------------------------------------------------------------------------

bool Canvas::isModal(const Base *control) const
{
    for(const ModalEntry &entry : m_modals)
    {
        if(entry.control == control)
            return true;
    }
    return false;
}

bool Canvas::aboveModals(Base *child) const
{
    if((child == m_popup) || (child == m_toolTip) || floatingMenu(child) || gweni_cast<Popup>(child))
        return true;

    if(std::find(m_overlays.begin(), m_overlays.end(), child) != m_overlays.end())
        return true;

    // a tool's (an Inspector's window)
    return child->statsExcluded();
}

bool Canvas::modalAllows(Base *control)
{
    Base *top=topModal();

    if(!top || !control)
        return true;

    // the canvas's child it is in
    Base *child=control;

    while(child && (child->getParent() != this))
        child=child->getParent();

    if(!child)
        return control != this;

    return (child == top) || (!isModal(child) && aboveModals(child));
}

void Canvas::pushModal(Base *control, Base *focusReturn)
{
    if(!control || (control == this) || isModal(control))
        return;

    if(control->getParent() != this)
        control->setParent(this);

    size_t focusId=0;

    if(focusReturn)
        focusId=focusReturn->getControlId();
    else if(KeyboardFocus && (KeyboardFocus->getCanvas() == this) && !within(KeyboardFocus, control))
        focusId=KeyboardFocus->getControlId();
    else if(m_modalFocusPending)
        // the top modal was deleted with the focus in it, and its focus
        // has not come back yet: this one gives it back instead
        focusId=m_modalFocusRestore;

    // what was open over the app goes, as for a press outside it
    closePopup();
    closeMenus();

    m_modals.push_back({control, focusId});

    if(KeyboardFocus && (KeyboardFocus->getCanvas() == this) && !modalAllows(KeyboardFocus))
        KeyboardFocus->unfocus();

    keepOverlaysLast();
    // the control under the mouse may be under the modal now
    hoverMayHaveMoved();
    redraw();
}

void Canvas::popModal(Base *control)
{
    for(size_t i=0; i < m_modals.size(); ++i)
    {
        if(m_modals[i].control != control)
            continue;

        const bool top=(i+1 == m_modals.size());
        const size_t focusId=m_modals[i].focusId;

        eraseModal(i);

        if(m_releasingChildren)
            return;

        // a modal left shown is an app child now, which the ones below go over
        if(!m_modals.empty() && control->visible())
            m_zOrderDirty=true;

        if(top)
            restoreModalFocus(focusId, control);
        hoverMayHaveMoved();
        redraw();
        return;
    }
}

void Canvas::restoreModalFocus(size_t focusId, Base *leaving)
{
    Base *focus=KeyboardFocus;

    if(focus && (focus->getCanvas() != this))
        return;

    // the app (or the modal below) moved the focus on itself: it stays
    if(focus && !(leaving && within(focus, leaving)) && modalAllows(focus))
        return;

    Base *target=focusId ? Base::getControlById(focusId) : nullptr;

    if(target && (target->getCanvas() == this) && target->visible() && modalAllows(target))
        target->focus();
    else if(focus && !modalAllows(focus))
        focus->unfocus();
}

void Canvas::eraseModal(size_t index)
{
    if(index+1 < m_modals.size())
    {
        // the one above took its focus from this one: it gives back this
        // one's instead (its subtree is still there, also when it is deleted)
        ModalEntry &above=m_modals[index+1];
        Base *recorded=above.focusId ? Base::getControlById(above.focusId) : nullptr;

        if(!recorded || within(recorded, m_modals[index].control))
            above.focusId=m_modals[index].focusId;
    }
    m_modals.erase(m_modals.begin()+index);
}

void Canvas::modalDeleted(Base *control)
{
    for(size_t i=0; i < m_modals.size(); ++i)
    {
        if(m_modals[i].control != control)
            continue;

        const bool top=(i+1 == m_modals.size());

        if(top && !m_releasingChildren)
        {
            // its subtree is not gone yet: the focus comes back once it is
            m_modalFocusPending=true;
            m_modalFocusRestore=m_modals[i].focusId;
        }
        eraseModal(i);

        if(!m_releasingChildren)
        {
            hoverMayHaveMoved();
            redraw();
        }
        return;
    }
}

void Canvas::updateModals()
{
    for(size_t i=m_modals.size(); i-- > 0;)
    {
        if(i >= m_modals.size())
            continue;

        Base *control=m_modals[i].control;

        if(!control->visible() || (control->getParent() != this))
            popModal(control);
    }

    if(m_modalFocusPending)
    {
        m_modalFocusPending=false;
        restoreModalFocus(m_modalFocusRestore, nullptr);
        m_modalFocusRestore=0;
    }
}

void Canvas::onChildHiddenChanged(Base *child)
{
    ParentClass::onChildHiddenChanged(child);

    // shown after the modals were put in place: they go over it again
    if(!m_modals.empty() && !child->hidden() && !isModal(child) && !aboveModals(child))
        m_zOrderDirty=true;
}

void Canvas::onChildRemoved(Base *child)
{
    ParentClass::onChildRemoved(child);

    if(isModal(child))
        popModal(child);
}

void Canvas::keepModalsInPlace()
{
    if(m_modals.empty())
        return;

    // the last shown app child, which the modals go right after
    Base::List::reverse_iterator last=m_children.rbegin();

    for(; last != m_children.rend(); ++last)
    {
        Base *child=*last;

        if(!child->hidden() && !isModal(child) && !aboveModals(child))
            break;
    }

    // in place when they follow it in stack order, with only hidden
    // children between them
    Base::List::iterator iter=last.base();
    bool inPlace=true;

    for(const ModalEntry &entry : m_modals)
    {
        if(entry.control->getParent() != this)
            continue;

        while((iter != m_children.end()) && (*iter != entry.control)
            && (*iter)->hidden() && !isModal(*iter))
            ++iter;

        if((iter == m_children.end()) || (*iter != entry.control))
        {
            inPlace=false;
            break;
        }
        ++iter;
    }

    if(inPlace)
        return;

    const bool front=(last == m_children.rend());
    Base::List::iterator anchor=front ? m_children.end() : std::prev(last.base());

    for(const ModalEntry &entry : m_modals)
    {
        if(entry.control->getParent() == this)
            m_children.remove(entry.control);
    }

    Base::List::iterator at=front ? m_children.begin() : std::next(anchor);

    for(const ModalEntry &entry : m_modals)
    {
        if(entry.control->getParent() != this)
            continue;

        m_children.insert(at, entry.control);
        zOrderMoved(entry.control);
    }
}

Base *Canvas::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    Base *top=topModal();

    if(!top || (top->getParent() != this))
        return ParentClass::getControlAt(x, y, bOnlyIfMouseEnabled);

    countHitTestVisit();

    if(hidden())
        return nullptr;

    if(x < 0 || y < 0 || x >= getWidth() || y >= getHeight())
        return nullptr;

    // the top modal and what is above it, from the front; nothing under it
    for(Base::List::reverse_iterator iter=m_children.rbegin(); iter != m_children.rend(); ++iter)
    {
        Base *child=*iter;
        const bool isTop=(child == top);

        if(isTop || (!isModal(child) && aboveModals(child)))
        {
            if(Base *found=child->getControlAt(x-child->getX(), y-child->getY(), bOnlyIfMouseEnabled))
                return found;
        }

        if(isTop)
            break;
    }

    if(bOnlyIfMouseEnabled && !getMouseInputEnabled())
        return nullptr;

    return this;
}

//------------------------------------------------------------------------------
// Toasts
//------------------------------------------------------------------------------

namespace
{
int toastKindIndex(ToastKind kind)
{
    return std::max(0, std::min(3, static_cast<int>(kind)));
}
}

Toast *Canvas::notify(ToastKind kind, const String &text)
{
    return notify(kind, text, ToastOptions());
}

Toast *Canvas::notify(ToastKind kind, const String &text, const ToastOptions &options)
{
    if(!m_toastHost)
    {
        m_toastHost=newChild<internal::ToastHost>();
        // with the overlays: above the modals, popups and menus, below the
        // ToolTip box
        addOverlay(m_toastHost);
    }

    // the oldest goes first, hovered or not
    std::vector<Toast *> shown=toasts();

    for(size_t i=0; (i < shown.size()) && (shown.size()-i >= m_toastLimit); ++i)
        shown[i]->dismiss();

    const double timeout=(options.timeout < 0.0) ? m_toastTimeouts[toastKindIndex(kind)] : options.timeout;
    const double deadline=(timeout > 0.0) ? toastClock()+timeout : NoWork;

    Toast *toast=m_toastHost->newChild<Toast>();

    toast->setup(kind, text, options.title, timeout, deadline);
    if(deadline < m_toastDeadline)
        m_toastDeadline=deadline;
    return toast;
}

void Canvas::setToastTimeout(ToastKind kind, double seconds)
{
    m_toastTimeouts[toastKindIndex(kind)]=std::max(0.0, seconds);
}

double Canvas::getToastTimeout(ToastKind kind) const
{
    return m_toastTimeouts[toastKindIndex(kind)];
}

void Canvas::setToastLimit(size_t limit)
{
    m_toastLimit=std::max<size_t>(1, limit);
}

std::vector<Toast *> Canvas::toasts() const
{
    std::vector<Toast *> shown;

    if(!m_toastHost)
        return shown;

    for(Base *child : m_toastHost->getChildren())
    {
        Toast *toast=gweni_cast<Toast>(child);

        if(toast && !toast->isDismissed())
            shown.push_back(toast);
    }
    return shown;
}

double Canvas::toastClock() const
{
    const double now=platform::getTimeInSeconds();
    const double paused=m_toastPausedTotal+(m_toastPaused ? now-m_toastPausedSince : 0.0);

    return now-paused;
}

bool Canvas::inToastHost(Base *control) const
{
    return m_toastHost && control && within(control, m_toastHost);
}

void Canvas::toastRemoved(Toast *toast)
{
    m_toastDeadlineDirty=true;
    // what was under the mouse may be another control now (the next toast,
    // or the app between them)
    hoverMayHaveMoved();
    redraw();
}

void Canvas::findToastDeadline()
{
    m_toastDeadline=NoWork;
    m_toastDeadlineDirty=false;

    for(Toast *toast : toasts())
        m_toastDeadline=std::min(m_toastDeadline, toast->getDeadline());
}

void Canvas::toastControlDeleted(Base *control)
{
    if(control == m_toastHost)
    {
        m_toastHost=nullptr;
        m_toastDeadline=NoWork;
        m_toastDeadlineDirty=false;
        m_toastPaused=false;
        return;
    }

    if(m_toastHost && (control->getParent() == m_toastHost))
        m_toastDeadlineDirty=true;
}

void Canvas::updateToasts()
{
    if(!m_toastHost)
        return;

    if(m_toastDeadlineDirty)
        findToastDeadline();

    // The pause: while the hover is in the host, the toast clock stands
    // still; leaving adds the time it stood to the paused total.
    const bool hovered=inToastHost(HoveredControl) && (m_toastDeadline != NoWork);
    const double now=platform::getTimeInSeconds();

    if(hovered != m_toastPaused)
    {
        if(hovered)
            m_toastPausedSince=now;
        else
            m_toastPausedTotal+=now-m_toastPausedSince;
        m_toastPaused=hovered;
    }

    // one comparison a frame while nothing is due
    if(m_toastPaused || (toastClock() < m_toastDeadline))
        return;

    const double clock=toastClock();

    for(Toast *toast : toasts())
    {
        if(toast->getDeadline() <= clock)
            toast->dismiss();
    }
    findToastDeadline();
}

bool Canvas::inputModifierKey(int key, bool down)
{
    if(m_inputCapture)
    {
        // As for a press: a key the canvas has down goes up there, so it does
        // not repeat once the capture is gone.
        InputCapture *capture=m_inputCapture;

        if(!down && (key > keys::Invalid) && (key < keys::KeysCount)
            && input::isKeyDown(key))
            input::onKeyEvent(this, key, false);

        if(m_inputCapture == capture)
            capture->captureKey(key, down);
        return true;
    }

    if(hidden())
        return false;

    if(key <= keys::Invalid)
        return false;

    if(key >= keys::KeysCount)
        return false;

    return input::onKeyEvent(this, key, down);
}

bool Canvas::inputCharacter(UnicodeChar chr)
{
    if(m_inputCapture)
    {
        m_inputCapture->captureCharacter(chr);
        return true;
    }

    if(hidden())
        return false;

    // Only a codepoint that is text gets in: no C0 controls (backspace, tab,
    // ...), DEL or C1 controls, no surrogate (half of a UTF-16 pair, not a
    // codepoint) and nothing past U+10FFFF.
    if(chr < 0x20 || (chr >= 0x7F && chr <= 0x9F)
        || (chr >= 0xD800 && chr <= 0xDFFF) || chr > 0x10FFFF)
        return false;

    // Handle Accelerators
    if(input::handleAccelerator(this, chr))
        return true;

    // Handle characters
    if(!KeyboardFocus)
        return false;

    if(KeyboardFocus->getCanvas() != this)
        return false;

    if(!KeyboardFocus->visible())
        return false;

    if(input::isControlDown())
        return false;

    return KeyboardFocus->onChar(chr);
}

bool Canvas::inputMouseLeave()
{
    if(m_inputCapture)
    {
        m_inputCapture->captureMouseLeave();
        return true;
    }

    if(hidden())
        return false;

    input::onMouseLeft(this);
    return true;
}

bool Canvas::inputMouseWheel(int val)
{
    if(m_inputCapture)
    {
        m_inputCapture->captureMouseWheel(val);
        return true;
    }

    if(hidden())
        return false;

    if(!HoveredControl)
        return false;

    if(HoveredControl == this)
        return false;

    if(HoveredControl->getCanvas() != this)
        return false;

    if(!modalAllows(HoveredControl))
        return false;

    return HoveredControl->onMouseWheeled(val);
}

}//namespace controls
}//namespace gweni
