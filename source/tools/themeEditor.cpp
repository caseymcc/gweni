/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/themeEditor.h>

#include <gweni/gweni.h>
#include <gweni/controls/button.h>
#include <gweni/controls/colorButton.h>
#include <gweni/controls/colorEdit.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/utility.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gweni
{
namespace tools
{

namespace tokyonight=skin::textured::tokyonight;

const char *const ThemeEditor::TextColorsCaption="Text colours";

namespace
{

//! The rows of a surface's node, in order.
const ThemeSurfaceField SurfaceFields[]={ThemeSurfaceField::Top, ThemeSurfaceField::Bottom,
    ThemeSurfaceField::Border, ThemeSurfaceField::Radius, ThemeSurfaceField::BorderWidth};

const char *fieldName(ThemeSurfaceField field)
{
    switch(field)
    {
    case ThemeSurfaceField::Top:         return "top";
    case ThemeSurfaceField::Bottom:      return "bottom";
    case ThemeSurfaceField::Border:      return "border";
    case ThemeSurfaceField::Radius:      return "radius";
    case ThemeSurfaceField::BorderWidth: return "borderWidth";
    }
    return "";
}

bool isColorField(ThemeSurfaceField field)
{
    return field == ThemeSurfaceField::Top || field == ThemeSurfaceField::Bottom
        || field == ThemeSurfaceField::Border;
}

gweni::Color *surfaceColor(tokyonight::Surface &surface, ThemeSurfaceField field)
{
    switch(field)
    {
    case ThemeSurfaceField::Top:    return &surface.top;
    case ThemeSurfaceField::Bottom: return &surface.bottom;
    case ThemeSurfaceField::Border: return &surface.border;
    default:                        return nullptr;
    }
}

float *surfaceNumber(tokyonight::Surface &surface, ThemeSurfaceField field)
{
    switch(field)
    {
    case ThemeSurfaceField::Radius:      return &surface.radius;
    case ThemeSurfaceField::BorderWidth: return &surface.borderWidth;
    default:                             return nullptr;
    }
}

tokyonight::Surface *findSurface(const String &name)
{
    auto &registry=tokyonight::surfaceRegistry();
    auto iter=registry.find(name);

    return (iter == registry.end())?nullptr:iter->second;
}

//! Whether a control in \p control's subtree has the keyboard focus.
bool holdsFocus(const controls::Base *control)
{
    for(const controls::Base *at=KeyboardFocus; at; at=at->getParent())
    {
        if(at == control)
            return true;
    }
    return false;
}

//! Gives every Text that follows a skin colour its value again and lets every
//! control re-pick its colours.
void refreshColors(controls::Base *control)
{
    if(controls::internal::Text *text=gweni_cast<controls::internal::Text>(control))
        text->refreshSkinTextColor();
    control->updateColors();

    for(controls::Base *child : control->getActualChildren())
        refreshColors(child);
}

}//namespace

///////////////////////////////////////////////////////////////////////////
// ThemeColorProperty

// The ColorEdit fills the row's value column and sets its height.
GWENI_CONTROL_CONSTRUCTOR(ThemeColorProperty)
{
    setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});

    m_edit=newChild<controls::ColorEdit>();
    m_edit->setMode(controls::ColorEditMode::Hex);
    m_edit->setDock(DockPosition::Left);
    m_edit->onColorChanged.add(this, &ThemeColorProperty::onColorChanged);
}

String ThemeColorProperty::getPropertyValue()
{
    return controls::ColorButton::toHex(m_edit->getColor(), true);
}

void ThemeColorProperty::setPropertyValue(const String &value, bool fireChangeEvents)
{
    gweni::Color color;

    if(controls::ColorButton::fromHex(value, color, true))
        m_edit->setColor(color, fireChangeEvents);
}

bool ThemeColorProperty::isEditing()
{
    return holdsFocus(m_edit);
}

void ThemeColorProperty::onColorChanged(event::Info)
{
    doChanged();
}

///////////////////////////////////////////////////////////////////////////
// ThemeNumberProperty

