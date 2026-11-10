/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/toolSupport.h>

#include <gweni/controls/canvas.h>
#include <gweni/skin.h>

namespace gweni
{
namespace tools
{

bool setLabelText(controls::Label *label, const String &text)
{
    if(label->getText() == text)
        return false;
    label->setText(text);
    return true;
}

void appendSubtreeIds(const controls::Base *control, std::vector<size_t> &primitiveIds,
    std::vector<size_t> &textIds)
{
    if(const skin::Control *skinControl=control->getSkinControl())
        skinControl->appendIds(primitiveIds, textIds);
    for(const controls::Base *child : control->getActualChildren())
        appendSubtreeIds(child, primitiveIds, textIds);
}

AppFrameWrites appFrameWrites(controls::Canvas *canvas, renderer::Base *render)
{
    AppFrameWrites writes;
    const renderer::FrameCounts counts=render?render->frameStats():renderer::FrameCounts();

    if(!counts.counted)
        return writes;
    writes.counted=true;

    const std::vector<size_t> &primitives=render->writtenPrimitiveIds();
    const std::vector<renderer::WrittenText> &texts=render->writtenTextIds();

    if(!primitives.empty() || !texts.empty())
    {
        // built once a frame, whoever reads it first
        const controls::ExcludedSlots *excluded=canvas?&canvas->excludedSlots():nullptr;

        for(size_t id : primitives)
        {
            if(!excluded || !excluded->primitive(id))
                ++writes.primitiveSlots;
        }
        for(const renderer::WrittenText &text : texts)
        {
            if(!excluded || !excluded->text(text.textId))
                writes.glyphSlots+=text.glyphSlots;
        }
    }

    if((writes.primitiveSlots != 0) || (writes.glyphSlots != 0))
    {
        writes.uploadCalls=counts.uploadCalls;
        writes.uploadBytes=counts.uploadBytes;
    }
    return writes;
}

}//namespace tools
}//namespace gweni
