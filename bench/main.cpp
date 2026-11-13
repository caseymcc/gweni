/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_bench: runs one scene on the full path (gweni, the textured skin and
 *  OpenGLCore on a surfaceless EGL context, or with --renderer vulkan
 *  renderer::Vulkan on a headless Vulkan device) and prints, per measured
 *  frame, what each phase cost and how much work it did. See --help for the
 *  output.
 *
 *  Every run is deterministic: the scene builds its tree in code, the clock is
 *  stepped 1/60 s per frame (platform::setTimeSource), and the input is the
 *  scene's own. So the same arguments always give the same counters; only the
 *  times vary.
 */
#include "bench.h"

#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/platforms/renderers/openglCore.h>
#ifdef GWENI_HAS_VULKAN
#include <gweni/platforms/renderers/vulkan.h>
#endif
#include <gweni/primitiveIds.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include "capture.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace bench=gweni::bench;
using bench::Phase;

namespace
{

const double g_frameStep=1.0/60.0;
// whole steps, read as steps*(1.0/60) in double
int64_t g_clockSteps=0;

// input::hitTestVisits() when the frame began: hit_tests is the difference.
size_t g_hitTestsAtFrame=0;

double steppedClock()
{
    return double(g_clockSteps)*g_frameStep;
}

//! A renderer (a SlotRenderer backend) with its end(), which is only the core's
//! flush(), as a phase of its own: the render walk calls it last, so render and
//! flush split without touching the renderer.
template<typename Renderer>
class TimedFlush: public Renderer
{
public:
    using Renderer::Renderer;

