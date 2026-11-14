/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_field_validation_example_h_
#define _gweni_field_validation_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
class NumericUpDown;
class PasswordTextBox;
class TextBox;
class TextBoxNumeric;
}

//! TextBox fields with a validation state: a host name with helper text and a
//! "required" check as the focus leaves it, a password checked on every
//! change, a port and a percentage checked against their ranges, a key whose
//! error the app sets and clears from two buttons, and a disabled field in
//! its error state.
class FieldValidationDemo: public controls::Base
{
public:
    GWENI_CONTROL(FieldValidationDemo, controls::Base);

public:
    //! The app's path: what the "Set error" and "Clear error" buttons do.
    void setKeyError();
    void clearKeyError();

    //! The parts a test drives.
    controls::TextBox *hostField() const { return m_host; }
    controls::PasswordTextBox *passwordField() const { return m_password; }
    controls::TextBoxNumeric *portField() const { return m_port; }
    controls::NumericUpDown *percentField() const { return m_percent; }
    controls::TextBox *keyField() const { return m_key; }
    controls::Button *setErrorButton() const { return m_setError; }
    controls::Button *clearErrorButton() const { return m_clearError; }
    controls::TextBox *disabledField() const { return m_disabled; }

    //! The password validator's message for a short password.
    static const char *PasswordTooShort;

private:
    void onSetError(event::Info) { setKeyError(); }
    void onClearError(event::Info) { clearKeyError(); }

    controls::TextBox *m_host=nullptr;
    controls::PasswordTextBox *m_password=nullptr;
    controls::TextBoxNumeric *m_port=nullptr;
    controls::NumericUpDown *m_percent=nullptr;
    controls::TextBox *m_key=nullptr;
    controls::Button *m_setError=nullptr;
    controls::Button *m_clearError=nullptr;
    controls::TextBox *m_disabled=nullptr;
};

}//namespace gweni

#endif//_gweni_field_validation_example_h_
