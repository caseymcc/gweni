/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_skin_h_
#define _gweni_skin_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/uiTypes.h>

namespace gweni
{
namespace controls
{
class Base;
}//namespace controls

namespace skin
{
namespace symbol
{
enum
{
    None,
    ArrowRight,
    Check,
    Dot
};
}//namespace symbol


class Details
{
    std::vector<size_t > primitiveIds;
};

enum State
{
    Generate,
    Update,
    Remove
};

class GWENI_EXPORT Control
{
public:
    Control()
    {}

    virtual ~Control()
    {}

    virtual std::string getTypeName()=0;
    virtual size_t requiredPrimitives()=0;

    // The number of consecutive z values this control's primitives use,
    // starting at the control's getZIndex(). Its first child is numbered
    // after them, so an overlay drawn at getZIndex()+1 needs 2 here: the z
    // values past them are not this control's even where they are free
    // today, since the canvas hands its gaps out to controls inserted there
    // (Canvas::ZGap). Read
    // when the canvas renumbers the z indices, which it does only after the
    // tree order or a control's skin Control changed: a Control whose count
    // changes on its own must call Canvas::invalidateZOrder(), whose
    // renumber enters every control, since the canvas cannot tell whose
    // count changed.
    virtual size_t depthLayers()
    {
        return 1;
    }

    // The number of z values this control's primitives use after its whole
    // subtree (controls::Base::getOverlayZIndex() is the first), for what it
    // draws in front of its own children, a TextBox's caret over its text.
    // Read when the canvas renumbers, as depthLayers() is; the range keeps
    // its z while the subtree before it still ends below it.
    virtual size_t overlayLayers()
    {
        return 0;
    }

    virtual void generate(renderer::Base *baseRenderer, controls::Base *control)
    {
        generatePrimitives(baseRenderer, requiredPrimitives());
    }

    virtual void remove(renderer::Base *baseRenderer, controls::Base *control)
    {
        releasePrimitives(baseRenderer);
    }

    virtual void update(renderer::Base *render, controls::Base *control) 
    {}

    const std::vector<size_t> &getPrimitives() const
    {
        return m_primitives;
    }

    //! Appends the renderer ids this Control holds, for a tool that tells its
    //! own slots from the app's (gweni::tools::Inspector): primitive ids to
    //! \p primitiveIds and text ids to \p textIds. By default every id in
    //! getPrimitives() is a primitive id; a Control that takes text ids
    //! (generateTextPrimitive) says which are.
    virtual void appendIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const
    {
        (void)textIds;
        primitiveIds.insert(primitiveIds.end(), m_primitives.begin(), m_primitives.end());
    }

protected:
    void generatePrimitives(renderer::Base *baseRenderer, size_t count)
    {
        if(count == 0)
            return;

        releasePrimitives(baseRenderer);
        m_primitives.resize(count);

        for(size_t i=0; i<count; ++i)
        {
            baseRenderer->generatePrimitive(m_primitives[i]);
        }
    }
    void releasePrimitives(renderer::Base *baseRenderer)
    {
        for(size_t i=0; i<m_primitives.size(); ++i)
        {
            baseRenderer->releasePrimitive(m_primitives[i]);
        }
        m_primitives.clear();
    }

    std::vector<size_t> m_primitives;
};


class GWENI_EXPORT Base
{
public:

    Base(gweni::renderer::Base *renderer=nullptr)
    {
        m_defaultFont.facename="OpenSans";
        m_defaultFont.size=11.0f;
        m_render=renderer;
    }

    virtual ~Base()
    {
        freeDefaultFont();
    }

    //! loadFont and releaseFont are a counted pair on the skin's renderer, as
    //! IResourceLoader's loadFont/freeFont are: release only a font this
    //! loaded, once for each load (a load that failed left nothing to free).
    virtual gweni::Font::Status loadFont(gweni::Font *fnt)
    {
        if(fnt && m_render)
            return m_render->getLoader().loadFont(*fnt);
        return gweni::Font::Status::Unloaded;
    }

    virtual void releaseFont(gweni::Font *fnt)
    {
        if(fnt && m_render)
            m_render->getLoader().freeFont(*fnt);
    }

    virtual std::unique_ptr<Control> getControlRenderer(std::string typeName)=0;

