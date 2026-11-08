/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_bench: runs one scene on the full path (gweni, the textured skin and
 *  OpenGLCore on a surfaceless EGL context) and prints, per measured frame,
 *  what each phase cost and how much work it did. See --help for the output.
 *
 *  Every run is deterministic: the scene builds its tree in code, the clock is
 *  stepped 1/60 s per frame (platform::setTimeSource), and the input is the
 *  scene's own. So the same arguments always give the same counters; only the
 *  times vary.
 */
#include "bench.h"

#include <gweni/platforms/renderers/glApi.h>
#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/platforms/renderers/openglCore.h>
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

const float g_frameStep=1.0f/60.0f;
float g_clockSeconds=0.0f;

// input::hitTestVisits() when the frame began: hit_tests is the difference.
size_t g_hitTestsAtFrame=0;

float steppedClock()
{
    return g_clockSeconds;
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

using BenchRenderer=TimedFlush<gweni::renderer::OpenGLCore>;

// The per-frame phases, in output order.
const Phase g_framePhases[]={Phase::Input, Phase::Update, Phase::Render, Phase::Flush, Phase::Gpu};
const size_t g_framePhaseCount=sizeof(g_framePhases)/sizeof(g_framePhases[0]);

// The per-frame counters, in output order. The names are ControlTest's
// printFrameStats keys where they overlap.
const char *const g_counterNames[]=
{
    "visited", "children_scanned", "controls_changed", "created", "visibility", "geometry", "background", "text",
    "prim_slots_written", "text_slots_written", "upload_calls", "upload_bytes", "mirror_bytes",
    "draw_calls", "text_draws", "atlas_upload_calls", "atlas_upload_bytes", "buffer_regrows",
    "layout_passes", "layout_capped", "render_layout", "layout_visited", "measure_calls", "z_visited", "z_changed", "anims_run", "hit_tests", "prim_ids", "text_ids",
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

    size_t allocs=0, allocBytes=0;
    for(size_t i=0; i<g_framePhaseCount; ++i)
    {
        record.us[i]=phases.us[size_t(g_framePhases[i])];
        allocs+=phases.allocs[size_t(g_framePhases[i])];
        allocBytes+=phases.allocBytes[size_t(g_framePhases[i])];
    }

    const size_t values[g_counterCount]=
    {
        changes.visited, changes.childrenScanned, changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten, gpu.uploadCalls, gpu.uploadBytes,
        gpu.mirrorBytes, gpu.drawCalls, gpu.textDraws, gpu.atlasUploadCalls, gpu.atlasUploadBytes,
        gpu.bufferRegrows,
        size_t(layout.passes), layout.capped ? 1u : 0u, layout.renderLayout ? 1u : 0u,
        layout.visited, layout.measureCalls, update.zVisited, update.zChanged, update.animsRun,
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
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        primer->renderCanvas();
        glFinish();
    }

    scene.reset();
    delete primer;
}

// One frame: the scene's input, the two phases of the canvas and a fenced
// finish, each a phase of its own. glClear belongs to none of them.
void runFrame(bench::Scene &scene, bench::SceneContext &ctx, int frame)
{
    gweni::controls::resetChangeStats();
    g_hitTestsAtFrame=gweni::input::hitTestVisits();
    bench::resetPhases();

    bench::inPhase(Phase::Input, [&]{ scene.step(ctx, frame); });
    bench::inPhase(Phase::Update, [&]{ ctx.canvas->updateCanvas(); });

    bench::enterPhase(Phase::None);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    bench::inPhase(Phase::Render, [&]{ ctx.canvas->renderCanvas(); });
    bench::inPhase(Phase::Gpu, []{ glFinish(); });

    bench::enterPhase(Phase::None);
    g_clockSeconds+=g_frameStep;
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
    std::string glRenderer;
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

    add("gl_renderer", token(summary.glRenderer));
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
    return key == "scene" || key == "scaling" || key == "size" || key == "gl_renderer";
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
        "Runs one scene on gweni + OpenGLCore on a surfaceless EGL context, with\n"
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
        "    construct_allocs= construct_alloc_bytes=\n"
        "    <phase>_us_min= <phase>_us_median= <phase>_us_max= (each frame phase)\n"
        "    <counter>_total= <counter>_max= (each counter) gl_renderer=\n"
        "\n"
        "Phases (wall time, microseconds):\n"
        "  construct  Scene::build(), creating the tree before the first frame (summary only)\n"
        "  input      the scene's input events or edits for the frame\n"
        "  update     Canvas::updateCanvas()\n"
        "  render     Canvas::renderCanvas() without the renderer's end()\n"
        "  flush      the renderer's end(), i.e. SlotRenderer::flush(): slot diff, uploads, draws\n"
        "  gpu        glFinish() after the frame. On Mesa llvmpipe (the container) this is\n"
        "             the CPU rasteriser, not a GPU; gl_renderer= says which it was.\n"
        "\n"
        "Counters (per frame):\n"
        "  visited children_scanned controls_changed created visibility geometry background text\n"
        "             controls::ChangeStats: controls the render walk entered, the children\n"
        "             its loop passed over in them (pending or not), and the controls it\n"
        "             found a state change on, by kind\n"
        "  prim_slots_written text_slots_written upload_calls upload_bytes mirror_bytes\n"
        "  draw_calls text_draws atlas_upload_calls atlas_upload_bytes buffer_regrows\n"
        "             SlotRenderer::FrameStats of the frame's flush; the slots written are\n"
        "             the distinct primitive and glyph slots it uploads, each once, whether\n"
        "             a draw, a hide, a show or a release changed it; buffer_regrows the\n"
        "             vertex buffers and clip-box textures the mirror outgrew, each\n"
        "             re-allocated (geometrically) and sent whole once\n"
        "  layout_passes layout_capped render_layout\n"
        "             controls::LayoutStats: update-phase layout passes, whether they hit\n"
        "             the cap, whether the render phase laid out again\n"
        "  layout_visited  controls::LayoutStats: controls the frame's layout passes entered,\n"
        "             one per calculateSize() and one per arrange(), each axis counted\n"
        "  measure_calls   controls::LayoutStats: renderer measureText() calls the controls\n"
        "             made in the update phase and any render-phase layout\n"
        "  z_visited  controls::UpdateStats: controls the z renumber entered\n"
"  z_changed  controls::UpdateStats: controls of those whose z or overlay z it changed\n"
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
    gweni::Point size(1280, 800);
    int n=-1, k=10, frames=-1, warmup=-1;
    bool json=false;

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

    gweni::capture::Context ctx;
    if(!gweni::capture::initEglContext(ctx))
        return EXIT_FAILURE;
    if(!gweni::capture::initGL(ctx))
    {
        gweni::capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

    gweni::capture::Framebuffer fb;
    if(!gweni::capture::createFramebuffer(fb, size.x, size.y))
    {
        gweni::capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

    BenchRenderer *renderer=new BenchRenderer(*paths, gweni::Rect(gweni::Point(0, 0), size));
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    if(!skin->init("DefaultSkin.json"))
        return EXIT_FAILURE;
    if(gweni::capture::loadDefaultFont(*renderer, skin->getDefaultFont()) != gweni::Font::Status::Loaded)
        return EXIT_FAILURE;

    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

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
    sceneCtx.setTarget=[&fb, renderer, canvas](const gweni::Point &logical, float scale)
    {
        const gweni::Point pixels(gweni::renderer::Base::snap(float(logical.x), scale),
            gweni::renderer::Base::snap(float(logical.y), scale));

        gweni::capture::destroyFramebuffer(fb);
        if(!gweni::capture::createFramebuffer(fb, pixels.x, pixels.y))
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
    if(const GLubyte *name=glGetString(GL_RENDERER))
        summary.glRenderer=reinterpret_cast<const char *>(name);

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
    gweni::capture::destroyFramebuffer(fb);
    gweni::capture::destroyContext(ctx);
    return EXIT_SUCCESS;
#endif
}
