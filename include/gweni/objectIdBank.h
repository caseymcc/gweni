
#ifndef _gweni_objectIdBank_h_
#define _gweni_objectIdBank_h_

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
    returnedIds.push_back(id);
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
