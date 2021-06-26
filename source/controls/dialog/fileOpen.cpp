/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/controls/dialogs/fileOpen.h>

namespace gweni
{
namespace dialogs
{

bool fileOpen(bool useSystem, const String &name, const String &startPath,
    const String &extension, String &fileOpenOut)
{
    String fileOpened;

    if(useSystem)
    {
        return gweni::platform::fileOpen(name, startPath, extension, fileOpened);
    }

    // TODO: SHOW Gweni FILE SELECTION DIALOG

    return false;
}

}//namespace dialogs
}//namespace gweni
