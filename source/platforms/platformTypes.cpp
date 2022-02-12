#include "gweni/platforms/platformTypes.h"

namespace gweni
{

const Font &getDefaultFont()
{
    static Font defaultFont;

    defaultFont.facename="Arial";
    defaultFont.size=10.0f;
    defaultFont.bold=false;

    return defaultFont;
}

}//namespace gweni
