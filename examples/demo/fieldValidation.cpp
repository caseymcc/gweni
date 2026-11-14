/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/fieldValidation.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/layout/layout.h"
#include "gweni/controls/numericUpDown.h"
#include "gweni/controls/textBox.h"

#include <cstdlib>

namespace gweni
{

const char *FieldValidationDemo::PasswordTooShort="at least 8 characters";

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 8, 0, 4));
}

// A row of controls side by side, docked Top in the column.
controls::Base *line(controls::Base *column)
{
    controls::Base *row=column->newChild<controls::Base>();
    controls::HorizontalLayout *layout=new controls::HorizontalLayout();

    layout->setSpacing(12);
    row->setLayout(layout);
    row->setDock(DockPosition::Top);
    return row;
}

// The rest of a row's width, after its controls, so they stay packed.
void endLine(controls::Base *row)
{
    controls::Base *rest=row->newChild<controls::Base>();

    rest->setSizeFlags({SizeFlag::Bloat, SizeFlag::Fixed});
    rest->setHeight(1);
}

template<typename T>
T *field(controls::Base *row, int width)
{
    T *box=row->newChild<T>();

    box->setWidth(width);
    return box;
}

// The error for a number outside [low, high], or for no number at all.
String rangeError(const String &text, long low, long high)
{
    if(text.empty())
        return "required";

    char *end=nullptr;
    const long value=std::strtol(text.c_str(), &end, 10);

    if(end == text.c_str() || *end != '\0')
        return "a whole number";
    if(value < low || value > high)
        return "from "+std::to_string(low)+" to "+std::to_string(high);
    return String();
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(FieldValidationDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(520);

    caption(column, "Helper text, checked as the focus leaves");
    {
        controls::Base *row=line(column);

        m_host=field<controls::TextBox>(row, 220);
        m_host->setPlaceholder("Host name");
        m_host->setHelperText("e.g. db-1.internal");
        m_host->setValidator([](const String &text) { return text.empty()?String("required"):String(); });
        endLine(row);
    }

    caption(column, "Checked on every change");
    {
        controls::Base *row=line(column);

        m_password=field<controls::PasswordTextBox>(row, 220);
        m_password->setPlaceholder("Password");
        m_password->setValidator([](const String &text)
            {
                return (!text.empty() && text.size() < 8)?String(PasswordTooShort):String();
            }, controls::ValidateOn::Change);
        endLine(row);
    }

    caption(column, "Ranges");
    {
        controls::Base *row=line(column);

        m_port=field<controls::TextBoxNumeric>(row, 120);
        m_port->setText("5432");
        m_port->setHelperText("port, 1 to 65535");
        m_port->setValidator([](const String &text) { return rangeError(text, 1, 65535); },
            controls::ValidateOn::Change);

        m_percent=field<controls::NumericUpDown>(row, 120);
        m_percent->setMin(-1000);
        m_percent->setMax(1000);
        m_percent->setIntValue(50);
        m_percent->setHelperText("percent, 0 to 100");
        m_percent->setValidator([](const String &text) { return rangeError(text, 0, 100); },
            controls::ValidateOn::Change|controls::ValidateOn::FocusLoss);
        endLine(row);
    }

    caption(column, "Set and cleared by the app");
    {
        controls::Base *row=line(column);

        m_key=field<controls::TextBox>(row, 220);
        m_key->setText("replicas");
        m_key->setReserveMessageLine(true);

        m_setError=row->newChild<controls::Button>();
        m_setError->setText("Set error");
        m_setError->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        m_setError->setSize(90, 22);
        m_setError->onPressCaller.add(this, &FieldValidationDemo::onSetError);

        m_clearError=row->newChild<controls::Button>();
        m_clearError->setText("Clear error");
        m_clearError->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        m_clearError->setSize(90, 22);
        m_clearError->onPressCaller.add(this, &FieldValidationDemo::onClearError);
        endLine(row);
    }

    caption(column, "Disabled, in its error state");
    {
        controls::Base *row=line(column);

        m_disabled=field<controls::TextBox>(row, 220);
        m_disabled->setText("node-07");
        m_disabled->setError("this host is unreachable");
        m_disabled->setDisabled(true);
        endLine(row);
    }
}

void FieldValidationDemo::setKeyError()
{
    m_key->setError("a key already in this file");
}

void FieldValidationDemo::clearKeyError()
{
    m_key->clearError();
}

}//namespace gweni
