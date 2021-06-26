# imglib - This is a work in progress (WIP)

imglib is a simple image library based on templates. All of the functions included work on your data types with a little bit of template vodoo.

Expected features (WIP)
- IO
- color convertion
- resize
- opengl/opencl/cuda support
- equality testing

imglib works with a concept of an image. It expects images to have a location (system memory, cuda, opencl, etc...), format (RGB, RGBA, etc...), bit depth of channels (8bit, 16bit, etc...), width, stride (line lenght if padded), height, data (buffer) and data size. There is a SimpleImage class in the library that can be used but it is really setup to use whatever image class you already have defined.

To get imglib to use your data structure you need to define the following im the imglib namespace.

````
namespace imglib
{
  template<>
  Format format<{YourType}>(const _Image &){return Format::Unknown;}

  template<>
  Depth depth<{YourType}>(const _Image &){return Depth::Unknown;}
  
  template<>
  Location location<{YourType}>(const _Image &){return Location::System;}
  
  template<>
  size_t width<{YourType}>(const _Image &){return 0;}
  
  template<>
  size_t height<{YourType}>(const _Image &){return 0;}
  
  template<>
  size_t stride<{YourType}>(const _Image &){return 0;}
  
  template<>
  size_t nativeId<{YourType}>(const _Image &){return 0;}

  template<>
  uint8_t *data<{YourType}>(_Image &){return nullptr;}
  
  template<>
  size_t dataSize<{YourType}>(const _Image &){return 0;}

  template<>
  bool resize<{YourType}>(_Image &image, Format format, Depth depth, size_t width, size_t height){return false;}
}
````
An example of how to do this can be seen with the `SimpleImage` in the simpleImage.h file.

Once the above functions are defined you can make any calls to the library, like
````
#include <imglib/simpleImage.h>
#include <imglib/imageIo.h>

SimpleImage image;

imglib::load(image, "test.jpg");
````
