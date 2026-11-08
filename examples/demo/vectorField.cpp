#include "demo/vectorField.h"
#include "gweni/controls/label.h"
#include "gweni/controls/vectorField.h"

namespace gweni
{

// VectorFields, each after a Label naming it: a float2, a float3 with X/Y/Z
// tags, an integer vector of four with tags and a disabled float3. A drag
// scrubs one component, a double-click or Ctrl+click types into it, Tab steps
// to the next.
GWENI_CONTROL_CONSTRUCTOR(VectorFieldDemo)
{
    setDock(DockPosition::Center);

    auto row=[this](const char *caption, int y, int components)
    {
        controls::Label *label=newChild<controls::Label>();
        label->setText(caption);
        label->setPos(10, y+3);

        controls::VectorField *field=newChild<controls::VectorField>();
        field->setComponentCount(components);
        field->setPos(150, y);
        field->setSize(320, 20);
        return field;
    };

    controls::VectorField *size=row("Float2", 10, 2);
    size->setFormat("%.2f");
    size->setValues({640.0, 480.0, 0.0, 0.0});

    m_tagged=row("Float3, tagged", 40, 3);
    m_tagged->setAxisTags(true);
    m_tagged->setValues({1.5, -0.25, 3.0, 0.0});

    controls::VectorField *integers=row("Int4, tagged", 70, 4);
    integers->setAxisTags(true);
    integers->setInteger(true);
    integers->setRange(0, 255);
    integers->setValues({255, 158, 100, 255});

    controls::VectorField *disabled=row("Disabled", 100, 3);
    disabled->setAxisTags(true);
    disabled->setValues({0.0, 1.0, 0.0, 0.0});
    disabled->setDisabled(true);
}

}//namespace gweni
