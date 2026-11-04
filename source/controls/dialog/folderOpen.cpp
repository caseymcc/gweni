/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/controls/dialogs/folderOpen.h>

namespace gweni
{
namespace dialogs
{

bool folderOpen(bool useSystem, const String &name, const String &startPath,
    String &folderChosenOut)
{
    if(useSystem)
    {
        return platform::folderOpen(name, startPath, folderChosenOut);
    }

    // TODO: SHOW Gweni FOLDER SELECTION DIALOG

    return false;
}

}//namespace dialogs
}//namespace gweni
