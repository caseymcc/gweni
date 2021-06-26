#include "demo/collapsibleList.h"
#include "gweni/controls/collapsibleList.h"

namespace gweni
{

CollapsibleListDemo::CollapsibleListDemo(gweni::controls::Base *parent, const gweni::String &name):
    Base(parent)
{
    controls::CollapsibleList *control=new controls::CollapsibleList(this);

    control->setSize(100, 200);
    control->setPos(10, 10);

    {
        controls::CollapsibleCategory *cat=control->add("Category One");

        cat->add("Hello");
        cat->add("Two");
        cat->add("Three");
        cat->add("Four");
    }
    {
        controls::CollapsibleCategory *cat=control->add("Shopping");

        cat->add("Special");
        cat->add("Two Noses");
        cat->add("Orange ears");
        cat->add("Beer");
        cat->add("Three Eyes");
        cat->add("Special");
        cat->add("Two Noses");
        cat->add("Orange ears");
        cat->add("Beer");
        cat->add("Three Eyes");
        cat->add("Special");
        cat->add("Two Noses");
        cat->add("Orange ears");
        cat->add("Beer");
        cat->add("Three Eyes");
    }
    {
        controls::CollapsibleCategory *cat=control->add("Category One");

        cat->add("Hello");
        cat->add("Two");
        cat->add("Three");
        cat->add("Four");
    }
}


}//namespace gweni
