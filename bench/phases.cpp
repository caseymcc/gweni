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
    // the clock first, so reading the counters is not charged to the phase
    const Clock::time_point now=Clock::now();
    const size_t index=size_t(g_phase);

    g_totals.us[index]+=std::chrono::duration<double, std::micro>(now-g_phaseStart).count();
    g_totals.allocs[index]+=allocCount()-g_phaseAllocs;
    g_totals.allocBytes[index]+=allocBytes()-g_phaseAllocBytes;

    startPhase(phase);
}

void resetPhases()
{
    g_totals=PhaseTotals();
    startPhase(g_phase);
}

const PhaseTotals &phaseTotals()
{
    return g_totals;
}

}//namespace bench
}//namespace gweni
