/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dialogs_folderopen_h_
#define _gweni_controls_dialogs_folderopen_h_

#include <gweni/gweni.h>

namespace gweni
{
namespace dialogs
{

//! Open a dialog to choose a folder.
//!
//! @param useSystem : If true dialog may use the system's modal dialog - which
//!                     will steal focus and pause the rest of Gweni until it's continued.
//! Usage:
//! ~~~~
//!  String folderChosenOut;
//!  gweni::Dialogs::folderOpen( true, "Open Map folder", "C:/my/folder/",
//!                             "My Map Format|*.bmf", folderChosenOut );
//! ~~~~
//! @note Templated function simply to avoid having to manually cast the
//!       callback function.
//
bool folderOpen(bool useSystem, const String &name, const String &startPath,
    String &folderChosenOut);

}//namespace dialogs
}//namespace gweni

#endif//_gweni_controls_dialogs_folderopen_h_
