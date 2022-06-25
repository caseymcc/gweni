/*!
 *  \file Utility.h
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */


#ifndef _gweni_utility_h_
#define _gweni_utility_h_

#include <gweni/platforms/platformTypes.h>

#include <sstream>
#include <vector>
#include <stdarg.h>
#include <algorithm>
#include <array>
#include <stdio.h>

 // Some platforms (e.g. Windows) define these. We want std::min/max.
#ifdef min
#   undef min
#endif
#ifdef max
#   undef max
#endif

namespace gweni
{


#ifdef _MSC_VER

//namespace detail
//{
//
//template<typename ... _Types>
//std::string &join(_Types const &... strings)
//{
//    static std::string value=(std::string(strings)+...);
//    return value;
//}
//
//template<typename ... _Types>
//const std::string join(_Types const &... strings)
//{
//    (std::puts(std::string(strings).c_str()),...);
//    return (std::string(strings)+...);
//}
//
//template<std::string_view const &... _Strings>
//const std::string join = detail::join(_Strings...);

namespace detail
{

template<typename ... _Types>
std::string joinFunc(_Types const &... strings)
{
    return (std::string(strings)+...);
}

template<std::string_view const &... _Strings>
struct join
{
    inline static const std::string arr=joinFunc(_Strings...);
    inline static const std::string_view value={arr.data(), arr.size()};
};

//template<std::string_view const &... _Strings>
//const std::string join<_Strings...>::arr=joinFunc(_Strings...);
//template<std::string_view const &... _Strings>
//const std::string_view join<_Strings...>::value={join<_Strings...>::arr.data(), join<_Strings...>::arr.size() - 1};

}//namespace detail

template<std::string_view const &... _Strings>
std::string_view join = detail::join<_Strings...>::value;

#define JoinConstExpr

#else

namespace detail
{

template<std::string_view const &... _Strings>
struct join
{
    static constexpr int len=(_Strings.size()+...+0);
    using StringArray=std::array<char, len+1>;

    static constexpr void append(StringArray &local, size_t &i, std::string_view const &s)
    { 
        for(std::string_view::value_type c:s)
            local[i++]=c;
    };

    static constexpr StringArray impl() noexcept
    {
        StringArray local{};

        size_t i=0;
        (append(local, i, _Strings), ...);
        local[len]=0;
        return local;
    }

    static constexpr StringArray arr=impl();
    static constexpr std::string_view value{arr.data(), arr.size() - 1};
};

}//namespace detail

template<std::string_view const &... _Strings>
static constexpr std::string_view join = detail::join<_Strings...>::value;

#define JoinConstExpr constexpr

#endif



//! Get the number of items in a static array.
//! @param A : The array.
#define GWENI_ARRAY_COUNT(A) (sizeof(A)/sizeof(A[0]))

GWENI_EXPORT int vsnprintf(char *_str, size_t _count, const char *_format, va_list _argList);
GWENI_EXPORT int vsnwprintf(wchar_t *_str, size_t _count, const wchar_t *_format, va_list _argList);
GWENI_EXPORT int snprintf(char *_str, size_t _count, const char *_format, ...);
GWENI_EXPORT int swnprintf(wchar_t *_out, size_t _count, const wchar_t *_format, ...);

GWENI_EXPORT void printfVargs(String &_out, const char *_format, va_list _argList);
GWENI_EXPORT void printf(String &_out, const char *_format, ...);
GWENI_EXPORT String format(const char *fmt, ...);

GWENI_EXPORT std::wstring widen(const String &nstr);
GWENI_EXPORT String narrow(const std::wstring &wstr);

template <typename TYPE>
inline String toString(const TYPE &object)
{
    std::ostringstream os;
    os << object;
    return os.str();
}

GWENI_EXPORT void replace(String &str, const String &strFind, const String &strReplace);


template<class T, class U>
static inline std::shared_ptr<T> dynamic_pointer_cast(const std::shared_ptr<U> &r) noexcept
{
    if(auto p=dynamic_cast<typename std::shared_ptr<T>::element_type *>(r.get()))
        return std::shared_ptr<T>(r, p);
    else
        return std::shared_ptr<T>();
}

typedef std::vector<String> StringList;

GWENI_EXPORT void split(const String &str, const String &seperator,
    StringList &outbits, bool bLeaveSeperators=false);
GWENI_EXPORT bool wildcard(const String &strWildcard,
    const String &strHaystack);

GWENI_EXPORT void toUpper(String &str);
GWENI_EXPORT void strip(String &str, const String &chars);

template <typename T>
T trimLeft(const T &str, const T &strChars)
{
    T outstr=str;
    outstr.erase(0, outstr.find_first_not_of(strChars));
    return outstr;
}

//! \brief Widen a UTF8 code to a wide encoding.
//!
//! Source: https://stackoverflow.com/a/148766/3233
//
static inline wchar_t utf8_to_wchart(char *&in)
{
#if defined(__clang__) && defined(__apple_build_version__) && __apple_build_version__ >= 8
    // thread_local not supported on Xcode versions below 8
    static unsigned int next=0x10000;
#else
    thread_local static unsigned int next=0x10000;
#endif
    if(next != 0x10000)
    {
        wchar_t ret=static_cast<wchar_t>(next);
        next=0x10000;
        return ret;
    }
    unsigned int codepoint;

    while(*in != 0)
    {
        unsigned char ch=static_cast<unsigned char>(*in);
        if(ch <= 0x7f)
            codepoint=ch;
        else if(ch <= 0xbf)
            codepoint=(codepoint << 6) | (ch & 0x3f);
        else if(ch <= 0xdf)
            codepoint=ch & 0x1f;
        else if(ch <= 0xef)
            codepoint=ch & 0x0f;
        else
            codepoint=ch & 0x07;
        ++in;
        if(((*in & 0xc0) != 0x80) && (codepoint <= 0x10ffff))
        {
            if(sizeof(wchar_t) > 2)
                return static_cast<wchar_t>(codepoint);
            else if(codepoint > 0xffff)
            {
                next=static_cast<wchar_t>(0xdc00 + (codepoint & 0x03ff));
                return static_cast<wchar_t>(0xd800 + (codepoint >> 10));
            }
            else if(codepoint < 0xd800 || codepoint >= 0xe000)
                return static_cast<wchar_t>(codepoint);
        }
    }
    return 0;
}

GWENI_EXPORT bool toBool(const String &str);
GWENI_EXPORT int toInt(const String &str);
GWENI_EXPORT float toFloat(const String &str);
GWENI_EXPORT float toFloat(const String &str);
GWENI_EXPORT bool toFloats(const String &str, float *f, size_t count);

template <typename T>
inline T clamp(T current, T vmin, T vmax)
{
    return current < vmin?vmin:(current > vmax?vmax:current);
}

}//namespace gweni

#endif //_gweni_utility_h_
