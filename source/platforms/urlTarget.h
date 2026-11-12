/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What pushUrl()/replaceUrl() make of their argument, shared by every
 *  target's history (crossPlatform.cpp), so the web and the in-process
 *  history take the same forms and refuse the same strings.
 */
#ifndef _gweni_platforms_urlTarget_h_
#define _gweni_platforms_urlTarget_h_

#include <gweni/platforms/platform.h>

namespace gweni
{
namespace platform
{
namespace detail
{

//! The URL target names from current: "/path[?query][#hash]" all three parts,
//! "?query[#hash]" the query and hash, "#hash" the hash. \return False, with
//! why it was refused in whyOut, for anything else (see pushUrl()).
bool resolveUrlTarget(const Url &current, const String &target, Url &resultOut,
    String &whyOut);

}//namespace detail
}//namespace platform
}//namespace gweni

#endif //_gweni_platforms_urlTarget_h_
