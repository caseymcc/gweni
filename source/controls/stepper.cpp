/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/stepper.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

#include <algorithm>
#include <string>

namespace gweni
{
namespace controls
{

namespace
{

const char *const ErrorMark="!";
const char *const OptionalCaption="Optional";

}//namespace

///////////////////////////////////////////////////////////////////////////
//StepperStep

// Owner-placed by its stepper, as large as its indicator and title block; the
// title (the Button's Text) is placed by the step, beside the indicator.
GWENI_CONTROL_CONSTRUCTOR(StepperStep)
{
    setDock(DockPosition::None);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setSize(0, 0);
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Left|Alignment::Top);
    m_text->setPlacement(Placement::Owner);
    // a linear stepper's steps take no input (the Stepper sets it per mode)
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    setTabable(false);
}

void StepperStep::setLook(StepLook look)
{
    if(look == m_look)
        return;

    m_look=look;
    // updateColors() and the background: the indicator's slots and run
    redraw();
}

void StepperStep::setSubtitle(const String &subtitle)
{
    if(subtitle == getSubtitle())
        return;

    if(subtitle.empty())
    {
        if(m_subtitle)
            m_subtitle->setHidden(true);
    }
    else
    {
        makeCaption(m_subtitle)->setText(subtitle);
        m_subtitle->setHidden(false);
    }
    invalidate();
}

String StepperStep::getSubtitle() const
{
    return (m_subtitle && !m_subtitle->hidden())?m_subtitle->getText():String();
}

void StepperStep::setOptional(bool optional)
{
    if(optional == m_optional)
        return;

    m_optional=optional;
    if(optional)
    {
        makeCaption(m_optionalText)->setText(OptionalCaption);
        m_optionalText->setHidden(false);
    }
    else if(m_optionalText)
        m_optionalText->setHidden(true);
    invalidate();
}

// After the title, so numbered and drawn after it; in the subtitle colour.
internal::Text *StepperStep::makeCaption(internal::Text *&text)
{
    if(!text)
    {
        text=newChild<internal::Text>();
        text->setPlacement(Placement::Owner);
        text->setMouseInputEnabled(false);
        updateColors();
    }
    return text;
}

String StepperStep::getIndicatorText() const
{
    switch(m_look)
    {
    case StepLook::Done:
        return String();
    case StepLook::Error:
        return ErrorMark;
    default:
        return std::to_string(m_index+1);
    }
}

Point StepperStep::indicatorTextOrigin() const
{
    const Point &size=(m_look == StepLook::Error)?m_errorSize:m_numberSize;
    const Rect &bounds=getBounds();

    return Point(bounds.x+(IndicatorSize-size.x)/2, bounds.y+(IndicatorSize-size.y)/2);
}

void StepperStep::updateColors()
{
    const auto &colors=getSkin()->Colors.Stepper;
    const gweni::Color *title=&colors.Title;

    switch(m_look)
    {
    case StepLook::Current:
        title=&colors.TitleCurrent;
        break;
    case StepLook::Error:
        title=&colors.Error;
        break;
    case StepLook::Disabled:
        title=&colors.TitleDisabled;
        break;
    default:
        break;
    }
    setTextColor(*title);
    if(m_subtitle)
        m_subtitle->setTextColor((m_look == StepLook::Disabled)?colors.TitleDisabled:colors.Subtitle);
    if(m_optionalText)
        m_optionalText->setTextColor((m_look == StepLook::Disabled)?colors.TitleDisabled:colors.Subtitle);
}

int StepperStep::titleBlockHeight() const
{
    int height=m_text->getPreferredSize().height;

    if(m_subtitle && !m_subtitle->hidden())
        height+=m_subtitle->getPreferredSize().height;
    if(m_optionalText && !m_optionalText->hidden())
        height+=m_optionalText->getPreferredSize().height;
    return height;
}

void StepperStep::calculateSize(skin::Base *skin, Dim dim)
{
    // the title and the captions are measured for themselves; none counts
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        // the indicator's two texts, measured here so a new look lays
        // nothing out (the number follows the index, which only addStep sets)
        m_numberSize=measureText(getFont(), std::to_string(m_index+1));
        m_errorSize=measureText(getFont(), ErrorMark);

        int width=m_text->getPreferredSize().width;

        if(m_subtitle && !m_subtitle->hidden())
            width=std::max(width, m_subtitle->getPreferredSize().width);
        if(m_optionalText && !m_optionalText->hidden())
            width=std::max(width, m_optionalText->getPreferredSize().width);
        m_preferredSize.width=preferredFromContent(IndicatorSize+(width > 0?LabelGap+width:0), Dim::X);
        return;
    }

