/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_tools_themeEditor_h_
#define _gweni_tools_themeEditor_h_

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/propertyTree.h>
#include <gweni/controls/property/baseProperty.h>
#include <gweni/controls/windowControl.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Button;
class ColorEdit;
class DragNumber;
}

namespace tools
{

class ThemeEditor;

//! The value of a surface a ThemeEditor row edits.
enum class ThemeSurfaceField
{
    Top,
    Bottom,
    Border,
    Radius,
    BorderWidth
};

//! A ThemeEditor row's value: one colour, in a ColorEdit (hex, with alpha).
//! Its property value is the colour's hex ("#7AA2F7", "#00000000").
class GWENI_EXPORT ThemeColorProperty: public controls::property::Base
{
public:
    GWENI_CONTROL(ThemeColorProperty, controls::property::Base);
    //! Its ColorEdit draws itself.
    GWENI_DRAWS_NOTHING
public:

    controls::ColorEdit *colorEdit() const { return m_edit; }

    String getPropertyValue() override;
    //! A hex colour, as ColorButton::fromHex reads it; anything else is
    //! ignored.
    void setPropertyValue(const String &value, bool fireChangeEvents=false) override;
    //! One of its fields has the keyboard focus.
    bool isEditing() override;

private:
    friend class ThemeEditor;

    void onColorChanged(event::Info info);

    controls::ColorEdit *m_edit=nullptr;
    //! The surface (and field) or the text colour it edits.
    String m_name;
    ThemeSurfaceField m_field=ThemeSurfaceField::Top;
    bool m_textColor=false;
};

//! A ThemeEditor row's value: a surface's radius or border width, in a
//! DragNumber. Its property value is the number's text.
class GWENI_EXPORT ThemeNumberProperty: public controls::property::Base
{
public:
    GWENI_CONTROL(ThemeNumberProperty, controls::property::Base);
    //! Its DragNumber draws itself.
    GWENI_DRAWS_NOTHING
public:

    controls::DragNumber *dragNumber() const { return m_number; }

    String getPropertyValue() override;
    void setPropertyValue(const String &value, bool fireChangeEvents=false) override;
    bool isEditing() override;

private:
    friend class ThemeEditor;

    void onNumberChanged(event::Info info);

    controls::DragNumber *m_number=nullptr;
    String m_name;
    ThemeSurfaceField m_field=ThemeSurfaceField::Radius;
};

//! A node of the ThemeEditor's tree: one surface (its top, bottom, border,
//! radius and border width) or the text colours (one row each). Its rows are
//! made when it is first opened, so a closed node holds an empty Properties.
class GWENI_EXPORT ThemeEditorNode: public controls::PropertyTreeNode
{
public:
    GWENI_CONTROL(ThemeEditorNode, controls::PropertyTreeNode);
public:

    //! The surface's name, or "Text colours".
    const String &name() const { return m_name; }
    bool isTextColors() const { return m_textColors; }
    //! Its rows are made (it was opened once).
    bool populated() const { return m_populated; }
    controls::Properties *properties() const { return m_properties; }
    //! The rows, in order (none until populated()).
    std::vector<controls::PropertyRow *> rows() const;

    void open() override;

private:
    friend class ThemeEditor;

    ThemeEditor *m_editor=nullptr;
    controls::Properties *m_properties=nullptr;
    String m_name;
    bool m_textColors=false;
    bool m_populated=false;
};

//! A window editing the textured skin's theme live (imgui's style editor;
//! docs/architecture.md, "The theme editor"): a PropertyTree of a node per
//! tokyonight surface, sorted by name, each with rows for its top, bottom and
//! border colours (a ColorEdit each, hex with alpha) and its radius and
//! border width (a DragNumber each), then a node of the skin's text colours
//! (TexturedSkin::textColorRegistry()), one ColorEdit row each.
//!
//! An edit writes the value at once: the surfaces are tokyonight's process
//! globals, the text colours the skin's. Then it redraws the editor's canvas
//! with applyToCanvas(), once per edit (a drag edits once a frame it moves the
//! value), never on a frame with no edit, so a settled editor writes nothing.
//! Only the editor's canvas is redrawn: another canvas drawing with the same
//! skin (or any textured skin, for the surfaces) shows the new values as its
//! controls redraw, or at once if the app calls applyToCanvas() on it.
//!
//! Revert gives back every value the editor was opened with. Save writes the
//! whole theme to getThemePath() as TexturedSkin::saveThemeFile() does (into
//! the "theme" object of a skin config there, or as a standalone theme file),
//! Load applies the file there through TexturedSkin::loadThemeFile(). Both
//! say what they did in the status line at the bottom.
//!
//! With a skin other than the textured one there is nothing to edit: the
//! tree is empty and the status line says so. The library binds no key: an
//! app opens and closes it (open(), close(), toggle()). Its close button
//! deletes it, keeping the theme as it is.
class GWENI_EXPORT ThemeEditor: public controls::WindowControl
{
public:
    GWENI_CONTROL(ThemeEditor, controls::WindowControl);
public:

