/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_base_h_
#define _gweni_controls_base_h_

#include <limits>

#include <gweni/config.h>
#include <gweni/export.h>
#include <gweni/platforms/baseRender.h>

#include <gweni/uiTypes.h>
#include <gweni/events.h>
#include <gweni/utility.h>
#include <gweni/skin.h>
#include <gweni/controlList.h>
#include <gweni/userData.h>
#include <gweni/primitiveIds.h>

#include <list>
#include <map>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// Verbose per-control debug output. Enabled by the GWENI_DEBUG_LOGGING cmake
// option; undefined (so: off) for consumers building without it.
#ifdef GWENI_DEBUG_LOGGING
#include <cstdio>
#define DEBUG_ID
#define DEBUG_LAYOUT
#define logUpdates(...) ::printf(__VA_ARGS__)
#else
#define logUpdates(...)
#endif

namespace gweni
{
namespace controls
{

size_t countParents(Base *base);
std::string spaceByParent(Base *base);
size_t getControlType(std::string name);
std::string getFullName(Base *base);

class Canvas;
class LayoutItem;
struct UpdateStats;
class Menu;

constexpr int StateChange_Nothing=0x0000;
constexpr int StateChange_Created=0x0001;
constexpr int StateChange_Visibility=0x0002;
constexpr int StateChange_Geometry=0x0004;
constexpr int StateChange_Background=0x0008;
constexpr int StateChange_Text=0x0010;
//! Only a TextBox's caret blinked: its skin shows or hides the caret primitive
//! and rewrites nothing else.
constexpr int StateChange_Caret=0x0020;

//! Tally of the controls Base::render() found something to regenerate for,
//! plus the controls the render walk entered at all. This is what "per-frame
//! cost scales with what changed" means, measured: on a settled frame every
//! field is zero, except visited, which is at most 1 (the Canvas), and
//! childrenScanned, then at most the Canvas's child count.
struct ChangeStats
{
    //! controls Base::renderRecursive() entered: the changed ones and their
    //! ancestors; doRender() skips a subtree with nothing pending in it
    size_t visited=0;
    //! children the walk's loop passed over in the controls it entered, each
    //! handed to doRender() whether or not it had anything pending: the walk
    //! is still O(siblings) of every entered control in time (it allocates
    //! nothing for them)
    size_t childrenScanned=0;
    size_t controls=0;      //!< controls with getStateChange() != StateChange_Nothing
    size_t created=0;
    size_t visibility=0;
    size_t geometry=0;
    size_t background=0;
    size_t text=0;
};

//! Changes counted since the last resetChangeStats().
GWENI_EXPORT const ChangeStats &getChangeStats();
GWENI_EXPORT void resetChangeStats();

//! The control types setSkin() found no skin renderer for: its walk up the
//! parent types reached the skin's Base entry, for a type that is not Base and
//! not drawsNothing(). Each is printed once on std::cerr when first seen
//! ("gweni: no skin renderer for <TypeName>"); this is the set, sorted, for a
//! harness to fail on.
GWENI_EXPORT std::vector<std::string> typesWithoutSkinRenderer();

//! The renderer::Base::measureText() and lineHeight() calls the controls made
//! since the process started, every one going through Base::measureText() or
//! Base::measureLineHeight(); a renderer's default lineHeight() is one
//! measureText(), so a renderer counting its measures sees the same number. It
//! is counted here, in the controls, rather than in a renderer, so it is the
//! same whichever backend measures; a canvas takes its frame's share as
//! LayoutStats::measureCalls.
GWENI_EXPORT size_t measureTextCalls();

//! The tree changes made during a render walk: the render walk only draws, and
//! adding, removing, reordering or deleting a control belongs to the update
//! phase or to layout. Base's addChild(), addChildOfLayout(), removeChild()
//! (and so setParent()), bringToFront(), sendToBack(), bringNextToControl()
//! and ~Base report a call made while a RenderWalkScope is open: one line on
//! std::cerr, "gweni: <Type>#<id> <operation> during the render walk
//! (rendering <Type>#<id>)", the first time each (operation, type) pair is
//! seen, in every build. ~Base sees the type as Base (the derived destructors
//! have run). The walk carries on over the changed list, so what that frame
//! does next is undefined. This is the set of the reported pairs, sorted, each
//! "<Type> <operation> (rendering <Type>)", for a harness to fail on.
GWENI_EXPORT std::vector<std::string> renderWalkMutations();
//! Forgets the reported pairs, so the next of each is printed and recorded
//! again (for tests).
GWENI_EXPORT void resetRenderWalkMutations();

//! Open while a canvas walks its tree to draw it (Canvas::renderCanvas() and
//! WindowCanvas's render, around doRender() only: renderPhaseLayout() runs
//! before it and may create controls, which is layout). Scopes nest.
struct GWENI_EXPORT RenderWalkScope
{
    RenderWalkScope();
    ~RenderWalkScope();
    RenderWalkScope(const RenderWalkScope &)=delete;
    RenderWalkScope &operator=(const RenderWalkScope &)=delete;
};

size_t getControlType(std::string name);

template<typename _Control>
_Control *newControl(const gweni::String &name="")
{
    _Control *control=_Control::newInstance(name);

    return control;
}
//! Who places a child in its parent (Base::setPlacement).
enum class Placement
{
    //! The generic arrange: the child's dock, alignment and size flags, or
    //! the parent's LayoutItem. Its size counts in the parent's.
    Parent,
    //! The control that owns it, which writes its bounds through
    //! Base::placeChild: the generic arrange leaves it alone, and neither its
    //! position nor its size counts in the parent's preferred size.
    Owner
};

//
//! This is the base class for all Gweni controls.
//
class GWENI_EXPORT Base: public event::Handler
{
public:
    friend class LayoutItem;

    typedef std::list<Base *> List;

    typedef std::map<gweni::String, gweni::event::Caller *> AccelMap;

protected:
    Base(const gweni::String &name="");

public:
    virtual ~Base();

    static Base *newInstance(const gweni::String &name)
    {
        Base *instance=new Base(name);
        instance->recursiveInit(name);
        return instance;
    }

    void recursiveInit(const gweni::String &name)
    {
        init(name);
    }
    void init(const gweni::String &name);