    void end() override
    {
        bench::inPhase(Phase::Flush, [this]{ Renderer::end(); });
        bench::enterPhase(Phase::Render);
    }
};

//! The renderer the bench times, of \a device's backend, drawing into its target.
gweni::renderer::SlotRenderer *newBenchRenderer(gweni::capture::Device &device, gweni::ResourcePaths &paths,
    const gweni::Rect &viewRect)
{
#ifdef GWENI_HAS_VULKAN
    if(device.backend() == gweni::capture::Backend::Vulkan)
        return new TimedFlush<gweni::renderer::Vulkan>(paths, viewRect, *device.vulkanDevice(),
            *device.vulkanTarget());
#else
    (void)device;
#endif
    return new TimedFlush<gweni::renderer::OpenGLCore>(paths, viewRect);
}

//! What every frame clears the target to, and how it waits for the device (a
//! glFinish, a vkQueueWaitIdle): set once both are made.
gweni::renderer::SlotRenderer *g_renderer=nullptr;
gweni::capture::Device *g_device=nullptr;
gweni::Color g_background;

// The per-frame phases, in output order.
const Phase g_framePhases[]={Phase::Input, Phase::Update, Phase::Render, Phase::Flush, Phase::Gpu};
const size_t g_framePhaseCount=sizeof(g_framePhases)/sizeof(g_framePhases[0]);

// The per-frame counters, in output order. The names are ControlTest's
// printFrameStats keys where they overlap.
const char *const g_counterNames[]=
{
    "visited", "render_carried", "controls_changed", "created", "visibility", "geometry", "background", "text",
    "prim_slots_written", "text_slots_written", "upload_calls", "upload_bytes", "mirror_bytes",
    "draw_calls", "primitive_slots_drawn", "primitive_ranges", "primitive_slots_culled", "text_runs_culled", "text_draws", "atlas_upload_calls", "atlas_upload_bytes", "buffer_regrows",
    "layout_passes", "layout_capped", "render_layout", "layout_visited", "layout_children_scanned",
    "measure_calls", "measure_calls_input", "measure_calls_update", "measure_calls_render",
    "invalidations", "invalidations_input", "invalidations_update", "invalidations_render", "z_visited", "z_changed", "anims_run", "hit_tests", "prim_ids", "text_ids",
    "allocs", "alloc_bytes",
    "allocs_input", "allocs_update", "allocs_render", "allocs_flush", "allocs_gpu"
};
const size_t g_counterCount=sizeof(g_counterNames)/sizeof(g_counterNames[0]);

struct FrameRecord
{
    double us[g_framePhaseCount]={};
    size_t counters[g_counterCount]={};
};

void collectCounters(FrameRecord &record, const bench::PhaseTotals &phases,
    const gweni::renderer::SlotRenderer &renderer, const gweni::controls::Canvas &canvas)
{
    const gweni::controls::ChangeStats &changes=gweni::controls::getChangeStats();
    const gweni::renderer::SlotRenderer::FrameStats &gpu=renderer.getFrameStats();
    const gweni::controls::LayoutStats &layout=canvas.getLayoutStats();
    const gweni::controls::UpdateStats &update=canvas.getUpdateStats();

    size_t allocs=0, allocBytes=0, measures=0, invalidations=0;
    for(size_t i=0; i<g_framePhaseCount; ++i)
    {
        record.us[i]=phases.us[size_t(g_framePhases[i])];
        allocs+=phases.allocs[size_t(g_framePhases[i])];
        allocBytes+=phases.allocBytes[size_t(g_framePhases[i])];
        measures+=phases.measures[size_t(g_framePhases[i])];
        invalidations+=phases.invalidations[size_t(g_framePhases[i])];
    }

    const size_t values[g_counterCount]=
    {
        changes.visited, changes.renderCarried, changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten, gpu.uploadCalls, gpu.uploadBytes,
        gpu.mirrorBytes, gpu.drawCalls, gpu.primitiveSlotsDrawn, gpu.primitiveRanges, gpu.primitiveSlotsCulled, gpu.textRunsCulled, gpu.textDraws, gpu.atlasUploadCalls, gpu.atlasUploadBytes,
        gpu.bufferRegrows,
        size_t(layout.passes), layout.capped ? 1u : 0u, layout.renderLayout ? 1u : 0u,
        layout.visited, layout.childrenScanned, measures, phases.measures[size_t(Phase::Input)],
        phases.measures[size_t(Phase::Update)], phases.measures[size_t(Phase::Render)],
        invalidations, phases.invalidations[size_t(Phase::Input)],
        phases.invalidations[size_t(Phase::Update)], phases.invalidations[size_t(Phase::Render)], update.zVisited, update.zChanged, update.animsRun,
        gweni::input::hitTestVisits()-g_hitTestsAtFrame,
        gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount(),
        gweni::textIdCount()-gweni::releasedTextIdCount(),
        allocs, allocBytes,
        phases.allocs[size_t(Phase::Input)], phases.allocs[size_t(Phase::Update)],
        phases.allocs[size_t(Phase::Render)], phases.allocs[size_t(Phase::Flush)],
        phases.allocs[size_t(Phase::Gpu)]
    };
    std::copy(values, values+g_counterCount, record.counters);
}

size_t countControls(const gweni::controls::Base *control)
{
    size_t count=1;
    for(const gweni::controls::Base *child : control->getActualChildren())
        count+=countControls(child);
    return count;
}

// llvmpipe compiles a shader variant in LLVM at the first draw that needs it,
// and LLVM's allocations differ from run to run (and with Mesa's on-disk
// shader cache). So before the scene is built, a primer canvas holding a few
// of each control the scenes use draws two frames and is deleted: the measured
// frames draw with compiled shaders, and the font atlas is uploaded. Its ids
// go back to the banks and are handed out again, the same way every run.
void primeShaders(gweni::skin::Base *skin, const gweni::Point &size)
{
    gweni::controls::Canvas *primer=new gweni::controls::Canvas(skin);
    primer->setSize(size.x, size.y);

    bench::SceneContext ctx;
    ctx.canvas=primer;
    ctx.size=size;
    ctx.n=40;
    ctx.k=1;

    std::unique_ptr<bench::Scene> scene=bench::scenes().front().make();
    scene->build(ctx);

    for(int frame=0; frame<2; ++frame)
    {
        primer->updateCanvas();
        g_renderer->clearTarget(g_background);
        primer->renderCanvas();
        g_device->finish();
    }

    scene.reset();
    delete primer;
}

// With the clip cull on, a scene can draw an index chunk for the first time in a
// measured frame (a line of a text grown into a chunk all of whose runs were
// culled until then), and llvmpipe compiles what the first draw through a
// chunk's vertex array needs right there, so LLVM's allocations would count in
// that frame. So the scene's first frame is drawn with the cull off, every chunk
// in it, and the whole tree drawn again with it on: two frames before the
// warm-up, with no input and the clock standing, leaving the tree as a first
// frame with the cull on would. Not for a scene measured from its first frame.
// A run with the cull off is primed too (its second frame drawn with it off), so
// --clip-cull on and off have the same frames before the warm-up.
void primeChunks(gweni::controls::Canvas *canvas)
{
    const bool cull=g_renderer->clipCulling();

    for(int frame=0; frame<2; ++frame)
    {
        g_renderer->setClipCulling(cull && (frame == 1));
        if(frame == 1)
            canvas->addStateChangeRecursive(gweni::controls::StateChange_Geometry
                |gweni::controls::StateChange_Background|gweni::controls::StateChange_Text);
        canvas->updateCanvas();
        g_renderer->clearTarget(g_background);
        canvas->renderCanvas();
        g_device->finish();
    }
}

// One frame: the scene's input, the two phases of the canvas and a fenced
// finish, each a phase of its own. The clear belongs to none of them.
void runFrame(bench::Scene &scene, bench::SceneContext &ctx, int frame)
{
    gweni::controls::resetChangeStats();
    g_hitTestsAtFrame=gweni::input::hitTestVisits();
    bench::resetPhases();

    bench::inPhase(Phase::Input, [&]{ scene.step(ctx, frame); });
    bench::inPhase(Phase::Update, [&]{ ctx.canvas->updateCanvas(); });

    bench::enterPhase(Phase::None);
    g_renderer->clearTarget(g_background);

    bench::inPhase(Phase::Render, [&]{ ctx.canvas->renderCanvas(); });
    bench::inPhase(Phase::Gpu, []{ g_device->finish(); });

    bench::enterPhase(Phase::None);
    ++g_clockSteps;
}

//-----------------------------------------------------------------------------
// Output
//-----------------------------------------------------------------------------
struct Summary
{
    std::string scene;
    int n=0;
    int k=0;
    int warmup=0;
    std::string scaling;
    gweni::Point size;
    size_t controls=0;
    double constructUs=0.0;
    size_t constructAllocs=0;
    size_t constructAllocBytes=0;
    size_t constructInvalidations=0;
    std::string renderer;   // the backend, gl or vulkan
    std::string device;     // GL_RENDERER, or the Vulkan device's name
};

// A string as one token: spaces and '=' become '_', so a key=value line splits
// on whitespace.
std::string token(std::string text)
{
    for(char &c : text)
        if(c == ' ' || c == '=' || c == '\t' || c == '"' || c == '\\')
            c='_';
    return text;
}

double median(std::vector<double> values)
{
    if(values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const size_t mid=values.size()/2;
    return (values.size()%2) ? values[mid] : (values[mid-1]+values[mid])/2.0;
}

// The summary's fields as key/value strings, in output order.
std::vector<std::pair<std::string, std::string>> summaryFields(const Summary &summary,
    const std::vector<FrameRecord> &frames)
{
    std::vector<std::pair<std::string, std::string>> fields;
    auto add=[&fields](const std::string &key, const std::string &value){ fields.emplace_back(key, value); };
    auto number=[](double value){ char text[32]; std::snprintf(text, sizeof(text), "%.1f", value); return std::string(text); };

    add("scene", summary.scene);
    add("n", std::to_string(summary.n));
    add("k", std::to_string(summary.k));
    add("frames", std::to_string(frames.size()));
    add("warmup", std::to_string(summary.warmup));
    add("scaling", summary.scaling);
    add("size", std::to_string(summary.size.x)+"x"+std::to_string(summary.size.y));
    add("controls", std::to_string(summary.controls));
    add("construct_us", number(summary.constructUs));
    add("construct_allocs", std::to_string(summary.constructAllocs));
    add("construct_alloc_bytes", std::to_string(summary.constructAllocBytes));
    add("construct_invalidations", std::to_string(summary.constructInvalidations));

    for(size_t p=0; p<g_framePhaseCount; ++p)
    {
        std::vector<double> values;
        for(const FrameRecord &frame : frames)
            values.push_back(frame.us[p]);

        const std::string name=bench::phaseName(g_framePhases[p]);
        add(name+"_us_min", number(values.empty() ? 0.0 : *std::min_element(values.begin(), values.end())));
        add(name+"_us_median", number(median(values)));
        add(name+"_us_max", number(values.empty() ? 0.0 : *std::max_element(values.begin(), values.end())));
    }

    for(size_t c=0; c<g_counterCount; ++c)
    {
        size_t total=0, max=0;
        for(const FrameRecord &frame : frames)
        {
            total+=frame.counters[c];
            max=std::max(max, frame.counters[c]);
        }
        add(std::string(g_counterNames[c])+"_total", std::to_string(total));
        add(std::string(g_counterNames[c])+"_max", std::to_string(max));
    }

    add("renderer", summary.renderer);
    add("device", token(summary.device));
    return fields;
}

void printLines(const Summary &summary, const std::vector<FrameRecord> &frames)
{
    for(size_t f=0; f<frames.size(); ++f)
    {
        const FrameRecord &frame=frames[f];

        std::printf("scene=%s n=%d frame=%zu", summary.scene.c_str(), summary.n, f);
        for(size_t p=0; p<g_framePhaseCount; ++p)
            std::printf(" %s_us=%.1f", bench::phaseName(g_framePhases[p]), frame.us[p]);
        for(size_t c=0; c<g_counterCount; ++c)
            std::printf(" %s=%zu", g_counterNames[c], frame.counters[c]);
        std::printf("\n");
    }

    std::printf("summary");
    for(const auto &field : summaryFields(summary, frames))
        std::printf(" %s=%s", field.first.c_str(), field.second.c_str());
    std::printf("\n");
}

// Numbers stay JSON numbers; the few string fields are quoted.
bool isStringField(const std::string &key)
{
    return key == "scene" || key == "scaling" || key == "size" || key == "renderer" || key == "device";
}

void printJson(const Summary &summary, const std::vector<FrameRecord> &frames)
{
    std::printf("{\n  \"scene\": \"%s\",\n  \"n\": %d,\n  \"frames\": [\n", summary.scene.c_str(), summary.n);

    for(size_t f=0; f<frames.size(); ++f)
    {
        const FrameRecord &frame=frames[f];

        std::printf("    {\"frame\": %zu", f);
        for(size_t p=0; p<g_framePhaseCount; ++p)
            std::printf(", \"%s_us\": %.1f", bench::phaseName(g_framePhases[p]), frame.us[p]);
        for(size_t c=0; c<g_counterCount; ++c)
            std::printf(", \"%s\": %zu", g_counterNames[c], frame.counters[c]);
        std::printf("}%s\n", (f+1 < frames.size()) ? "," : "");
    }

    std::printf("  ],\n  \"summary\": {");
    bool first=true;
    for(const auto &field : summaryFields(summary, frames))
    {
        if(isStringField(field.first))
            std::printf("%s\n    \"%s\": \"%s\"", first ? "" : ",", field.first.c_str(), field.second.c_str());
        else
            std::printf("%s\n    \"%s\": %s", first ? "" : ",", field.first.c_str(), field.second.c_str());
        first=false;
    }
    std::printf("\n  }\n}\n");
}

//-----------------------------------------------------------------------------
// Command line
//-----------------------------------------------------------------------------
void printUsage()
{
    std::printf(
        "Usage: gweni_bench --scene <name> [options]\n"
        "       gweni_bench --list\n"
        "\n"
        "Runs one scene on gweni + OpenGLCore on a surfaceless EGL context (or\n"
        "renderer::Vulkan on a headless Vulkan device, --renderer vulkan), with\n"
        "the clock stepped 1/60 s per frame, and prints what each measured frame\n"
        "cost. The same arguments always give the same counters; only the times\n"
        "vary.\n"
        "\n"
        "Options:\n"
        "  --scene <name>     the scene to run (--list shows them)\n"
        "  --n <N>            the scene's size (default per scene)\n"
        "  --k <K>            the scene's edits per frame, where it has any (default 10)\n"
        "  --frames <F>       measured frames (default per scene)\n"
        "  --warmup <W>       frames run first and not reported (default per scene)\n"
        "  --size <WxH>       canvas size in logical units, at scale 1 (default 1280x800)\n"
        "  --resources <dir>  load the skin and fonts from <dir> (relative to the cwd,\n"
        "                     or absolute) instead of the resources/ next to the exe\n"
        "  --renderer <gl|vulkan>  the backend (default gl, or GWENI_RENDERER; the\n"
        "                     flag beats it); with GWENI_VULKAN_VALIDATION=1 a Vulkan\n"
        "                     run has the validation layer on, which slows it\n"
        "  --draw-slots <ranges|every>  which primitive slots each index chunk draws: its\n"
        "                     ranges of shown slots (the default), or every slot of the\n"
        "                     mirror (SlotRenderer::setDrawEveryPrimitiveSlot)\n"
        "  --clip-cull <on|off>  whether a primitive quad or text run drawn wholly outside\n"
        "                     its clip box is left out of the draws (the default on,\n"
        "                     unless GWENI_CLIP_CULL=0; SlotRenderer::setClipCulling)\n"
        "  --range-gap <G> --range-cap <R>  the ranges' limits (setPrimitiveRangeLimits): a\n"
        "                     gap of fewer than G hidden slots merged, at most R ranges a\n"
        "                     chunk; one not given keeps the renderer's own\n"
        "                     (GWENI_PRIMITIVE_RANGES, else kPrimitiveRangeGap and\n"
        "                     kMaxPrimitiveRanges)\n"
        "  --json             print one JSON object instead of the key=value lines\n"
        "  --list             print the scenes, one per line:\n"
        "                       <name> takes_n=<0|1> default_n=<N> warmup=<W> frames=<F>\n"
        "                       scaling=<local|linear> <description>\n"
        "                     scaling=local: the scene changes a fixed part of the tree each\n"
        "                     frame, so its per-frame counters must not grow with N;\n"
        "                     linear: it touches every control by design\n"
        "  --help             this text\n"
        "\n"
        "Output: one line per measured frame, then one summary line, all key=value\n"
        "pairs separated by spaces:\n"
        "  scene=<name> n=<N> frame=<i> <phase>_us=<t>... <counter>=<v>...\n"
        "  summary scene= n= k= frames= warmup= scaling= size= controls= construct_us=\n"
        "    construct_allocs= construct_alloc_bytes= construct_invalidations=\n"
        "    <phase>_us_min= <phase>_us_median= <phase>_us_max= (each frame phase)\n"
        "    <counter>_total= <counter>_max= (each counter) renderer= device=\n"
        "\n"
        "Phases (wall time, microseconds):\n"
        "  construct  Scene::build(), creating the tree before the first frame (summary only)\n"
        "  input      the scene's input events or edits for the frame\n"
        "  update     Canvas::updateCanvas()\n"
        "  render     Canvas::renderCanvas() without the renderer's end()\n"
        "  flush      the renderer's end(), i.e. SlotRenderer::flush(): slot diff, uploads, draws\n"
        "  gpu        glFinish() (Vulkan: vkQueueWaitIdle) after the frame. On Mesa's\n"
        "             llvmpipe or lavapipe (the container) this is the CPU rasteriser, not\n"
        "             a GPU; renderer= says which backend it was, device= which device.\n"
        "\n"
        "Counters (per frame):\n"
        "  visited render_carried controls_changed created visibility geometry background text\n"
        "             controls::ChangeStats: controls the render phase rendered (the queued\n"
        "             ones, not their ancestors), the queue entries it left for a later\n"
        "             frame (controls off the canvas), and the controls it found a state\n"
        "             change on, by kind\n"
        "  prim_slots_written text_slots_written upload_calls upload_bytes mirror_bytes\n"
        "  draw_calls primitive_slots_drawn primitive_ranges primitive_slots_culled\n"
        "  text_runs_culled text_draws atlas_upload_calls atlas_upload_bytes buffer_regrows\n"
        "             SlotRenderer::FrameStats of the frame's flush; the slots written are\n"
        "             the distinct primitive and glyph slots it uploads, each once, whether\n"
        "             a draw, a hide, a show or a release changed it; buffer_regrows the\n"
        "             vertex buffers and clip-box textures the mirror outgrew, each\n"
        "             re-allocated (geometrically) and sent whole once; primitive_slots_drawn\n"
        "             the primitive slots the draws' ranges covered (hidden ones in a\n"
        "             merged gap among them), primitive_ranges those ranges;\n"
        "             primitive_slots_culled and text_runs_culled sizes, not work: the\n"
        "             shown slots and text runs whose last draw lay wholly outside its\n"
        "             clip box, left out of the draws (--clip-cull)\n"
        "  layout_passes layout_capped render_layout\n"
        "             controls::LayoutStats: update-phase layout passes, whether they hit\n"
        "             the cap, whether the render phase laid out again\n"
        "  layout_visited  controls::LayoutStats: controls the frame's layout passes entered,\n"
        "             one per calculateSize() and one per arrange(), each axis counted\n"
        "  layout_children_scanned  controls::LayoutStats: children those passes' generic\n"
        "             loops (Base::sizeOfChildren, arrangeHorizontal/Vertical, arrange,\n"
        "             doArrange) went over, entered or not: a parent entered for one dirty\n"
        "             child counts all its siblings\n"
        "  measure_calls   renderer measureText(), lineHeight() and measureCarets() calls the\n"
        "             controls made in the frame (controls::measureTextCalls()), every phase, and\n"
        "             measure_calls_input/_update/_render in each: an edit wraps in the\n"
        "             input phase, layout in the update phase and any render-phase layout\n"
        "             in the render phase (LayoutStats::measureCalls holds the last two)\n"
        "  invalidations   Base::invalidate() calls in the frame (controls::invalidateCalls()),\n"
        "             each step of one's walk up to the canvas a call, every phase, and\n"
        "             invalidations_input/_update/_render in each: what marking layout dirty\n"
        "             cost before any pass ran (a resize or scale walks the whole tree)\n"
        "  z_visited  controls::UpdateStats: controls the z renumber entered\n"
        "  z_changed  controls::UpdateStats: of those, the controls whose z or overlay z it changed\n"
        "  anims_run  controls::UpdateStats: animations the update phase ran\n"
        "  hit_tests  input::hitTestVisits() over the frame: controls a hit test entered\n"
        "             (getControlAt calls), the hover's in the input phase included\n"
        "  prim_ids text_ids   live primitive and text ids after the frame\n"
        "  allocs alloc_bytes  C++ heap allocations (operator new) in the frame's phases,\n"
        "             and allocs_<phase> for each phase; C malloc is not counted\n");
}

void printList()
{
    for(const bench::SceneInfo &info : bench::scenes())
        std::printf("%s takes_n=%d default_n=%d warmup=%d frames=%d scaling=%s %s\n", info.name,
            info.takesN ? 1 : 0, info.defaultN, info.defaultWarmup, info.defaultFrames,
            bench::scalingName(info.scaling), info.description);
}

bool parseInt(const char *value, int &out, int minimum)
{
    char *end=nullptr;
    const long parsed=std::strtol(value, &end, 10);
    if(end == value || *end != '\0' || parsed < minimum || parsed > 100000000)
        return false;
    out=int(parsed);
    return true;
}

bool parseSize(const std::string &value, gweni::Point &size)
{
    const size_t sep=value.find_first_of("xX");
    if(sep == std::string::npos) return false;

    int w=0, h=0;
    if(!parseInt(value.substr(0, sep).c_str(), w, 1) || !parseInt(value.substr(sep+1).c_str(), h, 1))
        return false;

    size=gweni::Point(w, h);
    return true;
}

}//namespace

int main(int argc, char *argv[])
{
    gweni::capture::setToolName("gweni_bench");
    bench::countAllocationsOnThisThread();


    std::string sceneName;
    std::string resourceDir;
    std::string rendererName;
    gweni::Point size(1280, 800);
    int n=-1, k=10, frames=-1, warmup=-1;
    bool json=false;
    std::string drawSlots="ranges";
    // empty: not given, the renderer keeps its own (GWENI_CLIP_CULL, else on)
    std::string clipCull;
    // -1: not given, the renderer keeps its own (GWENI_PRIMITIVE_RANGES, else
    // the compiled defaults)
    int rangeGap=-1, rangeCap=-1;

    for(int i=1; i<argc; ++i)
    {
        const std::string arg=argv[i];
        const bool hasValue=(i+1) < argc;
        bool ok=true;

        if(arg == "--help" || arg == "-h"){ printUsage(); return EXIT_SUCCESS; }
        else if(arg == "--list"){ printList(); return EXIT_SUCCESS; }
        else if(arg == "--json") json=true;
        else if(arg == "--scene" && hasValue) sceneName=argv[++i];
        else if(arg == "--resources" && hasValue) resourceDir=argv[++i];
        else if(arg == "--renderer" && hasValue) rendererName=argv[++i];
        else if(arg == "--draw-slots" && hasValue)
        {
            drawSlots=argv[++i];
            ok=(drawSlots == "every" || drawSlots == "ranges");
        }
        else if(arg == "--clip-cull" && hasValue)
        {
            clipCull=argv[++i];
            ok=(clipCull == "on" || clipCull == "off");
        }
        else if(arg == "--range-gap" && hasValue) ok=parseInt(argv[++i], rangeGap, 0);
        else if(arg == "--range-cap" && hasValue) ok=parseInt(argv[++i], rangeCap, 1);
        else if(arg == "--n" && hasValue) ok=parseInt(argv[++i], n, 1);
        else if(arg == "--k" && hasValue) ok=parseInt(argv[++i], k, 1);
        else if(arg == "--frames" && hasValue) ok=parseInt(argv[++i], frames, 1);
        else if(arg == "--warmup" && hasValue) ok=parseInt(argv[++i], warmup, 0);
        else if(arg == "--size" && hasValue) ok=parseSize(argv[++i], size);
        else
        {
            std::cerr<<"gweni_bench: unknown or incomplete option \""<<arg<<"\" (see --help)\n";
            return EXIT_FAILURE;
        }

        if(!ok)
        {
            std::cerr<<"gweni_bench: bad value \""<<argv[i]<<"\" for "<<arg<<"\n";
            return EXIT_FAILURE;
        }
    }

    const bench::SceneInfo *info=nullptr;
    for(const bench::SceneInfo &candidate : bench::scenes())
        if(sceneName == candidate.name)
            info=&candidate;

    if(!info)
    {
        if(sceneName.empty())
            std::cerr<<"gweni_bench: no --scene given; the scenes are:\n";
        else
            std::cerr<<"gweni_bench: no such scene \""<<sceneName<<"\"; the scenes are:\n";
        for(const bench::SceneInfo &candidate : bench::scenes())
            std::cerr<<"  "<<candidate.name<<"\n";
        return EXIT_FAILURE;
    }

    gweni::capture::Backend backend;
    if(!gweni::capture::chooseBackend(rendererName, backend))
        return EXIT_FAILURE;

    if(n < 0) n=info->defaultN;
    if(frames < 0) frames=info->defaultFrames;
    if(warmup < 0) warmup=info->defaultWarmup;

#if !GWENI_EXAMPLE_EGL
    std::cerr<<"gweni_bench: needs the surfaceless EGL path, which this platform lacks\n";
    return EXIT_FAILURE;
#else
    // Declared before the renderer, which keeps a reference to it.
    // Unlike the examples, a frame drawn from missing resources measures nothing
    // worth having, so each of these stops the run.
    bool resourcesFound=true;
    const std::unique_ptr<gweni::ResourcePaths> paths=gweni::capture::resourcePaths(resourceDir, resourcesFound);
    if(!resourcesFound)
        return EXIT_FAILURE;

    // always headless: surfaceless EGL, or a Vulkan device with no window
    gweni::capture::DeviceOptions deviceOptions;
    deviceOptions.size=size;
    deviceOptions.capturing=true;
    deviceOptions.headless=true;

    std::unique_ptr<gweni::capture::Device> device=gweni::capture::openDevice(backend, deviceOptions);
    if(!device)
        return EXIT_FAILURE;

    gweni::renderer::SlotRenderer *renderer=newBenchRenderer(*device, *paths,
        gweni::Rect(gweni::Point(0, 0), size));
    renderer->init();
    renderer->setDrawEveryPrimitiveSlot(drawSlots == "every");
    if(!clipCull.empty())
        renderer->setClipCulling(clipCull == "on");
    if(rangeGap >= 0 || rangeCap >= 0)
        renderer->setPrimitiveRangeLimits(
            rangeGap >= 0 ? size_t(rangeGap) : renderer->primitiveRangeGap(),
            rangeCap >= 0 ? size_t(rangeCap) : renderer->maxPrimitiveRanges());

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    if(!skin->init("DefaultSkin.json"))
        return EXIT_FAILURE;
    if(gweni::capture::loadDefaultFont(*renderer, skin->getDefaultFont()) != gweni::Font::Status::Loaded)
        return EXIT_FAILURE;

    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;

    g_renderer=renderer;
    g_device=device.get();
    g_background=background;

    gweni::platform::setTimeSource(&steppedClock);

    primeShaders(skin, size);

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
    canvas->setScale(1.0f);
    canvas->setSize(size.x, size.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(background);

    bench::SceneContext sceneCtx;
    sceneCtx.canvas=canvas;
    sceneCtx.size=size;
    sceneCtx.n=n;
    sceneCtx.k=k;
    sceneCtx.frames=frames;
    // A window's framebuffer-size and content-scale callbacks, called directly:
    // a new offscreen target and the renderer's view at the device size, and
    // the canvas's logical size and scale.
    sceneCtx.setTarget=[&device, renderer, canvas](const gweni::Point &logical, float scale)
    {
        const gweni::Point pixels(gweni::renderer::Base::snap(float(logical.x), scale),
            gweni::renderer::Base::snap(float(logical.y), scale));

        if(!device->resizeTarget(pixels))
            return false;

        renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), pixels));
        canvas->setScale(scale);
        canvas->setSize(logical.x, logical.y);
        return true;
    };

    std::unique_ptr<bench::Scene> scene=info->make();

    Summary summary;
    summary.scene=info->name;
    summary.n=n;
    summary.k=k;
    summary.warmup=warmup;
    summary.scaling=bench::scalingName(info->scaling);
    summary.size=size;
    summary.renderer=gweni::capture::backendName(backend);
    summary.device=device->deviceName();

    // Under callgrind only the measured work is collected: the build scene's
    // construction and the measured frames, not the warm-up.
    bench::resetPhases();
    bench::setCollecting(info->profileConstruct);
    bench::inPhase(Phase::Construct, [&]{ scene->build(sceneCtx); });
    bench::enterPhase(Phase::None);
    bench::setCollecting(false);
    summary.constructUs=bench::phaseTotals().us[size_t(Phase::Construct)];
    summary.constructAllocs=bench::phaseTotals().allocs[size_t(Phase::Construct)];
    summary.constructAllocBytes=bench::phaseTotals().allocBytes[size_t(Phase::Construct)];
    summary.constructInvalidations=bench::phaseTotals().invalidations[size_t(Phase::Construct)];

    if(warmup > 0)
        primeChunks(canvas);

    for(int frame=-warmup; frame<0; ++frame)
        runFrame(*scene, sceneCtx, frame);

    std::vector<FrameRecord> records(static_cast<size_t>(frames));
    bench::setCollecting(true);
    for(int frame=0; frame<frames; ++frame)
    {
        runFrame(*scene, sceneCtx, frame);
        collectCounters(records[size_t(frame)], bench::phaseTotals(), *renderer, *canvas);
    }
    bench::setCollecting(false);
    // after the frames: layout may add parts of its own
    summary.controls=countControls(canvas);

    if(json)
        printJson(summary, records);
    else
        printLines(summary, records);
    std::fflush(stdout);

    scene.reset();
    delete canvas;
    delete skin;
    delete renderer;
    g_renderer=nullptr;
    g_device=nullptr;
    device.reset();
    return EXIT_SUCCESS;
#endif
}
