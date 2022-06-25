/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in Gweni.h
 */

#ifndef _gweni_config_h_
#define _gweni_config_h_

 // Enable animation functions.
#define GWENI_ANIMATE 1

#if GWENI_ALLOC_STATS
#   include <memory>
void *operator new(std::size_t size) throw(std::bad_alloc);
void operator delete(void *mem) throw();
#   define GWENI_IF_ALLOC_STATS(SRC) SRC
#else
#   define GWENI_IF_ALLOC_STATS(SRC) // ignore
#endif // GWENI_ALLOC_STATS

#endif //_gweni_config_h_
