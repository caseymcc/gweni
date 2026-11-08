/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/anim.h>
#include <gweni/utility.h>
#include <gweni/controls/canvas.h>
#include <cmath>
#include <vector>

namespace gweni
{
namespace anim
{

#if GWENI_ANIMATE

// Only active animations are here, in the order they were added, so a
// settled frame runs none and every process runs them in the same order.
// While a think() is running (g_thinking), a cancelled or finished animation is
// only marked m_removed: the loop, and an animation further down the stack,
// may still hold it. sweep() deletes the marked ones once no think() runs.
static std::vector<Animation *> g_animations;
static int g_thinking=0;
static bool g_swept=true;

static void markRemoved(Animation *animation)
{
    animation->m_removed=true;
    g_swept=false;
}

static void sweep()
{
    if(g_thinking || g_swept)
        return;

    size_t kept=0;

    for(Animation *animation : g_animations)
    {
        if(animation->m_removed)
            delete animation;
        else
            g_animations[kept++]=animation;
    }

    g_animations.resize(kept);
    g_swept=true;
}

void add(gweni::controls::Base *control, Animation *animation)
{
    animation->m_control=control;
    g_animations.push_back(animation);
}

void cancel(gweni::controls::Base *control)
{
    for(Animation *animation : g_animations)
    {
        if(animation->m_control == control && !animation->m_removed)
            markRemoved(animation);
    }

    sweep();
}

void cancel(gweni::controls::Base *control, Animation *animation)
{
    // compared, never dereferenced, until found: the caller may hold a pointer
    // to one that finished and is gone
    for(Animation *registered : g_animations)
    {
        if(registered == animation && registered->m_control == control
            && !registered->m_removed)
        {
            markRemoved(registered);
            break;
        }
    }

    sweep();
}

size_t think(controls::Canvas *canvas)
{
    size_t run=0;
    // one added from inside the loop waits for the next frame
    const size_t count=g_animations.size();

    ++g_thinking;

    for(size_t i=0; i < count; ++i)
    {
        // by index: an add() may reallocate the vector
        Animation *animation=g_animations[i];

        // a removed one's control may be gone: m_removed is read first
        if(animation->m_removed || (animation->m_control->getCanvas() != canvas))
            continue;

        // an excluded subtree's animation runs but is not counted
        // (Base::setExcludedFromStats); read before think(), which may delete the control
        const bool counted=!animation->m_control->statsExcluded();

        animation->think();
        if(counted)
            ++run;

        if(!animation->m_removed && animation->finished())
            markRemoved(animation);
    }

    --g_thinking;
    sweep();
    return run;
}

size_t activeCount(const controls::Base *control)
{
    size_t count=0;

    for(const Animation *animation : g_animations)
    {
        if(!animation->m_removed && (!control || (animation->m_control == control)))
            ++count;
    }

    return count;
}

TimedAnimation::TimedAnimation(float length, float delay, float ease)
{
    m_start=platform::getTimeInSeconds()+delay;
    m_end=m_start+length;
    m_ease=ease;
    m_started=false;
    m_finished=false;
}

static float getEased(float time, float ease)
{
    // Ease in and Out if ease is < 0
    if(ease < 0)
        return -time/2*(std::cos(3.14159f*time)-1);

    return std::pow(time, ease);
}

void TimedAnimation::think()
{
    if(m_finished)
        return;

    float current=platform::getTimeInSeconds();
    float secondsIn=current-m_start;

    if(secondsIn < 0.0f)
        return;

    if(!m_started)
    {
        m_started=true;
        onStart();
    }

    // a zero length (tools::Remove) is over as soon as it starts
    float delta=(m_end > m_start) ? clamp(secondsIn/(m_end-m_start), 0.f, 1.f) : 1.0f;

    float eased=getEased(delta, m_ease);
    run(eased);

    if(delta == 1.0f)
    {
        m_finished=true;
        onFinish();
    }
}

bool TimedAnimation::finished()
{
    return m_finished;
}

}//namespace anim
}//namespace gweni

#endif // if GWENI_ANIMATE
