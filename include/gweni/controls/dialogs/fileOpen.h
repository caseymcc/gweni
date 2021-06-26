/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dialogs_fileopen_h_
#define _gweni_controls_dialogs_fileopen_h_

#include <gweni/gweni.h>

namespace gweni
{
namespace dialogs
{

//! Create a file open dialog box.
//! @param useSystem : If true dialog may use the system's modal dialog - which
//!                     will steal focus and pause the rest of Gweni until it's continued.
//!
//! Usage:
//! ~~~~
//!     String fileOpenOut;
//!     if (gweni::Dialogs::fileOpen( true, "Open Map", "C:/my/folder/", "My Map Format|*.bmf",
//!                                 fileOpenOut ))
//!     // .. do something with fileOpenOut ..
//! ~~~~
//!
//! @note Templated function simply to avoid having to manually cast the
//!       callback function.
//!
bool fileOpen(bool useSystem, const String &name, const String &startPath,
    const String &extension, String &fileOpenOut);

}//namespace dialogs
}//namespace gweni

#endif//_gweni_controls_dialogs_fileopen_h_
