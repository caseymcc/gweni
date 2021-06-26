
#ifndef _gweni_controls_layout_base_h_
#define _gweni_controls_layout_base_h_

#include <gweni/controls/base.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

enum LayoutType
{
    Horizontal,
    Vertical
};

class LayoutItem
{
public:
    LayoutItem();
    LayoutItem(LayoutType type);
    ~LayoutItem();

    void setParent(Base *parent);
    Base *getParent() { return m_parent; }
    void setLayoutParent(LayoutItem *layoutParent);
    LayoutItem *getLayoutParent() { return m_layoutParent; }

    void addControl(Base *control);
    void removeControl(Base *control);
    LayoutItem *findControl(Base *control);

    void addLayout(LayoutItem *item);
    void removeLayout(LayoutItem *item);

    Base *getControl();
    void setControl(Base *control);
    void updateControl();

    void calculateSize(skin::Base *skin, Dim dim);
    void arrange(skin::Base *skin, Dim dim);

    const gweni::Rect &getBounds() const { return m_bounds; }
    const void setBounds(gweni::Rect &bounds) { m_bounds=bounds; }

    void setPosition(int x, int y) { m_bounds.x=x; m_bounds.y=y; }

    const SizeFlags &getSizeFlags() const { return m_sizeFlags; }
    const Size &getMinimumSize() const { return m_minimumSize; }
    const Size &getMaximumSize() const { return m_maximumSize; }
    const Size &getPreferredSize() const { return m_preferredSize; }

private:
    void calculateHorizontal();
    void calculateMaxHeight();
    void calculateVertical();
    void calculateMaxWidth();

    void updateControlSize(Dim dim);

    void arrangeHorizontal();
    void arrangeVerticalPos();
    void arrangeVertical();
    void arrangeHorizontalPos();

    Base *m_parent;
    LayoutItem *m_layoutParent;
    std::vector<LayoutItem *> m_children;
    Base *m_control;

    LayoutType m_type;
    gweni::Rect m_bounds;
    Margin m_margin;
    Margin m_padding;
    int m_spacing;

    SizeFlags m_sizeFlags;
    Size m_minimumSize;
    Size m_maximumSize;
    Size m_preferredSize;
};

//class LayoutBase:public LayoutItem
//{
//public:
//    LayoutBase(Layout type);
//
//    int GetSpacing();
//    void SetSpacing(int value);
//
//    int getMargin();
//    void setMargin(int value);
//    void SetMargins(int left, int top, int right, int bottom);
//
//    Size getMinimumSize();
//    Size getMaximumSize();
//
//    void doThink();
//};

class HorizontalLayout:public LayoutItem
{
public:
    HorizontalLayout():LayoutItem(LayoutType::Horizontal) {}
};

class VerticalLayout:public LayoutItem
{
public:
    VerticalLayout():LayoutItem(LayoutType::Vertical) {}
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_layout_base_h_