    // the title's first line centred on the indicator, the captions under it
    const int titleTop=std::max(0, (IndicatorSize-m_text->getPreferredSize().height)/2);

    m_preferredSize.height=preferredFromContent(std::max(IndicatorSize, titleTop+titleBlockHeight()), Dim::Y);
}

void StepperStep::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    const int x=IndicatorSize+LabelGap;

    placeChild(m_text, x, m_text->getY(), m_text->getPreferredSize().width, m_text->getHeight());
    if(m_subtitle)
        placeChild(m_subtitle, x, m_subtitle->getY(), m_subtitle->getPreferredSize().width, m_subtitle->getHeight());
    if(m_optionalText)
        placeChild(m_optionalText, x, m_optionalText->getY(), m_optionalText->getPreferredSize().width,
            m_optionalText->getHeight());
}

void StepperStep::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int titleHeight=m_text->getPreferredSize().height;
    int y=std::max(0, (IndicatorSize-titleHeight)/2);

    placeChild(m_text, m_text->getX(), y, m_text->getWidth(), titleHeight);
    y+=titleHeight;
    for(internal::Text *caption : {m_subtitle, m_optionalText})
    {
        if(!caption)
            continue;

        const int height=caption->getPreferredSize().height;

        placeChild(caption, caption->getX(), y, caption->getWidth(), height);
        if(!caption->hidden())
            y+=height;
    }
}

///////////////////////////////////////////////////////////////////////////
//StepperConnector

GWENI_CONTROL_CONSTRUCTOR(StepperConnector)
{
    setDock(DockPosition::None);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
}

void StepperConnector::setDone(bool done)
{
    if(done == m_done)
        return;

    m_done=done;
    addStateChange(StateChange_Background);
}

void StepperConnector::setVertical(bool vertical)
{
    if(vertical == m_vertical)
        return;

    m_vertical=vertical;
    addStateChange(StateChange_Background);
}

///////////////////////////////////////////////////////////////////////////
//Stepper

// The body holds the page area, which holds every page: each page fills it
// and only the current one is shown (PageControl's container).
GWENI_CONTROL_CONSTRUCTOR(Stepper)
{
    m_body=newChild<Base>();
    m_body->setPlacement(Placement::Owner);
    m_pageArea=m_body->newChild<Base>();
    m_pageArea->setDock(DockPosition::Center);
}

// The steps are deleted by Base's destructor; none may ask the stepper
// anything by then.
Stepper::~Stepper()
{
    for(StepperStep *step : m_steps)
        step->m_stepper=nullptr;
}

Base *Stepper::addStep(const String &title, const String &subtitle)
{
    const int index=getStepCount();

    if(index > 0)
    {
        StepperConnector *connector=newChild<StepperConnector>();

        connector->setPlacement(Placement::Owner);
        connector->setVertical(isVertical());
        m_connectors.push_back(connector);
    }

    StepperStep *step=newChild<StepperStep>();

    step->setPlacement(Placement::Owner);
    step->m_stepper=this;
    step->m_index=index;
    step->setText(title);
    step->setSubtitle(subtitle);
    m_steps.push_back(step);

    Step data;

    data.page=m_pageArea->newChild<Base>();
    data.page->setDock(DockPosition::Center);
    // hidden before the app adds anything to it (Base::addChild)
    if(index != m_current)
        data.page->setHidden(true);
    m_stepData.push_back(data);

    // its own look before updateLooks() compares (a new step is Pending)
    step->m_look=getStepLook(index);
    step->updateColors();
    updateLooks();
    invalidate();
    return data.page;
}

