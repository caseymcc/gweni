/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The callgrind client requests gweni_bench uses, when valgrind's headers are
 *  there (the image has them; -DGWENI_BENCH_NO_VALGRIND builds as if they were
 *  not, and then every request below does nothing). Outside valgrind the
 *  requests are a few no-op instructions.
 */
#ifndef _gweni_bench_callgrind_h_
#define _gweni_bench_callgrind_h_

#if defined(__has_include) && !defined(GWENI_BENCH_NO_VALGRIND)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#define GWENI_BENCH_CALLGRIND 1
#endif
#endif

#ifndef GWENI_BENCH_CALLGRIND
#define CALLGRIND_TOGGLE_COLLECT do {} while(0)
#define CALLGRIND_START_INSTRUMENTATION do {} while(0)
#define CALLGRIND_STOP_INSTRUMENTATION do {} while(0)
#define RUNNING_ON_VALGRIND 0
#endif

#endif//_gweni_bench_callgrind_h_