    //! The canvas's theme editor, made and shown if it has none.
    static ThemeEditor *open(controls::Canvas *canvas);
    //! Deletes the canvas's theme editor (delayed), if it has one.
    static void close(controls::Canvas *canvas);
    //! Opens the editor, or closes it if it is open. Returns whether it is open.
    static bool toggle(controls::Canvas *canvas);
    //! The canvas's theme editor, or null.
    static ThemeEditor *find(controls::Canvas *canvas);

    //! Redraws every control of \p canvas with the theme as it is now: a
    //! Text that follows a skin colour (internal::SkinTextColor) takes its
    //! value again, every control re-picks its colours (updateColors()), and
    //! the whole tree is marked StateChange_Background|StateChange_Text, once.
    //! With \p relayout (a metric changed) the whole tree is laid out again
    //! too.
    static void applyToCanvas(controls::Canvas *canvas, bool relayout=false);

    //! The file Save writes and Load reads (a path as given, not through the
    //! ResourcePaths). Empty, the default, until the app sets one: Save and
    //! Load then only say so.
    void setThemePath(const String &path);
    const String &getThemePath() const { return m_themePath; }

    //! Writes the whole theme to getThemePath(). Returns whether it did.
    bool save();
    //! Applies the theme at getThemePath() and shows it in the rows. Returns
    //! whether it did.
    bool load();
    //! Gives back every value the editor was opened with.
    void revert();

    //! Set one value as an edit in its row does (the row shows it if it is
    //! made), redrawing the canvas if it changed. Return false for a name the
    //! skin has not or (no textured skin) for any.
    bool setSurfaceColor(const String &surface, ThemeSurfaceField field, const gweni::Color &color);
    bool setSurfaceNumber(const String &surface, ThemeSurfaceField field, float value);
    bool setTextColor(const String &name, const gweni::Color &color);

    //! The textured skin it edits, or null.
    skin::textured::TexturedSkin *texturedSkin() const { return m_skin; }
    //! The values it was opened with, which revert() gives back.
    const skin::textured::TexturedSkin::ThemeValues &openedWith() const { return m_openedWith; }

    controls::PropertyTree *tree() const { return m_tree; }
    //! Its nodes: the surfaces sorted by name, then the text colours.
    const std::vector<ThemeEditorNode *> &nodes() const { return m_nodes; }
    ThemeEditorNode *surfaceNode(const String &surface) const;
    ThemeEditorNode *textColorsNode() const { return m_textColorsNode; }
    controls::Button *revertButton() const { return m_revert; }
    controls::Button *loadButton() const { return m_load; }
    controls::Button *saveButton() const { return m_save; }
    //! What the last save, load or revert did.
    controls::Label *statusLine() const { return m_status; }
    //! The applyToCanvas() calls its edits, revert(), load() made.
    size_t redraws() const { return m_redraws; }

    //! The caption of the text colours' node.
    static const char *const TextColorsCaption;

private:
    friend class ThemeEditorNode;
    friend class ThemeColorProperty;
    friend class ThemeNumberProperty;

    void populate(ThemeEditorNode *node);
    //! Shows the current values in every row made.
    void syncRows();
    //! Redraws the canvas, \p relayout when a metric changed.
    void redrawCanvas(bool relayout=false);
    void setStatus(const String &text);

    void onRevert(event::Info);
    void onLoad(event::Info);
    void onSave(event::Info);
    //! A row's value was edited: info.controlCaller is its property.
    void onPropertyChanged(event::Info info);
    void colorEdited(ThemeColorProperty *property);
    void numberEdited(ThemeNumberProperty *property);

    skin::textured::TexturedSkin *m_skin=nullptr;
    skin::textured::TexturedSkin::ThemeValues m_openedWith;
    controls::PropertyTree *m_tree=nullptr;
    std::vector<ThemeEditorNode *> m_nodes;
    ThemeEditorNode *m_textColorsNode=nullptr;
    controls::Button *m_revert=nullptr;
    controls::Button *m_load=nullptr;
    controls::Button *m_save=nullptr;
    controls::Label *m_status=nullptr;
    String m_themePath;
    size_t m_redraws=0;
    //! Set while syncRows() gives the rows their values.
    bool m_syncing=false;
};

}//namespace tools
}//namespace gweni

#endif//_gweni_tools_themeEditor_h_
