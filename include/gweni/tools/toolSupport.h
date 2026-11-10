/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_tools_toolSupport_h_
#define _gweni_tools_toolSupport_h_

#include <gweni/anim.h>
#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/platforms/baseRender.h>

#include <vector>

namespace gweni
{
namespace tools
{

// What the tools that show a canvas's frame counts share (Inspector,
// StatsOverlay). Each is setExcludedFromStats(), so none of its controls count
// in the canvas's counters; what is left is to keep the tools' slots out of
// the renderer's (every tool's, through the canvas's one record,
// controls::Canvas::excludedSlots()), and to change nothing on a frame nothing
// it shows changed.

//! Sets label's text to text unless it shows it already, so a tool that sets
//! every line on every refresh writes only the lines that changed. \return
//! whether it set it.
GWENI_EXPORT bool setLabelText(controls::Label *label, const String &text);

//! Appends the ids every skin Control of control's subtree holds.
GWENI_EXPORT void appendSubtreeIds(const controls::Base *control, std::vector<size_t> &primitiveIds,
    std::vector<size_t> &textIds);

//! What the renderer's last frame wrote of the app's: its slots written, less
//! those under the ids of the canvas's tools (appFrameWrites()).
struct AppFrameWrites
{
    //! false: the renderer does not count (FrameCounts::counted), and every
    //! field is 0
    bool counted=false;
    size_t primitiveSlots=0;    //!< primitive slots written under no id of a tool's
    size_t glyphSlots=0;        //!< glyph slots written under no text id of a tool's
    //! Uploads cannot be told apart by id: the frame's whole uploads when the app
    //! wrote a slot in it, 0 for a frame only the tools wrote in.
    size_t uploadCalls=0;
    size_t uploadBytes=0;
};

//! The renderer's last frame (render->frameStats() and its written ids), read
//! in the update phase of the frame after it, less the slots under the ids
//! canvas->excludedSlots() has: those every setExcludedFromStats() subtree on
//! the canvas holds, and those they gave back for that frame's flush to clear.
//! Any number of tools may call it in a frame and get the same answer.
GWENI_EXPORT AppFrameWrites appFrameWrites(controls::Canvas *canvas, renderer::Base *render);

#if GWENI_ANIMATE
//! Runs Tool::refresh() once a frame, at the start of the canvas's update
//! phase, on the tool it is added to (anim::add(tool, new
//! RefreshAnimation<Tool>())). Its control is the tool, which is
//! setExcludedFromStats(), so it is counted in no animsRun.
template<class Tool>
class RefreshAnimation: public anim::Animation
{
public:
    void think() override
    {
        static_cast<Tool *>(m_control)->refresh();
    }
};
#endif

}//namespace tools
}//namespace gweni

#endif//_gweni_tools_toolSupport_h_
