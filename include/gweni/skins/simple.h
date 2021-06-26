/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios

 *  See license in Gweni.h
 */


#ifndef _gweni_skins_simple_h_
#define _gweni_skins_simple_h_

#include <gweni/skin.h>
#include <gweni/gweni.h>
#include <gweni/controls/base.h>

namespace gweni
{
namespace skin
{

class Simple: public gweni::skin::Base
{
public:

    Simple(gweni::renderer::Base *renderer)
        : gweni::skin::Base(renderer)
    {}

    gweni::Color m_colBorderColor;
    gweni::Color m_colControlOutlineLight;
    gweni::Color m_colControlOutlineLighter;
    gweni::Color m_colBG;
    gweni::Color m_colBGDark;
    gweni::Color m_colControl;
    gweni::Color m_colControlBorderHighlight;
    gweni::Color m_colControlDarker;
    gweni::Color m_colControlOutlineNormal;
    gweni::Color m_colControlBright;
    gweni::Color m_colControlDark;
    gweni::Color m_colHighlightBG;
    gweni::Color m_colHighlightBorder;
    gweni::Color m_colTooltipBackground;
    gweni::Color m_colTooltipBorder;
    gweni::Color m_colModal;

    Simple()
    {
        m_colBorderColor=gweni::Color(80, 80, 80, 255);
        m_colBG=gweni::Color(248, 248, 248, 255);
        m_colBGDark=gweni::Color(235, 235, 235, 255);
        m_colControl=gweni::Color(240, 240, 240, 255);
        m_colControlBright=gweni::Color(255, 255, 255, 255);
        m_colControlDark=gweni::Color(214, 214, 214, 255);
        m_colControlDarker=gweni::Color(180, 180, 180, 255);
        m_colControlOutlineNormal=gweni::Color(112, 112, 112, 255);
        m_colControlOutlineLight=gweni::Color(144, 144, 144, 255);
        m_colControlOutlineLighter=gweni::Color(210, 210, 210, 255);
        m_colHighlightBG=gweni::Color(192, 221, 252, 255);
        m_colHighlightBorder=gweni::Color(51, 153, 255, 255);
        m_colTooltipBackground=gweni::Color(255, 255, 225, 255);
        m_colTooltipBorder=gweni::Color(0, 0, 0, 255);
        m_colModal=gweni::Color(25, 25, 25, 150);
        m_defaultFont.facename="Microsoft Sans Serif";
        m_defaultFont.size=11;
    }

    void drawControl(controls::Base *control) override
    {

    }

    void drawGenericPanel(controls::Base *control, State state)
    {
        // TODO.
    }

    void drawButton(gweni::controls::Base *control, State state, bool depressed, bool hovered,
        bool disabled)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 18)
            enlargePrimitiveIds(control, primitiveIds, 18);

        int w=control->getWidth();
        int h=control->getHeight();
        drawButton(&primitiveIds[0], w, h, depressed, hovered);
    }

    void drawWindowCloseButton(gweni::controls::Base *control, State state, bool depressed,
        bool hovered, bool disabled)
    {
        // TODO.
        drawButton(control, state, depressed, hovered, disabled);
    }

    void drawWindowMaximizeButton(gweni::controls::Base *control, State state, bool depressed,
        bool hovered, bool disabled, bool bMaximized)
    {
        // TODO.
        drawButton(control, state, depressed, hovered, disabled);
    }

    void drawWindowMinimizeButton(gweni::controls::Base *control, State state, bool depressed,
        bool hovered, bool disabled)
    {
        // TODO.
        drawButton(control, state, depressed, hovered, disabled);
    }

