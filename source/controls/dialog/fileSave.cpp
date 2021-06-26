/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/controls/dialogs/fileSave.h>


namespace gweni
{
namespace dialogs
{

bool fileSave(bool useSystem, const String &name, const String &startPath,
    const String &extension, String &fileOpenOut)
{
    if(useSystem)
    {
        return platform::fileSave(name, startPath, extension, fileOpenOut);
    }

    // TODO: SHOW Gweni FILE SELECTION DIALOG

    return false;
}

}//namespace dialogs
}//namespace gweni
