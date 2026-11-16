/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/markdown.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/markdownView.h"
#include "gweni/anim.h"
#include "gweni/platforms/baseRender.h"
#include "gweni/skin.h"

#include <fstream>
#include <iterator>
#include <string>
#include <iostream>
#include <sstream>

namespace gweni
{

// A resource file's path, found as the skin's resources are
static String resourcePath(controls::Base *demo, const char *name)
{
    skin::Base *skin=demo->getSkin();

    return (skin && skin->getRenderer())?
        skin->getRenderer()->getResourcePaths().getPath(ResourcePaths::Type::Other, name):String(name);
}

// A resource file's bytes, empty (and one line on stderr) when it cannot be
// read: the handle showing them then fails
static std::vector<uint8_t> readBytes(controls::Base *demo, const char *name)
{
    const String path=resourcePath(demo, name);
    std::ifstream file(path, std::ios::binary);

    if(!file)
    {
        std::cerr<<"MarkdownDemo: cannot read "<<path<<"\n";
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// The file as bytes, or a line naming it (and one on stderr) when it cannot
// be read
static String readReference(controls::Base *demo)
{
    const String path=resourcePath(demo, MarkdownDemo::FileName);
    std::ifstream file(path, std::ios::binary);

    if(!file)
    {
        std::cerr<<"MarkdownDemo: cannot read "<<path<<"\n";
        return "*"+path+" could not be read.*\n";
    }

    std::ostringstream text;

    text<<file.rdbuf();
    return text.str();
}

#if GWENI_ANIMATE
// The delivery's frame schedule: one think() a frame, from the demo's first
class MarkdownDemo::DeliveryAnimation: public anim::Animation
{
public:
    explicit DeliveryAnimation(MarkdownDemo *demo): m_demo(demo) {}

    void think() override
    {
        // paused: a scenario delivers
        if(m_demo->m_deliveryDone)
            return;
        // the first frame lays the alt texts out; the bytes come later
        if(m_frame == 1)
        {
            m_demo->deliver(Delivery::FirstSize);
            m_demo->deliver(Delivery::ThirdFailed);
        }
        else if(m_frame == 2)
        {
            m_demo->deliver(Delivery::FirstBytes);
            m_demo->deliver(Delivery::SecondBytes);
            m_demo->m_deliveryDone=true;
        }
        ++m_frame;
    }
    bool finished() override { return m_demo->m_deliveryDone; }

private:
    MarkdownDemo *m_demo;
    int m_frame=0;
};
#endif

GWENI_CONTROL_CONSTRUCTOR(MarkdownDemo)
{
    setDock(DockPosition::Center);

    m_wrapToggle=newChild<controls::CheckBoxWithLabel>("wrap");
    m_wrapToggle->label()->setText("Wrap code");
    m_wrapToggle->setPos(10, 8);
    m_wrapToggle->checkBox()->onCheckChanged.add(this, &MarkdownDemo::onWrapChanged);

    m_status=newChild<controls::Label>();
    m_status->setText("Click a link: the demo shows its url here");
    m_status->setPos(110, 10);

    m_view=newChild<controls::MarkdownView>();
    m_view->setDock(DockPosition::Center);
    m_view->setMargin(Margin(10, 34, 10, 10));
    m_view->onLinkClicked.add(this, &MarkdownDemo::onLinkClicked);
    for(int i=0; i < 3; ++i)
        m_handles.push_back(ImageHandle::create());
    m_pipelineBytes=readBytes(this, "MarkdownDemoPipeline.png");
    m_duskBytes=readBytes(this, "MarkdownDemoDusk.jpg");
    m_view->setImageResolver([this](const String &url) { return resolve(url); });

    m_reference=readReference(this);
    m_view->setMarkdown(m_reference);

#if GWENI_ANIMATE
    anim::add(this, new DeliveryAnimation(this));
#else
    deliver(Delivery::FirstSize);
    deliver(Delivery::ThirdFailed);
    deliver(Delivery::FirstBytes);
    deliver(Delivery::SecondBytes);
    m_deliveryDone=true;
#endif
}

std::shared_ptr<ImageHandle> MarkdownDemo::resolve(const String &url)
{
    for(size_t i=0; i < m_handles.size(); ++i)
    {
        if(url == "attachment:"+std::to_string(i+1))
            return m_handles[i];
    }
    return nullptr;
}

void MarkdownDemo::pauseDelivery()
{
    m_deliveryDone=true;
}

void MarkdownDemo::deliver(Delivery step)
{
    // the image's own dimensions, as an app's metadata would give them first
    const int pipelineWidth=1600;
    const int pipelineHeight=120;

    switch(step)
    {
    case Delivery::FirstSize:
        m_handles[0]->setSize(pipelineWidth, pipelineHeight);
        break;
    case Delivery::ThirdFailed:
        m_handles[2]->setFailed();
        break;
    case Delivery::FirstBytes:
        if(m_pipelineBytes.empty())
            m_handles[0]->setFailed();
        else
            m_handles[0]->setBytes(m_pipelineBytes);
        break;
    case Delivery::SecondBytes:
        if(m_duskBytes.empty())
            m_handles[1]->setFailed();
        else
            m_handles[1]->setBytes(m_duskBytes);
        break;
    }
}

void MarkdownDemo::onLinkClicked(event::Info info)
{
    m_lastLink=info.string;
    ++m_linkClicks;
    m_status->setText("Link: "+m_lastLink);
}

void MarkdownDemo::onWrapChanged(event::Info)
{
    m_view->setCodeWrap(m_wrapToggle->checkBox()->isChecked());
}

}//namespace gweni
