/*  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */
 /*! \file commonPlatform.h */



#ifndef _gweni_platform_common_h_
#define _gweni_platform_common_h_

#include <gweni/platforms/platformTypes.h>
#include <vector>

namespace gweni
{
namespace platform
{

// Cross-platform, low level functionality shared by all of the platforms.

class CurrentPath:public ResourcePaths
{
public:
    explicit CurrentPath(String const &resourceDir=String());

    String getPath(Type type, String const &filename) final;

private:
    String m_resDir;
};

//! Calculate resource paths relative to the executable.
//! I.e. path = exe_dir + resource_dir + relative_path.
class RelativeToExecutablePaths: public ResourcePaths
{
    String m_resDir;
public:
    //! Constructor
    //! \param resourceDir : Optional directory, relative to the executable.
    explicit RelativeToExecutablePaths(String const &resourceDir=String());

    //! Get full path to file.
    //! \param type : File resource type.
    //! \param relPath : Relative path to file.
    //! \return Path relative to the resource directory.
    String getPath(Type type, String const &relPath) final;
};

#if GWENI_ALLOC_STATS

struct AllocStats
{
    std::size_t currentAllocBytes=0;
    std::size_t currentNumAllocs=0;
    std::size_t cumulativeAllocBytes=0;
    std::size_t cumulativeNumAllocs=0;
};

//! Get the current allocation stats. Only active if GWENI_ALLOC_STATS on.
const AllocStats &getAllocStats();

void AllocStatsAddMark(const char *const name);
void AllocStatsDump(FILE *fh=stdout);

#endif // GWENI_ALLOC_STATS

}//namespace platform

namespace log
{

enum class Level    //!< Logging classification.
{
    Info,       //!< Informational.
    Warning,    //!< A warning.
    Error,      //!< An error.
    Fatal       //!< Whoa! Fatal error!
};

//! Write a message to the platform log.
//! \param lvl : The level of the log.
//! \param format : A message, using printf() style formatting.
void write(Level lvl, const char *format, ...);

typedef void (*LogCaller)(Level, const char *);

//! Set where logs messages are sent to.
//! \param caller : The new caller.
//! \return The current log caller.
LogCaller setLogCaller(LogCaller caller);

// Do not use this! Use gweni::log::write()
// Needs to be implemented by the platform.
void defaultLogCaller(Level lvl, const char *message);

}//namespace log

namespace debug
{

//! \def GWENI_ASSERT(TEST)
//! Assertion check.
//! If fails, application aborts.
//! \param TEST : The Boolean assertion check.
#define GWENI_ASSERT(TEST) if (!(TEST)) gweni::debug::assertFail(#TEST);

//! \def GWENI_ASSERT_MSG(TEST, MESSAGE)
//! Assertion check.
//! If fails, application logs message and aborts.
//! \param TEST : The Boolean assertion check.
//! \param MESSAGE : The message to output on failure.
#define GWENI_ASSERT_MSG(TEST, MESSAGE) if (!(TEST)) gweni::debug::assertFail(MESSAGE)        

//! Called when an assertion fails.
GWENI_EXPORT void assertFail(const char *message);

}//namespace debug
}//namespace gweni

#endif //_gweni_platform_common_h_
