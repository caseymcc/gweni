/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dialogs_filesave_h_
#define _gweni_controls_dialogs_filesave_h_

#include <gweni/gweni.h>

namespace gweni
{
namespace dialogs
{

//! Open a dialog to save a file.
//!
//! @param useSystem : If true dialog may use the system's modal dialog - which
//!                     will steal focus and pause the rest of Gweni until it's continued.
//!
//! Usage:
//! ~~~~
//!     String fileSaveOut;
//!     gweni::Dialogs::fileSave( true, "Save Map", "C:/my/folder/", "My Map Format|*.bmf",
//!                             fileSaveOut );
//! ~~~~
//!
//! @note Templated function simply to avoid having to manually cast the
//!       callback function.
//
bool fileSave(bool useSystem, const String &name, const String &startPath,
    const String &extension, String &fileSaveOut);

}//namespace dialogs
}//namespace gweni

#endif//_gweni_controls_dialogs_filesave_h_
