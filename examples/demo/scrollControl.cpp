#include "demo/scrollControl.h"
#include "gweni/controls/scrollControl.h"
#include <gweni/controls/button.h>

namespace gweni
{

namespace
{

//! One region: a 100x100 ScrollControl holding a Button of a fixed size, or
//! with no size one docked to fill it, which fits whatever room the bars leave.
struct Region
{
    const char *text;
    int contentWidth;
    int contentHeight;
    bool horizontal;
    bool vertical;
};

const Region g_regions[]=
{
    {"Twice As Big", 200, 200, true, true},
    {"Same Size", 0, 0, true, true},
    {"Wide", 200, 50, true, true},
    {"Tall", 50, 200, true, true},
    {"Vertical", 200, 200, false, true},
    {"Horizontal", 200, 200, true, false},
};

}//namespace

// Two rows of the same six regions, placed where their bounds say: the top
// row always shows its bars (disabled where the content fits), the bottom row
// hides them until they are needed.
GWENI_CONTROL_CONSTRUCTOR(ScrollControlDemo)
{
    for(int row=0; row < 2; ++row)
    {
        int x=10;

        for(const Region &region : g_regions)
        {
            controls::ScrollControl *control=newChild<controls::ScrollControl>();
            control->setBounds(x, 10+row*100, 100, 100);
            if(!region.horizontal || !region.vertical)
                control->setScroll(region.horizontal, region.vertical);
            control->setAutoHideBars(row == 1);

            controls::Button *testButton=control->newChild<controls::Button>();
            testButton->setText(region.text);

            if(region.contentWidth == 0)
                testButton->setDock(DockPosition::Center);
            else
            {
                testButton->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
                testButton->setBounds(0, 0, region.contentWidth, region.contentHeight);
            }

            x+=100;
        }
    }
}

}//namespace gweni
