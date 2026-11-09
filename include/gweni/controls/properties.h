/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_properties_h_
#define _gweni_controls_properties_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/property/baseProperty.h>
#include <gweni/controls/property/text.h>
#include <gweni/controls/splitterBar.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
class PropertyRow;

class GWENI_EXPORT Properties: public Base
{
public:

    GWENI_CONTROL(Properties, Base);

public:
    //! A container of its PropertyRows.
    GWENI_DRAWS_NOTHING

    void arrange(gweni::skin::Base *skin, Dim dim) override;

    PropertyRow *add(const String &text, const String &value="");
    PropertyRow *add(const String &text, property::Base *prop,
        const String &value="");
    PropertyRow *find(const String &text);

    //! Where the label column ends: the split, clamped to this control's
    //! width once it has one wider than the bar.
    virtual int getSplitWidth();
    //! The split as set (dragged), not clamped: what a row asks for, which
    //! does not depend on the width an earlier pass gave this control.
    int getRequestedSplitWidth() const { return m_splitWidth; }

    //! The least width a row asks for its value column, in logical units
    //! (default 100). A row asks for the wider of this and its property's
    //! preferred width with its margins, so this control's preferred width is
    //! one its values can be used at; an app's own setWidth() on a property
    //! does not count. Setting it lays the rows out again.
    void setValueWidth(int width);
    int getValueWidth() const { return m_valueWidth; }

    virtual void clear();

protected:

    virtual void onSplitterMoved(event::Info info);

    static const int SplitterBarWidth=3;

    controls::SplitterBar *m_splitterBar;

    // The requested split, independent of the bar's clamped position: a layout
    // pass against a not-yet-sized parent must not be able to lose it.
    int m_splitWidth;
    int m_valueWidth;

};


class GWENI_EXPORT PropertyRow: public Base
{
public:

    GWENI_CONTROL(PropertyRow, Base);

public:
    virtual Label *getLabel()
    {
        return m_label;
    }

    virtual void            setProperty(property::Base *prop);
    virtual property::Base *getProperty()
    {
        return m_property;
    }

    void calculateSize(gweni::skin::Base *skin, Dim dim) override;
    void arrange(gweni::skin::Base *skin, Dim dim) override;

    void onMouseEnter() override;
    void onMouseLeave() override;
    //! The property (or a control in it) was hovered or focused: the row and
    //! its label take the row's new look, if it changed.
    void onDescendantInteraction(Base *control) override;

    //! The property's editor has the keyboard focus. The row's Editing look,
    //! which wins over Hover.
    virtual bool isEditing()
    {
        return m_property && m_property->isEditing();
    }

    //! The mouse is over the row or any control in it: the label, the
    //! property and whatever the property holds (a ColorSelector's button).
    bool isHovered() const override;

    //! Called in the update phase when isEditing() or isHovered() changed:
    //! each repaints the row and its label.
    virtual void onEditingChanged();
    virtual void onHoverChanged();

    event::Caller onChange;

protected:

    void onPropertyValueChanged(event::Info info);
    //! Calls onEditingChanged() and onHoverChanged() for what changed since
    //! the last call, in the update phase, so a move that keeps the row's look
    //! (from its label to its property) repaints nothing.
    void pollInteraction();

    Label *m_label;
    property::Base *m_property;

    bool m_lastEditing;
    bool m_lastHover;

};


}
}
#endif//_gweni_controls_properties_h_
