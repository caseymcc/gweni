/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_bench's phase clock: enterPhase() charges the time and allocations
 *  since the last call to the phase that was running.
 */
#include "bench.h"

#include <chrono>

// The callgrind client requests, when valgrind's headers are there (the image
// has them; GWENI_BENCH_NO_VALGRIND builds as if they were not). The requests
// are a few no-op instructions outside valgrind.
#if defined(__has_include) && !defined(GWENI_BENCH_NO_VALGRIND)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#define GWENI_BENCH_CALLGRIND 1
#endif
#endif

#ifndef GWENI_BENCH_CALLGRIND
#define CALLGRIND_TOGGLE_COLLECT do {} while(0)
#endif

// The phases' marker functions (bench.h, "Profiling"): C names, so a profile
// shows them as gweni_bench_phase_<name> and tools/callgrind_phases.py finds
// them by that name. noinline, and each is its own function, so callgrind
// records a call to it with the body's whole inclusive cost.
extern "C"
{
#define GWENI_BENCH_MARKER(name) \
    __attribute__((noinline)) void gweni_bench_phase_##name(void (*body)(void *), void *context) \
    { \
        body(context); \
        __asm__ __volatile__("" ::: "memory"); \
    }

GWENI_BENCH_MARKER(construct)
GWENI_BENCH_MARKER(input)
GWENI_BENCH_MARKER(update)
GWENI_BENCH_MARKER(render)
GWENI_BENCH_MARKER(flush)
GWENI_BENCH_MARKER(gpu)

#undef GWENI_BENCH_MARKER
}

namespace gweni
{
namespace bench
{

namespace
{

typedef std::chrono::steady_clock Clock;

PhaseTotals g_totals;
Phase g_phase=Phase::None;
Clock::time_point g_phaseStart=Clock::now();
size_t g_phaseAllocs=0;
size_t g_phaseAllocBytes=0;

bool g_collectionWanted=false;  // setCollecting()
bool g_collecting=false;        // whether callgrind collects now

// Collects while it is wanted and a measured phase runs.
void syncCollection()
{
    const bool collect=g_collectionWanted && g_phase != Phase::None;
    if(collect != g_collecting)
    {
        CALLGRIND_TOGGLE_COLLECT;
        g_collecting=collect;
    }
}

// The bench's own bookkeeping is never collected.
void pauseCollection()
{
    if(g_collecting)
    {
        CALLGRIND_TOGGLE_COLLECT;
        g_collecting=false;
    }
}

void startPhase(Phase phase)
{
    g_phase=phase;
    g_phaseAllocs=allocCount();
    g_phaseAllocBytes=allocBytes();
    g_phaseStart=Clock::now();
}

}//namespace

const char *phaseName(Phase phase)
{
    switch(phase)
    {
    case Phase::None:      return "none";
    case Phase::Construct: return "construct";
    case Phase::Input:     return "input";
    case Phase::Update:    return "update";
    case Phase::Render:    return "render";
    case Phase::Flush:     return "flush";
    case Phase::Gpu:       return "gpu";
    case Phase::Count:     break;
    }
    return "?";
}

void enterPhase(Phase phase)
{
    pauseCollection();

    // the clock first, so reading the counters is not charged to the phase
    const Clock::time_point now=Clock::now();
    const size_t index=size_t(g_phase);

    g_totals.us[index]+=std::chrono::duration<double, std::micro>(now-g_phaseStart).count();
    g_totals.allocs[index]+=allocCount()-g_phaseAllocs;
    g_totals.allocBytes[index]+=allocBytes()-g_phaseAllocBytes;

    startPhase(phase);
    syncCollection();
}

void resetPhases()
{
    pauseCollection();
    g_totals=PhaseTotals();
    startPhase(g_phase);
    syncCollection();
}

void setCollecting(bool collecting)
{
    g_collectionWanted=collecting;
    syncCollection();
}

void callPhaseMarker(Phase phase, void (*body)(void *), void *context)
{
    switch(phase)
    {
    case Phase::Construct: gweni_bench_phase_construct(body, context); return;
    case Phase::Input:     gweni_bench_phase_input(body, context); return;
    case Phase::Update:    gweni_bench_phase_update(body, context); return;
    case Phase::Render:    gweni_bench_phase_render(body, context); return;
    case Phase::Flush:     gweni_bench_phase_flush(body, context); return;
    case Phase::Gpu:       gweni_bench_phase_gpu(body, context); return;
    case Phase::None:
    case Phase::Count:     break;
    }
    body(context);
}

const PhaseTotals &phaseTotals()
{
    return g_totals;
}

}//namespace bench
}//namespace gweni
