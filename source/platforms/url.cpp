/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The URL helpers of platform.h: splitting and joining a path, query and
 *  hash, percent-encoding a query and a path's segments, and the three forms
 *  pushUrl()/replaceUrl() take. Plain string functions, the same on every
 *  target; the history they feed is in crossPlatform.cpp.
 */
#include "urlTarget.h"

namespace gweni
{
namespace platform
{

namespace
{

int hexValue(char c)
{
    if(c >= '0' && c <= '9')
        return c-'0';
    if(c >= 'a' && c <= 'f')
        return c-'a'+10;
    if(c >= 'A' && c <= 'F')
        return c-'A'+10;
    return -1;
}

//! %XX is the byte XX; a '%' not followed by two hex digits stays as it is,
//! as a browser leaves it. With plusIsSpace (a query) '+' is a space.
String percentDecode(const String &s, bool plusIsSpace)
{
    String out;

    out.reserve(s.size());
    for(size_t i=0; i < s.size(); ++i)
    {
        const char c=s[i];

        if(c == '%' && i+2 < s.size())
        {
            const int high=hexValue(s[i+1]);
            const int low=hexValue(s[i+2]);

            if(high >= 0 && low >= 0)
            {
                out+=static_cast<char>((high << 4)|low);
                i+=2;
                continue;
            }
        }
        out+=(plusIsSpace && c == '+')?' ':c;
    }
    return out;
}

bool isUnreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '.' || c == '_' || c == '~';
}

//! Kept in a path segment besides the unreserved bytes: RFC 3986's sub-delims
//! and ':' '@', none of which a browser encodes in a path. Not '\'' (encoded
//! in a query, so encoded everywhere for one rule).
bool isPathSafe(unsigned char c)
{
    switch(c)
    {
    case '!': case '$': case '&': case '(': case ')': case '*': case '+':
    case ',': case ';': case '=': case ':': case '@':
        return true;
    default:
        return isUnreserved(c);
    }
}

void appendEscaped(String &out, unsigned char c)
{
    static const char digits[]="0123456789ABCDEF";

    out+='%';
    out+=digits[c >> 4];
    out+=digits[c & 0xf];
}

String encodeQueryPart(const String &s)
{
    String out;

    out.reserve(s.size());
    for(const char ch:s)
    {
        const unsigned char c=static_cast<unsigned char>(ch);

        if(c == ' ')
            out+='+';
        else if(isUnreserved(c))
            out+=ch;
        else
            appendEscaped(out, c);
    }
    return out;
}

String encodeSegment(const String &s)
{
    String out;

    out.reserve(s.size());
    for(const char ch:s)
    {
        const unsigned char c=static_cast<unsigned char>(ch);

        if(isPathSafe(c))
            out+=ch;
        else
            appendEscaped(out, c);
    }
    return out;
}

//! The part of a URL a byte is checked in: each has the bytes a browser
//! percent-encodes there itself.
enum class UrlPart
{
    Path,
    Query,
    Hash
};

//! A byte a browser would percent-encode itself in that part of a URL, so the
//! URL it shows would not be the one the app pushed: WHATWG's path, query
//! (special scheme) and fragment sets, and Chromium's additions to the path
//! set ('\' '^' '|'), as Chromium's location shows them after
//! history.replaceState. What location gives back in a part is never one of
//! them, so currentUrl()'s parts can be pushed again as they are.
bool browserEncodes(unsigned char c, UrlPart part)
{
    if(c <= 0x20 || c >= 0x7f)
        return true;
    switch(c)
    {
    case '"': case '<': case '>':
        return true;
    case '`':
        return part != UrlPart::Query;
    case '\\': case '^': case '{': case '|': case '}':
        return part == UrlPart::Path;
    case '\'':
        return part == UrlPart::Query;
    default:
        return false;
    }
}

//! Whether s has a byte the browser would encode in part.
bool hasEncoded(const String &s, UrlPart part)
{
    for(const char ch:s)
    {
        if(browserEncodes(static_cast<unsigned char>(ch), part))
            return true;
    }
    return false;
}

}//namespace

Url parseUrl(const String &url)
{
    Url result;
    String rest=url;
    const size_t hash=rest.find('#');

    if(hash != String::npos)
    {
        result.hash=rest.substr(hash+1);
        rest.resize(hash);
    }

    const size_t query=rest.find('?');

    if(query != String::npos)
    {
        result.query=rest.substr(query+1);
        rest.resize(query);
    }
    result.path=rest;
    return result;
}

String formatUrl(const Url &url)
{
    String out=url.path;

    if(!url.query.empty())
        out+="?"+url.query;
    if(!url.hash.empty())
        out+="#"+url.hash;
    return out;
}

QueryParams parseQuery(const String &query)
{
    QueryParams params;
    size_t start=(!query.empty() && query[0] == '?')?1:0;

    while(start <= query.size())
    {
        size_t end=query.find('&', start);

        if(end == String::npos)
            end=query.size();

        const String piece=query.substr(start, end-start);

        if(!piece.empty())
        {
            const size_t equals=piece.find('=');

            if(equals == String::npos)
                params.emplace_back(percentDecode(piece, true), String());
            else
                params.emplace_back(percentDecode(piece.substr(0, equals), true),
                    percentDecode(piece.substr(equals+1), true));
        }
        start=end+1;
    }
    return params;
}

String buildQuery(const QueryParams &params)
{
    String out;

    for(const auto &param:params)
    {
        if(!out.empty())
            out+='&';
        out+=encodeQueryPart(param.first);
        // a flag ("?stats-overlay") stays one
        if(!param.second.empty() || param.first.empty())
        {
            out+='=';
            out+=encodeQueryPart(param.second);
        }
    }
    return out;
}

std::vector<String> splitPath(const String &path)
{
    std::vector<String> segments;
    const size_t begin=(!path.empty() && path[0] == '/')?1:0;

    if(begin >= path.size())
        return segments;

    size_t start=begin;

    while(true)
    {
        const size_t end=path.find('/', start);

        if(end == String::npos)
        {
            segments.push_back(percentDecode(path.substr(start), false));
            break;
        }
        segments.push_back(percentDecode(path.substr(start, end-start), false));
        start=end+1;
    }
    return segments;
}

String joinPath(const std::vector<String> &segments)
{
    if(segments.empty())
        return "/";

    String out;

    for(const String &segment:segments)
    {
        out+='/';
        out+=encodeSegment(segment);
    }
    return out;
}

namespace detail
{

bool resolveUrlTarget(const Url &current, const String &target, Url &resultOut,
    String &whyOut)
{
    if(target.empty())
    {
        whyOut="empty; it takes /path, ?query or #hash";
        return false;
    }

    const char form=target[0];

    if(form != '/' && form != '?' && form != '#')
    {
        whyOut="not /path, ?query or #hash (no relative path, scheme or host)";
        return false;
    }
    if(target.compare(0, 2, "//") == 0)
    {
        whyOut="'//' starts a host; it takes /path, ?query or #hash";
        return false;
    }

    const Url parsed=parseUrl(target);

    const struct
    {
        const String &text;
        UrlPart part;
        const char *name;
    } parts[]={
        {parsed.path, UrlPart::Path, "path"},
        {parsed.query, UrlPart::Query, "query"},
        {parsed.hash, UrlPart::Hash, "hash"}
    };

    for(const auto &part:parts)
    {
        if(hasEncoded(part.text, part.part))
        {
            whyOut=String("has a character the browser would encode in the ")+part.name
                +"; percent-encode it";
            return false;
        }
    }

    switch(form)
    {
    case '/':
        for(const String &segment:splitPath(parsed.path))
        {
            if(segment == "." || segment == "..")
            {
                whyOut="has a '.' or '..' path segment";
                return false;
            }
        }
        resultOut=parsed;
        break;

    case '?':
        resultOut.path=current.path;
        resultOut.query=parsed.query;
        resultOut.hash=parsed.hash;
        break;

    default:    // '#'
        resultOut.path=current.path;
        resultOut.query=current.query;
        resultOut.hash=target.substr(1);
        break;
    }
    return true;
}

}//namespace detail

}//namespace platform
}//namespace gweni
