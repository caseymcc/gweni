/*
 *  Gweni - the bound handle classes (python/src): one struct per Python class,
 *  each a Control (a control's id) with no state of its own, so nanobind tells
 *  the classes apart by C++ type. Bound in controls.cpp and simple.cpp.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_python_handles_h_
#define _gweni_python_handles_h_

#include "control.h"

namespace gweni
{
namespace python
{

struct Canvas: Control {};
struct Panel: Control {};
struct Label: Control {};
struct Button: Label {};

struct CheckBox: Button {};
struct CheckBoxWithLabel: Control {};
struct RadioButtonController: Control {};
struct RadioButton: Control {};
struct TextBox: Label {};
struct TextBoxMultiline: TextBox {};
struct TextBoxNumeric: TextBox {};
struct PasswordTextBox: TextBox {};
struct Slider: Control {};
struct HorizontalSlider: Slider {};
struct VerticalSlider: Slider {};
struct ComboBox: Button {};
struct ScrollControl: Control {};
struct ListBox: ScrollControl {};
struct GroupBox: Label {};
struct TabControl: Control {};
struct TabButton: Button {};
struct Window: Control {};
struct Menu: ScrollControl {};
struct MenuStrip: Menu {};
struct MenuItem: Button {};
struct Dialog: Control {};
struct PromptDialog: Dialog {};
struct Toast: Control {};
struct Flow: Control {};
struct ProgressBar: Label {};
struct Switch: Button {};
struct Spinner: Control {};
struct Chip: Button {};


}//namespace python
}//namespace gweni

#endif//_gweni_python_handles_h_