    static const char *getTypeNameStatic() { return "Base"; }
    virtual const char *getTypeName() const { return getTypeNameStatic(); }

    static const char *getControlNameStatic() { return "Base"; }
    virtual const char *getControlName() const { return getControlNameStatic(); }

    virtual const char *getParentTypeName() const { return nullptr; }
    virtual const char *getParentTypeName(size_t index) const { return nullptr; }

    //! True for a type that has no skin renderer by design (it paints nothing,
    //! or draws itself outside the skin), so setSkin() does not report it.
    //! Override it with GWENI_DRAWS_NOTHING, which holds for the declaring type
    //! only: a subclass with no renderer (SliderBar of Dragger) is still reported.
    virtual bool drawsNothing() const { return false; }

    virtual void delayedDelete();
    virtual void preDelete(gweni::skin::Base *skin) {}

    size_t getControlId()
    {
        return m_id;
    }

//Control hierarchy
    virtual void setParent(controls::Base *parent);
    virtual controls::Base *getParent() const
    {
        return m_parent;
    }
    virtual void removeAllChildren();

    virtual Base::List &getChildren()
    {
        if(m_innerPanel)
            return m_innerPanel->getChildren();

        return m_children;
    }

    //! The children actually parented to this control, inner panel and private
    //! parts (a ScrollControl's scrollbars) included; getChildren() skips past
    //! the inner panel to the children placed in it.
    const Base::List &getActualChildren() const
    {
        return m_children;
    }

    //! The panel getChildren() hands out in place of this control's own
    //! children (a ScrollControl's scrolled panel), or nullptr if it has none.
    const Base *getInnerPanel() const
    {
        return m_innerPanel;
    }

    template<typename _Control>
    friend _Control *newControl(const gweni::String &name);
    template<typename _Control>
    _Control *newChild(const gweni::String name="");

    virtual bool            isChild(controls::Base *child);
    virtual unsigned int    numChildren();
    virtual controls::Base *getChild(unsigned int i);
    virtual bool            sizeToChildren(bool w=true, bool h=true);
    virtual gweni::Point      childrenSize();
    virtual controls::Base *findChildByName(const gweni::String &name,
        bool recursive=false);

    template <class T>
    T *findChild(const gweni::String &name, bool recursive=false);


    virtual controls::Canvas *getCanvas();
    virtual void setDock(DockPosition dock);
    virtual DockPosition getDock() const;

    virtual Alignment getAlignment();
    virtual void setAlignment(Alignment alignment);

    virtual SizeFlags getSizeFlags();
    virtual void setSizeFlags(SizeFlags sizeFlags);

    virtual void setLayout(LayoutItem *layoutItem);
    virtual LayoutItem *getLayout();

    virtual void sendToBack(void);
    virtual void bringToFront(void);
    virtual void bringNextToControl(controls::Base *child, bool bBehind);

    virtual gweni::Point localPosToCanvas(const gweni::Point &in=Point(0, 0));
    virtual gweni::Point canvasPosToLocal(const gweni::Point &in);

//Attributes

    virtual void setName(const gweni::String &name) { m_name=name; }
    virtual const gweni::String &getName() { return m_name; }

protected:

    virtual void addChild(controls::Base *child);
    //used to add a child that is part of a layout not directly attached to parent
    virtual void addChildOfLayout(Base *child);
    virtual void removeChild(controls::Base *parent);
    virtual void onChildAdded(controls::Base *child);
    virtual void onChildRemoved(controls::Base *child);

public:
    
    virtual int getStateChange() { return m_stateChange; }
    //! Sets the bits and marks the path to the root (see
    //! m_childStateChanged), so the render walk reaches this control.
    virtual void addStateChange(int stateChange)
    {
        m_stateChange|=stateChange;
        if(stateChange != StateChange_Nothing)
            markAncestorsChanged();
    }
    virtual void addStateChangeRecursive(int stateChange);
    virtual bool hasState(int state) { return m_stateChange&state; }
    

    virtual void restrictToParent(bool restrict) { m_restrictToParent=restrict; }
    virtual bool shouldRestrictToParent() { return m_restrictToParent; }

    virtual int getX() const { return m_bounds.x; }  //!< Get X position of Control.
    virtual int getY() const { return m_bounds.y; }  //!< Get Y position of Control.

    virtual int getWidth() const { return m_bounds.w; }  //!< Get Control width.
    virtual int getHeight() const { return m_bounds.h; }  //!< Get Control height.

    virtual int getZIndex() const { return m_zIndex; }
    //! The first z value after this control's subtree, which it keeps for
    //! itself when its skin Control's overlayLayers() is not 0: what it draws
    //! there is in front of every one of its descendants. 0 otherwise.
    int getOverlayZIndex() const { return m_overlayZIndex; }

    virtual int bottom() const
    {
        return m_bounds.y+m_bounds.h+m_margin.bottom;
    }

    //! Get right boundary of control.
    virtual int right() const
    {
        return m_bounds.x+m_bounds.w+m_margin.right;
    }

    //! Get the margin of the control.
    virtual const Margin &getMargin() const { return m_margin; }

    //! Get the padding for the control.
    virtual const Padding &getPadding() const { return m_padding; }

    virtual void setPos(int x, int y);
    virtual void setPos(const Point &p) { return setPos(p.x, p.y); }
    virtual Point getPos() const { return Point(getX(), getY()); }

    virtual void setWidth(int w);
    virtual void setHeight(int h);

    virtual bool  setSize(int w, int h);
    virtual bool  setSize(const Point &p);
    virtual Point getSize() const { return Point(getWidth(), getHeight()); }

    //! A call from the app is a request: setPos/moveTo record the position,
    //! setSize/setWidth/setHeight the size, setBounds both. So is a
    //! constructor's default size. Bounds the library computes are not: the
    //! generic arrange code (the dock branches of arrangeHorizontal/Vertical,
    //! policyResize, a LayoutItem placing its control) writes them through
    //! setArrangedBounds, and a control placing a child it owns through
    //! placeChild. A dock-None control is sized from its request, its
    //! preferred size and its size flags, never from the bounds an earlier
    //! pass gave it (see policyResize).
    virtual bool setBounds(int x, int y, int w, int h);
    virtual bool setBounds(const gweni::Rect &bounds);

