/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_bench: the phases a frame is split into, the allocation counter, and
 *  the scene interface. A scene builds its tree in code and drives the input
 *  or edits of each frame; the frame loop (main.cpp) measures it.
 */
#ifndef _gweni_bench_bench_h_
#define _gweni_bench_bench_h_

#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace gweni
{
namespace controls
{
class Canvas;
}

namespace bench
{

//-----------------------------------------------------------------------------
// Phases. Every measured instant belongs to exactly one; enterPhase() is the
// only place a phase starts or ends, so it is where a profiler hook goes.
//-----------------------------------------------------------------------------
enum class Phase
{
    None,       //!< not measured: setup, glClear, reading the counters
    Construct,  //!< Scene::build(): creating the tree, before the first frame
    Input,      //!< Scene::step(): the frame's input events or edits
    Update,     //!< Canvas::updateCanvas()
    Render,     //!< Canvas::renderCanvas() without the renderer's end()
    Flush,      //!< OpenGLCore::end(), which is OpenGLCore::flush()
    Gpu,        //!< glFinish() after the frame
    Count
};

//! The phase's name in the output: "construct", "input", "update", ...
const char *phaseName(Phase phase);

//! Ends the current phase, charging its time and allocations to it, and starts
//! \a phase.
void enterPhase(Phase phase);

//! Starts \a phase (enterPhase) and runs \a body in it, inside \a phase's own
//! marker function, so a profile attributes the body's whole cost to the
//! phase. The phase runs on until the next enterPhase() or inPhase().
template<typename Body>
void inPhase(Phase phase, Body &&body);

//! What the phases cost since the last resetPhases().
struct PhaseTotals
{
    double us[size_t(Phase::Count)]={};
    size_t allocs[size_t(Phase::Count)]={};
    size_t allocBytes[size_t(Phase::Count)]={};
};

//! Clears the totals; the current phase restarts now.
void resetPhases();
const PhaseTotals &phaseTotals();

//-----------------------------------------------------------------------------
// Profiling. Under callgrind (tools/profile.sh, which starts it with
// --instr-atstart=no --collect-atstart=no), the measured work is instrumented
// and only the phases are collected, both switched with client requests
// (bench/callgrind.h):
// setCollecting() starts instrumentation for the build scene's construction
// and for the measured frames and stops it after them, and enterPhase()
// collects while a phase other than None runs. So setup, EGL and GL init,
// resource loading, the primer, warm-up frames, glClear, the bench's own
// bookkeeping and teardown are never in the profile. Collection alone would
// keep their costs out but still count their calls at call sites the frames
// share (a "(25x)" for 20 frames); uninstrumented they run at nearly native
// speed too. Starting instrumentation discards valgrind's translations, which
// is why it is switched only around those two stretches, and collection per
// phase. Outside valgrind the requests do nothing.
//
// Each phase's work also runs inside a marker function of its own,
// gweni_bench_phase_<name> (inPhase()), and the per-phase instruction counts
// are those functions' inclusive costs, read from the call costs callgrind
// records for them (tools/callgrind_phases.py): exact, not a guess from
// gweni's function names. One collection over the whole run keeps one
// profile for kcachegrind, callgrind_annotate and gprof2dot; dumping a part
// per phase (CALLGRIND_DUMP_STATS_AT) would give the totals too, but as
// hundreds of parts that callgrind_annotate and gprof2dot do not merge. The
// flush runs inside the render walk (the renderer's end()), so the render
// phase is its marker's cost minus the flush marker's.
//-----------------------------------------------------------------------------

//! Whether the measured phases are instrumented and collected from now on
//! (off at start).
void setCollecting(bool collecting);

//! Runs body(context) in \a phase's marker function.
void callPhaseMarker(Phase phase, void (*body)(void *), void *context);

template<typename Body>
void inPhase(Phase phase, Body &&body)
{
    typedef typename std::remove_reference<Body>::type BodyType;

    enterPhase(phase);
    callPhaseMarker(phase, [](void *context){ (*static_cast<BodyType *>(context))(); },
        const_cast<void *>(static_cast<const void *>(&body)));
}

//-----------------------------------------------------------------------------
// Allocations: the bench replaces the global operator new, so this counts every
// C++ allocation made on the counting thread (gweni, the vendored libraries'
// C++ parts, Mesa's LLVM compiling a shader there), not C malloc
// (freetype-gl, the rest of Mesa) and nothing on Mesa's own threads.
//-----------------------------------------------------------------------------

//! Counts the calling thread's allocations from now on, and no other thread's.
void countAllocationsOnThisThread();
size_t allocCount();
size_t allocBytes();

//-----------------------------------------------------------------------------
// Scenes
//-----------------------------------------------------------------------------

//! What a scene gets to build and drive the canvas with.
struct SceneContext
{
    controls::Canvas *canvas=nullptr;
    Point size;         //!< the canvas, logical units
    int n=0;            //!< --n
    int k=0;            //!< --k
    int frames=0;       //!< measured frames (--frames)

    //! Makes the canvas \a size logical units at \a scale, with the offscreen
    //! target and the renderer's view that size in device pixels, as a
    //! window's framebuffer-size and content-scale callbacks do. \return false
    //! if the target could not be made. Null on the primer canvas.
    std::function<bool(const Point &size, float scale)> setTarget;
};

class Scene
{
public:
    virtual ~Scene() {}

    //! Create the tree on ctx.canvas. Timed as the construct phase, before the
    //! first frame.
    virtual void build(SceneContext &ctx)=0;

    //! The input or edit of one frame, before its updateCanvas(). Warm-up
    //! frames are numbered -warmup..-1, measured frames 0..frames-1.
    virtual void step(SceneContext &/*ctx*/, int /*frame*/) {}
};

//! How a scene's per-frame work is meant to grow with N. A local scene changes
//! a fixed part of the tree each frame, so its per-frame counters must be the
//! same at any N; a linear one touches every control by design (the first
//! frame, the whole-tree z renumber, a relayout of everything, every row of a
//! scrolled list moving).
enum class Scaling
{
    Local,
    Linear
};

//! "local" or "linear", as --list and the summary print it.
const char *scalingName(Scaling scaling);

struct SceneInfo
{
    const char *name;
    const char *description;
    bool takesN;            //!< whether --n changes the tree
    int defaultN;
    int defaultWarmup;      //!< frames run and not reported before the first measured one
    int defaultFrames;      //!< measured frames
    Scaling scaling;
    std::function<std::unique_ptr<Scene>()> make;
    //! Whether Scene::build() is measured work, not setup: profiled as the
    //! construct phase (only the build scene)
    bool profileConstruct=false;
};

//! Every scene, in --list order.
const std::vector<SceneInfo> &scenes();

}//namespace bench
}//namespace gweni

#endif//_gweni_bench_bench_h_
