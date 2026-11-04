/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_progressbar_h_
#define _gweni_controls_progressbar_h_
#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT ProgressBar: public Label
{
public:

    GWENI_CONTROL(ProgressBar, Label);

public:
    //void render(skin::Base *skin) override;

    virtual void setVertical()
    {
        m_horizontal=false;
    }

    virtual void setHorizontal()
    {
        m_horizontal=true;
    }

    virtual void  setProgress(float val);
    virtual float getProgress() const
    {
        return m_progress;
    }

    virtual void setAutoLabel(bool b)
    {
        m_autoLabel=b;
    }

    virtual float getCycleSpeed();
    virtual void  setCycleSpeed(float f);

    virtual void cycleThink(float delta);

protected:

    float m_progress;
    float m_cycleSpeed;

    bool m_horizontal;
    bool m_autoLabel;
};


}
}
#endif//_gweni_controls_progressbar_h_
