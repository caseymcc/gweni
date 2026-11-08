/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcaseInput.h"
#include "gweni/controls/colorEdit.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/numericUpDown.h"
#include "gweni/controls/textBox.h"
#include "gweni/controls/vectorField.h"
#include "gweni/controls/verticalSlider.h"

namespace gweni
{

namespace
{

//! A caption docked Top, above the control added after it.
void addCaption(controls::Base *parent, const String &text)
{
    controls::Label *caption=parent->newChild<controls::Label>();

    caption->setText(text);
    caption->setDock(DockPosition::Top);
    caption->setMargin(Margin(0, 0, 0, 2));
}

//! A text box row, as wide as its section and as high as its text line: a
//! TextBox is Fixed, and a Top dock leaves a Fixed width as it was asked for
//! (200, a NumericUpDown's 100).
template<typename T>
T *textRow(controls::Base *parent)
{
    T *row=showcase::addRow<T>(parent);

    row->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    return row;
}

}//namespace

// Three columns of titled sections filling the page (the shell's centre pane,
// 865x545, or ControlTest's canvas), each section's controls docked Top in it.
// Every state is set here, the focus too.
GWENI_CONTROL_CONSTRUCTOR(ShowcaseInputDemo)
{
    showcase::initPage(this);

    controls::Base *left=showcase::addColumn(this);
    controls::Base *middle=showcase::addColumn(this);
    controls::Base *right=showcase::addColumn(this);

    {
        controls::GroupBox *section=showcase::addSection(left, "TextBox");

        addCaption(section, "Placeholder");
        textRow<controls::TextBox>(section)->setPlaceholder("Search files...");

        addCaption(section, "PasswordTextBox");
        textRow<controls::PasswordTextBox>(section)->setText("hunter2");

        addCaption(section, "TextBoxNumeric");
        textRow<controls::TextBoxNumeric>(section)->setText("2026");

        addCaption(section, "Coloured text");
        {
            controls::TextBox *coloured=textRow<controls::TextBox>(section);

            coloured->setText("Unsaved changes");
            coloured->setTextColor(Color(255, 158, 100, 255));
        }

        addCaption(section, "Read-only");
        {
            controls::TextBox *readOnly=textRow<controls::TextBox>(section);

            readOnly->setText("/home/gweni/notes.txt");
            readOnly->setEditable(false);
        }

        addCaption(section, "Focused, the caret at the end");
        m_focused=textRow<controls::TextBox>(section);
        m_focused->setText("Hello, gweni");
        m_focused->focus();
        m_focused->moveCaretToend();
        m_notes.push_back("TextBox: a placeholder, a password, a number, coloured text, read-only, one focused");
    }
    {
        controls::GroupBox *section=showcase::addSection(left, "ColorEdit", false);

        addCaption(section, "R, G, B, A, translucent");
        showcase::addRow<controls::ColorEdit>(section)->setColor(Color(224, 175, 104, 200));

        addCaption(section, "Hex, no alpha");
        controls::ColorEdit *hex=showcase::addRow<controls::ColorEdit>(section);

        hex->setMode(controls::ColorEditMode::Hex);
        hex->setAlphaEnabled(false);
        hex->setColor(Color(158, 206, 106, 255));
        m_notes.push_back("ColorEdit: R, G, B, A fields on a translucent colour, a hex field");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "TextBoxMultiline");
        controls::TextBoxMultiline *text=section->newChild<controls::TextBoxMultiline>();

        text->setDock(DockPosition::Center);
        text->setText("gweni lays its controls out in an update phase and draws only what "
            "changed in a render phase.\n\n"
            "Each control keeps its primitives in a persistent buffer, so an idle frame "
            "uploads nothing.\n\n"
            "Long lines wrap at the box's width.");
        m_notes.push_back("TextBoxMultiline: three paragraphs, wrapped");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "ComboBox", false);

        controls::ComboBox *combo=showcase::addRow<controls::ComboBox>(section);

        combo->addItem("Grayscale", "gray");
        combo->addItem("Subpixel RGB", "rgb");
        combo->addItem("Subpixel BGR", "bgr");
        combo->selectItemByName("rgb", false);

        controls::ComboBox *disabled=showcase::addRow<controls::ComboBox>(section);

        disabled->addItem("Disabled");
        disabled->setDisabled(true);
        m_notes.push_back("ComboBox: \"Subpixel RGB\" selected, one disabled");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "NumericUpDown", false);
        controls::NumericUpDown *number=textRow<controls::NumericUpDown>(section);

        number->setMin(0);
        number->setMax(100);
        number->setIntValue(42);
        m_notes.push_back("NumericUpDown: 42 of 0..100");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "DragNumber", false);
        controls::DragNumber *integer=textRow<controls::DragNumber>(section);

        integer->setInteger(true);
        integer->setNumber(128);

        controls::DragNumber *ranged=textRow<controls::DragNumber>(section);

        ranged->setRange(0.0, 1.0);
        ranged->setFormat("%.2f of 1");
        ranged->setNumber(0.75);
        m_notes.push_back("DragNumber: an integer, 128; a float in 0..1, 0.75");
    }
    {
        controls::GroupBox *section=showcase::addSection(middle, "VectorField", false);
        controls::VectorField *position=textRow<controls::VectorField>(section);

        position->setAxisTags(true);
        position->setFormat("%.2f");
        position->setValues({1.5, -0.25, 3.0, 0.0});
        m_notes.push_back("VectorField: a tagged float3, (1.50, -0.25, 3.00)");
    }
    {
        controls::GroupBox *section=showcase::addSection(right, "HorizontalSlider", false);

        const struct
        {
            const char *caption;
            float value;
            int notches;
            bool disabled;
            const char *format;
        } rows[]=
        {
            {"Plain, 35 %", 35, 0, false, ""},
            {"10 notches, 70 %", 70, 10, false, ""},
            {"Disabled", 50, 0, true, ""},
            {"Its value shown", 62.5f, 0, false, "%.1f %%"},
        };

        for(const auto &row : rows)
        {
            addCaption(section, row.caption);

            controls::HorizontalSlider *slider=showcase::addRow<controls::HorizontalSlider>(section);

            slider->setHeight(20);
            slider->setRange(0, 100);
            if(row.notches)
            {
                slider->setNotchCount(row.notches);
                slider->setClampToNotches(true);
            }
            slider->setFloatValue(row.value);
            slider->setDisabled(row.disabled);
            slider->setValueFormat(row.format);
        }
    }
    {
        // side by side from the left, each the section's height
        controls::GroupBox *section=showcase::addSection(right, "VerticalSlider");

        const struct
        {
            float value;
            int notches;
            bool disabled;
        } sliders[]=
        {
            {80, 0, false},
            {40, 5, false},
            {20, 10, false},
            {60, 0, true},
        };

        for(const auto &entry : sliders)
        {
            controls::VerticalSlider *slider=section->newChild<controls::VerticalSlider>();

            slider->setDock(DockPosition::Left);
            slider->setWidth(20);
            slider->setMargin(Margin(0, 0, 12, 0));
            slider->setRange(0, 100);
            if(entry.notches)
            {
                slider->setNotchCount(entry.notches);
                slider->setClampToNotches(true);
            }
            slider->setFloatValue(entry.value);
            slider->setDisabled(entry.disabled);
        }
        m_notes.push_back("Slider: plain and notched, horizontal and vertical, one disabled, one showing its value");
    }
}

void ShowcaseInputDemo::showcaseShown()
{
    if(m_focused)
        m_focused->focus();
}

}//namespace gweni