    //! The bounds last asked for by a request (see setBounds). Starts as the
    //! initial bounds; hasRequestedSize() says whether a size was ever asked.
    const gweni::Rect &getRequestedBounds() const { return m_requestedBounds; }
    bool hasRequestedSize(Dim dim) const { return dim == Dim::X?m_widthRequested:m_heightRequested; }

    //! Who places this control in its parent (see Placement). A composite
    //! declares the children it places itself Owner when it creates or adopts
    //! them, before anything measures them: the parent's calculateSize runs
    //! before its arrange, and must already leave them out.
    void setPlacement(Placement placement);
    Placement getPlacement() const { return m_placement; }

    virtual void setPadding(const Padding &padding);
    virtual void setMargin(const Margin &margin);

    //! MoveTo is identical to setPos except it uses shouldRestrictToParent().
    virtual void moveTo(int x, int y);
    //! Where moveTo(x, y) would put this control: (x, y), kept inside the
    //! parent when shouldRestrictToParent().
    Point restrictedPos(int x, int y) const;
    virtual void moveBy(int x, int y);

    virtual const gweni::Rect &getBounds() const { return m_bounds; }

    //            virtual void SetNeeded(const gweni::Rect &needed) { m_needed=needed; }
    //            virtual const gweni::Rect &GetNeeded() const { return m_needed; }
    virtual Size getMinimumSize();
    virtual Size getMaximumSize();

    virtual void setPreferredSize(const Size &size) { m_preferredSize=size; }
    virtual const Size getPreferredSize() const { return m_preferredSize; }

    //! The control under (x, y), in this control's coordinates: the last
    //! child in child order holding the point (inside its
    //! getChildClipRect() too, if it has one), searched the same way, else
    //! this control (nullptr if it is hidden, the point is outside it, or
    //! bOnlyIfMouseEnabled and its mouse input is off). Every call is one
    //! hit-test visit (input::hitTestVisits()).
    virtual controls::Base *getControlAt(int x, int y, bool bOnlyIfMouseEnabled=true);

protected:

    //! Counts one hit-test visit: an override of getControlAt() calls it once,
    //! on entry, whether it then searches its children itself or falls back
    //! to getControlAtWalk(), which counts nothing.
    static void countHitTestVisit();
    //! Base::getControlAt() without counting the visit: the hidden and bounds
    //! checks, every child in reverse child order, then this control.
    controls::Base *getControlAtWalk(int x, int y, bool bOnlyIfMouseEnabled);

    virtual void onBoundsChanged(gweni::Rect oldBounds);
    virtual void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child);
    //! A child's own hidden flag changed (its setHidden()).
    virtual void onChildHiddenChanged(Base * /*child*/) {}

    virtual void onScaleChanged();

public:

    //! InnerBounds is the area inside the control that
    //! doesn't have child controls docked to it.
    virtual const gweni::Rect &getInnerBounds() const { return m_innerBounds; }

protected:

    gweni::Rect m_innerBounds;

public:

    virtual const gweni::Rect &getRenderBounds() const { return m_renderBounds; }

protected:

    virtual void updateRenderBounds();

public:
    virtual void doRender(gweni::skin::Base *skin);
    virtual void doCacheRender(gweni::skin::Base *skin, gweni::controls::Base *master);
    virtual void renderRecursive(gweni::skin::Base *skin, const gweni::Rect &cliprect);

    virtual void doHide(gweni::skin::Base *skin);
    virtual void hideRecursive(skin::Base *skin);

    virtual bool shouldClip() { return true; }

    //! A rect, in this control's coordinates, that \p child's subtree is
    //! clipped to and hit-tested in on top of the child's own bounds, or
    //! nullptr (the default) for none: the child is then clipped to its bounds
    //! and this control's region, as every control is. A ScrollControl gives
    //! its viewport for its inner panel, so content scrolled past the viewport
    //! neither draws nor takes the mouse in the inset around it. A change of
    //! the rect has to mark the child's subtree (StateChange_Geometry), as a
    //! bounds change does, for the new region to reach slots already drawn.
    virtual const gweni::Rect *getChildClipRect(const Base * /*child*/) const { return nullptr; }

    //! The skin's renderer object for this control, which owns its primitive and
    //! text ids; null until the control has been rendered once.
    const skin::Control *getSkinControl() const { return m_skinControl.get(); }

protected:
    void setPrimitiveIdsSize(size_t size);

    virtual void render(gweni::skin::Base *skin);
    virtual void renderUnder(gweni::skin::Base * /*skin*/) {}
    virtual void renderOver(gweni::skin::Base * /*skin*/) {}
    virtual void renderFocus(gweni::skin::Base * /*skin*/);

    void removeSkinControl();

    //! One z renumber's state, carried through renumberZRecursive().
    struct ZNumbering
    {
        //! The first z the next range (a control's depth layers, or its
        //! overlay layers after its subtree) may take.
        int cursor=1;
        //! Every range takes the cursor, leaving `gap` free after it: the
        //! canvas's first numbering, or a compaction.
        bool full=false;
        int gap=0;
        //! The highest z the previous numbering used. A range taken with the
        //! cursor past it has no kept label after it, so it leaves a gap free
        //! instead of being packed: `tailGap` for a control numbered for the
        //! first time, `movedTailGap` for one moved there (brought to the
        //! front), which takes new z at the top of the range every time.
        int high=0;
        int tailGap=0;
        int movedTailGap=0;
        //! The largest z a range may end at; past it ranges are clamped and
        //! tie (only a tree too large to number at all, packed).
        int clampTo=std::numeric_limits<int>::max();
        //! The highest z this pass used, the layers and the ranges it numbered.
        int maxUsed=0;
        size_t layers=0;
        size_t ranges=0;
        UpdateStats *stats=nullptr;
    };

