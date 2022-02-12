/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_base_h_
#define _gweni_controls_base_h_

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
#include <algorithm>

namespace gweni
{
namespace controls
{

class Canvas;
class LayoutItem;

constexpr int StateChange_Nothing=0x0000;
constexpr int StateChange_Created=0x0001;
constexpr int StateChange_Visibility=0x0002;
constexpr int StateChange_Geometry=0x0004;
constexpr int StateChange_Background=0x0008;
constexpr int StateChange_Text=0x0010;

size_t getControlType(std::string name);

template<typename _Control>
_Control *newControl(const gweni::String &name="")
{
    _Control *control=_Control::newInstance(name);

    return control;
}
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

    virtual void delayedDelete();
    virtual void preDelete(gweni::skin::Base *skin) {}

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

    template<typename _Control>
    friend _Control *newControl(const gweni::String &name);
    template<typename _Control>
    _Control *newChild(const gweni::String &name);

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
//Position
    virtual void dock(Position dock);
    virtual Position getDock() const;

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
    virtual void addStateChange(int stateChange) { m_stateChange|=stateChange; }
    

    virtual void restrictToParent(bool restrict) { m_restrictToParent=restrict; }
    virtual bool shouldRestrictToParent() { return m_restrictToParent; }

    virtual int getX() const { return m_bounds.x; }  //!< Get X position of Control.
    virtual int getY() const { return m_bounds.y; }  //!< Get Y position of Control.

    virtual int getWidth() const { return m_bounds.w; }  //!< Get Control width.
    virtual int getHeight() const { return m_bounds.h; }  //!< Get Control height.

    virtual int getZIndex() const { return m_zIndex; }

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

    virtual void setWidth(int w) { setSize(w, getHeight()); }
    virtual void setHeight(int h) { setSize(getWidth(), h); }

    virtual bool  setSize(int w, int h);
    virtual bool  setSize(const Point &p);
    virtual Point getSize() const { return Point(getWidth(), getHeight()); }

    virtual bool setBounds(int x, int y, int w, int h);
    virtual bool setBounds(const gweni::Rect &bounds);

    virtual void setPadding(const Padding &padding);
    virtual void setMargin(const Margin &margin);

    //! MoveTo is identical to setPos except it uses shouldRestrictToParent().
    virtual void moveTo(int x, int y);
    virtual void moveBy(int x, int y);

    virtual const gweni::Rect &getBounds() const { return m_bounds; }

    //            virtual void SetNeeded(const gweni::Rect &needed) { m_needed=needed; }
    //            virtual const gweni::Rect &GetNeeded() const { return m_needed; }
    virtual Size getMinimumSize();
    virtual Size getMaximumSize();

    virtual void setPreferredSize(const Size &size) { m_preferredSize=size; }
    virtual const Size getPreferredSize() const { return m_preferredSize; }

    virtual controls::Base *getControlAt(int x, int y, bool bOnlyIfMouseEnabled=true);

protected:

    virtual void onBoundsChanged(gweni::Rect oldBounds);
    virtual void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child);

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
    PrimitiveIds &getPrimitiveIds() { return m_primitiveIds; }


    virtual void doRender(gweni::skin::Base *skin);
    virtual void doCacheRender(gweni::skin::Base *skin, gweni::controls::Base *master);
    virtual void renderRecursive(gweni::skin::Base *skin, const gweni::Rect &cliprect);

    virtual void doHide(gweni::skin::Base *skin);
    virtual void hideRecursive(skin::Base *skin);

    virtual bool shouldClip() { return true; }

protected:
    void setPrimitiveIdsSize(size_t size);

    virtual void think() {}

    virtual void render(gweni::skin::Base *skin);
    virtual void renderUnder(gweni::skin::Base * /*skin*/) {}
    virtual void renderOver(gweni::skin::Base * /*skin*/) {}
    virtual void renderFocus(gweni::skin::Base * /*skin*/);

    virtual void update();
    void updateRecursive(int &zIndex);

public:

    virtual void setHidden(bool hidden)
    {
        if(m_hidden == hidden)
            return;

        m_hidden=hidden;
        
        for(auto &&child : m_children)
        {
            child->setHidden(hidden);
        }

        addStateChange(StateChange_Visibility);
        invalidate();
        redraw();
    }

    virtual bool hidden() const;    //!< Returns true only if this control is hidden.
    virtual bool visible() const;   //!< Returns false if this control or its parents are hidden.

