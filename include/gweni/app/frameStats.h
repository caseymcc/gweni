/*
 *  Gweni - a frame's counters as key=value fields (gweni_app, gweni::app):
 *  the change, renderer, layout and update counts of one frame in the order
 *  and under the keys ControlTest --stats prints them, the one list
 *  app::Headless::stats() and ControlTest's line are both made of, so the
 *  names cannot drift apart.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_app_frameStats_h_
#define _gweni_app_frameStats_h_

#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/platforms/renderers/slotRenderer.h>

#include <cstddef>
#include <string>
#include <vector>

namespace gweni
{
namespace app
{

//! One counter: its key and its value as printed.
struct StatField
{
    std::string key;
    std::string value;
};

using StatFields=std::vector<StatField>;

//! The control that re-dirtied layout during a pass, as Type#id, or "none"
//! (LayoutStats::redirtiedType).
std::string redirtiedBy(const controls::LayoutStats &layout);
//! The control that first recorded a request during a pass, as Type#id, or
//! "none" (LayoutStats::requests).
std::string requestedBy(const controls::LayoutStats &layout);

//! The fields of one frame, frame= and control= first: \p changes is
//! controls::getChangeStats() after the render (reset before the update),
//! \p gpu the renderer's frame stats and \p layout and \p update the canvas's,
//! read after renderCanvas(); \p measures and \p invalidations the
//! controls::measureTextCalls() and invalidateCalls() since the frame before
//! (the input between them included); prim_ids is read from the id bank now.
StatFields frameStatFields(int frame, const std::string &control,
    const controls::ChangeStats &changes,
    const renderer::SlotRenderer::FrameStats &gpu,
    const controls::LayoutStats &layout,
    const controls::UpdateStats &update,
    size_t measures, size_t invalidations);

//! "key=value" for each field, joined by single spaces.
std::string formatStatFields(const StatFields &fields);

}//namespace app
}//namespace gweni

#endif//_gweni_app_frameStats_h_
