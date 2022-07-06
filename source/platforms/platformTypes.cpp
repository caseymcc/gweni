#include "gweni/platforms/platformTypes.h"

namespace gweni
{

const Font &getDefaultFont()
{
    static Font defaultFont;

    defaultFont.facename="arial";
    defaultFont.size=10.0f;
    defaultFont.bold=false;

    return defaultFont;
}

}//namespace gweni