    virtual void hide() { setHidden(true); }    //!< Make control invisible.
    virtual void show() { setHidden(false); }   //!< Make control visible if hidden.

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

    virtual void redraw()
    {
        updateColors();
        m_cacheTextureDirty=true;

        if(m_parent)
            m_parent->redraw();
    }

    virtual void updateColors() {}

    //! Enable caching to texture optimisation for this control.
    //! @note Must have getCTT() implemented in the Renderer.
    virtual void enableCacheToTexture() { m_cacheToTexture=true; }

    //! Query if this control is cached to a texture.
    virtual bool isCachedToTexture() const { return m_cacheToTexture; }

    virtual void setCursor(unsigned char c) { m_cursor=c; }

    virtual void updateCursor();

    virtual void setToolTipText(const gweni::String &text);

    virtual void setToolTip(Base *tooltip)
    {
        m_toolTip=tooltip;
        if(m_toolTip)
        {
            m_toolTip->setParent(this);
            m_toolTip->setHidden(true);
        }
    }
    virtual Base *getToolTip()
    {
        return m_toolTip;
    }

    virtual bool isMenuComponent();
    virtual void closeMenus();

    virtual bool isTabable() { return m_tabable; }
    virtual void setTabable(bool isTabable) { m_tabable=isTabable; }

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

    Base *m_toolTip;

    skin::Base *m_skin;
    std::unique_ptr<skin::Control> m_skinControl;

    PrimitiveIds m_primitiveIds;
    //something altered in last ui loop so rendering changing
    bool m_updatePrimitives;

    //            gweni::Rect m_needed;
    gweni::Rect m_bounds;
    gweni::Rect m_renderBounds;
    int m_zIndex;

    SizeFlags m_sizeFlags;
    Size m_preferredSize;
    Size m_minimumSize;
    Size m_maximumSize;

    LayoutItem *m_layoutItem;
    Padding m_padding;
    Margin m_margin;

    gweni::String m_name;

    int m_stateChange;
    bool m_restrictToParent;
    bool m_disabled;
    bool m_hidden;
    bool m_mouseInputEnabled;
    bool m_keyboardInputEnabled;
    bool m_drawBackground;

    Position m_dock;

    unsigned char m_cursor;

    bool m_tabable;

public:

    bool needsLayout()
    {
        return m_needsLayout;
    }

    void invalidate();
    void invalidateParent()
    {
        if(m_parent)
            m_parent->invalidate();
    }

    void invalidateChildren(bool recursive=false);

    virtual bool hasAlignment() { return false; }
    void setPosition(Position pos, int xpadding=0, int ypadding=0);

    virtual void calculateSize(skin::Base *skin, Dim dim);
    virtual void arrange(skin::Base *skin, Dim dim);

protected:
    bool processLayout(skin::Base *skin, Dim dim);
    Size sizeOfChildren(skin::Base *skin, Dim dim);

    void arrangeHorizontal(skin::Base *skin);
    void arrangeVertical(skin::Base *skin);

    virtual void recurseLayout(skin::Base *skin);
    virtual void layout(skin::Base *skin);
    virtual void postLayout(skin::Base * /*skin*/)
    {}

    bool m_needsLayout;
    bool m_cacheTextureDirty;
    bool m_cacheToTexture;

    //
    // Drag + Drop

public:

    // Giver

    virtual void dragAndDrop_setPackage(bool draggable, const String &name="",
        void *userData=nullptr);
    virtual bool dragAndDrop_draggable();
    virtual bool dragAndDrop_shouldStartDrag() { return true; }

    virtual void dragAndDrop_startDragging(draganddrop::Package *package, int x, int y);
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

    static size_t getControlId()
    {
        return getControlType(getTypeNameStatic());
    }

    virtual gweni::controls::Base *dynamicCast(const char *Variable)
    {
        return nullptr;
    }

public:

    void doNotIncludeInSize() { m_includeInSize=false; }
    bool shouldIncludeInSize() { return m_includeInSize; }

protected:

    bool m_includeInSize;

public:

    virtual String getChildValue(const gweni::String &name);
    virtual String getValue();
    virtual void       setValue(const String &value);
    virtual void       doAction() {}

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
_Control *Base::newChild(const gweni::String &name="")
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

} // namespace gweni

#endif//_gweni_controls_base_h_
