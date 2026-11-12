/*
 *  Gweni
 *  See license in gweni.h
 */

#ifndef _gweni_skins_textured_tokyonight_h_
#define _gweni_skins_textured_tokyonight_h_

// Tokyo Night (Night variant) theme for the textured skin, rendered with the
// OpenGLCore SDF rounded-rect path (renderer::Base::drawRoundedRect). Control
// backgrounds are described as a Surface (top/bottom gradient fill + border +
// radius); paintSurface() drives the standard draw+show-on-state-change flow so
// each control renderer's update() collapses to picking a Surface for its state.
//
// Palette source: https://github.com/tokyo-night/tokyo-night-vscode-theme

#include "gweni/platforms/platformTypes.h"
#include "gweni/platforms/baseRender.h"
#include "gweni/controls/base.h"

#include <string>
#include <unordered_map>

namespace gweni
{
namespace skin
{
namespace textured
{
namespace tokyonight
{

// ---- base palette ---------------------------------------------------------
inline const Color Bg          (0x1a, 0x1b, 0x26); // editor background
inline const Color BgDark      (0x16, 0x16, 0x1e); // darker (insets, tracks)
inline const Color BgStorm     (0x24, 0x28, 0x3b); // popup/elevated background
inline const Color BgHighlight (0x29, 0x2e, 0x42); // subtle borders
inline const Color Surf        (0x2c, 0x30, 0x47); // control fill (lower)
inline const Color SurfHi      (0x3b, 0x42, 0x61); // control fill (upper)
inline const Color SurfHover   (0x44, 0x4b, 0x6e); // hovered control fill
inline const Color Line        (0x41, 0x48, 0x68); // terminal black / strong border
inline const Color Fg          (0xc0, 0xca, 0xf5); // primary text
inline const Color FgDark      (0xa9, 0xb1, 0xd6); // secondary text
inline const Color Muted       (0x56, 0x5f, 0x89); // comments / disabled text
inline const Color Blue        (0x7a, 0xa2, 0xf7); // primary accent
inline const Color Cyan        (0x7d, 0xcf, 0xff);
inline const Color DarkBlue    (0x3d, 0x59, 0xa1); // deep accent
inline const Color Red         (0xf7, 0x76, 0x8e);
inline const Color Green       (0x9e, 0xce, 0x6a);
inline const Color Yellow      (0xe0, 0xaf, 0x68);
inline const Color Orange      (0xff, 0x9e, 0x64);
inline const Color Magenta     (0xbb, 0x9a, 0xf7);

// ---- surface description --------------------------------------------------
struct Surface
{
    Color top;          //!< gradient top fill
    Color bottom;       //!< gradient bottom fill
    Color border;       //!< border color (ignored when borderWidth<=0)
    float radius;       //!< corner radius (px)
    float borderWidth;  //!< border thickness (px)
};

// Common geometry.
inline constexpr float Radius     = 6.0f;
inline constexpr float RadiusSm   = 4.0f;
inline constexpr float RadiusLg   = 8.0f;
inline constexpr float BorderW    = 1.0f;

// ---- named surfaces -------------------------------------------------------
// Windows / containers
inline Surface Window       { Color(0x1f,0x22,0x33), BgDark,                BgHighlight, RadiusLg, BorderW };
inline Surface WindowInact  { Color(0x1b,0x1d,0x2b), BgDark,                BgHighlight, RadiusLg, BorderW };
inline Surface Panel        { Color(0x20,0x23,0x36), Bg,                    BgHighlight, Radius,   BorderW };
inline Surface PanelDark    { BgDark,                BgDark,                BgHighlight, Radius,   BorderW };
inline Surface GroupBox     { Color(0x1d,0x20,0x31), Bg,                    Line,        Radius,   BorderW };
inline Surface Tooltip      { BgStorm,               Color(0x1f,0x22,0x33), Muted,       RadiusSm, BorderW };
inline Surface StatusBar    { Color(0x1f,0x22,0x33), BgDark,                BgHighlight, 0.0f,     BorderW };
// The canvas fill behind everything. Flat, so only top is read.
inline Surface Canvas       { Bg,                    Bg,                    Bg,          0.0f,     0.0f };

// Buttons
inline Surface ButtonNormal   { SurfHi,                Surf,                  Line,  Radius, BorderW };
inline Surface ButtonHover    { SurfHover,             SurfHi,                Blue,  Radius, BorderW };
inline Surface ButtonDown     { Surf,                  BgStorm,               Blue,  Radius, BorderW };
inline Surface ButtonDisabled { BgStorm,               Color(0x1f,0x22,0x33), BgHighlight, Radius, BorderW };
// A toggle Button that is on: an accent-tinted fill, so it reads unlike a
// momentary press (ButtonDown); disabled, it stays on, dimmed
inline Surface ButtonOn         { Color(0x33,0x46,0x7a), Color(0x28,0x34,0x57), Blue,  Radius, BorderW };
inline Surface ButtonOnHover    { Color(0x3d,0x55,0x94), Color(0x31,0x42,0x6e), Blue,  Radius, BorderW };
inline Surface ButtonOnDisabled { Color(0x2a,0x35,0x5a), Color(0x22,0x2b,0x47), Color(0x3a,0x4d,0x85), Radius, BorderW };

// Text entry
inline Surface TextBox        { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface TextBoxFocus   { BgDark,                BgDark,                Blue,  RadiusSm, BorderW };
inline Surface TextBoxDisabled{ Color(0x1f,0x22,0x33), Color(0x1f,0x22,0x33), BgHighlight, RadiusSm, BorderW };
// The selection behind a TextBox's selected glyphs (Tokyo Night's visual-mode
// background), drawn square and borderless by default.
inline Surface TextSelection  { Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), 0.0f, 0.0f };

// A CodeEditor: its field (an inset, like a TextBox's, its border blue with
// the focus), its line-number gutter (flat and square, a shade lighter than
// the field), the line with the caret (flat and square, a shade over the
// field) and the selection behind the selected glyphs (flat and square, as a
// TextBox's).
inline Surface CodeEditor      { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface CodeEditorFocus { BgDark,                BgDark,                Blue,  RadiusSm, BorderW };
inline Surface CodeGutter      { Bg,                    Bg,                    Bg,    0.0f, 0.0f };
inline Surface CodeCurrentLine { Color(0x1f,0x21,0x30), Color(0x1f,0x21,0x30), Color(0x1f,0x21,0x30), 0.0f, 0.0f };
inline Surface CodeSelection   { Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), 0.0f, 0.0f };

// A LogView's field: a CodeEditor's (an inset, its border blue with the focus).
inline Surface LogView         { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface LogViewFocus    { BgDark,                BgDark,                Blue,  RadiusSm, BorderW };
// A selected row, behind its glyphs (a TextBox's selection's colour, flat and
// square).
inline Surface LogViewSelection{ Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), 0.0f, 0.0f };

// Menus
inline Surface Menu           { Color(0x1f,0x22,0x33), Bg,                    Line,  Radius,   BorderW };
inline Surface MenuStrip      { Color(0x1f,0x22,0x33), Bg,                    BgHighlight, 0.0f, BorderW };
inline Surface MenuHover      { SurfHi,                SurfHi,                Blue,  RadiusSm, BorderW };

// Tabs, drawn as a notebook's: the page frame (TabControl, painted by the
// TabControlInner panel; the TabControl itself draws nothing, so the strip shows
// its parent's background) is one flat fill, and the selected tab is that fill
// with the frame's border, square and borderless on the page's side, so the two
// are one outline. An unselected tab (TabInactive, hovered TabHover) is a darker
// box drawn shorter, behind the frame's border line.
inline Surface TabControl     { Color(0x1f,0x22,0x33), Color(0x1f,0x22,0x33), BgHighlight, Radius, BorderW };
inline Surface TabActive      { Color(0x1f,0x22,0x33), Color(0x1f,0x22,0x33), BgHighlight, Radius, BorderW };
inline Surface TabInactive    { Color(0x1b,0x1d,0x2a), Color(0x1b,0x1d,0x2a), BgHighlight, Radius, BorderW };
inline Surface TabHover       { BgStorm,               BgStorm,               Line,        Radius, BorderW };

// Category list (the left collapsible panel)
inline Surface CategoryList   { BgDark,                BgDark,                BgHighlight, Radius, BorderW };
inline Surface CategoryInner  { Bg,                    Bg,                    BgStorm,     RadiusSm, BorderW };
inline Surface CategoryHeader { DarkBlue,              Color(0x2c,0x3d,0x6b), Blue,  RadiusSm, BorderW };
inline Surface ListItem       { Bg,                    Bg,                    Bg,    RadiusSm, 0.0f };
inline Surface ListItemSel    { SurfHi,                Surf,                  Blue,  RadiusSm, BorderW };
inline Surface ListItemHover  { Color(0x24,0x28,0x3b), Color(0x24,0x28,0x3b), BgStorm, RadiusSm, 0.0f };

// Scrollbars
inline Surface ScrollTrack    { BgDark,                BgDark,                BgDark, RadiusSm, 0.0f };
inline Surface ScrollThumb    { Line,                  Color(0x37,0x3d,0x5a), Line,  RadiusSm, 0.0f };
inline Surface ScrollThumbHover{ Muted,                Line,                  Muted, RadiusSm, 0.0f };
inline Surface ScrollThumbDown { Blue,                 DarkBlue,              Blue,  RadiusSm, 0.0f };

// Progress
inline Surface ProgressBack   { BgDark,                BgDark,                BgHighlight, Radius, BorderW };
inline Surface ProgressFront  { Blue,                  DarkBlue,              Blue,  Radius, 0.0f };

// Slider: a thin inset track, a tick per notch, and the thumb (its own control)
inline Surface SliderTrack        { BgDark,                BgDark,                Line,  2.0f,     BorderW };
inline Surface SliderNotch        { Muted,                 Muted,                 Muted, 0.0f,     0.0f };
inline Surface SliderThumb        { SurfHi,                Surf,                  Blue,  RadiusSm, BorderW };
inline Surface SliderThumbHover   { SurfHover,             SurfHi,                Cyan,  RadiusSm, BorderW };
inline Surface SliderThumbDown    { Blue,                  DarkBlue,              Cyan,  RadiusSm, BorderW };
inline Surface SliderThumbDisabled{ BgStorm,               Color(0x1f,0x22,0x33), BgHighlight, RadiusSm, BorderW };

// Splitter bars: a flat bar a few units thick, so a small radius and no border
inline Surface SplitterBar        { Line,                  Line,                  Line,    2.0f,     0.0f };
inline Surface SplitterBarHover   { Blue,                  Blue,                  Blue,    2.0f,     0.0f };
inline Surface SplitterBarDown    { Cyan,                  Cyan,                  Cyan,    2.0f,     0.0f };
inline Surface SplitterBarDisabled{ BgStorm,               BgStorm,               BgStorm, 2.0f,     0.0f };

// Check / radio
inline Surface CheckOff       { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface CheckOn        { Blue,                  DarkBlue,              Blue,  RadiusSm, BorderW };
inline Surface CheckHover     { Surf,                  BgStorm,               Blue,  RadiusSm, BorderW };
// sunk into the background, with a faint frame; the tick or dot is Muted
inline Surface CheckDisabled  { Bg,                    Bg,                    BgHighlight, RadiusSm, BorderW };

// Colour picker: the SV box's frame (its fill is the bilinear hue blend, so
// only border/radius are read) and the opaque cursor/selection markers
inline Surface ColorField     { Bg,                    Bg,                    Line,  RadiusSm, BorderW };
inline Surface ColorMarker    { Color(0xff,0xff,0xff), Color(0xff,0xff,0xff), BgDark, 2.0f,    BorderW };
// The swatch (internal::ColorDisplay): its fill is the swatch's colour, so only
// border/radius are read. A translucent colour is shown over a checker of
// ColorChecker's top (light) and bottom (dark) cells; only those two are read.
inline Surface ColorSwatch    { Bg,                    Bg,                    Line,  2.0f,     BorderW };
inline Surface ColorChecker   { Color(0xc8,0xc8,0xc8), Color(0x7a,0x7a,0x7a), Line,  0.0f,     0.0f };
// A ColorButton is a swatch that is a button: at rest it is ColorSwatch, and
// only the border (and radius and border width) of these is read, the fill
// being its colour as for ColorSwatch.
inline Surface ColorButtonHover   { Bg,                Bg,                    Blue,        2.0f, BorderW };
inline Surface ColorButtonDown    { Bg,                Bg,                    Cyan,        2.0f, BorderW };
inline Surface ColorButtonDisabled{ Bg,                Bg,                    BgHighlight, 2.0f, BorderW };

// Separator rules (and menu dividers): flat, so only top is read, as the rule's
// colour; the menu's border colour
inline Surface Separator      { Line,                  Line,                  Line,  0.0f,     0.0f };

// Selectable rows: nothing while idle (the slot is hidden), the list's hover
// and selection looks otherwise, so a Selectable reads as a ListBox row
inline Surface SelectableHover        { Color(0x24,0x28,0x3b), Color(0x24,0x28,0x3b), BgStorm, RadiusSm, 0.0f };
inline Surface SelectableDown         { Surf,                  BgStorm,               BgStorm, RadiusSm, 0.0f };
inline Surface SelectableSelected     { SurfHi,                Surf,                  Blue,    RadiusSm, BorderW };
inline Surface SelectableSelectedHover{ SurfHover,             SurfHi,                Blue,    RadiusSm, BorderW };
inline Surface SelectableSelectedDisabled{ BgStorm,            Color(0x1f,0x22,0x33), BgHighlight, RadiusSm, BorderW };

// A PropertyRow's label column: flat and square, so only top is read. A dark
// fill at rest, lifted while the row is hovered, blue-tinted while its value
// is edited; the label's text (Properties.Label_*) reads over each.
inline Surface PropertyColumn       { Color(0x1d,0x20,0x31), Color(0x1d,0x20,0x31), BgHighlight, 0.0f, 0.0f };
inline Surface PropertyColumnHover  { BgStorm,               BgStorm,               BgHighlight, 0.0f, 0.0f };
inline Surface PropertyColumnEditing{ Color(0x26,0x31,0x55), Color(0x26,0x31,0x55), BgHighlight, 0.0f, 0.0f };

// A DataTable: its header strip (a gradient like a tab strip's, its rule along
// the bottom is the Separator's), a header cell's hover and press (flat, over
// the strip; at rest the cell's slot is hidden) and the fill of a striped
// table's odd rows (flat, a shade over the table's PanelDark backdrop). Rows
// otherwise hover and select as Selectables.
inline Surface DataTableHeader     { Color(0x24,0x28,0x3b), Color(0x1f,0x22,0x33), BgHighlight, 0.0f, 0.0f };
inline Surface DataTableHeaderHover{ BgHighlight,           BgHighlight,           BgHighlight, 0.0f, 0.0f };
inline Surface DataTableHeaderDown { Surf,                  Surf,                  Surf,        0.0f, 0.0f };
inline Surface DataTableStripe     { Color(0x1f,0x21,0x30), Color(0x1f,0x21,0x30), BgDark,      0.0f, 0.0f };

// A PlotHistogram: its frame (an inset, like a TextBox's) and its bars, flat
// and square, so only top is read; the hovered bar is lifted.
inline Surface PlotFrame      { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface PlotBar        { Blue,                  Blue,                  Blue,  0.0f,     0.0f };
inline Surface PlotBarHover   { Cyan,                  Cyan,                  Cyan,  0.0f,     0.0f };
// A PlotLines: its frame is PlotFrame, its line is drawn in PlotLine's top.
inline Surface PlotLine       { Blue,                  Blue,                  Blue,  0.0f,     0.0f };

// gweni::tools::StatsOverlay's panel, in the canvas's corner: translucent, so
// the app's fills show dimly through it.
inline Surface StatsOverlay   { Color(0x16,0x16,0x1e,0xe0), Color(0x16,0x16,0x1e,0xe0), BgHighlight, Radius, BorderW };

// A DragNumber: a field filled like a button, not inset like a TextBox (it is
// dragged, not typed into, until its edit box comes up), lit while hovered,
// its border blue with the focus and cyan while it is being dragged.
inline Surface DragNumber        { Surf,                  Surf,                  Line,        RadiusSm, BorderW };
inline Surface DragNumberHover   { SurfHi,                SurfHi,                Line,        RadiusSm, BorderW };
inline Surface DragNumberFocus   { Surf,                  Surf,                  Blue,        RadiusSm, BorderW };
inline Surface DragNumberDown    { SurfHi,                SurfHi,                Cyan,        RadiusSm, BorderW };
inline Surface DragNumberDisabled{ Color(0x1f,0x22,0x33), Color(0x1f,0x22,0x33), BgHighlight, RadiusSm, BorderW };

// A VectorField component's axis tag, a block of the axis's colour before its
// field (its letter in the skin's AxisTagText): X red, Y green, Z blue and W
// grey; a disabled row's tags are filled as a disabled field.
inline Surface VectorTagX        { Red,                   Red,                   Red,         RadiusSm, 0.0f };
inline Surface VectorTagY        { Green,                 Green,                 Green,       RadiusSm, 0.0f };
inline Surface VectorTagZ        { Blue,                  Blue,                  Blue,        RadiusSm, 0.0f };
inline Surface VectorTagW        { FgDark,                FgDark,                FgDark,      RadiusSm, 0.0f };

// A NodeGraph and its nodes. The graph's backdrop is GraphGrid's top and
// bottom, its grid lines GraphGrid's border colour, every GridMajorEvery-th one
// GraphGridMajor's top. A node's frame is NodeBody (NodeSelected while it is
// selected), its title bar NodeTitle's fill (an app's title colour instead)
// joined to the frame's border. A pin's marker is PinIn's or PinOut's top
// colour (an app's instead), a ring of their borderWidth around their bottom
// fill until it is connected. A hovered link is drawn in LinkHover's top; the
// marquee is Marquee, translucent, over everything in the graph.
inline Surface GraphGrid      { Color(0x15,0x16,0x20),  Color(0x15,0x16,0x20), Color(0x1f,0x22,0x31), 0.0f, 0.0f };
inline Surface GraphGridMajor { Color(0x29,0x2d,0x40),  Color(0x29,0x2d,0x40), Color(0x29,0x2d,0x40), 0.0f, 0.0f };
inline Surface NodeBody       { Color(0x22,0x25,0x37),  Color(0x1d,0x20,0x30), Line,        Radius, BorderW };
inline Surface NodeSelected   { Color(0x24,0x29,0x3f),  Color(0x1f,0x23,0x36), Blue,        Radius, 2.0f };
inline Surface NodeTitle      { SurfHi,                 Surf,                  Line,        Radius, BorderW };
inline Surface PinIn          { Cyan,                   BgDark,                Cyan,        2.0f,   1.5f };
inline Surface PinOut         { Green,                  BgDark,                Green,       2.0f,   1.5f };
inline Surface LinkHover      { Cyan,                   Cyan,                  Cyan,        0.0f,   0.0f };
inline Surface Marquee        { Color(0x7a,0xa2,0xf7,0x30), Color(0x7a,0xa2,0xf7,0x30), Blue, 0.0f, BorderW };

// Window title-bar buttons: nothing at rest (a transparent fill hides its slot),
// red under the close button while hovered, a deeper red while pressed. The
// minimise and maximise buttons hover and press as ButtonHover/ButtonDown.
inline Surface WindowButton     { Color(0,0,0,0),        Color(0,0,0,0),        Color(0,0,0,0), RadiusSm, 0.0f };
inline Surface WindowCloseHover { Red,                   Red,                   Red,            RadiusSm, 0.0f };
inline Surface WindowCloseDown  { Color(0xdb,0x4b,0x4b), Color(0xdb,0x4b,0x4b), Color(0xdb,0x4b,0x4b), RadiusSm, 0.0f };

// Generic transparent (draw nothing visible but keep a slot)
inline Surface None           { Color(0,0,0,0),        Color(0,0,0,0),        Color(0,0,0,0), 0.0f, 0.0f };

// Name -> surface, so an external theme file can override any surface by name
// without recompiling. The compiled values above are the defaults.
inline std::unordered_map<std::string, Surface*>& surfaceRegistry()
{
    static std::unordered_map<std::string, Surface*> m={
        {"Window",&Window},{"WindowInact",&WindowInact},{"Panel",&Panel},{"PanelDark",&PanelDark},
        {"GroupBox",&GroupBox},{"Tooltip",&Tooltip},{"StatusBar",&StatusBar},{"Canvas",&Canvas},
        {"ButtonNormal",&ButtonNormal},{"ButtonHover",&ButtonHover},{"ButtonDown",&ButtonDown},{"ButtonDisabled",&ButtonDisabled},
        {"ButtonOn",&ButtonOn},{"ButtonOnHover",&ButtonOnHover},{"ButtonOnDisabled",&ButtonOnDisabled},
        {"TextBox",&TextBox},{"TextBoxFocus",&TextBoxFocus},{"TextBoxDisabled",&TextBoxDisabled},{"TextSelection",&TextSelection},
        {"CodeEditor",&CodeEditor},{"CodeEditorFocus",&CodeEditorFocus},{"CodeGutter",&CodeGutter},
        {"CodeCurrentLine",&CodeCurrentLine},{"CodeSelection",&CodeSelection},
        {"LogView",&LogView},{"LogViewFocus",&LogViewFocus},{"LogViewSelection",&LogViewSelection},
        {"Menu",&Menu},{"MenuStrip",&MenuStrip},{"MenuHover",&MenuHover},
        {"TabActive",&TabActive},{"TabInactive",&TabInactive},{"TabHover",&TabHover},{"TabControl",&TabControl},
        {"CategoryList",&CategoryList},{"CategoryInner",&CategoryInner},{"CategoryHeader",&CategoryHeader},
        {"ListItem",&ListItem},{"ListItemSel",&ListItemSel},{"ListItemHover",&ListItemHover},
        {"ScrollTrack",&ScrollTrack},{"ScrollThumb",&ScrollThumb},{"ScrollThumbHover",&ScrollThumbHover},{"ScrollThumbDown",&ScrollThumbDown},
        {"ProgressBack",&ProgressBack},{"ProgressFront",&ProgressFront},
        {"SliderTrack",&SliderTrack},{"SliderNotch",&SliderNotch},{"SliderThumb",&SliderThumb},
        {"SliderThumbHover",&SliderThumbHover},{"SliderThumbDown",&SliderThumbDown},{"SliderThumbDisabled",&SliderThumbDisabled},
        {"SplitterBar",&SplitterBar},{"SplitterBarHover",&SplitterBarHover},{"SplitterBarDown",&SplitterBarDown},
        {"SplitterBarDisabled",&SplitterBarDisabled},
        {"CheckOff",&CheckOff},{"CheckOn",&CheckOn},{"CheckHover",&CheckHover},{"CheckDisabled",&CheckDisabled},
        {"ColorField",&ColorField},{"ColorMarker",&ColorMarker},{"ColorSwatch",&ColorSwatch},{"ColorChecker",&ColorChecker},
        {"ColorButtonHover",&ColorButtonHover},{"ColorButtonDown",&ColorButtonDown},{"ColorButtonDisabled",&ColorButtonDisabled},
        {"Separator",&Separator},
        {"SelectableHover",&SelectableHover},{"SelectableDown",&SelectableDown},{"SelectableSelected",&SelectableSelected},
        {"SelectableSelectedHover",&SelectableSelectedHover},{"SelectableSelectedDisabled",&SelectableSelectedDisabled},
        {"PropertyColumn",&PropertyColumn},{"PropertyColumnHover",&PropertyColumnHover},
        {"PropertyColumnEditing",&PropertyColumnEditing},
        {"DataTableHeader",&DataTableHeader},{"DataTableHeaderHover",&DataTableHeaderHover},
        {"DataTableHeaderDown",&DataTableHeaderDown},{"DataTableStripe",&DataTableStripe},
        {"PlotFrame",&PlotFrame},{"PlotBar",&PlotBar},{"PlotBarHover",&PlotBarHover},{"PlotLine",&PlotLine},
        {"StatsOverlay",&StatsOverlay},
        {"DragNumber",&DragNumber},{"DragNumberHover",&DragNumberHover},{"DragNumberFocus",&DragNumberFocus},
        {"DragNumberDown",&DragNumberDown},{"DragNumberDisabled",&DragNumberDisabled},
        {"VectorTagX",&VectorTagX},{"VectorTagY",&VectorTagY},{"VectorTagZ",&VectorTagZ},
        {"VectorTagW",&VectorTagW},
        {"GraphGrid",&GraphGrid},{"GraphGridMajor",&GraphGridMajor},{"NodeBody",&NodeBody},
        {"NodeSelected",&NodeSelected},{"NodeTitle",&NodeTitle},{"PinIn",&PinIn},{"PinOut",&PinOut},
        {"LinkHover",&LinkHover},{"Marquee",&Marquee},
        {"WindowButton",&WindowButton},{"WindowCloseHover",&WindowCloseHover},{"WindowCloseDown",&WindowCloseDown},
    };
    return m;
}

// Paints every registered surface in the missing colour (colors::Missing fill,
// colors::MissingDark border), keeping its radius and border width, so what a
// theme does not provide shows. The surfaces stay so until set again.
inline void setAllSurfacesMissing()
{
    for(auto &entry:surfaceRegistry())
    {
        Surface &s=*entry.second;

        s.top=colors::Missing;
        s.bottom=colors::Missing;
        s.border=colors::MissingDark;
    }
}

// ---- paint helper ---------------------------------------------------------
// Standard "draw if visible, toggle on visibility/created change" flow used by
// almost every control renderer. ids must point at >=1 primitive id.
// The overload taking a rect paints it in place of the control's render bounds,
// with an edge mask (renderer::RoundedEdge: the corners it names square, the
// sides it names without a border) and the reach of those sides over a
// neighbour's border (renderer::Base::drawRoundedRect) when given.
inline void paintSurface(renderer::Base *render, size_t *ids, controls::Base *c, const Surface &s,
    const Rect &rect, uint8_t edges=0, float reach=0.0f)
{
    if(!ids)
        return;

    const bool visible=c->visible();

    if(visible)
    {
        if(edges || (reach > 0.0f))
            render->drawRoundedRect(ids[0], rect, s.radius, s.borderWidth,
                s.top, s.bottom, s.border, c->getZIndex(), edges, reach);
        else
            render->drawRoundedRect(ids[0], rect, s.radius, s.borderWidth,
                s.top, s.bottom, s.border, c->getZIndex());
    }

    const unsigned int sc=c->getStateChange();
    if((sc&controls::StateChange_Visibility) || (sc&controls::StateChange_Created))
        render->showPrimitive(visible?4:0, ids[0]);
}

inline void paintSurface(renderer::Base *render, size_t *ids, controls::Base *c, const Surface &s)
{
    paintSurface(render, ids, c, s, c->getRenderBounds());
}

}//namespace tokyonight
}//namespace textured
}//namespace skin
}//namespace gweni

#endif//_gweni_skins_textured_tokyonight_h_
