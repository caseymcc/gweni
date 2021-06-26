/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_userdata_h_
#define _gweni_userdata_h_

#include <map>

namespace gweni
{
/**
 *
  * Allows you to store multiple and various user data
 *
  * ~~~
  * //
  * // Valid
  * //
  * UserDataStorage.set( 100.0f );
  * UserDataStorage.<float>get();
 *
  * //
  * // Invalid - didn't Add a float type!
  * //
  * UserDataStorage.set( 100 );
  * UserDataStorage.<float>get();
 *
  * //
  * // You can pass structs as long as they can be copied safely.
  * //
  * UserDataStorage.set( mystruct );
  * UserDataStorage.<MyStruct>get();
 *
  * //
  * // If you pass a pointer then whatever it's pointing to
  * // should be valid for the duration. And it won't be freed.
  * //
  * UserDataStorage.set( &mystruct );
  * UserDataStorage.<MyStruct*>get();
  * ~~~
 */
class UserDataStorage
{
    struct ValueBase
    {
        virtual ~ValueBase() {}
        virtual void deleteThis()=0;
    };


    template <typename T>
    struct Value: public ValueBase
    {
        T val;

        Value(const T &v)
            : val(v)
        {}

        // Give value a chance to clean itself up
        // Note: could avoid using smart pointers.
        virtual void deleteThis()
        {
            delete this;
        }
    };

    typedef std::map<gweni::String, ValueBase *> Container;
    Container m_list;

public:

    UserDataStorage() {}

    ~UserDataStorage()
    {
        for(auto &&item : m_list)
        {
            item.second->deleteThis();
        }
    }

    template <typename T>
    void set(const gweni::String &str, const T &var)
    {
        Value<T> *val=nullptr;
        Container::iterator it=m_list.find(str);

        if(it != m_list.end())
        {
            static_cast<Value<T> *>(it->second)->val=var;
        }
        else
        {
            val=new Value<T>(var);
            m_list[str]=val;
        }
    }

    bool exists(const gweni::String &str) const
    {
        return m_list.find(str) != m_list.end();
    }

    template <typename T>
    T &get(const gweni::String &str)
    {
        Value<T> *v=static_cast<Value<T> *>(m_list[str]);
        return v->val;
    }
};


} // namespace gweni

#endif//_gweni_userdata_h_
