#ifndef _gweni_json_h_
#define _gweni_json_h_

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>

namespace rapidjson
{

template<typename _Type> 
_Type returnValue(const Value &value){ assert(false); return _Type(); }

template<> 
inline std::string returnValue<std::string>(const Value &value)
{
    if(!value.IsString())
    {
        std::cerr<<"is not a string\n";
        return "";
    }
    return value.GetString(); 
}

// The unsigned integer types by their names, not size_t/uint64_t: on x86-64
// those are both unsigned long, but on wasm32 size_t is unsigned long (32 bits)
// and uint64_t unsigned long long, so a size_t specialisation left
// returnValue<uint64_t> (the skin's bbox) to the assert above.
template<typename _Type>
inline _Type returnUnsignedValue(const Value &value)
{
    if(!value.IsNumber())
    {
        std::cerr<<"is not a number\n";
        return 0;
    }
    return (_Type)value.GetInt();
}

template<> 
inline unsigned int returnValue<unsigned int>(const Value &value)
{ 
    return returnUnsignedValue<unsigned int>(value);
}

template<> 
inline unsigned long returnValue<unsigned long>(const Value &value)
{ 
    return returnUnsignedValue<unsigned long>(value);
}

template<> 
inline unsigned long long returnValue<unsigned long long>(const Value &value)
{ 
    return returnUnsignedValue<unsigned long long>(value);
}

template<> 
inline uint8_t returnValue<uint8_t>(const Value &value)
{ 
    if(!value.IsUint())
    {
        std::cerr<<"is not an unsigned number\n";
        return 0;
    }
    return (uint8_t)value.GetUint(); 
}

template<> 
inline float returnValue<float>(const Value &value)
{ 
    if(!value.IsNumber())
    {
        std::cerr<<"is not a number\n";
        return 0;
    }
    return value.GetFloat(); 
}

template<> 
inline double returnValue<double>(const Value &value)
{ 
    if(!value.IsNumber())
    {
        std::cerr<<"is not a number\n";
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

        std::cerr<<"\""<<name<<"\" member missing\n";
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
        std::cerr<<"\""<<name<<"\" member missing\n";
        return retValues;
    }

    const Value &nameValue = value[name.c_str()];

    if(!nameValue.IsArray())
    {
        std::cerr<<"\""<<name<<"\" member is not an array\n";
        return retValues;
    }

    for(const Value &arrayValue:nameValue.GetArray())
    {
        retValues.push_back(returnValue<_Type>(arrayValue));
    }

    return retValues;
}

struct OffsetInfo
{
    size_t line;
    size_t pos;
};

inline OffsetInfo getOffsetInfo(std::vector<char> &buffer, size_t offset)
{
    size_t line;
    size_t pos;
    size_t linePos=0;
    
    line=1;
    offset=std::min(offset, buffer.size());
    for(size_t i=0; i<offset; ++i)
    {
        if(buffer[i] == '\n')
        {
            line++;
            linePos=i+1;
        }
    }
    pos=(offset-linePos)+1;

    return {line, pos};
}

}//namespace json

namespace json=rapidjson;

#endif//_gweni_json_h_
