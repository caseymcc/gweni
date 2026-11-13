/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_searchcombo_example_h_
#define _gweni_searchcombo_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class ComboBox;
class Label;
class SearchComboBox;
}

//! SearchComboBox in its four modes: beside a ComboBox showing the same
//! options (plain, they look alike), searchable over 300 generated
//! references, free text with a validator (host names), and multi-select
//! with and without search, the last one taking free-text tags too. A label
//! at the bottom says what the last event was.
class SearchComboDemo: public controls::Base
{
public:
    GWENI_CONTROL(SearchComboDemo, controls::Base);

public:
    //! The references the searchable box shows at first.
    static constexpr size_t ReferenceCount=300;

    //! The parts a test drives.
    controls::ComboBox *comboBox() const { return m_combo; }
    controls::SearchComboBox *plainBox() const { return m_plain; }
    controls::SearchComboBox *referenceBox() const { return m_references; }
    controls::SearchComboBox *hostBox() const { return m_hosts; }
    controls::SearchComboBox *tagBox() const { return m_tags; }
    controls::SearchComboBox *areaBox() const { return m_areas; }
    //! The searchable box's options: the first \p count references, the
    //! first selected.
    void setReferenceCount(size_t count);
    //! Reference \p index's label: the same for an index at any count.
    static String referenceLabel(size_t index);
    //! Whether \p text is a host name the free-text box takes: lower-case
    //! letters, digits, '-' and '.', with a dot, neither first nor last.
    static bool validHost(const String &text);
    //! What the last event was.
    const String &lastEvent() const;
    controls::Label *eventLabel() const { return m_event; }

private:
    void onPicked(event::Info info);
    void onHost(event::Info info);
    void onPicksChanged(event::Info info);
    void say(const String &text);

    controls::ComboBox *m_combo=nullptr;
    controls::SearchComboBox *m_plain=nullptr;
    controls::SearchComboBox *m_references=nullptr;
    controls::SearchComboBox *m_hosts=nullptr;
    controls::SearchComboBox *m_tags=nullptr;
    controls::SearchComboBox *m_areas=nullptr;
    controls::Label *m_event=nullptr;
};

}//namespace gweni

#endif//_gweni_searchcombo_example_h_