    //! Numbers this control and its subtree depth-first against \p numbering's
    //! cursor: each range (the control's depthLayers(), then its children, then
    //! its overlayLayers()) keeps its z if that is still at or past the cursor,
    //! and otherwise takes the cursor, so after an insert only the inserted
    //! controls and the ones up to the next free gap get a new z. A control
    //! moved since the last renumber (\p moved, or its own m_zMoved) takes the
    //! cursor with its whole subtree. Marks StateChange_Geometry on each
    //! control whose z or overlay z changed; the stats count the controls
    //! entered (zVisited) and changed (zChanged). Canvas::renumberZOrder() runs
    //! it only after the tree order changed (Canvas::invalidateZOrder()).
    void renumberZRecursive(ZNumbering &numbering, bool moved=false);
    //! Places one range of \p layers z values at \p z: keeps it where it is
    //! if that is still at or past the cursor and \p take does not force a new
    //! one, else gives it the cursor. Returns whether \p z changed.
    static bool placeZRange(ZNumbering &numbering, int &z, int layers, bool take);

    //! \p control moved in the tree order (or to another parent): its subtree
    //! gives up its z labels, which are out of order where it is now, and
    //! takes new ones at the next renumber. Reports the change to the canvas.
    static void zOrderMoved(Base *control);

    //! The tree order below the canvas, or a control's depthLayers(), changed:
    //! tells this control's canvas to renumber the z indices.
    void zOrderChanged();

public:

    //! Hides or shows this control: sets its own flag, the one hidden()
    //! returns. Its descendants keep theirs, so showing it again gives back
    //! the children it or its subclass hid for its own reasons (a ComboBox's
    //! list, a TabControl's other pages, a collapsed node). Marks
    //! StateChange_Visibility on each control whose visible() changed, which
    //! is none under a hidden ancestor, and does not enter the subtree of a
    //! descendant that hid itself.
    virtual void setHidden(bool hidden);

    //! This control's own flag: what setHidden(), hide() or show() last set on
    //! it. Hiding or showing an ancestor does not change it.
    virtual bool hidden() const;
    //! Whether it is drawn: false if this control or any ancestor (on the
    //! m_actualParent chain, inner panels included) is hidden. O(1).
    virtual bool visible() const;

    virtual void hide() { setHidden(true); }    //!< Make control invisible.
    virtual void show(bool show=true) { setHidden(!show); }   //!< Make control visible if hidden.

    // Skin
    virtual void setSkin(skin::Base *skin, bool doChildren=true);
    virtual gweni::skin::Base *getSkin(void);

    // Background drawing
    virtual bool shouldDrawBackground()
    {
        return m_drawBackground;
    }

    virtual void setShouldDrawBackground(bool b)
    {
        m_drawBackground=b;
        addStateChange(StateChange_Background);
    }

protected:

    virtual void onSkinChanged(gweni::skin::Base *newSkin);

public:

    virtual void onMouseMoved(int x, int y, int deltaX, int deltaY);
    virtual bool onMouseWheeled(int iDelta);

    virtual void onMouseClickLeft(int /*x*/, int /*y*/, bool /*down*/) {}
    virtual void onMouseClickRight(int /*x*/, int /*y*/, bool /*down*/) {}

    virtual void onMouseDoubleClickLeft(int x, int y)
    {
        onMouseClickLeft(x, y, true);
    }

    virtual void onMouseDoubleClickRight(int x, int y)
    {
        onMouseClickRight(x, y, true);
    }

    virtual void onMouseEnter();
    virtual void onMouseLeave();

    virtual void onLostKeyboardFocus() {}
    virtual void onKeyboardFocus() {}

    virtual void setMouseInputEnabled(bool b) { m_mouseInputEnabled=b; }
    virtual bool getMouseInputEnabled() { return m_mouseInputEnabled; }

    virtual void setKeyboardInputEnabled(bool b) { m_keyboardInputEnabled=b; }
    virtual bool getKeyboardInputEnabled() const { return m_keyboardInputEnabled; }

    virtual bool needsInputChars() { return false; }

    virtual bool onChar(gweni::UnicodeChar /*c*/) { return false; }

    virtual bool onKeyPress(int key, bool bPress=true);
    virtual bool onKeyRelease(int key);

    virtual void onPaste(event::Info info) {}
    virtual void onCopy(event::Info info) {}
    virtual void onCut(event::Info info) {}
    virtual void onSelectAll(event::Info info) {}

    virtual bool onKeyTab(bool down);
    virtual bool onKeySpace(bool /*down*/) { return false; }
    virtual bool onKeyReturn(bool /*down*/) { return false; }
    virtual bool onKeyBackspace(bool /*down*/) { return false; }
    virtual bool onKeyDelete(bool /*down*/) { return false; }
    virtual bool onKeyRight(bool /*down*/) { return false; }
    virtual bool onKeyLeft(bool /*down*/) { return false; }
    virtual bool onKeyHome(bool /*down*/) { return false; }
    virtual bool onKeyend(bool /*down*/) { return false; }
    virtual bool onKeyUp(bool /*down*/) { return false; }
    virtual bool onKeyDown(bool /*down*/) { return false; }
    virtual bool onKeyEscape(bool /*down*/) { return false; }

    virtual bool isHovered() const;
    virtual bool shouldDrawHover() const;

    virtual void touch();
    virtual void onChildTouched(controls::Base *child);

    virtual bool isOnTop();

    virtual bool isFocussed();
    virtual void focus();
    virtual void unfocus();

    virtual void setDisabled(bool active)
    {
        if(m_disabled == active)
            return;

        m_disabled=active;
        redraw();
    }

    virtual bool isDisabled()
    {
        return m_disabled;
    }

    //! Repaints this control: re-picks its colors and marks its background
    //! dirty, so the skin's update() re-emits it on the next render. Only this
    //! control is marked, plus the canvas's needsRedraw() flag. Call it from
    //! the update phase (input, animations, layout); doRender() clears the state
    //! change once the subtree has rendered, so one issued while rendering is
    //! lost.
    virtual void redraw();

    virtual void updateColors() {}

    //! Enable caching to texture optimisation for this control.
    //! @note Must have getCTT() implemented in the Renderer.
    virtual void enableCacheToTexture() { m_cacheToTexture=true; }

    //! Query if this control is cached to a texture.
    virtual bool isCachedToTexture() const { return m_cacheToTexture; }

    virtual void setCursor(unsigned char c) { m_cursor=c; }

    virtual void updateCursor();