void Stepper::setCurrentStep(int index)
{
    if((index < 0) || (index >= getStepCount()) || (index == m_current))
        return;

    m_stepData[static_cast<size_t>(m_current)].page->setHidden(true);
    m_current=index;
    m_stepData[static_cast<size_t>(m_current)].page->setHidden(false);
    updateLooks();
    // vertical: the body moves under the new step and the steps after it
    // with it
    if(isVertical())
        invalidate();
}

void Stepper::setStepState(int index, StepState state)
{
    if((index < 0) || (index >= getStepCount()))
        return;

    Step &step=m_stepData[static_cast<size_t>(index)];

    if(step.state == state)
        return;

    step.state=state;
    m_steps[static_cast<size_t>(index)]->setDisabled(state == StepState::Disabled);
    updateLooks();
}

Stepper::StepState Stepper::getStepState(int index) const
{
    if((index < 0) || (index >= getStepCount()))
        return StepState::Auto;
    return m_stepData[static_cast<size_t>(index)].state;
}

StepLook Stepper::getStepLook(int index) const
{
    switch(getStepState(index))
    {
    case StepState::Done:
        return StepLook::Done;
    case StepState::Error:
        return StepLook::Error;
    case StepState::Disabled:
        return StepLook::Disabled;
    default:
        break;
    }
    if(index < m_current)
        return StepLook::Done;
    if(index == m_current)
        return StepLook::Current;
    return StepLook::Pending;
}

void Stepper::setStepOptional(int index, bool optional)
{
    if(StepperStep *step=getStep(index))
        step->setOptional(optional);
}

bool Stepper::isStepOptional(int index) const
{
    const StepperStep *step=getStep(index);

    return step && step->isOptional();
}

Base *Stepper::getPage(int index) const
{
    if((index < 0) || (index >= getStepCount()))
        return nullptr;
    return m_stepData[static_cast<size_t>(index)].page;
}

StepperStep *Stepper::getStep(int index) const
{
    if((index < 0) || (index >= getStepCount()))
        return nullptr;
    return m_steps[static_cast<size_t>(index)];
}

StepperConnector *Stepper::getConnector(int index) const
{
    if((index < 0) || (index >= static_cast<int>(m_connectors.size())))
        return nullptr;
    return m_connectors[static_cast<size_t>(index)];
}

void Stepper::setOrientation(Orientation orientation)
{
    if(orientation == m_orientation)
        return;

    m_orientation=orientation;
    for(StepperConnector *connector : m_connectors)
        connector->setVertical(isVertical());
    invalidate();
}

void Stepper::updateLooks()
{
    for(int i=0; i < getStepCount(); ++i)
        m_steps[static_cast<size_t>(i)]->setLook(getStepLook(i));
    // a connector into a done step takes the done colour
    for(size_t i=0; i < m_connectors.size(); ++i)
        m_connectors[i]->setDone(getStepLook(static_cast<int>(i)+1) == StepLook::Done);
}

void Stepper::calculateSize(skin::Base *skin, Dim dim)
{
    // the steps, connectors and body are measured for their own subtrees;
    // none counts
    ParentClass::calculateSize(skin, dim);

    const Padding &padding=getPadding();
    const Size &body=m_body->getPreferredSize();
    const int count=getStepCount();
    int size=0;

    if(dim == Dim::X)
    {
        if(isVertical())
        {
            for(const StepperStep *step : m_steps)
                size=std::max(size, step->getPreferredSize().width);
            size=std::max(size, StepperStep::IndicatorSize+StepperStep::LabelGap+body.width);
        }
        else
        {
            for(const StepperStep *step : m_steps)
                size+=step->getPreferredSize().width;
            if(count > 1)
                size+=(count-1)*(2*ConnectorGap+ConnectorMinLength);
            size=std::max(size, body.width);
        }
        m_preferredSize.width=preferredFromContent(size+padding.left+padding.right, Dim::X);
        return;
    }

    if(isVertical())
    {
        for(const StepperStep *step : m_steps)
            size+=step->getPreferredSize().height;
        if(count > 1)
            size+=(count-1)*StepSpacing;
        if(count > 0)
            size+=BodyGap+body.height;
    }
    else
    {
        for(const StepperStep *step : m_steps)
            size=std::max(size, step->getPreferredSize().height);
        size+=HeaderGap+body.height;
    }
    m_preferredSize.height=preferredFromContent(size+padding.top+padding.bottom, Dim::Y);
}