    void drawMenuItem(gweni::controls::Base *control, State state, bool submenuOpen,
        bool checked)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 11)
            enlargePrimitiveIds(control, primitiveIds, 11);

        if(submenuOpen || control->isHovered())
        {
            m_render->setDrawColor(m_colHighlightBG);
            m_render->drawFilledRect(primitiveIds[0], control->getRenderBounds());
            m_render->setDrawColor(m_colHighlightBorder);
            m_render->drawLinedRect(&primitiveIds[1], control->getRenderBounds());
        }

        if(checked)
        {
            m_render->setDrawColor(Color(0, 0, 0, 255));
            gweni::Rect r(control->getWidth()/2-2, control->getHeight()/2-2, 5, 5);
            drawCheck(&primitiveIds[5], r);
        }
    }

    void drawMenuStrip(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 3)
            enlargePrimitiveIds(control, primitiveIds, 3);

        int w=control->getWidth();
        int h=control->getHeight();
        m_render->setDrawColor(gweni::Color(246, 248, 252, 255));
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(0, 0, w, h));
        m_render->setDrawColor(gweni::Color(218, 224, 241, 150));
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(0, h*0.4f, w, h*0.6f));
        m_render->drawFilledRect(primitiveIds[2], gweni::Rect(0, h/2, w, h/2));
    }

    void drawMenu(gweni::controls::Base *control, State state, bool paddingDisabled)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 3)
            enlargePrimitiveIds(control, primitiveIds, 3);

        int w=control->getWidth();
        int h=control->getHeight();
        m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(0, 0, w, h));

        if(!paddingDisabled)
        {
            m_render->setDrawColor(m_colControl);
            m_render->drawFilledRect(primitiveIds[1], gweni::Rect(1, 0, 22, h));
        }

        m_render->setDrawColor(m_colControlOutlineNormal);
        m_render->drawLinedRect(&primitiveIds[3], gweni::Rect(0, 0, w, h));
    }

    void drawShadow(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 3)
            enlargePrimitiveIds(control, primitiveIds, 3);

        const int w=control->getWidth(), h=control->getHeight();
        int x=4, y=6;
        m_render->setDrawColor(gweni::Color(0, 0, 0, 10));
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(x, y, w, h));
        x+=2;
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(x, y, w, h));
        y+=2;
        m_render->drawFilledRect(primitiveIds[2], gweni::Rect(x, y, w, h));
    }

    virtual void drawButton(size_t *primitiveIds, int w, int h, bool depressed, bool hovered,
        bool squared=false)
    {
        if(depressed)
            m_render->setDrawColor(m_colControlDark);
        else if(hovered)
            m_render->setDrawColor(m_colControlBright);
        else
            m_render->setDrawColor(m_colControl);
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(1, 1, w-2, h-2));

        if(depressed)
            m_render->setDrawColor(m_colControlDark);
        else if(hovered)
            m_render->setDrawColor(m_colControl);
        else
            m_render->setDrawColor(m_colControlDark);
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(1, h/2, w-2, h/2-2));

        if(!depressed)
        {
            m_render->setDrawColor(m_colControlBright);
            m_render->drawShavedCornerRect(&primitiveIds[2], gweni::Rect(1, 1, w-2, h-2), squared);
        }
        else
        {
            m_render->setDrawColor(m_colControlDarker);
            m_render->drawShavedCornerRect(&primitiveIds[2], gweni::Rect(1, 1, w-2, h-2), squared);
        }

        // Border
        m_render->setDrawColor(m_colControlOutlineNormal);
        m_render->drawShavedCornerRect(&primitiveIds[10], gweni::Rect(0, 0, w, h), squared);
    }

    void drawRadioButton(gweni::controls::Base *control, State state, bool selected,
        bool depressed)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 15)
            enlargePrimitiveIds(control, primitiveIds, 15);

        gweni::Rect rect=control->getRenderBounds();

        // Inside colour
        if(control->isHovered())
            m_render->setDrawColor(gweni::Color(220, 242, 254, 255));
        else
            m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(1, 1, rect.w-2, rect.h-2));

        // Border
        if(control->isHovered())
            m_render->setDrawColor(gweni::Color(85, 130, 164, 255));
        else
            m_render->setDrawColor(m_colControlOutlineLight);
        m_render->drawShavedCornerRect(&primitiveIds[1], rect);
        m_render->setDrawColor(gweni::Color(0, 50, 60, 15));
        m_render->drawFilledRect(primitiveIds[9], gweni::Rect(rect.x+2, rect.y+2, rect.w-4, rect.h-4));
        m_render->drawFilledRect(primitiveIds[10], gweni::Rect(rect.x+2, rect.y+2, rect.w*0.3f, rect.h-4));
        m_render->drawFilledRect(primitiveIds[11], gweni::Rect(rect.x+2, rect.y+2, rect.w-4, rect.h*0.3f));

        if(control->isHovered())
            m_render->setDrawColor(gweni::Color(121, 198, 249, 255));
        else
            m_render->setDrawColor(gweni::Color(0, 50, 60, 50));
        m_render->drawFilledRect(primitiveIds[12], gweni::Rect(rect.x+2, rect.y+3, 1, rect.h-5));
        m_render->drawFilledRect(primitiveIds[13], gweni::Rect(rect.x+3, rect.y+2, rect.w-5, 1));

        if(selected)
        {
            m_render->setDrawColor(gweni::Color(40, 230, 30, 255));
            m_render->drawFilledRect(primitiveIds[14], gweni::Rect(rect.x+2, rect.y+2, rect.w-4, rect.h-4));
        }
    }

    void drawCheckBox(gweni::controls::Base *control, State state, bool selected,
        bool depressed)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 15)
            enlargePrimitiveIds(control, primitiveIds, 15);

        gweni::Rect rect=control->getRenderBounds();

        // Inside colour
        if(control->isHovered())
            m_render->setDrawColor(gweni::Color(220, 242, 254, 255));
        else
            m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], rect);

        // Border
        if(control->isHovered())
            m_render->setDrawColor(gweni::Color(85, 130, 164, 255));
        else
            m_render->setDrawColor(m_colControlOutlineLight);
        m_render->drawLinedRect(&primitiveIds[1], rect);
        m_render->setDrawColor(gweni::Color(0, 50, 60, 15));
        m_render->drawFilledRect(primitiveIds[5], gweni::Rect(rect.x+2, rect.y+2, rect.w-4, rect.h-4));
        m_render->drawFilledRect(primitiveIds[6], gweni::Rect(rect.x+2, rect.y+2, rect.w*0.3f, rect.h-4));
        m_render->drawFilledRect(primitiveIds[7], gweni::Rect(rect.x+2, rect.y+2, rect.w-4, rect.h*0.3f));

        if(control->isHovered())
            m_render->setDrawColor(gweni::Color(121, 198, 249, 255));
        else
            m_render->setDrawColor(gweni::Color(0, 50, 60, 50));
        m_render->drawFilledRect(primitiveIds[8], gweni::Rect(rect.x+2, rect.y+2, 1, rect.h-4));
        m_render->drawFilledRect(primitiveIds[9], gweni::Rect(rect.x+2, rect.y+2, rect.w-4, 1));

        if(depressed)
        {
            m_render->setDrawColor(Color(100, 100, 100, 255));
            gweni::Rect r(control->getWidth()/2-2, control->getHeight()/2-2, 5, 5);
            drawCheck(&primitiveIds[10], r);
        }
        else if(selected)
        {
            m_render->setDrawColor(Color(0, 0, 0, 255));
            gweni::Rect r(control->getWidth()/2-2, control->getHeight()/2-2, 5, 5);
            drawCheck(&primitiveIds[10], r);
        }
    }

    void drawGroupBox(gweni::controls::Base *control, State state, int textStart, int textHeight,
        int textWidth)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 10)
            enlargePrimitiveIds(control, primitiveIds, 10);

        gweni::Rect rect=control->getRenderBounds();
        rect.y+=textHeight/2;
        rect.h-=textHeight/2;
        gweni::Color m_colDarker=gweni::Color(0, 50, 60, 50);
        gweni::Color m_colLighter=gweni::Color(255, 255, 255, 150);
        m_render->setDrawColor(m_colLighter);
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+1, rect.y+1, textStart-3, 1));
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+1+textStart+textWidth, rect.y+1,
            rect.w-textStart+textWidth-2, 1));
        m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+1, (rect.y+rect.h)-1, rect.w-2, 1));
        m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+1, rect.y+1, 1, rect.h));
        m_render->drawFilledRect(primitiveIds[4], gweni::Rect((rect.x+rect.w)-2, rect.y+1, 1, rect.h-1));
        m_render->setDrawColor(m_colDarker);
        m_render->drawFilledRect(primitiveIds[5], gweni::Rect(rect.x+1, rect.y, textStart-3, 1));
        m_render->drawFilledRect(primitiveIds[6], gweni::Rect(rect.x+1+textStart+textWidth, rect.y,
            rect.w-textStart-textWidth-2, 1));
        m_render->drawFilledRect(primitiveIds[7], gweni::Rect(rect.x+1, (rect.y+rect.h)-1, rect.w-2, 1));
        m_render->drawFilledRect(primitiveIds[8], gweni::Rect(rect.x, rect.y+1, 1, rect.h-1));
        m_render->drawFilledRect(primitiveIds[9], gweni::Rect((rect.x+rect.w)-1, rect.y+1, 1, rect.h-1));
    }

    void drawTextBox(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 9)
            enlargePrimitiveIds(control, primitiveIds, 9);

        gweni::Rect rect=control->getRenderBounds();
        bool bIsFocussed=control->isFocussed();

        // Box inside
        m_render->setDrawColor(gweni::Color(255, 255, 255, 255));
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(1, 1, rect.w-2, rect.h-2));
        m_render->setDrawColor(m_colControlOutlineLight);
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+1, rect.y, rect.w-2, 1));
        m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x, rect.y+1, 1, rect.h-2));
        m_render->setDrawColor(m_colControlOutlineLighter);
        m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+1, (rect.y+rect.h)-1, rect.w-2, 1));
        m_render->drawFilledRect(primitiveIds[4], gweni::Rect((rect.x+rect.w)-1, rect.y+1, 1, rect.h-2));

        if(bIsFocussed)
        {
            m_render->setDrawColor(gweni::Color(50, 200, 255, 150));
            m_render->drawLinedRect(&primitiveIds[5], rect);
        }
    }

    void drawTabButton(gweni::controls::Base *control, State state, bool active, Position dir)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 25)
            enlargePrimitiveIds(control, primitiveIds, 25);

        gweni::Rect rect=control->getRenderBounds();
        bool hovered=control->isHovered();

        if(hovered)
            m_render->setDrawColor(m_colControlBright);
        else
            m_render->setDrawColor(m_colControl);
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(1, 1, rect.w-2, rect.h-1));

        if(hovered)
            m_render->setDrawColor(m_colControl);
        else
            m_render->setDrawColor(m_colControlDark);
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(1, rect.h/2, rect.w-2, rect.h/2-1));
        m_render->setDrawColor(m_colControlBright);
        m_render->drawShavedCornerRect(&primitiveIds[9], gweni::Rect(1, 1, rect.w-2, rect.h));
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawShavedCornerRect(&primitiveIds[17], gweni::Rect(0, 0, rect.w, rect.h));
    }

    void drawTabControl(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rect=control->getRenderBounds();
        m_render->setDrawColor(m_colControl);
        m_render->drawFilledRect(primitiveIds[0], rect);
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawLinedRect(&primitiveIds[1], rect);
        //m_render->setDrawColor( m_colControl );
        //m_render->drawFilledRect( CurrentButtonRect );
    }

    void drawWindow(gweni::controls::Base *control, State state, int topHeight, bool inFocus)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 25)
            enlargePrimitiveIds(control, primitiveIds, 25);

        gweni::Rect rect=control->getRenderBounds();

        // Titlebar
        if(inFocus)
            m_render->setDrawColor(gweni::Color(87, 164, 232, 230));
        else
            m_render->setDrawColor(gweni::Color(87*0.70, 164*0.70, 232*0.70, 230));
        int iBorderSize=5;
        m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+1, rect.y+1, rect.w-2, topHeight-1));
        m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+1, rect.y+topHeight-1,
            iBorderSize, rect.h-2-topHeight));
        m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+rect.w-iBorderSize, rect.y+topHeight-1,
            iBorderSize, rect.h-2-topHeight));
        m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+1, rect.y+rect.h-iBorderSize,
            rect.w-2, iBorderSize));
        // Main inner
        m_render->setDrawColor(gweni::Color(m_colControlDark.r,
            m_colControlDark.g,
            m_colControlDark.b,
            230));
        m_render->drawFilledRect(primitiveIds[4], gweni::Rect(rect.x + iBorderSize + 1,
            rect.y + topHeight,
            rect.w - iBorderSize*2 - 2,
            rect.h - topHeight - iBorderSize - 1));
        // Light inner border
        m_render->setDrawColor(gweni::Color(255, 255, 255, 100));
        m_render->drawShavedCornerRect(&primitiveIds[5], gweni::Rect(rect.x+1, rect.y+1, rect.w-2, rect.h-2));
        // Dark line between titlebar and main
        m_render->setDrawColor(m_colBorderColor);
        // Inside border
        m_render->setDrawColor(m_colControlOutlineNormal);
        m_render->drawLinedRect(&primitiveIds[13], gweni::Rect(rect.x+iBorderSize, rect.y+topHeight-1,
            rect.w-10, rect.h-topHeight-(iBorderSize-1)));
        // Dark outer border
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawShavedCornerRect(&primitiveIds[17], gweni::Rect(rect.x, rect.y, rect.w, rect.h));
    }

    void drawHighlight(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 1)
            enlargePrimitiveIds(control, primitiveIds, 1);

        gweni::Rect rect=control->getRenderBounds();
        m_render->setDrawColor(gweni::Color(255, 100, 255, 255));
        m_render->drawFilledRect(primitiveIds[0], rect);
    }

    void drawScrollBar(gweni::controls::Base *control, State state, bool isHorizontal,
        bool depressed)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 1)
            enlargePrimitiveIds(control, primitiveIds, 1);

        gweni::Rect rect=control->getRenderBounds();

        if(depressed)
            m_render->setDrawColor(m_colControlDarker);
        else
            m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], rect);
    }

    void drawScrollBarBar(controls::Base *control, State state, bool depressed, bool isHovered,
        bool isHorizontal)
    {
        //TODO: something specialized
        drawButton(control, state, depressed, isHovered, false);
    }

    void drawTabTitleBar(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rect=control->getRenderBounds();
        m_render->setDrawColor(gweni::Color(177, 193, 214, 255));
        m_render->drawFilledRect(primitiveIds[0], rect);
        m_render->setDrawColor(m_colBorderColor);
        rect.h+=1;
        m_render->drawLinedRect(&primitiveIds[1], rect);
    }

    void drawProgressBar(gweni::controls::Base *control, State state, bool isHorizontal,
        float progress)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 26)
            enlargePrimitiveIds(control, primitiveIds, 26);

        gweni::Rect rect=control->getRenderBounds();
        gweni::Color FillColor(0, 211, 40, 255);

        if(isHorizontal)
        {
            //Background
            m_render->setDrawColor(m_colControlDark);
            m_render->drawFilledRect(primitiveIds[0], gweni::Rect(1, 1, rect.w-2, rect.h-2));
            //Right half
            m_render->setDrawColor(FillColor);
            m_render->drawFilledRect(primitiveIds[1], gweni::Rect(1, 1, rect.w*progress-2, rect.h-2));
            m_render->setDrawColor(gweni::Color(255, 255, 255, 150));
            m_render->drawFilledRect(primitiveIds[2], gweni::Rect(1, 1, rect.w-2, rect.h*0.45f));
        }
        else
        {
            //Background
            m_render->setDrawColor(m_colControlDark);
            m_render->drawFilledRect(primitiveIds[0], gweni::Rect(1, 1, rect.w-2, rect.h-2));
            //Top half
            m_render->setDrawColor(FillColor);
            m_render->drawFilledRect(primitiveIds[1], gweni::Rect(1, 1+(rect.h*(1-progress)),
                rect.w-2, rect.h*progress-2));
            m_render->setDrawColor(gweni::Color(255, 255, 255, 150));
            m_render->drawFilledRect(primitiveIds[2], gweni::Rect(1, 1, rect.w*0.45f, rect.h-2));
        }

        m_render->setDrawColor(gweni::Color(255, 255, 255, 150));
        m_render->drawShavedCornerRect(&primitiveIds[3], gweni::Rect(1, 1, rect.w-2, rect.h-2));
        m_render->setDrawColor(gweni::Color(255, 255, 255, 70));
        m_render->drawShavedCornerRect(&primitiveIds[11], gweni::Rect(2, 2, rect.w-4, rect.h-4));
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawShavedCornerRect(&primitiveIds[18], rect);
    }

    void drawListBox(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rect=control->getRenderBounds();
        m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], rect);
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawLinedRect(&primitiveIds[1], rect);
    }

    void drawListBoxLine(gweni::controls::Base *control, State state, bool selected, bool even)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 1)
            enlargePrimitiveIds(control, primitiveIds, 1);

        gweni::Rect rect=control->getRenderBounds();

        if(selected)
        {
            m_render->setDrawColor(m_colHighlightBorder);
            m_render->drawFilledRect(primitiveIds[0], rect);
        }
        else if(control->isHovered())
        {
            m_render->setDrawColor(m_colHighlightBG);
            m_render->drawFilledRect(primitiveIds[0], rect);
        }
    }

    void drawSlider(gweni::controls::Base *control, State state, bool isHorizontal,
        int numNotches, int barSize)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rect=control->getRenderBounds();

        if(isHorizontal)
        {
            rect.y+=rect.h*0.4f;
            rect.h-=rect.h*0.8f;
        }
        else
        {
            rect.x+=rect.w*0.4f;
            rect.w-=rect.w*0.8f;
        }

        m_render->setDrawColor(m_colBGDark);
        m_render->drawFilledRect(primitiveIds[0], rect);
        m_render->setDrawColor(m_colControlDarker);
        m_render->drawLinedRect(&primitiveIds[1], rect);
    }

    void drawComboBox(gweni::controls::Base *control, State state, bool isDown, bool isMenuOpen)
    {
        drawTextBox(control, state);
    }

    void drawKeyboardHighlight(gweni::controls::Base *control, State state, const gweni::Rect &r,
        int offset)
    {
//        gweni::Rect rect=r;
//        rect.x+=offset;
//        rect.y+=offset;
//        rect.w-=offset*2;
//        rect.h-=offset*2;
//
//        //draw the top and bottom
//        bool skip=true;
//
//        for(int i=0; i < rect.w/2; i++)
//        {
//            m_render->setDrawColor(gweni::Color(0, 0, 0, 255));
//
//            if(!skip)
//            {
//                m_render->drawPixel(&primitiveIds[0], rect.x+(i*2), rect.y);
//                m_render->drawPixel(&primitiveIds[1], rect.x+(i*2), rect.y+rect.h-1);
//            }
//            else
//            {
//                skip=!skip;
//            }
//        }
//
//        skip=false;
//
//        for(int i=0; i < rect.h/2; i++)
//        {
//            m_render->setDrawColor(gweni::Color(0, 0, 0, 255));
//
//            if(!skip)
//            {
//                m_render->drawPixel(rect.x, rect.y+i*2);
//                m_render->drawPixel(rect.x+rect.w-1, rect.y+i*2);
//            }
//            else
//            {
//                skip=!skip;
//            }
//        }
    }

    void drawTooltip(gweni::controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rct=control->getRenderBounds();
        rct.x-=3;
        rct.y-=3;
        rct.w+=6;
        rct.h+=6;
        m_render->setDrawColor(m_colTooltipBackground);
        m_render->drawFilledRect(primitiveIds[0], rct);
        m_render->setDrawColor(m_colTooltipBorder);
        m_render->drawLinedRect(&primitiveIds[1], rct);
    }

    void drawScrollButton(gweni::controls::Base *control, State state, Position direction,
        bool depressed, bool hovered, bool disabled)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 24)
            enlargePrimitiveIds(control, primitiveIds, 24);

        drawButton(&primitiveIds[0], control->getWidth(), control->getHeight(), depressed, false, false);
        m_render->setDrawColor(gweni::Color(0, 0, 0, 240));
        Rect r(control->getWidth()/2 - 2, control->getHeight() / 2 - 2, 5, 5);

        if(direction == Position::Top)
            drawArrowUp(&primitiveIds[18], r);
        else if(direction == Position::Bottom)
            drawArrowDown(&primitiveIds[18], r);
        else if(direction == Position::Left)
            drawArrowLeft(&primitiveIds[18], r);
        else
            drawArrowRight(&primitiveIds[18], r);
    }

    void drawComboDownArrow(gweni::controls::Base *control, State state, bool hovered,
        bool down, bool open, bool disabled)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 6)
            enlargePrimitiveIds(control, primitiveIds, 6);

        //drawButton( control->getWidth(), control->getHeight(), depressed, false, true );
        m_render->setDrawColor(gweni::Color(0, 0, 0, 240));
        gweni::Rect r(control->getWidth()/2-2, control->getHeight()/2-2, 5, 5);
        drawArrowDown(&primitiveIds[0], r);
    }

    void drawNumericUpDownButton(gweni::controls::Base *control, State state, bool depressed,
        bool up)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 6)
            enlargePrimitiveIds(control, primitiveIds, 6);

        //drawButton( control->getWidth(), control->getHeight(), depressed, false, true );
        m_render->setDrawColor(gweni::Color(0, 0, 0, 240));
        gweni::Rect r(control->getWidth()/2-2, control->getHeight()/2-2, 5, 5);

        if(up)
            drawArrowUp(&primitiveIds[0], r);
        else
            drawArrowDown(&primitiveIds[0], r);
    }

    void drawTreeButton(controls::Base *control, State state, bool open)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 7)
            enlargePrimitiveIds(control, primitiveIds, 7);

        gweni::Rect rect=control->getRenderBounds();
        rect.x+=2;
        rect.y+=2;
        rect.w-=4;
        rect.h-=4;
        m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], rect);
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawLinedRect(&primitiveIds[1], rect);
        m_render->setDrawColor(m_colBorderColor);

        if(!open)   // ! because the button shows intention, not the current state
            m_render->drawFilledRect(primitiveIds[5], gweni::Rect(rect.x+rect.w/2,
                rect.y+2,
                1,
                rect.h-4));

        m_render->drawFilledRect(primitiveIds[6], gweni::Rect(rect.x+2,
            rect.y+rect.h/2,
            rect.w-4,
            1));
    }

    void drawTreeControl(controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rect=control->getRenderBounds();
        m_render->setDrawColor(m_colControlBright);
        m_render->drawFilledRect(primitiveIds[0], rect);
        m_render->setDrawColor(m_colBorderColor);
        m_render->drawLinedRect(&primitiveIds[1], rect);
    }

    void drawTreeNode(controls::Base *control, State state, bool open, bool selected, int labelHeight,
        int labelWidth, int halfWay, int lastBranch, bool isRoot)
    {
//        if(selected)
//        {
//            getRender()->setDrawColor(Color(0, 150, 255, 100));
//            getRender()->drawFilledRect(gweni::Rect(17, 0, labelWidth+2, labelHeight-1));
//            getRender()->setDrawColor(Color(0, 150, 255, 200));
//            getRender()->drawLinedRect(gweni::Rect(17, 0, labelWidth+2, labelHeight-1));
//        }
//
//        Base::drawTreeNode(ctrl, open, selected, labelHeight, labelWidth, halfWay,
//            lastBranch, isRoot);
    }

    void drawStatusBar(controls::Base *control, State state)
    {
        // TODO.
    }

    void drawColorDisplay(controls::Base *control, State state, gweni::Color color)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 8)
            enlargePrimitiveIds(control, primitiveIds, 8);

        gweni::Rect rect=control->getRenderBounds();

        if(color.a != 255)
        {
            getRender()->setDrawColor(gweni::Color(255, 255, 255, 255));
            getRender()->drawFilledRect(primitiveIds[0], rect);
            getRender()->setDrawColor(gweni::Color(128, 128, 128, 128));
            getRender()->drawFilledRect(primitiveIds[1], gweni::Rect(0, 0, rect.w/2, rect.h/2));
            getRender()->drawFilledRect(primitiveIds[2], gweni::Rect(rect.w/2, rect.h/2, rect.w/2, rect.h/2));
        }

        getRender()->setDrawColor(color);
        getRender()->drawFilledRect(primitiveIds[3], rect);
        getRender()->setDrawColor(gweni::Color(0, 0, 0, 255));
        getRender()->drawLinedRect(&primitiveIds[4], rect);
    }

    void drawModalControl(controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 1)
            enlargePrimitiveIds(control, primitiveIds, 1);

        if(control->shouldDrawBackground())
        {
            gweni::Rect rect=control->getRenderBounds();
            getRender()->setDrawColor(m_colModal);
            getRender()->drawFilledRect(primitiveIds[0], rect);
        }
    }

    void drawMenuDivider(controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 5)
            enlargePrimitiveIds(control, primitiveIds, 5);

        gweni::Rect rect=control->getRenderBounds();
        getRender()->setDrawColor(m_colBGDark);
        getRender()->drawFilledRect(primitiveIds[0], rect);
        getRender()->setDrawColor(m_colControlDarker);
        getRender()->drawLinedRect(&primitiveIds[1], rect);
    }

    void drawMenuRightArrow(controls::Base *control, State state)
    {
        PrimitiveIds &primitiveIds=control->getPrimitiveIds();
        if(primitiveIds.size() < 6)
            enlargePrimitiveIds(control, primitiveIds, 6);

        drawArrowRight(&primitiveIds[0], control->getRenderBounds());
    }

    void drawSlideButton(gweni::controls::Base *control, State state, bool depressed,
        bool horizontal)
    {
        drawButton(control, state, depressed, control->isHovered(), control->isDisabled());
    }

    void drawCategoryHolder(controls::Base *control, State state) {}
    void drawCategoryInner(controls::Base *control, State state, bool collapsed) {}
};


}//namespace skin
}//namespace gweni


#endif //_gweni_skins_simple_h_