    //! \sect{Tooltip}
    //! Shown by the canvas, in its one ToolTip box, after the mouse has rested
    //! on this control (or on a child of it with no tooltip of its own) for
    //! Canvas::getToolTipDelay(). An empty text and no setToolTip() control
    //! means no tooltip.
    virtual void setToolTipText(const gweni::String &text);
    const gweni::String &getToolTipText() const { return m_toolTipText; }

    //! A control to show as the tooltip instead of the text. It stays a hidden
    //! child of this control, and the canvas moves it into its ToolTip box
    //! while it is shown (with its mouse input turned off) and back after.
    virtual void setToolTip(Base *tooltip);
    virtual Base *getToolTip()
    {
        return m_toolTip;
    }

    //! A text or a setToolTip() control is set.
    bool hasToolTip() const { return m_toolTip || !m_toolTipText.empty(); }
    //! \}

    //! \sect{Context menu}
    //! A right-button press on this control, or on a descendant with no
    //! context menu of its own (the nearest ancestor with one wins), opens the
    //! menu with its top-left at the cursor, flipped to the cursor's left or
    //! above it where it would run past the canvas, and kept inside the canvas
    //! (Menu::placement). It opens on the press, after the press has closed the
    //! menus that were open, so a right-click while one context menu is open
    //! closes it and opens the new one. onContextMenu fires just before it
    //! opens, so the application can fill in or disable its items. It closes on
    //! a click outside it, Escape, choosing an item, and when this control is
    //! hidden or deleted (Canvas::shownContextMenu()). A disabled or hidden
    //! control opens nothing.
    //!
    //! Ownership: a menu passed to setContextMenu() stays its creator's. It is
    //! shown as a child of the canvas (opening re-parents it there if it is not,
    //! as ComboBox does with its list), so make it with
    //! getCanvas()->newChild<Menu>(). Deleting this control only closes it, and
    //! deleting the menu clears it from every control it is set on: one menu can
    //! be shared by several controls. A menu createContextMenu() made is this
    //! control's: it is deleted with it, or when another menu (or nullptr) is
    //! set.
    virtual void setContextMenu(Menu *menu);
    Menu *getContextMenu() const { return m_contextMenu; }
    //! A new empty, hidden Menu on this control's canvas, set as its context
    //! menu and owned by this control (see above); null when the control is
    //! not on a canvas.
    Menu *createContextMenu();
    //! The control createContextMenu() made getContextMenu() for owns it.
    bool ownsContextMenu() const { return m_ownsContextMenu; }
    //! \}

    virtual bool isMenuComponent();
    virtual void closeMenus();

    virtual bool isTabable() { return m_tabable; }
    virtual void setTabable(bool isTabable) { m_tabable=isTabable; }

    //! The control Tab (or Shift+Tab, \p backwards) moves the keyboard focus to
    //! from this one: the next tabable, enabled, visible control in a pre-order
    //! walk of this control's canvas, wrapping at the end. A hidden or disabled
    //! control takes its whole subtree out of the order. Tab order is child
    //! order (m_children), as in GWEN, not on-screen order, so docking can make
    //! the two differ. Returns nullptr, never this, if there is no other one.
    //! Walks the tree on each call; nothing is kept between key presses.
    Base *nextTabable(bool backwards=false);

    // Accelerator functionality
    void defaultAccel(event::Info /*info*/)
    {
        acceleratePressed();
    }

    virtual void acceleratePressed() {}
    virtual bool AccelOnlyFocus() { return false; }

    virtual bool handleAccelerator(gweni::String &accelerator);

    template <typename T>
    void addAccelerator(const String &accelerator,
        T func,
        gweni::event::Handler *handler=nullptr)
    {
        if(handler == nullptr)
            handler=this;

        gweni::event::Caller *caller=new gweni::event::Caller();

        caller->add(handler, func);

        gweni::String str=accelerator;
        gweni::toUpper(str);
        gweni::strip(str, " ");

        m_accelerators[str]=caller;
    }

    void addAccelerator(const String &accelerator)
    {
        addAccelerator(accelerator, &Base::defaultAccel, this);
    }

    AccelMap m_accelerators;

    // Default Events

    gweni::event::Caller onHoverEnter;
    gweni::event::Caller onHoverLeave;
    //! Just before this control's context menu opens (see setContextMenu()).
    gweni::event::Caller onContextMenu;

protected:
//    void init(const String name);

    //Id of control, used to identify control to renderer
    size_t m_id;

    //! The logical parent.
    //! It's usually what you expect, the control you've parented it to.
    Base *m_parent;

    // Childrens List
    Base::List m_children;

    //! If the InnerPanel exists our children will automatically
    //! become children of that instead of us - allowing us to move
    //! them all around by moving that panel (useful for scrolling etc).
    Base *m_innerPanel;

    virtual Base *inner() { return m_innerPanel; }

    //! This is the panel's actual parent - most likely the logical
    //! parent's InnerPanel (if it has one). You should rarely need this.
    Base *m_actualParent;

    //! Some descendant has a pending state change, so doRender() has to walk
    //! into this control even when its own m_stateChange is Nothing. Set on
    //! the m_actualParent chain (the m_children lists the walk follows) by
    //! addStateChange() and when a pending control is attached; cleared when
    //! the walk enters the control, before it visits the children.
    bool m_childStateChanged;

    //! Sets m_childStateChanged on each m_actualParent up to the first that
    //! already has it: O(depth), and amortised O(1) per frame.
    void markAncestorsChanged()
    {
        for(Base *parent=m_actualParent; parent && !parent->m_childStateChanged;
            parent=parent->m_actualParent)
            parent->m_childStateChanged=true;
    }

    //! Called once child is in our m_children: marks our chain if it has
    //! anything pending.
    void markAttachedChanged(Base *child);

    //! An ancestor was hidden or shown, or it was attached to or detached from
    //! a parent: sets m_ancestorHidden and, if its visible() changed, marks
    //! StateChange_Visibility and passes it on to the children. Stops at a
    //! control that hid itself, which is not shown either way.
    void setAncestorHidden(bool ancestorHidden);

    Base *m_toolTip=nullptr;
    gweni::String m_toolTipText;