GWENI_CONTROL_CONSTRUCTOR(ThemeNumberProperty)
{
    setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});

    m_number=newChild<controls::DragNumber>();
    m_number->setDock(DockPosition::Left);
    m_number->setWidth(60);
    m_number->setRange(0.0, 32.0);
    m_number->setSpeed(0.1);
    m_number->setFormat("%g");
    m_number->onValueChanged.add(this, &ThemeNumberProperty::onNumberChanged);
}

String ThemeNumberProperty::getPropertyValue()
{
    return m_number->formatNumber(m_number->getNumber());
}

void ThemeNumberProperty::setPropertyValue(const String &value, bool fireChangeEvents)
{
    double number;

    if(controls::DragNumber::parseNumber(value, number))
        m_number->setNumber(number, fireChangeEvents);
}

bool ThemeNumberProperty::isEditing()
{
    return holdsFocus(m_number);
}

void ThemeNumberProperty::onNumberChanged(event::Info)
{
    doChanged();
}

///////////////////////////////////////////////////////////////////////////
// ThemeEditorNode

GWENI_CONTROL_CONSTRUCTOR(ThemeEditorNode)
{
    m_properties=newChild<controls::Properties>();
    m_properties->setDock(DockPosition::Top);
}

std::vector<controls::PropertyRow *> ThemeEditorNode::rows() const
{
    std::vector<controls::PropertyRow *> rows;

    for(controls::Base *child : m_properties->getChildren())
    {
        if(controls::PropertyRow *row=gweni_cast<controls::PropertyRow>(child))
            rows.push_back(row);
    }
    return rows;
}

void ThemeEditorNode::open()
{
    if(!m_populated && m_editor)
    {
        m_populated=true;
        m_editor->populate(this);
    }
    ParentClass::open();
}

///////////////////////////////////////////////////////////////////////////
// ThemeEditor

GWENI_CONTROL_CONSTRUCTOR(ThemeEditor)
{
    setTitle("Theme editor");
    setSize(420, 500);
    setDeleteOnClose(true);

    m_skin=dynamic_cast<skin::textured::TexturedSkin *>(getSkin());

    controls::Base *bar=newChild<controls::Base>();

    bar->setDock(DockPosition::Top);
    bar->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    bar->setMargin(Margin(0, 0, 0, 4));

    auto button=[&](const char *caption, void (ThemeEditor::*handler)(event::Info))
    {
        controls::Button *b=bar->newChild<controls::Button>();

        b->setText(caption);
        b->setDock(DockPosition::Left);
        b->setMargin(Margin(0, 0, 4, 0));
        b->onPressCaller.add(this, handler);
        return b;
    };

    m_revert=button("Revert", &ThemeEditor::onRevert);
    m_load=button("Load", &ThemeEditor::onLoad);
    m_save=button("Save", &ThemeEditor::onSave);

    m_status=newChild<controls::Label>();
    m_status->setDock(DockPosition::Bottom);
    m_status->setMargin(Margin(0, 4, 0, 0));
    m_status->setText(" ");

    m_tree=newChild<controls::PropertyTree>();
    m_tree->setDock(DockPosition::Center);

    if(!m_skin)
    {
        setStatus("Needs the textured skin: nothing to edit");
        return;
    }

    m_openedWith=m_skin->captureTheme();

    // the surfaces sorted by name (m_openedWith's map), then the text colours
    auto addNode=[&](const String &caption, bool textColors)
    {
        ThemeEditorNode *node=m_tree->newChild<ThemeEditorNode>();

        node->setText(caption);
        node->setDock(DockPosition::Top);
        node->m_editor=this;
        node->m_name=caption;
        node->m_textColors=textColors;
        m_nodes.push_back(node);
        return node;
    };

    for(const auto &entry : m_openedWith.surfaces)
        addNode(entry.first, false);
    m_textColorsNode=addNode(TextColorsCaption, true);
}

ThemeEditor *ThemeEditor::find(controls::Canvas *canvas)
{
    if(!canvas)
        return nullptr;
    for(controls::Base *child : canvas->getActualChildren())
    {
        if(ThemeEditor *editor=gweni_cast<ThemeEditor>(child))
            return editor;
    }
    return nullptr;
}

