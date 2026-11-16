/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_breadcrumb_h_
#define _gweni_controls_breadcrumb_h_

#include <gweni/controls/button.h>

#include <vector>

namespace gweni
{
namespace controls
{

class Breadcrumb;

namespace internal
{

//! A persistent path item. Button supplies its press and hover handling;
//! Breadcrumb gives it its index and decides whether it takes input.
class GWENI_EXPORT BreadcrumbItem: public Button
{
public:
    GWENI_CONTROL(BreadcrumbItem, Button);

public:
    size_t getIndex() const { return m_index; }
    bool isCurrent() const { return m_current; }
    bool isClickable() const { return m_clickable; }

    void onPress(event::Info info) override;
    void updateColors() override;

private:
    friend class controls::Breadcrumb;

    void setCurrent(bool current, bool clickable);

    Breadcrumb *m_breadcrumb=nullptr;
    size_t m_index=0;
    bool m_current=false;
    bool m_clickable=true;
};

//! The separator before a path item: a right chevron, or Label's text.
class GWENI_EXPORT BreadcrumbSeparator: public Label
{
public:
    GWENI_CONTROL(BreadcrumbSeparator, Label);

public:
    static constexpr int ChevronSize=8;
    static constexpr int ChevronWidth=16;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void updateColors() override;
};

}//namespace internal

//! A clickable path whose last item is the current location. Items and
//! separators are kept across pushes and pops; only the changed tail is
//! made or removed. Clicking reports an index and leaves navigation to the
//! app (which may popTo that index from its handler).
class GWENI_EXPORT Breadcrumb: public Base
{
public:
    GWENI_CONTROL(Breadcrumb, Base);
    GWENI_DRAWS_NOTHING;

public:
    struct Item
    {
        String label;
        String name; //!< Optional app id or name, as SearchComboBox::Option.
    };

    ~Breadcrumb() override;

    //! Sets the labels, clearing their app data. Existing item controls at
    //! shared indices stay in place, and equal labels do no work.
    void setItems(std::vector<String> items);
    void push(const String &text, const String &name="");
    //! Keeps items through index (inclusive); an out-of-range index does
    //! nothing. setItems({}) clears the path.
    void popTo(size_t index);

    size_t getItemCount() const { return m_items.size(); }
    const Item &getItem(size_t index) const { return m_items[index]; }
    //! App data changes no drawing and fires no event.
    void setItemData(size_t index, const String &name);
    const String &getItemData(size_t index) const { return m_items[index].name; }

    internal::BreadcrumbItem *getItemControl(size_t index) const;
    //! The separator before index, null for the first or an invalid index.
    internal::BreadcrumbSeparator *getSeparatorControl(size_t index) const;

    void setLastItemClickable(bool clickable);
    bool isLastItemClickable() const { return m_lastClickable; }

    //! Empty uses right chevrons (the default).
    void setSeparatorText(const String &text);
    const String &getSeparatorText() const { return m_separatorText; }

    void setDisabled(bool disabled) override;

    //! The info's integer is the path index; control is this Breadcrumb.
    event::Caller onItemClicked;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

private:
    friend class internal::BreadcrumbItem;

    void itemClicked(size_t index);
    void updateCurrent();
    void removeTail(size_t count);

    std::vector<Item> m_items;
    std::vector<internal::BreadcrumbItem *> m_itemControls;
    //! Indexed like the items, with null at zero.
    std::vector<internal::BreadcrumbSeparator *> m_separators;
    String m_separatorText;
    bool m_lastClickable=false;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_breadcrumb_h_
