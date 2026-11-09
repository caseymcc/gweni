/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#include <gweni/tools/toolSupport.h>

#include <gweni/skin.h>

#include <algorithm>

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

void OwnSlots::released(const controls::Base *control)
{
    appendSubtreeIds(control, m_primitives, m_texts);
}

AppFrameWrites OwnSlots::appWrites(renderer::Base *render, std::initializer_list<const controls::Base *> own)
{
    AppFrameWrites writes;
    const renderer::FrameCounts counts=render?render->frameStats():renderer::FrameCounts();

    if(!counts.counted)
    {
        m_primitives.clear();
        m_texts.clear();
        return writes;
    }
    writes.counted=true;

    const std::vector<size_t> &primitives=render->writtenPrimitiveIds();
    const std::vector<renderer::WrittenText> &texts=render->writtenTextIds();

    if(!primitives.empty() || !texts.empty())
    {
        // the ids its controls hold now, and those it gave back since the last
        // count
        std::vector<size_t> ownPrimitives=m_primitives;
        std::vector<size_t> ownTexts=m_texts;

        for(const controls::Base *control : own)
        {
            if(control)
                appendSubtreeIds(control, ownPrimitives, ownTexts);
        }
        std::sort(ownPrimitives.begin(), ownPrimitives.end());
        std::sort(ownTexts.begin(), ownTexts.end());

        for(size_t id : primitives)
        {
            if(!std::binary_search(ownPrimitives.begin(), ownPrimitives.end(), id))
                ++writes.primitiveSlots;
        }
        for(const renderer::WrittenText &text : texts)
        {
            if(!std::binary_search(ownTexts.begin(), ownTexts.end(), text.textId))
                writes.glyphSlots+=text.glyphSlots;
        }
    }
    m_primitives.clear();
    m_texts.clear();

    if((writes.primitiveSlots != 0) || (writes.glyphSlots != 0))
    {
        writes.uploadCalls=counts.uploadCalls;
        writes.uploadBytes=counts.uploadBytes;
    }
    return writes;
}

}//namespace tools
}//namespace gweni