ThemeEditor *ThemeEditor::open(controls::Canvas *canvas)
{
    if(ThemeEditor *editor=find(canvas))
        return editor;
    if(!canvas)
        return nullptr;

    ThemeEditor *editor=canvas->newChild<ThemeEditor>();

    editor->setPos(10, 10);
    return editor;
}

void ThemeEditor::close(controls::Canvas *canvas)
{
    if(ThemeEditor *editor=find(canvas))
    {
        editor->hide();
        editor->delayedDelete();
    }
}

bool ThemeEditor::toggle(controls::Canvas *canvas)
{
    ThemeEditor *editor=find(canvas);

    // one closed by its button is hidden until its delayed delete
    if(editor && editor->visible())
    {
        close(canvas);
        return false;
    }
    if(editor)
    {
        editor->setHidden(false);
        return true;
    }
    return open(canvas) != nullptr;
}

void ThemeEditor::applyToCanvas(controls::Canvas *canvas, bool relayout)
{
    if(!canvas)
        return;

    refreshColors(canvas);
    canvas->addStateChangeRecursive(controls::StateChange_Background|controls::StateChange_Text);

    if(relayout)
    {
        canvas->invalidate();
        canvas->invalidateChildren(true);
    }
}

ThemeEditorNode *ThemeEditor::surfaceNode(const String &surface) const
{
    for(ThemeEditorNode *node : m_nodes)
    {
        if(!node->isTextColors() && (node->name() == surface))
            return node;
    }
    return nullptr;
}

void ThemeEditor::setThemePath(const String &path)
{
    m_themePath=path;
}

void ThemeEditor::populate(ThemeEditorNode *node)
{
    if(!m_skin)
        return;

    controls::Properties *properties=node->properties();

    if(node->isTextColors())
    {
        for(const auto &entry : m_openedWith.textColors)
        {
            ThemeColorProperty *property=properties->newChild<ThemeColorProperty>();

            property->m_name=entry.first;
            property->m_textColor=true;
            properties->add(entry.first, property, "");
            property->onChange.add(this, &ThemeEditor::onPropertyChanged);
        }
    }
    else
    {
        for(ThemeSurfaceField field : SurfaceFields)
        {
            controls::property::Base *property;

            if(isColorField(field))
            {
                ThemeColorProperty *color=properties->newChild<ThemeColorProperty>();

                color->m_name=node->name();
                color->m_field=field;
                color->onChange.add(this, &ThemeEditor::onPropertyChanged);
                property=color;
            }
            else
            {
                ThemeNumberProperty *number=properties->newChild<ThemeNumberProperty>();

                number->m_name=node->name();
                number->m_field=field;
                number->onChange.add(this, &ThemeEditor::onPropertyChanged);
                property=number;
            }
            properties->add(fieldName(field), property, "");
        }
    }
    syncRows();
}

void ThemeEditor::syncRows()
{
    if(!m_skin)
        return;

    const std::unordered_map<std::string, gweni::Color *> textColors=m_skin->textColorRegistry();

    m_syncing=true;
    for(ThemeEditorNode *node : m_nodes)
    {
        if(!node->populated())
            continue;

        tokyonight::Surface *surface=node->isTextColors()?nullptr:findSurface(node->name());

        for(controls::PropertyRow *row : node->rows())
        {
            controls::property::Base *property=row->getProperty();

            if(ThemeColorProperty *color=gweni_cast<ThemeColorProperty>(property))
            {
                if(color->m_textColor)
                {
                    auto iter=textColors.find(color->m_name);

                    if(iter != textColors.end())
                        color->m_edit->setColor(*iter->second, false);
                }
                else if(surface)
                    color->m_edit->setColor(*surfaceColor(*surface, color->m_field), false);
            }
            else if(ThemeNumberProperty *number=gweni_cast<ThemeNumberProperty>(property))
            {
                if(surface)
                    number->m_number->setNumber(*surfaceNumber(*surface, number->m_field), false);
            }
        }
    }
    m_syncing=false;
}