    Menu *m_contextMenu=nullptr;
    bool m_ownsContextMenu=false;
    //! Takes the context menu off this control (closing it if it is open for
    //! it), deleting it if this control owns it.
    void dropContextMenu();
    //! Menu: the context menu is being deleted.
    void contextMenuDeleted()
    {
        m_contextMenu=nullptr;
        m_ownsContextMenu=false;
    }
    friend class Menu;

    skin::Base *m_skin;
    std::unique_ptr<skin::Control> m_skinControl;

    //something altered in last ui loop so rendering changing
    bool m_updatePrimitives;

    //            gweni::Rect m_needed;
    gweni::Rect m_bounds;
    //! See getRequestedBounds().
    gweni::Rect m_requestedBounds;
    bool m_widthRequested;
    bool m_heightRequested;
    gweni::Rect m_renderBounds;
    int m_zIndex;
    int m_overlayZIndex=0;
    //! Moved since the last renumber: see zOrderMoved().
    bool m_zMoved=false;

    SizeFlags m_sizeFlags;
    Size m_preferredSize;
    Size m_minimumSize;
    Size m_maximumSize;

    LayoutItem *m_layoutItem;
    Padding m_padding;
    Margin m_margin;
    Alignment m_alignment;

    gweni::String m_name;

    int m_stateChange;
    bool m_restrictToParent;
    bool m_disabled;
    //! Its own hidden flag (hidden()).
    bool m_hidden;
    //! Some control on its m_actualParent chain is hidden: kept by
    //! setAncestorHidden() from setHidden() and from attaching it, so visible()
    //! is one test. Set at construction only, not by init().
    bool m_ancestorHidden;
    bool m_mouseInputEnabled;
    bool m_keyboardInputEnabled;
    bool m_drawBackground;

    DockPosition m_dock;

    unsigned char m_cursor;

    bool m_tabable;

public:

    bool needsLayout()
    {
        return m_needsLayout;
    }

    void invalidate(int state=StateChange_Geometry);
    void invalidateParent(int state=StateChange_Geometry)
    {
        if(m_parent)
            m_parent->invalidate(state);
    }

    void invalidateChildren(bool recursive=false);
    //! Invalidates the children that readsParentLayout(): this control's
    //! padding, dock or requested size changed.
    void invalidateParentReaders();

    virtual bool hasAlignment() { return false; }

    virtual void calculateSize(skin::Base *skin, Dim dim);
    //! How layout code enters a control: recurseLayout on the canvas, and every
    //! walk over children (a parent's, a LayoutItem's, a Text's over its lines)
    //! calls these on the child, never its calculateSize()/arrange() directly.
    //! Each is one entry of the current layout pass (LayoutStats::visited)
    //! followed by the virtual. An override calling ParentClass::calculateSize
    //! or ParentClass::arrange stays in its own control: not another entry.
    //!
    //! A pass enters only what is dirty (layoutDirty()): a clean control keeps
    //! its preferred size and bounds, and nothing under it is entered, which is
    //! the same as running it, since a converged pass run again on the same
    //! inputs changes nothing (docs/layout.md). Outside a pass, and in layout
    //! verification's full pass (Canvas::verifyingLayout()), they always run.
    void doCalculateSize(skin::Base *skin, Dim dim);
    void doArrange(skin::Base *skin, Dim dim);
    //! Whether the next layout pass enters this control: it, or something
    //! under it, was invalidated, or its bounds changed, since a pass last
    //! arranged it in Y. Cleared at the end of its Y arrange, unless a shown
    //! child is still dirty then (placed after its own arrange, say), so the
    //! path down to every dirty control stays dirty.
    bool layoutDirty() const { return m_updateLayout; }
    //! True for a control whose calculateSize() or arrange() reads its
    //! parent's padding, dock or requested size, besides what reaches it as
    //! its bounds (a wrapped Text breaks at its parent's requested inner
    //! width): a change to those invalidates it as well as the parent.
    virtual bool readsParentLayout() const { return false; }
    //! Sizes this dock-None control along `dim` for arrangeHorizontal/Vertical
    //! of its parent: `preferred` is its preferred size (see dockNoneSize).
    virtual void policyResize(const Size &preferred, Dim dim);
    virtual void arrange(skin::Base *skin, Dim dim);

protected:
    //! The width (Dim::X) or height (Dim::Y) of this control when its parent
    //! places it with dock None, from its requested size r, its preferred size
    //! p and its size flag: Fixed r, Expand max(r,p), Shrink min(r,p), Elastic
    //! and Bloat p; with no size requested, p. There is no cell to fill, so
    //! nothing caps Expand: content larger than the parent overflows it (a
    //! ScrollControl's panel, say). Never reads the current bounds, so what an
    //! earlier pass gave the control does not feed back.
    int dockNoneSize(Dim dim, int preferred) const;
    //! The same for this control placed by a LayoutItem in a cell `cell` wide
    //! (Dim::X) or high: Fixed r, Shrink min(r,cell), Expand, Elastic and
    //! Bloat max(r,p) up to the cell; with no size requested, the cell.
    int layoutCellSize(Dim dim, int cell) const;
    //! setBounds for the generic arrange code, and for a control computing its
    //! own bounds: its result is not a request.
    bool setArrangedBounds(int x, int y, int w, int h);
    //! setBounds for a control placing a child it owns (Placement::Owner):
    //! computed bounds, not a request, and placement, since the owner lays the
    //! child out next (see PlacementScope). Static, so a subclass can call it
    //! on any child; placeChildPos keeps the size, placeChildSize the position.
    static bool placeChild(Base *child, const gweni::Rect &bounds);
    static bool placeChild(Base *child, int x, int y, int w, int h);
    static bool placeChildPos(Base *child, int x, int y);
    static bool placeChildSize(Base *child, int w, int h);

    //! Whether this control's content along `dim` is the size it was asked
    //! for (Base::calculateSize then takes the request, not its children's
    //! size): a Slider, whose track is drawn by its skin, and a ScrollControl,
    //! whose content scrolls inside it. False by default.
    virtual bool sizedByRequest(Dim /*dim*/) const { return false; }

