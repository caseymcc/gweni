#ifndef _gweni_primitiveIds_h_
#define _gweni_primitiveIds_h_

#include <vector>
#include "gweni/export.h"

namespace gweni
{

typedef std::vector<size_t> PrimitiveIds;

GWENI_EXPORT void enlargePrimitiveIds(void *object, PrimitiveIds &primitiveIds, size_t size);

GWENI_EXPORT size_t getPrimitiveId(void *object);
GWENI_EXPORT void releasePrimitiveId(size_t id);

GWENI_EXPORT size_t getTextId(void *object);
GWENI_EXPORT void releaseTextId(size_t id);

}//namespace gweni

#endif//_gweni_primitiveIds_h_
