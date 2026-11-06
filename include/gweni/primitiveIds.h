#ifndef _gweni_primitiveIds_h_
#define _gweni_primitiveIds_h_

#include <cstddef>
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

//! Id bank state, for tests and debug tooling. The count is the high-water
//! mark (every id ever handed out); the released list holds the ids waiting to
//! be handed out again, so count - released.size() ids are live.
GWENI_EXPORT size_t primitiveIdCount();
GWENI_EXPORT size_t releasedPrimitiveIdCount();
GWENI_EXPORT const std::vector<size_t> &releasedPrimitiveIds();

GWENI_EXPORT size_t textIdCount();
GWENI_EXPORT size_t releasedTextIdCount();
GWENI_EXPORT const std::vector<size_t> &releasedTextIds();

}//namespace gweni

#endif//_gweni_primitiveIds_h_
