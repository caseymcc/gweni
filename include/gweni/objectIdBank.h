
#ifndef _gweni_objectIdBank_h_
#define _gweni_objectIdBank_h_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace gweni
{

template<typename _Object, _Object _EmptyValue>
struct ObjectIdBank
{
    ObjectIdBank(){}

    size_t getNextId(_Object control);
    void releaseId(size_t id);

    _Object getObject(size_t id);

    size_t size();

    std::vector<_Object> ids;

    // Kept sorted descending, so the smallest free id is back() and handing one
    // out is O(1). Lowest-first matters: a primitive id is a fixed slot in the
    // renderer's buffer and the buffer is drawn in slot order, so the order ids
    // come back in decides the blend order of overlapping geometry. Handing them
    // out LIFO reversed it after a teardown, which changed antialiased edge
    // pixels relative to a fresh process.
    std::vector<size_t> returnedIds;
};

template<typename _Object, _Object _EmptyValue>
size_t ObjectIdBank<_Object, _EmptyValue>::getNextId(_Object control)
{
    size_t id;

    if(!returnedIds.empty())
    {
        id=returnedIds.back();
        returnedIds.pop_back();

        ids[id]=control;
    }
    else
    {
        id=ids.size();
        ids.push_back(control);
    }

    return id;
}

template<typename _Object, _Object _EmptyValue>
void ObjectIdBank<_Object, _EmptyValue>::releaseId(size_t id)
{
    ids[id]=_EmptyValue;
    returnedIds.insert(std::upper_bound(returnedIds.begin(), returnedIds.end(), id,
        std::greater<size_t>()), id);
}

template<typename _Object, _Object _EmptyValue>
_Object ObjectIdBank<_Object, _EmptyValue>::getObject(size_t id)
{
    return ids[id];
}

template<typename _Object, _Object _EmptyValue>
size_t ObjectIdBank<_Object, _EmptyValue>::size()
{
    return ids.size();
}

}//namespace gweni

#endif//_gweni_objectIdBank_h_
