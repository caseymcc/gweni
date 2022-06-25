/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_anim_h_
#define _gweni_anim_h_

#include <gweni/gweni.h>
#include <gweni/controls/base.h>
#include <gweni/platforms/platform.h>

#if GWENI_ANIMATE

namespace gweni
{
namespace anim
{

class GWENI_EXPORT Animation
{
public:
    typedef std::list<Animation *> ChildList;
    typedef std::map<controls::Base *, ChildList> List;

    virtual void think()=0;
    virtual bool finished()
    {
        return false;
    }

    virtual ~Animation()
    {}

    controls::Base *m_control;
};


GWENI_EXPORT void add(controls::Base *control, Animation *animation);
GWENI_EXPORT void cancel(controls::Base *control);
GWENI_EXPORT void think();

//
/// Timed animation. Provides a useful base for animations.
//
class GWENI_EXPORT TimedAnimation:public Animation
{
public:
    TimedAnimation(float length, float delay=0.0f, float ease=-1.0f);

    void think() override;
    bool finished() override;

    //
    // These are the magic functions you should be overriding
    //
    virtual void onStart() {}
    virtual void run(float /*delta*/) {}
    virtual void onFinish() {}

protected:
    bool m_started;
    bool m_finished;
    float m_start;
    float m_end;
    float m_ease;
};


namespace size
{

class GWENI_EXPORT Height:public TimedAnimation
{
public:

    Height(int startSize, int endSize, float length, bool hide=false,
        float delay=0.0f, float ease=-1.0f):
        TimedAnimation(length, delay, ease)
    {
        m_startSize=startSize;
        m_delta=endSize-m_startSize;
        m_hide=hide;
    }

    void onStart() override
    {
        m_control->setHeight(m_startSize);
    }

    void run(float delta) override
    {
        m_control->setHeight(m_startSize + m_delta  *delta);
    }

    void onFinish() override
    {
        m_control->setHeight(m_startSize+m_delta);
        m_control->setHidden(m_hide);
    }

protected:

    int m_startSize;
    int m_delta;
    bool m_hide;
};


class Width:public TimedAnimation
{
public:

    Width(int startSize, int endSize, float length, bool hide=false,
        float delay=0.0f, float ease=-1.0f):
        TimedAnimation(length, delay, ease)
    {
        m_startSize=startSize;
        m_delta=endSize-m_startSize;
        m_hide=hide;
    }

    void onStart() override
    {
        m_control->setWidth(m_startSize);
    }

    void run(float delta) override
    {
        m_control->setWidth(m_startSize + m_delta  *delta);
    }

    void onFinish() override
    {
        m_control->setWidth(m_startSize+m_delta);
        m_control->setHidden(m_hide);
    }

protected:

    int m_startSize;
    int m_delta;
    bool m_hide;
};

}//namespace size

namespace pos
{

class GWENI_EXPORT X: public anim::TimedAnimation
{
public:

    X(int startSize, int endSize, float length, bool hide=false,
        float delay=0.0f, float ease=1.0f)
        : TimedAnimation(length, delay, ease)
    {
        m_startSize=startSize;
        m_delta=endSize-m_startSize;
        m_hide=hide;
    }

    void onStart() override
    {
        m_control->setPos(m_startSize, m_control->getPos().y);
    }

    void run(float delta) override
    {
        m_control->setPos(m_startSize + static_cast<float>(m_delta)  *delta,
            m_control->getPos().y);
    }

    void onFinish() override
    {
        m_control->setPos(m_startSize+m_delta, m_control->getPos().y);
        m_control->setHidden(m_hide);
    }

protected:

    int m_startSize;
    int m_delta;
    bool m_hide;
};


class Y: public anim::TimedAnimation
{
public:

    Y(int startSize, int endSize, float length, bool hide=false,
        float delay=0.0f, float ease=1.0f)
        : TimedAnimation(length, delay, ease)
    {
        m_startSize=startSize;
        m_delta=endSize-m_startSize;
        m_hide=hide;
    }

    void onStart() override
    {
        m_control->setPos(m_control->getPos().x, m_startSize);
    }

    void run(float delta) override
    {
        m_control->setPos(m_control->getPos().x, m_startSize + m_delta  *delta);
    }

    void onFinish() override
    {
        m_control->setPos(m_control->getPos().x, m_startSize+m_delta);
        m_control->setHidden(m_hide);
    }

protected:

    int m_startSize;
    int m_delta;
    bool m_hide;
};

}//namespace pos

namespace tools
{

class Remove: public TimedAnimation
{
public:

    Remove(float delay=0.0f)
        : TimedAnimation(0.0f, delay)
    {}

    void onFinish() override
    {
        m_control->delayedDelete();
    }

};

}//namespace tools

}//namespace anim
}//namespace gweni

#endif // !GWENI_NO_ANIMATION
#endif // _gweni_anim_h_
