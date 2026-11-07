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

struct SceneInfo
{
    const char *name;
    const char *description;
    bool takesN;            //!< whether --n changes the tree
    int defaultN;
    int defaultWarmup;      //!< frames run and not reported before the first measured one
    int defaultFrames;      //!< measured frames
    std::function<std::unique_ptr<Scene>()> make;
};

//! Every scene, in --list order.
const std::vector<SceneInfo> &scenes();

}//namespace bench
}//namespace gweni

#endif//_gweni_bench_bench_h_
