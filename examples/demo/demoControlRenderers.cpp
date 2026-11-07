/*
  * Gweni demo
  * See license in gweni.h
 */

// The demo's content-wrapper classes (gweni::ButtonDemo, gweni::ComboBoxDemo, ...)
// are thin controls::Base subclasses used purely to group each demo's widgets on
// a PageControl page. Because they carry a distinct getTypeName() but the skin has
// no renderer registered for that type, they fall through to the textured skin's
// Base fallback, which paints unskinned controls bright red to flag them.
//
// They are intentionally transparent containers, so register a no-draw renderer
// for each wrapper type. The renderer's `name` must match the control's typeName;
// the explicit `template class ControlRegister<...>` instantiations below force
// the self-registration static initializers to run (without them the entries are
// never added to the factory — see textured-skin registration notes).

#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

namespace gweni
{
namespace skin
{
namespace textured
{

#define GWENI_DEMO_TRANSPARENT(Class)                                          \
class Class:public textured::ControlRegister<Class>                            \
{                                                                              \
public:                                                                        \
    static void staticInit(TexturedSkin *) { m_staticInit=true; }              \
    size_t requiredPrimitives() override { return 0; }                         \
    void update(renderer::Base *, controls::Base *) override {}                \
    inline static const std::string_view name=#Class;                          \
};

GWENI_DEMO_TRANSPARENT(Demo)
GWENI_DEMO_TRANSPARENT(ButtonDemo)
GWENI_DEMO_TRANSPARENT(CheckBoxDemo)
GWENI_DEMO_TRANSPARENT(CollapsibleListDemo)
GWENI_DEMO_TRANSPARENT(ColorPickerDemo)
GWENI_DEMO_TRANSPARENT(ComboBoxDemo)
GWENI_DEMO_TRANSPARENT(ContextMenuDemo)
GWENI_DEMO_TRANSPARENT(CrossSplitterDemo)
GWENI_DEMO_TRANSPARENT(GroupBoxDemo)
GWENI_DEMO_TRANSPARENT(ImagePanelDemo)
GWENI_DEMO_TRANSPARENT(LabelDemo)
GWENI_DEMO_TRANSPARENT(LabelMultilineDemo)
GWENI_DEMO_TRANSPARENT(LayoutDemo)
GWENI_DEMO_TRANSPARENT(ListBoxDemo)
GWENI_DEMO_TRANSPARENT(MenuStripDemo)
GWENI_DEMO_TRANSPARENT(NumericDemo)
GWENI_DEMO_TRANSPARENT(PageControlDemo)
GWENI_DEMO_TRANSPARENT(ProgressBarDemo)
GWENI_DEMO_TRANSPARENT(PropertiesDemo)
GWENI_DEMO_TRANSPARENT(RadioButtonDemo)
GWENI_DEMO_TRANSPARENT(ScrollControlDemo)
GWENI_DEMO_TRANSPARENT(SelectableDemo)
GWENI_DEMO_TRANSPARENT(SeparatorDemo)
GWENI_DEMO_TRANSPARENT(SliderDemo)
GWENI_DEMO_TRANSPARENT(StatusBarDemo)
GWENI_DEMO_TRANSPARENT(TabControlDemo)
GWENI_DEMO_TRANSPARENT(TextBoxDemo)
GWENI_DEMO_TRANSPARENT(TextDemo)
GWENI_DEMO_TRANSPARENT(TextSizesDemo)
GWENI_DEMO_TRANSPARENT(TreeControlDemo)
GWENI_DEMO_TRANSPARENT(UnicodeTextDemo)
GWENI_DEMO_TRANSPARENT(WindowDemo)
GWENI_DEMO_TRANSPARENT(WindowButtonsDemo)

#undef GWENI_DEMO_TRANSPARENT

// TextDemo's dark-on-light block: one flat light rectangle behind its labels.
class TextDemoLightPanel:public textured::ControlRegister<TextDemoLightPanel>
{
public:
    static void staticInit(TexturedSkin *) { m_staticInit=true; }
    size_t requiredPrimitives() override { return 1; }
    void update(renderer::Base *renderer, controls::Base *baseControl) override
    {
        if(m_primitives.empty())
            return;

        const bool visible=baseControl->visible();

        // Tokyo Night Day's background
        if(visible)
        {
            renderer->setDrawColor(Color(0xe1, 0xe2, 0xe7));
            renderer->drawFilledRect(m_primitives[0], baseControl->getRenderBounds(), baseControl->getZIndex());
        }

        // as tokyonight::paintSurface does: a hidden page's panel draws nothing
        const unsigned int stateChange=baseControl->getStateChange();
        if((stateChange&controls::StateChange_Visibility) || (stateChange&controls::StateChange_Created))
            renderer->showPrimitive(visible?1:0, m_primitives[0]);
    }
    inline static const std::string_view name="TextDemoLightPanel";
};

// Force the registrar static initializers to run so each renderer is registered.
template class ControlRegister<Demo>;
template class ControlRegister<ButtonDemo>;
template class ControlRegister<CheckBoxDemo>;
template class ControlRegister<CollapsibleListDemo>;
template class ControlRegister<ColorPickerDemo>;
template class ControlRegister<ComboBoxDemo>;
template class ControlRegister<ContextMenuDemo>;
template class ControlRegister<CrossSplitterDemo>;
template class ControlRegister<GroupBoxDemo>;
template class ControlRegister<ImagePanelDemo>;
template class ControlRegister<LabelDemo>;
template class ControlRegister<LabelMultilineDemo>;
template class ControlRegister<LayoutDemo>;
template class ControlRegister<ListBoxDemo>;
template class ControlRegister<MenuStripDemo>;
template class ControlRegister<NumericDemo>;
template class ControlRegister<PageControlDemo>;
template class ControlRegister<ProgressBarDemo>;
template class ControlRegister<PropertiesDemo>;
template class ControlRegister<RadioButtonDemo>;
template class ControlRegister<ScrollControlDemo>;
template class ControlRegister<SelectableDemo>;
template class ControlRegister<SeparatorDemo>;
template class ControlRegister<SliderDemo>;
template class ControlRegister<StatusBarDemo>;
template class ControlRegister<TabControlDemo>;
template class ControlRegister<TextBoxDemo>;
template class ControlRegister<TextDemo>;
template class ControlRegister<TextDemoLightPanel>;
template class ControlRegister<TextSizesDemo>;
template class ControlRegister<TreeControlDemo>;
template class ControlRegister<UnicodeTextDemo>;
template class ControlRegister<WindowDemo>;
template class ControlRegister<WindowButtonsDemo>;

}//namespace textured
}//namespace skin

// Anchor symbol: gweni_demo is a static library, so this translation unit is only
// linked (and its self-registration initializers only run) if something references
// it. The Demo constructor calls this to pull the object file in.
void registerDemoControlRenderers() {}

}//namespace gweni