void ThemeEditor::redrawCanvas(bool relayout)
{
    ++m_redraws;
    applyToCanvas(getCanvas(), relayout);
}

void ThemeEditor::setStatus(const String &text)
{
    if(m_status->getText() != text)
        m_status->setText(text);
}

void ThemeEditor::onPropertyChanged(event::Info info)
{
    if(ThemeColorProperty *color=gweni_cast<ThemeColorProperty>(info.controlCaller))
        colorEdited(color);
    else if(ThemeNumberProperty *number=gweni_cast<ThemeNumberProperty>(info.controlCaller))
        numberEdited(number);
}

void ThemeEditor::colorEdited(ThemeColorProperty *property)
{
    if(m_syncing)
        return;
    if(property->m_textColor)
        setTextColor(property->m_name, property->m_edit->getColor());
    else
        setSurfaceColor(property->m_name, property->m_field, property->m_edit->getColor());
}

void ThemeEditor::numberEdited(ThemeNumberProperty *property)
{
    if(m_syncing)
        return;
    setSurfaceNumber(property->m_name, property->m_field, (float)property->m_number->getNumber());
}

bool ThemeEditor::setSurfaceColor(const String &surfaceName, ThemeSurfaceField field, const gweni::Color &color)
{
    tokyonight::Surface *surface=m_skin?findSurface(surfaceName):nullptr;
    gweni::Color *value=surface?surfaceColor(*surface, field):nullptr;

    if(!value)
        return false;
    if(*value == color)
        return true;

    *value=color;
    syncRows();
    redrawCanvas();
    return true;
}

bool ThemeEditor::setSurfaceNumber(const String &surfaceName, ThemeSurfaceField field, float number)
{
    tokyonight::Surface *surface=m_skin?findSurface(surfaceName):nullptr;
    float *value=surface?surfaceNumber(*surface, field):nullptr;

    if(!value)
        return false;
    if(*value == number)
        return true;

    *value=number;
    syncRows();
    redrawCanvas();
    return true;
}

bool ThemeEditor::setTextColor(const String &name, const gweni::Color &color)
{
    if(!m_skin)
        return false;

    const std::unordered_map<std::string, gweni::Color *> textColors=m_skin->textColorRegistry();
    auto iter=textColors.find(name);

    if(iter == textColors.end())
        return false;
    if(*iter->second == color)
        return true;

    *iter->second=color;
    syncRows();
    redrawCanvas();
    return true;
}

void ThemeEditor::revert()
{
    if(!m_skin)
        return;

    const skin::textured::TexturedSkin::ThemeValues now=m_skin->captureTheme();

    if(now != m_openedWith)
    {
        m_skin->applyTheme(m_openedWith);
        syncRows();
        redrawCanvas(now.metrics != m_openedWith.metrics);
    }
    setStatus("Reverted to the theme it was opened with");
}

bool ThemeEditor::load()
{
    if(!m_skin)
        return false;
    if(m_themePath.empty())
    {
        setStatus("No theme file to load");
        return false;
    }

    const skin::textured::TexturedSkin::ThemeValues before=m_skin->captureTheme();

    if(!m_skin->loadThemeFile(m_themePath))
    {
        setStatus("Could not load "+m_themePath);
        return false;
    }

    const skin::textured::TexturedSkin::ThemeValues after=m_skin->captureTheme();

    if(after != before)
    {
        syncRows();
        redrawCanvas(after.metrics != before.metrics);
    }
    setStatus("Loaded "+m_themePath);
    return true;
}

bool ThemeEditor::save()
{
    if(!m_skin)
        return false;
    if(m_themePath.empty())
    {
        setStatus("No theme file to save to");
        return false;
    }
    if(!m_skin->saveThemeFile(m_themePath))
    {
        setStatus("Could not save "+m_themePath);
        return false;
    }
    setStatus("Saved "+m_themePath);
    return true;
}

void ThemeEditor::onRevert(event::Info)
{
    revert();
}

void ThemeEditor::onLoad(event::Info)
{
    load();
}

void ThemeEditor::onSave(event::Info)
{
    save();
}

}//namespace tools
}//namespace gweni
