#ifndef _gweni_chipbadge_example_h_
#define _gweni_chipbadge_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Badge;
class Button;
class Chip;
class Label;
}

//! Chips (tones, the palette, clickable and removable ones) and Badges (on a
//! row of buttons each overhanging the next, one at every corner, two in a
//! ScrollControl and a navigation list whose counts a button changes).
class ChipBadgeDemo: public controls::Base
{
public:
    GWENI_CONTROL(ChipBadgeDemo, controls::Base);

public:
    //! The parts a test drives.
    controls::Chip *clickableChip() const { return m_clickable; }
    //! The first removable chip still there, null when none is.
    controls::Chip *removableChip() const;
    controls::Base *chipRow() const { return m_removableRow; }
    controls::Button *rowButton(size_t i) const { return m_rowButtons[i]; }
    controls::Badge *rowBadge(size_t i) const { return m_rowBadges[i]; }
    size_t rowButtons() const { return m_rowButtons.size(); }
    controls::Button *cornerButton() const { return m_corner; }
    const std::vector<controls::Badge *> &cornerBadges() const { return m_cornerBadges; }
    controls::Button *navButton() const { return m_navInbox; }
    controls::Badge *navBadge() const { return m_navInboxBadge; }
    controls::Button *newMessageButton() const { return m_newMessage; }
    //! What the last click on a clickable chip said.
    const String &pressedText() const;

private:
    void onChipPressed(event::Info info);
    void onChipRemoved(event::Info info);
    void onNewMessage(event::Info info);
    void onClear(event::Info info);

    controls::Chip *m_clickable=nullptr;
    controls::Base *m_removableRow=nullptr;
    std::vector<controls::Chip *> m_removable;
    std::vector<controls::Button *> m_rowButtons;
    std::vector<controls::Badge *> m_rowBadges;
    controls::Button *m_corner=nullptr;
    std::vector<controls::Badge *> m_cornerBadges;
    controls::Button *m_navInbox=nullptr;
    controls::Badge *m_navInboxBadge=nullptr;
    controls::Badge *m_navReviewsBadge=nullptr;
    controls::Button *m_newMessage=nullptr;
    controls::Label *m_pressed=nullptr;
};

}//namespace gweni

#endif//_gweni_chipbadge_example_h_