    virtual void drawControl(controls::Base *controls)=0;

//    virtual void drawGenericPanel(controls::Base *control, State state)=0;
//
//    virtual void drawButton(controls::Base *control, State state, bool depressed, bool hovered,
//        bool disabled)=0;
//    virtual void drawTabButton(controls::Base *control, State state, bool active, DockPosition dir)=0;
//    virtual void drawTabControl(controls::Base *control, State state)=0;
//    virtual void drawTabTitleBar(controls::Base *control, State state)=0;
//
//
//    virtual void drawMenuItem(controls::Base *control, State state, bool submenuOpen,
//        bool checked)=0;
//    virtual void drawMenuStrip(controls::Base *control, State state)=0;
//    virtual void drawMenu(controls::Base *control, State state, bool paddingDisabled)=0;
//    virtual void drawMenuRightArrow(controls::Base *control, State state)=0;
//
//    virtual void drawRadioButton(controls::Base *control, State state, bool selected,
//        bool depressed)=0;
//    virtual void drawCheckBox(controls::Base *control, State state, bool selected, bool depressed)=0;
//    virtual void drawGroupBox(controls::Base *control, State state, int textStart, int textHeight,
//        int textWidth)=0;
//    virtual void drawTextBox(controls::Base *control, State state)=0;
//
//    virtual void drawWindow(controls::Base *control, State state, int topHeight, bool inFocus)=0;
//
//
//    virtual void drawHighlight(controls::Base *control, State state)=0;
//    virtual void drawStatusBar(controls::Base *control, State state)=0;
//
//    virtual void drawShadow(controls::Base *control, State state)=0;
//    virtual void drawScrollBarBar(controls::Base *control, State state, bool depressed, bool isHovered,
//        bool isHorizontal)=0;
//    virtual void drawScrollBar(controls::Base *control, State state, bool isHorizontal,
//        bool depressed)=0;
//    virtual void drawScrollButton(controls::Base *control, State state, DockPosition direction, bool depressed,
//        bool hovered, bool disabled)=0;
//    virtual void drawProgressBar(controls::Base *control, State state, bool isHorizontal,
//        float progress)=0;
//
//    virtual void drawListBox(controls::Base *control, State state)=0;
//    virtual void drawListBoxLine(controls::Base *control, State state, bool selected, bool even)=0;
//
//    virtual void drawSlider(controls::Base *control, State state, bool isHorizontal, int numNotches,
//        int barSize)=0;
//    virtual void drawSlideButton(gweni::controls::Base *control, State state, bool depressed,
//        bool horizontal)=0;
//
//    virtual void drawComboBox(controls::Base *control, State state, bool bIsDown, bool bIsMenuOpen)=0;
//    virtual void drawComboDownArrow(gweni::controls::Base *control, State state, bool hovered,
//        bool down, bool open, bool disabled)=0;
//    virtual void drawKeyboardHighlight(controls::Base *control, State state, const gweni::Rect &rect,
//        int offset)=0;
//    virtual void drawTooltip(controls::Base *control, State state)=0;
//
//    virtual void drawNumericUpDownButton(controls::Base *control, State state, bool depressed,
//        bool bUp)=0;
//
//    virtual void drawTreeButton(controls::Base *control, State state, bool open)=0;
//    virtual void drawTreeControl(controls::Base *control, State state)=0;
//    virtual void drawTreeNode(controls::Base *control, State state, bool open, bool selected,
//        int labelHeight, int labelWidth, int halfWay,
//        int lastBranch, bool isRoot);
//
//    virtual void drawPropertyRow(controls::Base *control, State state, int iWidth, bool bBeingEdited,
//        bool hovered);
//    virtual void drawPropertyTreeNode(controls::Base *control, State state, int BorderLeft,
//        int BorderTop);
//    virtual void drawColorDisplay(controls::Base *control, State state, gweni::Color color)=0;
//    virtual void drawModalControl(controls::Base *control, State state)=0;
//    virtual void drawMenuDivider(controls::Base *control, State state)=0;
//
//    virtual void drawCategoryHolder(controls::Base *control, State state)=0;
//    virtual void drawCategoryInner(controls::Base *control, State state, bool collapsed)=0;


    //! A default font the skin holds moves with it: freed on the old renderer
    //! and loaded on the new one.
    virtual void setRender(gweni::renderer::Base *renderer)
    {
        if(renderer == m_render)
            return;

        freeDefaultFont();
        m_render=renderer;
        if(m_holdsDefaultFont)
            loadDefaultFont();
    }

    virtual gweni::renderer::Base *getRenderer()
    {
        return m_render;
    }

//    virtual void drawArrowDown(size_t *primitiveIds, gweni::Rect rect);
//    virtual void drawArrowUp(size_t *primitiveIds, gweni::Rect rect);
//    virtual void drawArrowLeft(size_t *primitiveIds, gweni::Rect rect);
//    virtual void drawArrowRight(size_t *primitiveIds, gweni::Rect rect);
//    virtual void drawCheck(size_t *primitiveIds, gweni::Rect rect);


    struct
    {
        struct Window_t
        {
            gweni::Color TitleActive;
            gweni::Color TitleInactive;

        } Window;


        struct Label_t
        {
            gweni::Color Default;
            gweni::Color Bright;
            gweni::Color Dark;
            gweni::Color Highlight;

        } Label;


        struct Tree_t
        {
            gweni::Color Lines;
            gweni::Color Normal;
            gweni::Color Hover;
            gweni::Color Selected;

        } Tree;


