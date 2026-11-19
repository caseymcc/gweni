/*
 *  Gweni - a frame's counters as key=value fields (gweni/app/frameStats.h).
 *
 *  See license in Gweni.h
 */

#include <gweni/app/frameStats.h>

#include <gweni/primitiveIds.h>

namespace gweni
{
namespace app
{

std::string redirtiedBy(const controls::LayoutStats &layout)
{
    if(!layout.redirtiedType)
        return "none";
    return std::string(layout.redirtiedType)+"#"+std::to_string(layout.redirtiedId);
}

std::string requestedBy(const controls::LayoutStats &layout)
{
    if(!layout.requestedType)
        return "none";
    return std::string(layout.requestedType)+"#"+std::to_string(layout.requestedId);
}

StatFields frameStatFields(int frame, const std::string &control,
    const controls::ChangeStats &changes,
    const renderer::SlotRenderer::FrameStats &gpu,
    const controls::LayoutStats &layout,
    const controls::UpdateStats &update,
    size_t measures, size_t invalidations)
{
    using std::to_string;

    return {
        {"frame", to_string(frame)},
        {"control", control},
        {"controls_changed", to_string(changes.controls)},
        {"created", to_string(changes.created)},
        {"visibility", to_string(changes.visibility)},
        {"geometry", to_string(changes.geometry)},
        {"background", to_string(changes.background)},
        {"text", to_string(changes.text)},
        {"prim_slots_written", to_string(gpu.primitiveSlotsWritten)},
        {"text_slots_written", to_string(gpu.textSlotsWritten)},
        {"upload_calls", to_string(gpu.uploadCalls)},
        {"upload_bytes", to_string(gpu.uploadBytes)},
        {"mirror_bytes", to_string(gpu.mirrorBytes)},
        {"draw_calls", to_string(gpu.drawCalls)},
        {"primitive_slots_culled", to_string(gpu.primitiveSlotsCulled)},
        {"text_runs_culled", to_string(gpu.textRunsCulled)},
        {"atlas_upload_calls", to_string(gpu.atlasUploadCalls)},
        {"atlas_upload_bytes", to_string(gpu.atlasUploadBytes)},
        {"buffer_regrows", to_string(gpu.bufferRegrows)},
        {"image_regrows", to_string(gpu.imagePageRegrows)},
        {"layout_passes", to_string(layout.passes)},
        {"redirtied_by", redirtiedBy(layout)},
        {"render_layout", layout.renderLayout ? "1" : "0"},
        {"layout_requests", to_string(layout.requests)},
        {"requested_by", requestedBy(layout)},
        {"layout_visited", to_string(layout.visited)},
        {"layout_children_scanned", to_string(layout.childrenScanned)},
        {"measure_calls", to_string(measures)},
        {"invalidations", to_string(invalidations)},
        {"visited", to_string(changes.visited)},
        {"render_carried", to_string(changes.renderCarried)},
        {"z_visited", to_string(update.zVisited)},
        {"anims_run", to_string(update.animsRun)},
        {"prim_ids", to_string(primitiveIdCount()-releasedPrimitiveIdCount())},
        {"excluded_changed", to_string(changes.excluded)},
        {"sorted_slots", to_string(gpu.sortedSlots)},
        {"sorted_runs", to_string(gpu.sortedRuns)},
        {"sorted_groups", to_string(gpu.sortedGroups)},
        {"sorted_builds", to_string(gpu.sortedScheduleBuilds)},
        {"overlap_visited", to_string(gpu.overlapVisited)},
    };
}

std::string formatStatFields(const StatFields &fields)
{
    std::string line;

    for(const StatField &field : fields)
    {
        if(!line.empty())
            line+=' ';
        line+=field.key;
        line+='=';
        line+=field.value;
    }
    return line;
}

}//namespace app
}//namespace gweni
