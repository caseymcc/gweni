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

    const std::vector<size_t> &getPrimitives()
    {
        return m_primitives;
    }

protected:
    void generatePrimitives(renderer::Base *baseRenderer, size_t count)
    {
        if(count == 0)
            return;

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
    }

    std::vector<size_t> m_primitives;
};


class GWENI_EXPORT Base
{
public:

    Base(gweni::renderer::Base *renderer=nullptr)
    {
        m_defaultFont.facename="arial";
        m_defaultFont.size=10.0f;
        m_render=renderer;
    }

    virtual ~Base()
    {
        releaseFont(&m_defaultFont);
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
//    virtual void drawTabButton(controls::Base *control, State state, bool active, Position dir)=0;
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
//    virtual void drawWindowCloseButton(gweni::controls::Base *control, State state, bool depressed,
//        bool hovered, bool disabled)=0;
//    virtual void drawWindowMaximizeButton(gweni::controls::Base *control, State state, bool depressed,
//        bool hovered, bool disabled,
//        bool bMaximized)=0;
//    virtual void drawWindowMinimizeButton(gweni::controls::Base *control, State state, bool depressed,
//        bool hovered, bool disabled)=0;
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
//    virtual void drawScrollButton(controls::Base *control, State state, Position direction, bool depressed,
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


    virtual void setRender(gweni::renderer::Base *renderer)
    {
        m_render=renderer;
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

    } Colors;


public:

    virtual const gweni::Font &getDefaultFont()
    {
        return m_defaultFont;
    }

    virtual void setDefaultFont(const gweni::String &strFacename, float fSize=10.0f)
    {
        m_defaultFont.facename=strFacename;
        m_defaultFont.size=fSize;
    }

protected:

    gweni::Font m_defaultFont;
    gweni::renderer::Base *m_render;

};


}
}
#endif//_gweni_skin_h_
