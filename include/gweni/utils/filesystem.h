#ifndef _gweni_filesystem_h_
#define _gweni_filesystem_h_

#include <string>
#include <stdio.h>

#if GWENI_USE_FILESYSTEM == GWENI_STD_FILESYSTEM

#include <filesystem>
namespace fs=std::filesystem;

namespace std
{
namespace filesystem
{
#elif GWENI_USE_FILESYSTEM == DGWENI_EXPERIMENTAL_FILESYSTEM

#include <experimental/filesystem>
namespace fs=std::experimental::filesystem;

namespace std
{
namespace experimental
{
namespace filesystem
{
#else //GWENI_BOOST

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace boost
{
namespace filesystem
{
#endif

inline bool isValid(std::string &fileName)
{
    if(fileName.empty())
        return false;

    if(!exists(fileName))
        return false;

    return true;
}

#if GWENI_USE_FILESYSTEM == GWENI_STD_FILESYSTEM
}}
#elif GWENI_USE_FILESYSTEM == DGWENI_EXPERIMENTAL_FILESYSTEM
}}}
#else //GWENI_BOOST
}}
#endif

namespace gweni
{

inline std::string loadFileToString(std::string &fileName)
{
    FILE *file=fopen(fileName.c_str(), "r");
    std::string stringBuffer;
    
    fseek(file, 0, SEEK_END);
    size_t fileSize=(size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    stringBuffer.resize(fileSize);
    fread(&stringBuffer[0], 1, fileSize, file);
//    stringBuffer.back()='\0';

    return stringBuffer;
}

}//namespace gweni

#endif//_gweni_filesystem_h_