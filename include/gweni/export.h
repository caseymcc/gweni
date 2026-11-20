#ifndef _gweni_export_h_
#define _gweni_export_h_

// A tree that builds the Python module (GWENI_BUILD_PYTHON) compiles everything
// position-independent and hidden (CMakeLists.txt). -fvisibility=hidden leaves
// declarations as they are, so a class's static data (Canvas::s_layingOut) would
// still be read through the GOT; declared hidden it is read directly, as under
// the executables' -fPIE. gweni is static, so nothing needs these symbols
// exported; a consumer that does not define GWENI_HIDDEN_SYMBOLS links them the same.
#if defined(GWENI_HIDDEN_SYMBOLS) && defined(__GNUC__)
#  define GWENI_EXPORT __attribute__((visibility("hidden")))
#  define GWENI_NO_EXPORT __attribute__((visibility("hidden")))
#elif defined(GWENI_STATIC_DEFINE)
#  define GWENI_EXPORT
#  define GWENI_NO_EXPORT
#else
#   define GWENI_EXPORT 
#  ifndef GWENI_EXPORT
#    ifdef GWENI_EXPORTS
// We are building this library
#      define GWENI_EXPORT __declspec(dllexport)
#    else
// We are using this library
#      define GWENI_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef GWENI_NO_EXPORT
#    define GWENI_NO_EXPORT 
#  endif
#endif

#ifndef GWENI_DEPRECATED
#  define GWENI_DEPRECATED __declspec(deprecated)
#endif

#ifndef GWENI_DEPRECATED_EXPORT
#  define GWENI_DEPRECATED_EXPORT GWENI_EXPORT GWENI_DEPRECATED
#endif

#ifndef GWENI_DEPRECATED_NO_EXPORT
#  define GWENI_DEPRECATED_NO_EXPORT GWENI_NO_EXPORT GWENI_DEPRECATED
#endif

#if 0 // DEFINE_NO_DEPRECATED
#  ifndef GWENI_NO_DEPRECATED
#    define GWENI_NO_DEPRECATED
#  endif
#endif

#endif //_gweni_export_h_