    bool processLayout(skin::Base *skin, Dim dim);
    Size sizeOfChildren(skin::Base *skin, Dim dim);
    //! The size `text` takes in `font`: the skin's renderer's measureText().
    //! Controls measure through this, never the renderer directly, so every
    //! measure is counted (measureTextCalls()).
    gweni::Point measureText(const gweni::Font &font, const gweni::String &text);
    //! The height of one line box in `font`: the skin's renderer's lineHeight(),
    //! which is measureText()'s `y` for a one-line string. Counted with the
    //! measures (measureTextCalls()).
    int measureLineHeight(const gweni::Font &font);
    //! The preferred width (Dim::X) or height (Dim::Y) this control's size
    //! flags make of `contentSize`, the size its children ask for, padding
    //! included. sizeOfChildren() ends with it.
    int preferredFromContent(int contentSize, Dim dim) const;

    //! Place the children along X and Y from their docks, alignments and
    //! sizes; Base::arrange calls them before arranging each child. They skip
    //! a child whose owner places it (Placement::Owner).
    virtual void arrangeHorizontal(skin::Base *skin);
    virtual void arrangeVertical(skin::Base *skin);

    virtual void recurseLayout(skin::Base *skin);
    virtual void layout(skin::Base *skin);
    virtual void postLayout(skin::Base * /*skin*/)
    {}

    bool m_needsLayout;
    bool m_cacheTextureDirty;
    bool m_cacheToTexture;
    bool m_updateLayout;
    //! The Canvas::s_passSerial of the last pass that arranged this control
    //! in X (m_layoutPassX) and in Y (m_layoutPassY).
    unsigned m_layoutPassX=0;
    unsigned m_layoutPassY=0;
    //! The Canvas::s_passSerial of the last pass this control re-dirtied
    //! (noteRedirty()): it stays dirty past its Y arrange in that pass.
    unsigned m_redirtiedPass=0;
    //! Reports a re-dirty of this control to the running pass's canvas.
    void noteRedirty();

    //! Whether the running pass has arranged this control along `dim` yet.
    bool laidOutThisPass(Dim dim) const;
    //! Records that the running pass arranges this control along `dim`;
    //! Base::arrange does it at the start of each axis, before layout().
    void stampLayoutPass(Dim dim);
    //! Both axes at once, for a control that arranges itself and its subtree
    //! in X and Y in one go (ScrollControl's Y arrange).
    void stampLayoutPass();
    //! True while a canvas runs a layout pass.
    static bool inLayoutPass();

    //! While one is alive, the layout invalidations raised are placement, not
    //! a re-dirty (see Base::invalidate): for a control handing the children
    //! it is about to lay out, later in the same pass, their inputs other than
    //! bounds (visibility, docks, margins, a scroll range); bounds go through
    //! placeChild, which opens one itself. Keep it to such writes: anything an
    //! already laid out control read must still be reported.
    struct PlacementScope
    {
        PlacementScope();
        ~PlacementScope();
        PlacementScope(const PlacementScope &)=delete;
        PlacementScope &operator=(const PlacementScope &)=delete;
    private:
        bool m_outer;
    };

    //! While one is alive, a control made during a layout pass, as a part its
    //! owner places (a VirtualList's pooled rows), sets up its bounds the way
    //! any constructor does: its defaults are its requests, but they are not
    //! counted as layout code recording one (LayoutStats::requests). Keep it
    //! to the construction: what the owner writes afterwards is placement.
    struct PartScope
    {
        PartScope();
        ~PartScope();
        PartScope(const PartScope &)=delete;
        PartScope &operator=(const PartScope &)=delete;
    private:
        bool m_outer;
    };

    //
    // Drag + Drop

public:

    // Giver

    virtual void dragAndDrop_setPackage(bool draggable, const String &name="",
        void *userData=nullptr);
    virtual bool dragAndDrop_draggable();
    virtual bool dragAndDrop_shouldStartDrag() { return true; }

    virtual void dragAndDrop_startDragging(draganddrop::Package * /*package*/, int /*x*/,
        int /*y*/) {}
    virtual gweni::draganddrop::Package *dragAndDrop_getPackage(int x, int y);
    virtual void dragAndDrop_endDragging(bool /*bSuccess*/, int /*x*/, int /*y*/) {}

protected:

    draganddrop::Package *m_dragAndDrop_Package;

public:

    // Receiver
    virtual void dragAndDrop_hoverEnter(gweni::draganddrop::Package * /*package*/,
        int /*x*/, int /*y*/)
    {}

    virtual void dragAndDrop_hoverLeave(gweni::draganddrop::Package * /*package*/)
    {}

    virtual void dragAndDrop_hover(gweni::draganddrop::Package * /*package*/,
        int /*x*/, int /*y*/)
    {}

    virtual bool dragAndDrop_handleDrop(gweni::draganddrop::Package *package, int x, int y);
    virtual bool dragAndDrop_canAcceptPackage(gweni::draganddrop::Package * /*package*/)
    {
        return false;
    }

    //
    // Useful anim shortcuts
    //

public:

#if GWENI_ANIMATE

    virtual void anim_widthIn(float length, float delay=0.0f, float ease=1.0f);
    virtual void anim_heightIn(float length, float delay=0.0f, float ease=1.0f);
    virtual void anim_widthOut(float length, bool hide=true, float delay=0.0f,
        float ease=1.0f);
    virtual void anim_heightOut(float length, bool hide=true, float delay=0.0f,
        float ease=1.0f);

#endif

    //
    // Dynamic casting, see gweni_cast below
    //

public:

    static const char *getIdentifier()
    {
        return getTypeNameStatic();
    }

    static size_t getControlTypeId()
    {
        return getControlType(getTypeNameStatic());
    }

    virtual gweni::controls::Base *dynamicCast(const char *Variable)
    {
        return nullptr;
    }

public:

    //! Laid out by the generic arrange like any other child, but not counted
    //! in the parent's size (TextBox's placeholder). Not a Placement: the
    //! parent places such a child; a child its owner places is
    //! Placement::Owner, which already leaves it out of the size.
    void doNotIncludeInSize() { m_includeInSize=false; }
    //! Whether this control counts in its parent's size: not when it asked
    //! not to (doNotIncludeInSize), nor when its owner places it
    //! (Placement::Owner).
    bool shouldIncludeInSize() const { return m_includeInSize && m_placement == Placement::Parent; }

protected:

    bool m_includeInSize;
    Placement m_placement=Placement::Parent;

public:

