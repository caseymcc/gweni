#include "imglib/filesystem.h"
#include "imglib_filesystem.h"

namespace imglib
{

//used to hide boost::filesystem access
std::string getExt(char *filename)
{
    fs::path filePath(filename);

    return filePath.extension().string();
}

}//namespace imglib

