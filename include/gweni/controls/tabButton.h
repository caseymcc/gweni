/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_tabbutton_h_
#define _gweni_controls_tabbutton_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/windowButtons.h>

namespace gweni
{
namespace controls
{
class TabControl;

//! A closable tab's close button: the window close button's look (the
//! WindowCloseButton renderer, found up its parent types: a cross, icon kind 2,
//! and a red surface on hover and press), a CloseButtonSize square.
class GWENI_EXPORT TabCloseButton: public WindowCloseButton
{
    GWENI_CONTROL_INLINE(TabCloseButton, WindowCloseButton)
    {
        setTabable(false);
        setKeyboardInputEnabled(false);
    }
};

class GWENI_EXPORT TabButton: public Button
{
public:
    GWENI_CONTROL(TabButton, Button);

public:
//    void render(skin::Base *skin) override;
    void calculateSize(skin::Base *skin, Dim dim) override;
    //! Its padding follows the dock of the strip it is in.
    bool readsParentLayout() const override { return true; }

    void setPage(Base *page)
    {
        m_page=page;
    }

    Base *getPage()
    {
        return m_page;
    }

    void setTabControl(TabControl *ctrl);
    TabControl *getTabControl()
    {
        return m_control;
    }

    //! The selected tab: its page's own flag, so a tab is as active under a
    //! hidden ancestor as when it is shown (updateColors() reads it, and is not
    //! run again when an ancestor is shown).
    bool isActive()
    {
        return m_page && !m_page->hidden();
    }

    bool dragAndDrop_shouldStartDrag() override;
    void dragAndDrop_startDragging(gweni::draganddrop::Package * /*package*/,
        int /*x*/, int /*y*/) override
    {
        setHidden(true);
    }

    void dragAndDrop_endDragging(bool /*bSuccess*/, int /*x*/, int /*y*/) override
    {
        setHidden(false); setDepressed(false);
    }

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;

    void updateColors() override;

    bool shouldClip() override
    {
        return false;
    }

    //! The side of the close button's square, in units.
    static constexpr int CloseButtonSize=14;

    //! A closable tab owns a TabCloseButton (getCloseButton()) in a
    //! CloseButtonSize square at its trailing end, added to its right padding,
    //! so the tab is that much wider whether or not the button shows. The
    //! button shows on the selected tab and on the hovered one (the tab or the
    //! button itself under the mouse), and is hidden otherwise. Clicking it
    //! asks the TabControl to close the tab (TabControl::requestClose), which
    //! fires TabControl::onClose. Turning it off deletes the button.
    void setClosable(bool closable);
    bool isClosable() const
    {
        return m_closeButton != nullptr;
    }

    //! The close button, or null when the tab is not closable.
    TabCloseButton *getCloseButton() const
    {
        return m_closeButton;
    }

    //! The tab or its close button is under the mouse.
    bool isTabHovered() const;

    void arrange(skin::Base *skin, Dim dim) override;
    void onDescendantInteraction(Base *control) override;

private:
    //! Shows or hides the close button for the tab's state.
    void updateCloseButton();
    void onCloseButtonPressed(event::Info info);

    Base *m_page;
    TabControl *m_control;
    TabCloseButton *m_closeButton;

};

}
}

#endif//_gweni_controls_tabbutton_h_