    virtual String getChildValue(const gweni::String &name);
    virtual String getValue();
    virtual void setValue(const String &value);
    virtual void doAction() {}

    virtual void setAction(event::Handler *object,
        event::Caller::EventCaller function,
        const gweni::event::Packet &packet)
    {}

    virtual int getNamedChildren(gweni::ControlList &list, const gweni::String &name,
        bool bDeep=true);
    virtual gweni::ControlList getNamedChildren(const gweni::String &name,
        bool bDeep=true);

public:

    UserDataStorage m_userData;   // TODO - optimise memory usage.

};

template<typename _Control>
_Control *Base::newChild(const gweni::String name)
{
    _Control *control=newControl<_Control>(name);

    control->setParent(this);
    control->recursiveInit(name);
    control->setSkin(m_skin);

    return control;
}


}//namespace controls


/**
    * To avoid using dynamic_cast we have gweni_cast.
    *
    * Each class in Gweni includes GWENI_DYNAMIC. You don't have to include this
    * macro anywhere as it's automatically included in the GWENI_CONTROL macro.
    *
    * GWENI_DYNAMIC adds 2 functions:
    *
    *  *getIdentifier() :-
    *     a static function with a static variable inside, which returns
    *     the address of the static variable. The variable is defined
    *     as a string containing "BASECLASSNAME:CLASSNAME". This string
    *     should be as unique as possible - or the compiler might optimize the
    *     variables together - which means that when this function returns the
    *     address it could be the same on one or more variables. Something to
    *     bear in mind.
    *
    *  *DynamicCast :-
    *     Non static, takes an address returned by getIdentifier().
    *     Will return an address of a control if the control can safely be cast to
    *     the class from which the identifier was taken.
    *
    * Really you shouldn't actually have to concern yourself with that stuff.
    * The only thing you should use in theory is gweni_cast - which is used
    * just the same as dynamic cast - except for one difference. We pass in
    * the class name, not a pointer to the class:
    *
    *     gweni_cast<MyControl>(control)
    *     dynamic_cast<MyControl*>(control)
    *
    */
template <class T>
inline T *gweni_cast(gweni::controls::Base *p)
{
    if(!p)
        return nullptr;

    gweni::controls::Base *result=p->dynamicCast(T::getIdentifier());

    return result != nullptr?static_cast<T *>(result):nullptr;
}

template <class T>
inline T *controls::Base::findChild(const gweni::String &name, bool recursive)
{
    return gweni_cast<T>(findChildByName(name, recursive));
}

#define GWENI_DYNAMIC(THISNAME, BASENAME) \
    static const char *getIdentifier()                                  \
    {                                                                   \
        static const char *ident = #BASENAME ":" #THISNAME;             \
        return ident;                                                   \
    }                                                                   \
    gweni::controls::Base *dynamicCast(const char *Variable) override   \
    {                                                                   \
        if (getIdentifier() == Variable)                                \
            return this;                                                \
                                                                        \
        return ParentClass::dynamicCast(Variable);                      \
    }

#define GWENI_CLASS(THISNAME, BASENAME) \
    typedef BASENAME ParentClass; \
    typedef THISNAME ThisClass;

 //To be placed in the controls .h definition.
#define GWENI_CONTROL_BASE(THISNAME, BASENAME, CONTROLNAME) \
public: \
    GWENI_CLASS(THISNAME, BASENAME)  \
    GWENI_DYNAMIC(THISNAME, BASENAME) \
    static  const char *getTypeNameStatic() { return #THISNAME; } \
    const char *getTypeName() const override { return getTypeNameStatic(); } \
    static const char *getControlNameStatic(){ return #CONTROLNAME; } \
    const char *getControlName() const override{ return getControlNameStatic(); } \
    const char *getParentTypeName() const override { return ParentClass::getTypeNameStatic(); } \
    const char *getParentTypeName(size_t index) const override { if(index == 0) return ParentClass::getParentTypeName(); else return ParentClass::getParentTypeName(index-1); } \
    void recursiveInit(const gweni::String &name){BASENAME::recursiveInit(name); THISNAME::init(name);} \
    static THISNAME *newInstance(const gweni::String &name){THISNAME *instance=new THISNAME(name); return instance;} 

#define GWENI_CONTROL_FUNC(THISNAME, BASENAME, CONTROLNAME) \
    GWENI_CONTROL_BASE(THISNAME, BASENAME, CONTROLNAME) \
    void init(const gweni::String &name); \
protected: \
    THISNAME(const gweni::String &name = "")


#define GWENI_FUNC_SELECT(DUMMY, SPACER, FUNC, ...) DUMMY, SPACER, FUNC; ##__VA_ARGS__

#define GWENI_CONTROL(THISNAME, BASENAME) GWENI_CONTROL_FUNC(THISNAME, BASENAME, THISNAME)
//#define GWENI_DERIVED_CONTROL(THISNAME, BASENAME) GWENI_CONTROL_FUNC(THISNAME, BASENAME, BASENAME)

#define GWENI_CONTROL_INLINE_FUNC(THISNAME, BASENAME, CONTROLNAME) \
    GWENI_CONTROL_BASE(THISNAME, BASENAME, CONTROLNAME) \
protected: \
    THISNAME(const gweni::String &name="") : ParentClass(name) {} \
public: \
    void init(const gweni::String &name)

#define GWENI_CONTROL_INLINE(THISNAME, BASENAME, ...) GWENI_CONTROL_INLINE_FUNC(THISNAME, BASENAME, THISNAME)
//#define GWENI_DERIVED_CONTROL_INLINE(THISNAME, BASENAME) GWENI_CONTROL_INLINE_FUNC(THISNAME, BASENAME, BASENAME))

#define GWENI_CONTROL_CONSTRUCTOR(THISNAME) \
    THISNAME::THISNAME(const gweni::String &name) \
        : ParentClass(name){} \
    void THISNAME::init(const gweni::String &name) \

//! drawsNothing() for the class it is written in, and not for its subclasses,
//! which opt out with their own GWENI_DRAWS_NOTHING.
#define GWENI_DRAWS_NOTHING \
    bool drawsNothing() const override { return std::strcmp(getTypeName(), getTypeNameStatic()) == 0; }

} // namespace gweni

#endif//_gweni_controls_base_h_
