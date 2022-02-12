#include "demo/menuStrip.h"
#include "gweni/controls/menuStrip.h"

namespace gweni
{

MenuStripDemo::MenuStripDemo(const String &name):
    Base()
{
    dock(Position::Fill);
    controls::MenuStrip *menu=newChild<controls::MenuStrip>();
    {
        controls::MenuItem *root=menu->addItem("File");
        root->getMenu()->addItem("New", "test16.png", "Ctrl + N")->setAction(this, &MenuStripDemo::menuItemSelect);
        root->getMenu()->addItem("Load", "test16.png", "Ctrl+L")->setAction(this, &MenuStripDemo::menuItemSelect);
        root->getMenu()->addItem("Save", "", "CTRL+S")->setAction(this, &MenuStripDemo::menuItemSelect);
        root->getMenu()->addItem("Save As..", "", "Ctrl+Shift+S")->setAction(this, &MenuStripDemo::menuItemSelect);
        root->getMenu()->addItem("Quit", "", "Ctrl+Q")->setAction(this, &MenuStripDemo::menuItemSelect);
    }
    {
        controls::MenuItem *root=menu->addItem("n/a");
        root->getMenu()->addItem("n/a")->setAction(this, &MenuStripDemo::menuItemSelect);
        root->getMenu()->addItem("n/a")->setAction(this, &MenuStripDemo::menuItemSelect);
    }
    {
        controls::MenuItem *root=menu->addItem("Submenu");
        controls::MenuItem *pCheckable=root->getMenu()->addItem("Checkable");
        pCheckable->setCheckable(true);
        pCheckable->setChecked(true);
        {
            controls::MenuItem *rootB=root->getMenu()->addItem("Two");
            rootB->getMenu()->addItem("Two.One");
            rootB->getMenu()->addItem("Two.Two");
            rootB->getMenu()->addItem("Two.Three");
            rootB->getMenu()->addItem("Two.Four");
            rootB->getMenu()->addItem("Two.Five");
            rootB->getMenu()->addItem("Two.Six");
            rootB->getMenu()->addItem("Two.Seven");
            rootB->getMenu()->addItem("Two.Eight");
            rootB->getMenu()->addItem("Two.Nine", "test16.png");
        }
        root->getMenu()->addItem("Three");
        root->getMenu()->addItem("Four");
        root->getMenu()->addItem("Five");
        {
            controls::MenuItem *rootB=root->getMenu()->addItem("Six");
            rootB->getMenu()->addItem("Six.One");
            rootB->getMenu()->addItem("Six.Two");
            rootB->getMenu()->addItem("Six.Three");
            rootB->getMenu()->addItem("Six.Four");
            rootB->getMenu()->addItem("Six.Five", "test16.png");
            {
                controls::MenuItem *rootC=rootB->getMenu()->addItem("Six.Six");
                rootC->getMenu()->addItem("Sheep");
                rootC->getMenu()->addItem("Goose");
                {
                    controls::MenuItem *rootD=rootC->getMenu()->addItem("Camel");
                    rootD->getMenu()->addItem("Eyes");
                    rootD->getMenu()->addItem("Nose");
                    {
                        controls::MenuItem *rootE=rootD->getMenu()->addItem("Hair");
                        rootE->getMenu()->addItem("Blonde");
                        rootE->getMenu()->addItem("Black");
                        {
                            controls::MenuItem *rootF=rootE->getMenu()->addItem("Red");
                            rootF->getMenu()->addItem("Light");
                            rootF->getMenu()->addItem("Medium");
                            rootF->getMenu()->addItem("Dark");
                        }
                        rootE->getMenu()->addItem("Brown");
                    }
                    rootD->getMenu()->addItem("Ears");
                }
                rootC->getMenu()->addItem("Duck");
            }
            rootB->getMenu()->addItem("Six.Seven");
            rootB->getMenu()->addItem("Six.Eight");
            rootB->getMenu()->addItem("Six.Nine");
        }
        root->getMenu()->addItem("Seven");
    }
}


void MenuStripDemo::menuItemSelect(event::Info info)
{
//    controls::MenuItem *menuItem=(controls::MenuItem *) control;
//    UnitPrint(Format("Menu Selected: %ls", menuItem->getText().getUnicode().c_str()));
}

}//namespace gweni
