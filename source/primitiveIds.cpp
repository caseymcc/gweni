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

size_t getTextId(void *object)
{
    return g_textIds.getNextId(object);
}

void releaseTextId(size_t id)
{
    g_textIds.releaseId(id);
}

}//namespace gweni
