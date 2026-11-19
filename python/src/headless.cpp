/*
 *  Gweni - gweni.Headless (python/src): app::Headless, a canvas rendered
 *  offscreen on the stepped clock and driven by script steps, for tests and
 *  captures.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"

#include <gweni/app/headless.h>
#include <gweni/controls/canvas.h>

#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <optional>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

class Headless
{
public:
    Headless(std::pair<int, int> size, float scale, std::optional<std::string> resources)
    {
        app::HeadlessOptions options;

        options.size=Point(size.first, size.second);
        options.scale=scale;
        options.resources=resources ? *resources : packageResources();
        // the package has no lazy CJK face: read a fallback face at its first
        // miss, as a window does, rather than report the missing one
        options.preloadGlyphFallbacks=false;
        m_headless=std::make_unique<app::Headless>(options);
        if(!m_headless->ok())
        {
            const std::string error=m_headless->error();

            m_headless.reset();
            throw std::runtime_error("gweni.Headless: "+error);
        }
        beginSession("Headless", m_headless->canvas());
    }

    ~Headless()
    {
        // never throws: a handle dropped on another thread still frees it
        m_headless.reset();
        if(session().kind == std::string("Headless"))
            endSession();
    }

    void close()
    {
        if(!m_headless)
            return;
        checkThread();
        m_headless.reset();
        endSession();
    }

    app::Headless &get()
    {
        if(!m_headless)
            throw std::runtime_error("gweni.Headless: it is closed");
        checkThread();
        return *m_headless;
    }

    bool closed() const { return !m_headless; }

private:
    std::unique_ptr<app::Headless> m_headless;
};

//! A stat's value as an int where it is one, else its text.
static nb::object statValue(const std::string &text)
{
    if(!text.empty())
    {
        errno=0;
        char *end=nullptr;
        const long long value=std::strtoll(text.c_str(), &end, 10);

        if(errno == 0 && end != nullptr && *end == '\0')
            return nb::int_(value);
    }
    return nb::str(text.c_str());
}

void bindHeadless(nb::module_ &m)
{
    nb::class_<Headless>(m, "Headless",
        "A canvas rendered offscreen (app::Headless): surfaceless EGL, the textured skin,\n"
        "a stepped clock from 0 (1/60 s a frame, a second before each press) and\n"
        "input as ControlTest's flags give it. One App or Headless is alive at a time.")
        .def(nb::init<std::pair<int, int>, float, std::optional<std::string>>(),
            nb::arg("size")=std::make_pair(640, 480), nb::arg("scale")=1.0f, nb::arg("resources")=nb::none(),
            "size is the canvas in logical units, scale device px per unit, resources a\n"
            "directory to load the skin and fonts from (the package's by default).")
        .def("close", &Headless::close, "Deletes the canvas and frees the device; the handles go stale.")
        .def_prop_ro("closed", &Headless::closed)
        .def("__enter__", [](nb::handle self) { return nb::borrow(self); })
        .def("__exit__", [](Headless &self, nb::args) { self.close(); })
        .def_prop_ro("canvas", [](Headless &self) {
                return wrap(self.get().canvas());
            }, "The canvas's handle.")
        .def_prop_ro("pixel_size", [](Headless &self) {
                const Point size=self.get().pixelSize();
                return std::make_pair(size.x, size.y);
            }, "The target in device px.")
        .def_prop_ro("degraded", [](Headless &self) { return self.get().degraded(); },
            "A resource did not load (its line is on stderr).")
        .def_prop_ro("seconds", [](Headless &self) { return self.get().seconds(); }, "The clock now.")
        .def_prop_ro("frame_count", [](Headless &self) { return self.get().frameCount(); },
            "Frames drawn so far.")
        .def("frame", [](Headless &self, int count) { self.get().frame(count); }, nb::arg("count")=1,
            "Draws count frames: the update, the render, the clock a step on.")
        .def("move", [](Headless &self, int x, int y) { self.get().move(x, y); }, nb::arg("x"), nb::arg("y"),
            "The mouse to (x, y), in logical units.")
        .def("press", [](Headless &self, int x, int y, int button) { self.get().press(x, y, button); },
            nb::arg("x"), nb::arg("y"), nb::arg("button")=0,
            "A button (0 left, 1 right) pressed at (x, y), the clock a second on.")
        .def("release", [](Headless &self, int x, int y, int button) { self.get().release(x, y, button); },
            nb::arg("x"), nb::arg("y"), nb::arg("button")=0, "A button released at (x, y).")
        .def("click", [](Headless &self, int x, int y, int button) { self.get().click(x, y, button); },
            nb::arg("x"), nb::arg("y"), nb::arg("button")=0, "press() and release() at (x, y).")
        .def("wheel", [](Headless &self, int delta) { return self.get().wheel(delta); }, nb::arg("delta"),
            "A vertical wheel of delta (60 a notch, down negative); whether the UI took it.")
        .def("wheel_h", [](Headless &self, int delta) { return self.get().wheelH(delta); }, nb::arg("delta"),
            "A horizontal wheel of delta; whether the UI took it.")
        .def("key", [](Headless &self, const std::string &chord) {
                if(!self.get().key(chord))
                    throw nb::value_error(("gweni.Headless.key: not a key or chord: "+chord).c_str());
            }, nb::arg("chord"), "A key or chord as ControlTest's --key takes it: \"return\", \"ctrl+a\", \"x\".")
        .def("type", [](Headless &self, const std::string &text) { self.get().type(text); }, nb::arg("text"),
            "Text typed, one codepoint at a time.")
        .def("pixels", [](Headless &self) {
                std::vector<uint8_t> rows=self.get().pixels();
                return nb::bytes(reinterpret_cast<const char *>(rows.data()), rows.size());
            }, "The target as top-down RGBA8 rows, pixel_size large.")
        .def("save_png", [](Headless &self, const std::string &path) {
                if(!self.get().savePng(path))
                    throw std::runtime_error("gweni.Headless.save_png: "+path+" not written (see stderr)");
            }, nb::arg("path"), "Writes the target as a PNG (after a frame).")
        .def("stats", [](Headless &self) {
                nb::dict stats;

                for(const app::StatField &field : self.get().stats())
                    stats[field.key.c_str()]=statValue(field.value);
                return stats;
            }, "The last frame's counters, keyed as ControlTest --stats prints them.");
}

}//namespace python
}//namespace gweni