// The parts are placed before Base's arrange enters them, as SegmentedButtons
// places its segments: from MinChangedWalk children on, Base's arrange walks
// only the changed ones and calls neither arrangeHorizontal nor
// arrangeVertical.
void Stepper::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();
    const int count=getStepCount();
    const int indent=StepperStep::IndicatorSize+StepperStep::LabelGap;

    if(dim == Dim::X)
    {
        const int width=std::max(0, getWidth()-padding.left-padding.right);

        if(isVertical())
        {
            for(StepperStep *step : m_steps)
                placeChild(step, padding.left, step->getY(), step->getPreferredSize().width, step->getHeight());
            for(StepperConnector *connector : m_connectors)
                placeChild(connector, padding.left, connector->getY(), StepperStep::IndicatorSize,
                    connector->getHeight());
            placeChild(m_body, padding.left+indent, m_body->getY(), std::max(0, width-indent), m_body->getHeight());
        }
        else
        {
            // the connectors share what the steps leave, the first ones a
            // unit longer for the remainder
            int stepsWidth=0;

            for(const StepperStep *step : m_steps)
                stepsWidth+=step->getPreferredSize().width;

            const int connectors=std::max(0, count-1);
            const int room=std::max(0, width-stepsWidth-connectors*2*ConnectorGap);
            const int length=connectors?room/connectors:0;
            const int extra=connectors?room%connectors:0;
            int x=padding.left;

            for(int i=0; i < count; ++i)
            {
                StepperStep *step=m_steps[static_cast<size_t>(i)];
                const int stepWidth=step->getPreferredSize().width;

                placeChild(step, x, step->getY(), stepWidth, step->getHeight());
                x+=stepWidth;
                if(i < connectors)
                {
                    StepperConnector *connector=m_connectors[static_cast<size_t>(i)];
                    const int connectorLength=length+((i < extra)?1:0);

                    placeChild(connector, x+ConnectorGap, connector->getY(), connectorLength, connector->getHeight());
                    x+=2*ConnectorGap+connectorLength;
                }
            }
            placeChild(m_body, padding.left, m_body->getY(), width, m_body->getHeight());
        }
    }
    else
    {
        const int top=padding.top;
        const int height=std::max(0, getHeight()-padding.top-padding.bottom);

        if(isVertical())
        {
            int y=top;

            for(int i=0; i < count; ++i)
            {
                StepperStep *step=m_steps[static_cast<size_t>(i)];
                const int stepY=y;

                placeChild(step, step->getX(), y, step->getWidth(), step->getPreferredSize().height);
                y+=step->getPreferredSize().height;
                if(i == m_current)
                {
                    y+=BodyGap;
                    placeChild(m_body, m_body->getX(), y, m_body->getWidth(), m_body->getPreferredSize().height);
                    y+=m_body->getPreferredSize().height;
                }
                if(i < count-1)
                {
                    y+=StepSpacing;

                    // from under this indicator to above the next one,
                    // beside the page when this step is current
                    StepperConnector *connector=m_connectors[static_cast<size_t>(i)];
                    const int from=stepY+StepperStep::IndicatorSize+VerticalConnectorGap;

                    placeChild(connector, connector->getX(), from, connector->getWidth(),
                        std::max(0, y-VerticalConnectorGap-from));
                }
            }
            if(count == 0)
                placeChild(m_body, m_body->getX(), top, m_body->getWidth(), 0);
        }
        else
        {
            int header=0;

            for(StepperStep *step : m_steps)
            {
                placeChild(step, step->getX(), top, step->getWidth(), step->getPreferredSize().height);
                header=std::max(header, step->getPreferredSize().height);
            }
            // each rule through the indicators' centre row
            for(StepperConnector *connector : m_connectors)
                placeChild(connector, connector->getX(), top, connector->getWidth(), StepperStep::IndicatorSize);

            const int bodyY=top+header+HeaderGap;

            placeChild(m_body, m_body->getX(), bodyY, m_body->getWidth(), std::max(0, top+height-bodyY));
        }
    }

    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
