#include "gweni/primitiveIds.h"
#include <gweni/objectIdBank.h>

namespace gweni
{

ObjectIdBank<void *, nullptr> g_primitives;
ObjectIdBank<void *, nullptr> g_textIds;

void enlargePrimitiveIds(void *object, PrimitiveIds &primitiveIds, size_t size)
{
    size_t start=primitiveIds.size();

    primitiveIds.resize(size);

    for(size_t i=start; i<size; ++i)
    {
        primitiveIds[i]=getPrimitiveId(object);
    }
}

size_t getPrimitiveId(void *object)
{
    return g_primitives.getNextId(object);
}

void releasePrimitiveId(size_t id)
{
    g_primitives.releaseId(id);
}

bool isPrimitiveIdLive(size_t id)
{
    return g_primitives.isLive(id);
}

size_t getTextId(void *object)
{
    return g_textIds.getNextId(object);
}

void releaseTextId(size_t id)
{
    g_textIds.releaseId(id);
}

bool isTextIdLive(size_t id)
{
    return g_textIds.isLive(id);
}

size_t primitiveIdCount()
{
    return g_primitives.size();
}

size_t releasedPrimitiveIdCount()
{
    return g_primitives.returnedIds.size();
}

const std::vector<size_t> &releasedPrimitiveIds()
{
    return g_primitives.returnedIds;
}

size_t textIdCount()
{
    return g_textIds.size();
}

size_t releasedTextIdCount()
{
    return g_textIds.returnedIds.size();
}

const std::vector<size_t> &releasedTextIds()
{
    return g_textIds.returnedIds;
}

}//namespace gweni
