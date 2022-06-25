#ifndef _gweni_export_h_
#define _gweni_export_h_

#ifdef GWENI_STATIC_DEFINE
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
