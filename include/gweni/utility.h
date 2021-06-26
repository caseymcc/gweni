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

 // Some platforms (e.g. Windows) define these. We want std::min/max.
#ifdef min
#   undef min
#endif
#ifdef max
#   undef max
#endif

namespace gweni
{

namespace detail
{



//template<std::string_view const &... _Strings>
//struct join
//{
//    static constexpr auto impl() noexcept
//    {
//        constexpr std::size_t len=(_Strings.size()+...+0);
//        std::array<char, len+1> arr{};
//
//        auto append = [i=0, &arr](auto const &s) mutable
//        {
//            for(auto c:s)
//                arr[i++]=c;
//        };
//
//        (append(_Strings), ...);
//        arr[len]=0;
//        return arr;
//    }
//
//    static constexpr auto arr = impl();
//    static constexpr std::string_view value {arr.data(), arr.size() - 1};
//};

}//namespace detail

//template<std::string_view const &... _Strings>
//static constexpr auto join = detail::join<_Strings...>::value;

template<size_t _size>
struct StringRet
{
    char value[_size];
};

template<size_t ..._sizes>
constexpr auto join(const char (&..._strings)[_sizes])
{
    constexpr size_t length=(...+_sizes)-sizeof...(_sizes);
    StringRet result={};
    size_t srcIndex;
    size_t dstIndex=0;
    const char *dst=result.value;

    for(const char *src:{_strings...})
    {
        srcIndex=0;
        for(; src[srcIndex]!='\0'; ++srcIndex, ++dstIndex)
        {
            dst[dstIndex]=src[srcIndex];
        }
    }
    dst[length] = '\0';

    return result;
}


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
