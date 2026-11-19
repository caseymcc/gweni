/*
 *  Gweni - gweni_app's one-at-a-time rule (not a public header): one
 *  app::Window or app::Headless alive in a process, since the clock, the
 *  input handler's state and GLFW are the process's.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_app_instance_h_
#define _gweni_app_instance_h_

#include <string>

namespace gweni
{
namespace app
{
namespace internal
{

//! Claims the process's one instance for \a who ("app::Window"). \return
//! false, with \a error set and printed on stderr, when another holds it.
bool claimInstance(const char *who, std::string &error);
//! Gives the claim back.
void releaseInstance();

}//namespace internal
}//namespace app
}//namespace gweni

#endif//_gweni_app_instance_h_
