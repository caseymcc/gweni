/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_button_h_
#define _gweni_controls_button_h_

#include <gweni/controls/label.h>

namespace gweni
{
namespace controls
{

class ImagePanel;

class GWENI_EXPORT Button: public Label
{
public:

    GWENI_CONTROL(Button, Label);

public:
    void render(skin::Base *skin) override;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseClickRight(int x, int y, bool down) override;
    void onMouseDoubleClickLeft(int x, int y) override;
    bool onKeySpace(bool down) override;

    virtual void onPress(event::Info);
    virtual void onRightPress(event::Info);

    void acceleratePressed() override;

    //! Is the button currently being pressed?
    virtual bool isDepressed() const
    {
        return m_depressed;
    }

    virtual void setDepressed(bool b);

    //
    /// Buttons can be toggle type, which means that it is
    /// toggled on and off. Its toggle status is in IsDepressed.
    //
    virtual void setIsToggle(bool b)
    {
        m_toggle=b;
    }

    virtual bool isToggle() const
    {
        return m_toggle;
    }

    virtual bool getToggleState() const
    {
        return m_toggleStatus;
    }

    virtual void setToggleState(bool b);
    virtual void toggle()
    {
        setToggleState(!getToggleState());
    }

    virtual void setImage(const String &name, bool center=false);

    void sizeToContents() override;
    void postLayout(skin::Base *skin) override;
    void updateColors() override;

    virtual void setImageAlpha(float multiply);

    void doAction() override
    {
        event::Info info(this);
        onPress(info);
    }

    virtual void setAction(event::Handler *object,
        event::Caller::EventCaller function,
        const gweni::event::Packet &packet) override;

public:

    // TODO - It is inefficient to store all of these for every control when not used.
    gweni::event::Caller onPressCaller;
    gweni::event::Caller onRightPressCaller;
    gweni::event::Caller onDownCaller;
    gweni::event::Caller onUpCaller;
    gweni::event::Caller onDoubleClickCaller;
    gweni::event::Caller onToggleCaller;
    gweni::event::Caller onToggleOnCaller;
    gweni::event::Caller onToggleOffCaller;

protected:

    ImagePanel *m_image;    ///< Optional image to be displayed on button.

    bool m_depressed;          ///< Is button currently being pressed?
    bool m_toggle;             ///< Is this a toggling button, i.e. on or off.
    bool m_toggleStatus;

    bool m_centerImage;        ///< Centre the image in the button?
};


}//controls
}//gweni

#endif//_gweni_controls_button_h_
