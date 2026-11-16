/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_markdown_example_h_
#define _gweni_markdown_example_h_

#include <gweni/controls/base.h>
#include <gweni/imageHandle.h>

#include <memory>
#include <vector>

namespace gweni
{
namespace controls
{
class CheckBoxWithLabel;
class Label;
class MarkdownView;
}

//! The reference Markdown document, resources/MarkdownDemo.md (every element
//! a MarkdownView shows), in a MarkdownView filling the demo under a "Wrap
//! code" toggle and a Label showing the url of the last link clicked (the
//! demo routes it there and opens nothing).
//!
//! Its three images, attachment:1..3, are the demo's ImageHandles, filled in
//! a few frames late as an app's fetches would be, counted in frames (the
//! demo's update phases), never in time, so every capture sees the same
//! frames: in the second frame attachment:1's size (setSize) and
//! attachment:3's failure, in the third attachment:1's bytes and
//! attachment:2's (with no size before them). The bytes are
//! resources/MarkdownDemoPipeline.png and MarkdownDemoDusk.jpg, read when the
//! demo is built. pauseDelivery() stops the schedule before its first frame,
//! for a scenario delivering each step itself (deliver()).
class MarkdownDemo: public controls::Base
{
public:
    GWENI_CONTROL(MarkdownDemo, controls::Base);

public:
    //! The file the demo shows, found as the skin's resources are
    static constexpr const char *FileName="MarkdownDemo.md";

    controls::MarkdownView *view() const { return m_view; }
    controls::CheckBoxWithLabel *wrapToggle() const { return m_wrapToggle; }
    //! The document's text as read from the file (a line naming it when it
    //! could not be read)
    const gweni::String &referenceText() const { return m_reference; }
    //! The url of the last link clicked, empty before the first
    const gweni::String &lastLink() const { return m_lastLink; }
    //! How many link clicks the demo was told of
    int linkClicks() const { return m_linkClicks; }

    //! The steps of the images' delivery, in the schedule's order
    enum class Delivery
    {
        FirstSize,      //!< attachment:1's size
        ThirdFailed,    //!< attachment:3 failed
        FirstBytes,     //!< attachment:1's bytes
        SecondBytes     //!< attachment:2's bytes, its size not given before
    };
    //! attachment:<index+1>'s handle
    const std::shared_ptr<ImageHandle> &imageHandle(size_t index) const { return m_handles[index]; }
    //! Stops the frame schedule (deliveries already made stay)
    void pauseDelivery();
    //! Makes one step now
    void deliver(Delivery step);
    //! The frame schedule's steps all made, or paused
    bool deliveryDone() const { return m_deliveryDone; }

private:
    void onLinkClicked(event::Info info);
    void onWrapChanged(event::Info info);

    std::shared_ptr<ImageHandle> resolve(const gweni::String &url);

    controls::MarkdownView *m_view=nullptr;
    std::vector<std::shared_ptr<ImageHandle>> m_handles;
    std::vector<uint8_t> m_pipelineBytes;
    std::vector<uint8_t> m_duskBytes;
    bool m_deliveryDone=false;
    class DeliveryAnimation;
    controls::CheckBoxWithLabel *m_wrapToggle=nullptr;
    controls::Label *m_status=nullptr;
    gweni::String m_reference;
    gweni::String m_lastLink;
    int m_linkClicks=0;
};

}//namespace gweni

#endif//_gweni_markdown_example_h_
