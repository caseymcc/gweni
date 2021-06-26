#ifndef _gweni_json_h_
#define _gweni_json_h_

#include <rapidjson/document.h>
#include <iostream>
#include <cstdint>

namespace rapidjson
{

template<typename _Type> 
_Type returnValue(const Value &value){ assert(false); return _Type(); }

template<> 
inline std::string returnValue<std::string>(const Value &value)
{
    if(!value.IsString())
    {
        std::cout<<"is not a string\n";
        return "";
    }
    return value.GetString(); 
}

template<> 
inline size_t returnValue<size_t>(const Value &value)
{ 
    if(!value.IsNumber())
    {
        std::cout<<"is not a number\n";
        return 0;
    }
    return value.GetInt(); 
}

template<> 
inline uint8_t returnValue<uint8_t>(const Value &value)
{ 
    if(!value.IsUint())
    {
        std::cout<<"is not an unsigned number\n";
        return 0;
    }
    return (uint8_t)value.GetUint(); 
}

template<> 
inline float returnValue<float>(const Value &value)
{ 
    if(!value.IsNumber())
    {
        std::cout<<"is not a number\n";
        return 0;
    }
    return value.GetFloat(); 
}

template<> 
inline double returnValue<double>(const Value &value)
{ 
    if(!value.IsNumber())
    {
        std::cout<<"is not a number\n";
        return 0;
    }
    return value.GetDouble(); 
}

template<typename _Type> 
_Type returnMemberValue(const Value &value, const std::string &name)
{
    const Value &retValue=value[name.c_str()];

    return returnValue<_Type>(retValue);
}

template<typename _Type> 
_Type getValue(const Value &value, std::string name)
{
    if(!value.HasMember(name.c_str()))
    {
        _Type retValue;

        std::cout<<"\""<<name<<"\" member missing\n";
        return retValue;
    }

    return returnMemberValue<_Type>(value, name);
}

template<typename _Type>
std::vector<_Type> getVector(const Value &value, std::string name)
{
    std::vector<_Type> retValues;

    if(!value.HasMember(name.c_str()))
    {
        std::cout<<"\""<<name<<"\" member missing\n";
        return retValues;
    }

    const Value &nameValue = value[name.c_str()];

    if(!nameValue.IsArray())
    {
        std::cout<<"\""<<name<<"\" member is not an array\n";
        return retValues;
    }

    for(const Value &arrayValue:nameValue.GetArray())
    {
        retValues.push_back(returnValue<_Type>(arrayValue));
    }

    return retValues;
}

}//namespace json

namespace json=rapidjson;

#endif//_gweni_json_h_
