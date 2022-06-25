/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/anim.h>
#include <gweni/utility.h>
#include <cmath>

namespace gweni
{
namespace anim
{

#if GWENI_ANIMATE

static Animation::List g_animations;
static Animation::ChildList g_animationsListed;

void add(gweni::controls::Base *control, Animation *animation)
{
    animation->m_control=control;
    g_animations[control].push_back(animation);
}

void cancel(gweni::controls::Base *control)
{
    //    cannot use std::list iterator with algorithms based on pointers
    //    struct AnimDeletePredicate
    //    {
    //        AnimDeletePredicate(gweni::controls::Base *control)
    //        {
    //            this->control=control;
    //        }
    //        bool operator() (gweni::anim::Animation *anim)
    //        {
    //            return anim->m_control == control;
    //        }
    //        gweni::controls::Base *control;
    //    };
    //    std::remove_if(g_animations.begin(), g_animations.end(), AnimDeletePredicate(control));

    Animation::List::iterator iAnimations;

    if((iAnimations=g_animations.find(control)) != g_animations.end())
    {
        Animation::ChildList &ChildAnimationsForControl=iAnimations->second;
        Animation::ChildList::iterator iAnimationChild=
            ChildAnimationsForControl.begin();

        if(iAnimationChild != ChildAnimationsForControl.end())
        {
            do
            {
                delete(*iAnimationChild);
            } while(++iAnimationChild != ChildAnimationsForControl.end());
        }

        g_animations.erase(iAnimations);
    }
}

void think()
{
    Animation::List::iterator it=g_animations.begin();

    if(it != g_animations.end())
    {
        Animation::ChildList::iterator itChild;
        Animation *anim;

        do
        {
            if((itChild=it->second.begin()) != it->second.end())
            {
                do
                {
                    anim=*itChild;
                    anim->think();

                    if(anim->finished())
                    {
                        itChild=it->second.erase(itChild);
                        delete anim;
                    }
                    else
                    {
                        ++itChild;
                    }
                } while(itChild != it->second.end());
            }
        } while(++it != g_animations.end());
    }
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
        onFinish();
    }

    float delta=clamp(secondsIn/(m_end-m_start), 0.f, 1.f);

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