        struct Properties_t
        {
            gweni::Color Line_Normal;
            gweni::Color Line_Selected;
            gweni::Color Line_Hover;
            gweni::Color Column_Normal;
            gweni::Color Column_Selected;
            gweni::Color Column_Hover;
            gweni::Color Label_Normal;
            gweni::Color Label_Selected;
            gweni::Color Label_Hover;
            gweni::Color Border;
            gweni::Color Title;

        } Properties;


        struct Button_t
        {
            gweni::Color Normal;
            gweni::Color Hover;
            gweni::Color Down;
            gweni::Color Disabled;

        } Button;


        struct Tab_t
        {
            struct Active_t
            {
                gweni::Color Normal;
                gweni::Color Hover;
                gweni::Color Down;
                gweni::Color Disabled;
            } Active;


            struct Inactive_t
            {
                gweni::Color Normal;
                gweni::Color Hover;
                gweni::Color Down;
                gweni::Color Disabled;
            } Inactive;


        } Tab;


        struct Category_t
        {
            gweni::Color Header;
            gweni::Color Header_Closed;

            struct Line_t
            {
                gweni::Color Text;
                gweni::Color Text_Hover;
                gweni::Color Text_Selected;
                gweni::Color Button;
                gweni::Color Button_Hover;
                gweni::Color Button_Selected;
            } Line;


            struct LineAlt_t
            {
                gweni::Color Text;
                gweni::Color Text_Hover;
                gweni::Color Text_Selected;
                gweni::Color Button;
                gweni::Color Button_Hover;
                gweni::Color Button_Selected;
            } LineAlt;


        } Category;


        gweni::Color ModalBackground;
        gweni::Color TooltipText;
        //! A TextBox's hint, drawn while it is empty and unfocused.
        gweni::Color PlaceholderText{0x80, 0x80, 0x80};
        //! A VectorField's axis letter (X, Y, Z, W), drawn on its axis's colour.
        gweni::Color AxisTagText{0, 0, 0};

        //! A Selectable row's text, by its state.
        struct Selectable_t
        {
            gweni::Color Normal{0, 0, 0};
            gweni::Color Hover{0, 0, 0};
            gweni::Color Selected{0xff, 0xff, 0xff};
            gweni::Color Disabled{0x80, 0x80, 0x80};
        } Selectable;

    } Colors;

    //! Sizes a skin sets for its controls, in logical units. Read when a
    //! control lays out, so a control given another skin takes its values.
    struct
    {
        //! A scroll bar's thickness across it, and the side of each of its
        //! square arrow buttons.
        int ScrollBarThickness=15;
    } Metrics;


public:

    virtual const gweni::Font &getDefaultFont()
    {
        return m_defaultFont;
    }

    //! Call before building controls. Text and Label keep a pointer to
    //! m_defaultFont, so a later call changes their font in place without an
    //! addStateChange, and text already laid out does not relayout.
    //! The skin holds the font it sets (loadFont) and gives back the one it
    //! held before, after the new one is loaded, so an equal font is not freed
    //! and loaded again.
    virtual void setDefaultFont(const gweni::String &strFacename, float fSize=11.0f)
    {
        const gweni::Font previous=m_loadedDefaultFont;
        gweni::renderer::Base *previousRender=m_defaultFontLoadedOn;

        m_defaultFont.facename=strFacename;
        m_defaultFont.size=fSize;
        m_defaultFontLoadedOn=nullptr;
        loadDefaultFont();

        if(previousRender)
            previousRender->getLoader().freeFont(previous);
    }

protected:

    //! Loads m_defaultFont on the skin's renderer, unless the skin holds it
    //! already. A skin's init() calls it: the constructor can run before the
    //! renderer can load anything.
    void loadDefaultFont()
    {
        m_holdsDefaultFont=true;

        if(m_defaultFontLoadedOn || !m_render)
            return;

        const gweni::Font::Status status=m_render->getLoader().loadFont(m_defaultFont);

        if((status != gweni::Font::Status::Loaded) && (status != gweni::Font::Status::LoadedFallback))
            return;

        m_loadedDefaultFont=m_defaultFont;
        m_defaultFontLoadedOn=m_render;
    }

    //! Gives back the default font the skin loaded, on the renderer it was
    //! loaded on; nothing if it holds none.
    void freeDefaultFont()
    {
        if(!m_defaultFontLoadedOn)
            return;

        gweni::renderer::Base *render=m_defaultFontLoadedOn;

        m_defaultFontLoadedOn=nullptr;
        render->getLoader().freeFont(m_loadedDefaultFont);
    }

    gweni::Font m_defaultFont;
    gweni::renderer::Base *m_render;

    //! The default font as it was loaded and the renderer that loaded it, or
    //! nullptr while the skin holds none: what ~Base and setDefaultFont free,
    //! whatever m_defaultFont has been set to since.
    gweni::Font m_loadedDefaultFont;
    gweni::renderer::Base *m_defaultFontLoadedOn=nullptr;
    //! The skin has asked to hold its default font (init or setDefaultFont), so
    //! a setRender loads it on the new renderer, even after a null one.
    bool m_holdsDefaultFont=false;

};


}
}
#endif//_gweni_skin_h_
