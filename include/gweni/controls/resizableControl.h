/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_resizablecontrol_h_
#define _gweni_controls_resizablecontrol_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dragger.h>
#include <gweni/controls/label.h>
#include <gweni/controls/resizer.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <array>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT ResizableControl: public Base
{
public:

    GWENI_CONTROL(ResizableControl, Base);

    virtual void setClampMovement(bool shouldClamp)
    {
        m_clampMovement=shouldClamp;
    }

    virtual bool getClampMovement()
    {
        return m_clampMovement;
    }

    virtual void setMinimumSize(const Size &minSize)
    {
        m_minimumSize=minSize;
    }

    Size getMinimumSize() override
    {
        return m_minimumSize;
    }

    virtual void disableResizing();

    bool setBounds(int x, int y, int w, int h) override;

    virtual void onResized()
    {}

    event::Caller onResize;

    virtual internal::Resizer *getResizer(int iResizer) // TODO - remove this?
    {
        return m_resizers[iResizer];
    }

protected:

    void onResizedInternal(event::Info);

    Size m_minimumSize;
    bool m_clampMovement;
    bool m_resizable;

    std::array<internal::Resizer *, 8> m_resizers;
};


}
}
#endif//_gweni_controls_resizablecontrol_h_
