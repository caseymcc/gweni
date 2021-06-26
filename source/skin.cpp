/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>

namespace gweni
{
namespace skin
{
// Here we're drawing a few symbols such as the directional arrows and
// the checkBox check
// 
// Texture'd skins don't generally use these - but the Simple skin
// does. We did originally
// use the marlett font to draw these.. but since that's a Windows font
// it wasn't a very
// good cross platform solution.

//void Base::drawArrowDown(size_t *primitiveIds, gweni::Rect rect)
//{
//    const float x=(rect.w/5.0f);
//    const float y=(rect.h/5.0f);
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+x*0.0f, rect.y+y*1.0f, x, y*1.0f));
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+x*1.0f, rect.y+y*1.0f, x, y*2.0f));
//    m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+x*2.0f, rect.y+y*1.0f, x, y*3.0f));
//    m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+x*3.0f, rect.y+y*1.0f, x, y*2.0f));
//    m_render->drawFilledRect(primitiveIds[4], gweni::Rect(rect.x+x*4.0f, rect.y+y*1.0f, x, y*1.0f));
//}
//
//void Base::drawArrowUp(size_t *primitiveIds, gweni::Rect rect)
//{
//    const float x=(rect.w/5.0f);
//    const float y=(rect.h/5.0f);
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+x*0.0f, rect.y+y*3.0f, x, y*1.0f));
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+x*1.0f, rect.y+y*2.0f, x, y*2.0f));
//    m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+x*2.0f, rect.y+y*1.0f, x, y*3.0f));
//    m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+x*3.0f, rect.y+y*2.0f, x, y*2.0f));
//    m_render->drawFilledRect(primitiveIds[4], gweni::Rect(rect.x+x*4.0f, rect.y+y*3.0f, x, y*1.0f));
//}
//
//void Base::drawArrowLeft(size_t *primitiveIds, gweni::Rect rect)
//{
//    const float x=(rect.w/5.0f);
//    const float y=(rect.h/5.0f);
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+x*3.0f, rect.y+y*0.0f, x*1.0f, y));
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+x*2.0f, rect.y+y*1.0f, x*2.0f, y));
//    m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+x*1.0f, rect.y+y*2.0f, x*3.0f, y));
//    m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+x*2.0f, rect.y+y*3.0f, x*2.0f, y));
//    m_render->drawFilledRect(primitiveIds[4], gweni::Rect(rect.x+x*3.0f, rect.y+y*4.0f, x*1.0f, y));
//}
//
//void Base::drawArrowRight(size_t *primitiveIds, gweni::Rect rect)
//{
//    const float x=(rect.w/5.0f);
//    const float y=(rect.h/5.0f);
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+x*1.0f, rect.y+y*0.0f, x*1.0f, y));
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+x*1.0f, rect.y+y*1.0f, x*2.0f, y));
//    m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+x*1.0f, rect.y+y*2.0f, x*3.0f, y));
//    m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+x*1.0f, rect.y+y*3.0f, x*2.0f, y));
//    m_render->drawFilledRect(primitiveIds[4], gweni::Rect(rect.x+x*1.0f, rect.y+y*4.0f, x*1.0f, y));
//}
//
//void Base::drawCheck(size_t *primitiveIds, gweni::Rect rect)
//{
//    const float x=(rect.w/5.0f);
//    const float y=(rect.h/5.0f);
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x+x*0.0f, rect.y+y*3.0f, x*2, y*2));
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+x*1.0f, rect.y+y*4.0f, x*2, y*2));
//    m_render->drawFilledRect(primitiveIds[2], gweni::Rect(rect.x+x*2.0f, rect.y+y*3.0f, x*2, y*2));
//    m_render->drawFilledRect(primitiveIds[3], gweni::Rect(rect.x+x*3.0f, rect.y+y*1.0f, x*2, y*2));
//    m_render->drawFilledRect(primitiveIds[4], gweni::Rect(rect.x+x*4.0f, rect.y+y*0.0f, x*2, y*2));
//}
//
//void Base::drawTreeNode(controls::Base *control, State state, bool open, bool selected, int labelHeight,
//    int labelWidth, int halfWay, int lastBranch, bool isRoot)
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 2)
//        enlargePrimitiveIds(control, primitiveIds, 2);
//
//    getRender()->setDrawColor(Colors.Tree.Lines);
//
//    if(!isRoot)
//        getRender()->drawFilledRect(primitiveIds[0], gweni::Rect(8, halfWay, 16-9, 1));
//
//    if(!open)
//        return;
//
//    getRender()->drawFilledRect(primitiveIds[1], gweni::Rect(14+7, labelHeight+1, 1, lastBranch+halfWay-
//        labelHeight));
//}
//
//void Base::drawPropertyTreeNode(controls::Base *control, State state, int BorderLeft, int BorderTop)
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 2)
//        enlargePrimitiveIds(control, primitiveIds, 2);
//
//    const gweni::Rect &rect=control->getRenderBounds();
//    m_render->setDrawColor(Colors.Properties.Border);
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(rect.x, rect.y, BorderLeft, rect.h));
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(rect.x+BorderLeft, rect.y, rect.w-BorderLeft,
//        BorderTop));
//}
//
//void Base::drawPropertyRow(controls::Base *control, State state, int iWidth, bool bBeingEdited,
//    bool hovered)
//{
//    PrimitiveIds &primitiveIds=control->getPrimitiveIds();
//    if(primitiveIds.size() < 3)
//        enlargePrimitiveIds(control, primitiveIds, 3);
//
//    gweni::Rect rect=control->getRenderBounds();
//
//    if(bBeingEdited)
//        m_render->setDrawColor(Colors.Properties.Column_Selected);
//    else if(hovered)
//        m_render->setDrawColor(Colors.Properties.Column_Hover);
//    else
//        m_render->setDrawColor(Colors.Properties.Column_Normal);
//
//    m_render->drawFilledRect(primitiveIds[0], gweni::Rect(0, rect.y, iWidth, rect.h));
//
//    if(bBeingEdited)
//        m_render->setDrawColor(Colors.Properties.Line_Selected);
//    else if(hovered)
//        m_render->setDrawColor(Colors.Properties.Line_Hover);
//    else
//        m_render->setDrawColor(Colors.Properties.Line_Normal);
//
//    m_render->drawFilledRect(primitiveIds[1], gweni::Rect(iWidth, rect.y, 1, rect.h));
//    rect.y+=rect.h-1;
//    rect.h=1;
//    m_render->drawFilledRect(primitiveIds[1], rect);
//}

} // namespace skin
} // namespace gweni
