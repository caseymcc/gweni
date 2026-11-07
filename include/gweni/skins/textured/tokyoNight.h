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

// Text entry
inline Surface TextBox        { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface TextBoxFocus   { BgDark,                BgDark,                Blue,  RadiusSm, BorderW };
inline Surface TextBoxDisabled{ Color(0x1f,0x22,0x33), Color(0x1f,0x22,0x33), BgHighlight, RadiusSm, BorderW };
// The selection behind a TextBox's selected glyphs (Tokyo Night's visual-mode
// background), drawn square and borderless by default.
inline Surface TextSelection  { Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), Color(0x28,0x34,0x57), 0.0f, 0.0f };

// Menus
inline Surface Menu           { Color(0x1f,0x22,0x33), Bg,                    Line,  Radius,   BorderW };
inline Surface MenuStrip      { Color(0x1f,0x22,0x33), Bg,                    BgHighlight, 0.0f, BorderW };
inline Surface MenuHover      { SurfHi,                SurfHi,                Blue,  RadiusSm, BorderW };

// Tabs
inline Surface TabActive      { SurfHi,                Surf,                  Blue,  Radius, BorderW };
inline Surface TabInactive    { Color(0x20,0x23,0x36), Bg,                    BgHighlight, Radius, BorderW };
inline Surface TabControl     { Color(0x1d,0x20,0x31), Bg,                    BgHighlight, Radius, BorderW };

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

// Check / radio
inline Surface CheckOff       { BgDark,                BgDark,                Line,  RadiusSm, BorderW };
inline Surface CheckOn        { Blue,                  DarkBlue,              Blue,  RadiusSm, BorderW };

// Colour picker: the SV box's frame (its fill is the bilinear hue blend, so
// only border/radius are read) and the opaque cursor/selection markers
inline Surface ColorField     { Bg,                    Bg,                    Line,  RadiusSm, BorderW };
inline Surface ColorMarker    { Color(0xff,0xff,0xff), Color(0xff,0xff,0xff), BgDark, 2.0f,    BorderW };
// The swatch (internal::ColorDisplay): its fill is the swatch's colour, so only
// border/radius are read. A translucent colour is shown over a checker of
// ColorChecker's top (light) and bottom (dark) cells; only those two are read.
inline Surface ColorSwatch    { Bg,                    Bg,                    Line,  2.0f,     BorderW };
inline Surface ColorChecker   { Color(0xc8,0xc8,0xc8), Color(0x7a,0x7a,0x7a), Line,  0.0f,     0.0f };

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
        {"TextBox",&TextBox},{"TextBoxFocus",&TextBoxFocus},{"TextBoxDisabled",&TextBoxDisabled},{"TextSelection",&TextSelection},
        {"Menu",&Menu},{"MenuStrip",&MenuStrip},{"MenuHover",&MenuHover},
        {"TabActive",&TabActive},{"TabInactive",&TabInactive},{"TabControl",&TabControl},
        {"CategoryList",&CategoryList},{"CategoryInner",&CategoryInner},{"CategoryHeader",&CategoryHeader},
        {"ListItem",&ListItem},{"ListItemSel",&ListItemSel},{"ListItemHover",&ListItemHover},
        {"ScrollTrack",&ScrollTrack},{"ScrollThumb",&ScrollThumb},{"ScrollThumbHover",&ScrollThumbHover},{"ScrollThumbDown",&ScrollThumbDown},
        {"ProgressBack",&ProgressBack},{"ProgressFront",&ProgressFront},
        {"SliderTrack",&SliderTrack},{"SliderNotch",&SliderNotch},{"SliderThumb",&SliderThumb},
        {"SliderThumbHover",&SliderThumbHover},{"SliderThumbDown",&SliderThumbDown},{"SliderThumbDisabled",&SliderThumbDisabled},
        {"CheckOff",&CheckOff},{"CheckOn",&CheckOn},
        {"ColorField",&ColorField},{"ColorMarker",&ColorMarker},{"ColorSwatch",&ColorSwatch},{"ColorChecker",&ColorChecker},
    };
    return m;
}

// ---- paint helper ---------------------------------------------------------
// Standard "draw if visible, toggle on visibility/created change" flow used by
// almost every control renderer. ids must point at >=1 primitive id.
// The overload taking a rect paints it in place of the control's render bounds.
inline void paintSurface(renderer::Base *render, size_t *ids, controls::Base *c, const Surface &s,
    const Rect &rect)
{
    if(!ids)
        return;

    const bool visible=c->visible();

    if(visible)
        render->drawRoundedRect(ids[0], rect, s.radius, s.borderWidth,
            s.top, s.bottom, s.border, c->getZIndex());

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
