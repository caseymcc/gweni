/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_renders_slotRenderer_h_
#define _gweni_renders_slotRenderer_h_


#include <gweni/platforms/baseRender.h>
#include <gweni/objectIdBank.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/markup.h>
#include <freetype-gl/font-manager.h>
#include <freetype-gl/text-buffer.h>

#include <imglib/image.h>

namespace gweni
{
namespace renderer
{

//! What a backend names a texture it made on its device (an atlas or an image):
//! OpenGLCore's is the GL texture name. 0 is no texture.
using TextureHandle=unsigned int;

//! A loaded image: its pixels (kept only while readable) and the backend's texture.
struct SlotTextureData: public gweni::TextureData
{
    SlotTextureData()
    {}
    SlotTextureData(const SlotTextureData &)=delete;
    SlotTextureData(SlotTextureData &&other)
        : SlotTextureData()
    {
        std::swap(width, other.width);
        std::swap(height, other.height);
        std::swap(readable, other.readable);
        std::swap(m_handle, other.m_handle);

        m_readData.swap(other.m_readData);
    }

    //! The backend's texture, from SlotRenderer::createImageTexture; 0 if none
    TextureHandle m_handle{0};
    size_t m_index{std::numeric_limits<size_t>::max()};
    std::vector<unsigned char> m_readData;
};

//! One level of an image's mip chain below level 0 (the image itself).
struct ImageMipLevel
{
    int width=0;
    int height=0;
    std::vector<uint8_t> texels; //!< width x height texels of the image's depth, row 0 first
};

//! Levels 1 to the last (1x1) of the mip chain of a width x height image of
//! `depth` bytes a texel (3 or 4), each made on the CPU from the one above, so
//! every backend uploads the same bytes instead of leaving the filter to its
//! API's mipmap generation. Level n is max(1, width >> n) x max(1, height >> n),
//! GL's and Vulkan's sizes. Texel (x, y) is the 2x2 box (2x..2x+1, 2y..2y+1) of
//! the level above, each channel (a+b+c+d+2)/4: rounded to nearest, alpha
//! straight (not premultiplied), as glGenerateMipmap averaged the stored texels.
//! Where the level above is 1 texel wide (high), the box takes that column (row)
//! twice; where it is an odd size, its last column (row) is in no box.
std::vector<ImageMipLevel> imageMipChain(const uint8_t *texels, int width, int height, int depth);

//! How text below the SDF threshold is anti-aliased (SlotRenderer::setTextAA).
enum class TextAA
{
    Grayscale, //!< one coverage value per pixel
    LcdRGB,    //!< one coverage value per subpixel, red on the left
    LcdBGR     //!< one coverage value per subpixel, blue on the left
};

//! How the text shader turns an atlas sample into alpha. Set per draw, so each
//! atlas's runs can use their own.
enum class TextMode: int
{
    Coverage=0, //!< the sample's red channel is the glyph's coverage
    Sdf=1,      //!< the sample is a signed distance field, 0.5 on the glyph's edge
    LcdRGB=2,   //!< the sample's rgb is the coverage of the pixel's r, g and b subpixels
    LcdBGR=3    //!< as LcdRGB, with the panel's subpixels in b, g, r order
};

//! What SlotRenderer loads a font under: the font at the size its atlas is
//! rasterised at, and the mode it is drawn in, so atlases of different modes
//! are never shared.
struct FontKey
{
    Font font;
    TextMode mode=TextMode::Coverage;

    bool operator==(const FontKey &rhs) const
    {
        return (mode == rhs.mode) && (font == rhs.font);
    }
};

struct FontKeyHash
{
    size_t operator()(const FontKey &key) const noexcept
    {
        size_t res=std::hash<Font>{}(key.font);

        hash_combine<int>(res, static_cast<int>(key.mode));
        return res;
    }
};

//! One draw inside one index chunk (SlotRenderer::indexChunkSlots() slots) as
//! ranges of the static 16-bit quad index buffer, which every chunk reads from
//! its own first slot: an atlas's text runs there, or the runs of a primitive
//! chunk's slots that can show.
struct ChunkDraw
{
    size_t chunk=0; //!< the chunk's first slot is chunk*indexChunkSlots()
    //! each range's index count: 6 per slot
    std::vector<int> counts;
    //! each range's first index as a byte offset (sizeof(uint16_t) per index) from
    //! the chunk's first slot's, as glMultiDrawElements takes it; a backend that
    //! wants the first index divides by sizeof(uint16_t)
    std::vector<uintptr_t> offsets;
};

struct FontData
{
    FontData():
        markup{}, familyPath(nullptr), fontManager(nullptr), atlas(nullptr),
        textureFont(nullptr), spacing(0.f), width(0), height(0)
    {}

    FontData(const FontData &)=delete;

    FontData(FontData &&other)
        : FontData()
    {
        faceName=std::move(other.faceName);
        markup=other.markup;

        familyPath=other.familyPath;
        other.familyPath=nullptr;
        fontManager=other.fontManager;
        other.fontManager=nullptr;
        atlas=other.atlas;
        other.atlas=nullptr;
        textureFont=other.textureFont;
        other.textureFont=nullptr;

        baseSize=other.baseSize;
        spacing=other.spacing;
        width=other.width;
        height=other.height;

        mode=other.mode;
        textRuns=std::move(other.textRuns);
        drawChunks=std::move(other.drawChunks);
        drawListStale=other.drawListStale;
        owners=other.owners;
        freed=other.freed;
        fallback=other.fallback;
        asciiGlyphs=other.asciiGlyphs;
        other.asciiGlyphs.fill(nullptr);
        otherGlyphs=std::move(other.otherGlyphs);
        other.otherGlyphs.clear();
    }

    //! Deletes the manager (which owns the atlas, the texture_font_t and the cache
    //! string), then familyPath. The atlas's texture is the backend's, destroyed
    //! before this runs (SlotRenderer::eraseFont). A moved-from one holds nothing
    //! and frees nothing.
    ~FontData();

    //! texture_font_get_glyph(textureFont, codepoint), which rasterises a glyph it
    //! has not seen yet, from a table instead of its linear search of every glyph
    //! once the codepoint has a glyph. freetype-gl only appends to a font's glyphs
    //! and frees them with the font, so a glyph's pointer holds for textureFont's
    //! life, and its first match (the one it would find) stays the first. A
    //! codepoint with no glyph is not kept, so it is asked for again, as before.
    ftgl::texture_glyph_t *glyph(const char *codepoint);

    std::string faceName;
    ftgl::markup_t markup;
    char *familyPath; //!< owns the string markup.family borrows

    ftgl::font_manager_t *fontManager; //!< owned
    //! inside fontManager; atlas->id is the backend's texture of it (SlotRenderer::createAtlasTexture)
    texture_atlas_t *atlas;
    ftgl::texture_font_t *textureFont;

    //! The px size the atlas is rasterised at: the font's own size for a coverage
    //! font, the shared base size for an SDF one, whose metrics scale by px/baseSize.
    float baseSize=0.f;
    float spacing;

    int width;
    int height;

    //! Coverage: an atlas for one (face, px, bold), drawn as freetype-gl lays it
    //! out. Sdf: one distance field atlas for every px of a (face, bold), laid out
    //! by SlotRenderer::layoutSdfText. LcdRGB/LcdBGR: as Coverage, from a depth-3
    //! atlas of subpixel coverage, drawn with dual-source blending.
    TextMode mode=TextMode::Coverage;

    //! Text ids whose runs were laid out in this font, so flush() draws them with
    //! this atlas. SlotRenderer keeps it in step with each TextRun's fontData.
    std::unordered_set<size_t> textRuns;
    //! What flush() draws for this atlas, one draw per index chunk it touches, in
    //! slot order: the glyph slots in use of the runs in textRuns that draw
    //! something, adjacent ones merged, split at the chunks' edges. Rebuilt from
    //! textRuns when drawListStale, which is set only when a run's part of it
    //! (SlotRenderer::drawnRange) or its font changed; a rebuild reuses the
    //! entries and their vectors, so in steady state it allocates nothing.
    std::vector<ChunkDraw> drawChunks;
    bool drawListStale=true;
    //! Successful loadFont calls not yet matched by a freeFont. A font loaded only
    //! by a use (ensureFont, from renderText or measureText) has none, and is
    //! erased when its last run leaves it (setTextRunFont), so the sizes a zoom
    //! or a canvas scale passes through do not stay.
    int owners=0;
    //! Freed by its last owner (or by a freeFont with none) while runs are still
    //! laid out in it: the atlas is kept, every run drawing, until its last run
    //! leaves it (setTextRunFont), unless a load before then gives it an owner (a
    //! use does not: it goes with its last run all the same).
    bool freed=false;
    //! The face's file was missing or unreadable, so textureFont is the built-in
    //! fallback font (SlotRenderer::setFallbackFont), loaded from memory into this
    //! manager's atlas; loadFont reports it as Font::Status::LoadedFallback.
    bool fallback=false;
    //! glyph()'s table of textureFont's glyphs, by utf8_to_utf32's codepoint as
    //! freetype-gl keys them: the ASCII ones in place, the rest in otherGlyphs
    std::array<ftgl::texture_glyph_t *, 128> asciiGlyphs{};
    std::unordered_map<uint32_t, ftgl::texture_glyph_t *> otherGlyphs;
};

//
//! The part of a retained renderer that does not depend on the API it draws with.
//
//! Every primitive id is a fixed slot of 4 vertexes in a CPU mirror, every text id
//! a run of glyph slots in a second one, and each slot carries the clip box it was
//! drawn in. This class packs the vertexes (setVertex, writeRoundedQuad, drawLine's
//! placement, cullOutOfRangeQuad), diffs each redrawn slot against what it held so
//! only changed slots are marked, allocates the text runs and builds each atlas's
//! draw list, resolves fonts to their coverage, LCD or SDF atlases and lays text out
//! in them (the freetype-gl atlases are CPU-side, with a dirty row band), keeps
//! the loaded images and counts FrameStats.
//
//! A backend (OpenGLCore) implements the protected pure-virtual hooks below: it
//! creates the device's buffers and textures, copies the ranges flush() hands it
//! and records one draw per chunk. Every count in FrameStats is made here, from
//! what flush() asks the hooks to do, so two backends count alike by construction.
//! A backend's destructor calls releaseResources() first, while its device is
//! still there to destroy the textures in.
//
class GWENI_EXPORT SlotRenderer: public gweni::renderer::Base
{
public:

    SlotRenderer(ResourcePaths &paths, const gweni::Rect &viewRect);
    //! Frees the CPU-side fonts, images and mirrors; the device's textures are gone
    //! by then (releaseResources, which the backend's destructor calls).
    virtual ~SlotRenderer();

    //! Nothing of the mirrors is on the device yet: the next flush() sends it all
    //! once. A backend calls it from its own init(), then creates its objects.
    void init() override;
    //! A backend calls it from its own, then sets its projection and viewport.
    void setViewRect(const gweni::Rect &viewRect) override;
    const gweni::Rect &viewRect() const { return m_viewRect; }

    //! A projective transform of the view's own space (device px, y down, z a z
    //! index), applied to every vertex before the backend's projection: 16 floats,
    //! column-major (glm's layout). What it maps a point to must stay inside the
    //! view's z range, 0..kMaxZIndex, or is clipped. The identity, and no transform
    //! at all, draw the front view; tools::DepthView sets a camera. The backend
    //! multiplies it into its projection here and at setViewRect, never per frame,
    //! and nothing of the mirrors changes, so a frame after it uploads nothing.
    void setViewTransform(const float matrix[16]);
    //! Back to the front view, the backend's projection alone
    void clearViewTransform();
    bool hasViewTransform() const { return m_hasViewTransform; }
    //! The transform set, column-major; the identity when there is none
    const float *viewTransform() const { return m_viewTransform; }

    //! Vertex::z is 16 bits unsigned, so a z index is 0..65535. A backend's
    //! projection maps z=0 to the depth-clear value, and kMaxZIndex to the front.
    static constexpr int kMaxZIndex=65535;
    int maxZIndex() const override { return kMaxZIndex; }

    void begin() override;
    //! flush(): the uploads and the draws of the frame
    void end() override;

    void setDrawColor(gweni::Color color) override;
    void drawFilledRect(size_t primitiveId, gweni::Rect rect, int zIndex) override;
    void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
        gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex) override;
    //! Style 4 with the edge mask in Vertex::radius's high byte; the radius, in its
    //! low byte, is at most 255 device px (a larger one is that, which draws the
    //! same in a rect no more than 510 px across on its shorter side). \a reach
    //! widens the quad on the borderless sides (renderer::Base's).
    void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
        gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex,
        uint8_t edges, float reach=0.0f) override;
    void drawBilinearRect(size_t primitiveId, gweni::Rect rect, int zIndex,
        gweni::Color c00, gweni::Color c10, gweni::Color c01, gweni::Color c11,
        float radius, float border, gweni::Color borderColor) override;
    void drawCheckerRect(size_t primitiveId, gweni::Rect rect, int zIndex,
        gweni::Color fill, gweni::Color light, gweni::Color dark,
        float radius, float border, gweni::Color borderColor) override;
    void drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
        gweni::Color color, int zIndex) override;
    //! Style 3: the capsule's distance evaluated in the fragment shader, over a quad
    //! of the two endpoints' stroke squares and 1 px around them. The neighbours go
    //! in the slot's vertexes (Vertex: radius, bx/by, dx/dy); one farther than
    //! 16383.5 device px from the quad's centre on either axis is left out, and that
    //! joint is blended as one without neighbours is.
    void drawLine(size_t primitiveId, gweni::PointF p0, gweni::PointF p1,
        float thickness, int zIndex, const gweni::PointF *prev=nullptr,
        const gweni::PointF *next=nullptr) override;

    void startClip() override;
    void endClip() override;

    void generatePrimitive(size_t &primitiveId) override;
    void releasePrimitive(size_t primitiveId) override;

    void drawTexturedRect(const gweni::Texture &texture, size_t primitiveId, gweni::Rect targetRect, int zIndex, float u1=0.0f,
        float v1=0.0f, float u2=1.0f, float v2=1.0f) override;

    void showPrimitive(int style, size_t primitiveId) override;
    void hidePrimitive(size_t primitiveId) override;

    gweni::Color pixelColor(const gweni::Texture &texture,
        unsigned int x, unsigned int y,
        const gweni::Color &col_default) override;

    void generateTextPrimitive(size_t &primitiveId) override;
    void releaseTextPrimitive(size_t primitiveId) override;

    void renderText(size_t textId, const gweni::Font &font,
        gweni::Point pos, int zIndex,
        const gweni::String &text) override;

    void showText(size_t textId) override;
    void hideText(size_t textId) override;

    //! In canvas units at any scale(), unlike the legacy renderers (see the definition)
    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override;
    //! For tests: measureText as freetype-gl's text_buffer_add_text measures a
    //! coverage font, the run laid out in a text_buffer_t and its pen read: the
    //! pen after the last line rounded up, and one line box. It is the reference
    //! measureText's pen walk must equal for a run of one line, and of each line
    //! of a longer one (tests/measureTextPens.cpp). Where they are not null, penY
    //! gets the pen's y after the run (y up, from 0: a line's baseline is its
    //! ascender below the one before, and each '\n' moves it down again) and
    //! lineBox the line box unrounded. An Sdf font is measured as measureText
    //! measures it, and leaves penY and lineBox as they are.
    gweni::Point measureTextWithTextBuffer(const gweni::Font &font, const gweni::String &text,
        float *penY=nullptr, float *lineBox=nullptr);
    //! measureText's `y` for one line, read from the font's metrics: in canvas
    //! units at any scale(), 0 for a font that does not load
    int lineHeight(const gweni::Font &font) override;
    //! One layout of the whole string, the one measureText makes of each prefix
    //! (the coverage walkPens, or layoutSdfText), read at each boundary; the
    //! default where freetype-gl's codepoint steps are not utf8Next's
    void measureCarets(const gweni::Font &font, const gweni::String &text,
        std::vector<int> &xs) override;

    // Resource Loader
    gweni::Font::Status loadFont(const gweni::Font &font) override;
    void freeFont(const gweni::Font &font) override;
    bool ensureFont(const gweni::Font &font) override;

    Texture::Status loadTexture(gweni::Texture &texture) override;
    void freeTexture(const gweni::Texture &texture) override;
    TextureData getTextureData(const gweni::Texture &texture) const override;
    bool ensureTexture(const gweni::Texture &texture) override;
    SlotTextureData *getFreeTextureData();

    //! The backend's texture of a loaded texture, 0 if it is not loaded
    TextureHandle getTextureHandle(const gweni::Texture &texture) const;
    //! The backend's texture of a loaded font's glyph atlas, 0 if the font is not loaded
    TextureHandle getFontAtlasHandle(const gweni::Font &font) const;
    //! Bytes per texel of a loaded font's glyph atlas (1: coverage or distance,
    //! 3: subpixel coverage), 0 if the font is not loaded
    size_t getFontAtlasDepth(const gweni::Font &font) const;
    //! Fonts loaded and not freed yet
    size_t getLoadedFontCount() const { return m_fonts.size(); }
    //! The TextMode a font is drawn in: Sdf at or above the GWENI_SDF_TEXT_MIN
    //! threshold (effective px); below it Coverage, or LcdRGB/LcdBGR as textAA() says
    TextMode getTextMode(const gweni::Font &font) const;
    //! The whole device px a grayscale coverage atlas for text drawn at \a px is
    //! rasterised at: px rounded half up, at least 1 (an LCD one is at px).
    static float coverageSize(float px);

    //! How text below the SDF threshold is anti-aliased; GWENI_TEXT_AA=gray|rgb|bgr
    //! sets it at construction, Grayscale otherwise. A change replaces every
    //! coverage font with one in the new mode, lays no run out again by itself
    //! (each draws nothing until its next renderText) and raises
    //! textLayoutSerial(), so a canvas lays out and renders its whole tree again.
    //! Where the device cannot blend LCD text (lcdTextAvailable()), LcdRGB and
    //! LcdBGR are Grayscale, and the first time either is asked for (here or by
    //! GWENI_TEXT_AA) the renderer logs one warning saying why.
    void setTextAA(TextAA aa);
    TextAA textAA() const { return m_textAA; }

    //! Why the LCD modes are not available on this device, or nullptr when they
    //! are: they blend each channel by its own coverage, which needs dual-source
    //! blending. A backend that has it everywhere keeps this default; one that
    //! finds out in init() answers from then on and calls applyLcdTextSupport().
    virtual const char *lcdTextUnavailable() const { return nullptr; }
    bool lcdTextAvailable() const { return lcdTextUnavailable() == nullptr; }

    //! The LCD modes' contrast correction: the subpixel coverage of text of
    //! luminance L is raised through c*(k+1)/(c*k+1), k=contrast*L, so light text
    //! over a dark background gets more and black text none. 0 turns it off.
    //! GWENI_TEXT_CONTRAST=<f> sets it at construction, 1 otherwise. Grayscale and
    //! SDF text do not use it. A change shows from the next frame on, with nothing
    //! laid out again.
    void setTextContrast(float contrast) { m_textContrast=std::max(0.0f, contrast); }
    float textContrast() const { return m_textContrast; }

    //! Whether loadFont draws a face whose file is missing or unreadable in the
    //! built-in fallback font (OpenSans, compiled into the library), returning
    //! Font::Status::LoadedFallback, rather than failing with the face drawing no
    //! text. On by default; off is for tests of the failure path. A change applies
    //! to the next load: what is loaded stays as it is.
    void setFallbackFont(bool enabled) { m_fallbackFont=enabled; }
    bool fallbackFont() const { return m_fallbackFont; }

    //! The freetype-gl font a font's text is drawn from (resolved at its draw px),
    //! nullptr if it is not loaded: a test hook, to look its glyphs up.
    ftgl::texture_font_t *getTextureFont(const gweni::Font &font) const;

    //! Reads a loaded font's glyph atlas back from the device as RGBA8 rows (an R8
    //! texel's coverage in red), row 0 first. False, with rgba empty, if the font
    //! is not loaded or the backend cannot read it. A test hook.
    bool readFontAtlas(const gweni::Font &font, std::vector<uint8_t> &rgba);

    //! What the last flush() actually did. Reading it is how the "only what changed
    //! is uploaded" claim gets checked from outside the renderer.
    struct FrameStats
    {
        size_t frameIndex=0;            //!< frames flushed so far
        //! distinct primitive slots this frame uploads (vertexes or clip box): those a draw,
        //! hide, show or release changed since the last flush, each once. A slot changed and
        //! put back within the frame is uploaded and counted; a buffer regrow re-sends every
        //! slot but counts only these.
        size_t primitiveSlotsWritten=0;
        size_t textSlotsWritten=0;      //!< distinct glyph slots this frame uploads (vertexes or clip box)
        //! uploads of the vertex buffers (uploadVertexRange), and the clip boxes'
        //! (uploadClipBoxRows, one per row piece a dirty run covers)
        size_t uploadCalls=0;
        size_t uploadBytes=0;           //!< bytes those calls sent
        size_t mirrorBytes=0;           //!< size of the whole CPU mirror, for comparison
        //! draws: one per primitive chunk with a slot that can show, one per text
        //! chunk of an atlas (each a drawTextChunk, however many ranges it holds)
        size_t drawCalls=0;
        //! primitive slots the primitive draws covered: the slots of the ranges
        //! passed to drawPrimitiveChunk. A chunk with no slot whose style is not 0
        //! is not drawn, and a drawn chunk's ranges are its runs of such slots, a
        //! gap of fewer than primitiveRangeGap() hidden slots merged into the runs
        //! around it and at most maxPrimitiveRanges() ranges (the smallest gaps
        //! merged past that), so hidden and released slots (a hidden page, the ids a
        //! deleted tree gave back) cost no vertex work but in a merged gap, where
        //! each is 4 vertexes that make nothing.
        size_t primitiveSlotsDrawn=0;
        //! the ranges of those draws: one per run of drawn slots
        size_t primitiveRanges=0;
        //! primitive chunks whose ranges this flush built again from the slots that
        //! can show: those where a slot's visibility changed since their last
        //! draw (or every one, after a chunk size or range limit change), so a
        //! frame that shows and hides nothing builds none
        size_t primitiveRangeBuilds=0;
        size_t textDraws=0;             //!< of those, the text draws: one per atlas with runs to draw
        size_t atlasUploadCalls=0;      //!< uploads of font atlas row bands (uploadAtlasRows)
        size_t atlasUploadBytes=0;      //!< bytes those calls sent
        //! vertex buffers and clip-box tables re-allocated this frame because the
        //! mirror outgrew them, each sending its whole mirror once; capacity grows
        //! geometrically, so over K slots added that is O(log K) frames
        size_t bufferRegrows=0;
        //! how long this flush took, from its start to the end of submitFrame(), on
        //! platform::profileTimeMicros(): two reads of that clock per frame
        uint64_t flushMicros=0;
    };

    const FrameStats &getFrameStats() const { return m_frameStats; }

    //! getFrameStats() as renderer::Base's backend-independent counts
    FrameCounts frameStats() const override;
    //! The primitive slots and text runs the last flush() uploaded: the slots
    //! FrameStats::primitiveSlotsWritten and textSlotsWritten count, by id.
    const std::vector<size_t> &writtenPrimitiveIds() const override { return m_writtenPrimitiveIds; }
    const std::vector<WrittenText> &writtenTextIds() const override { return m_writtenTextIds; }

    //! The most slots one draw covers. Every primitive and glyph slot is a quad, 4
    //! vertexes and 6 indexes in a static 16-bit index buffer that every draw of its
    //! path shares (slot i of a chunk is vertexes i*4..i*4+3 from the chunk's first
    //! vertex), so the buffers are drawn in chunks of at most this many slots, each
    //! with its vertex attributes pointed at its first slot. 16383, not 16384: WebGL 2
    //! always restarts the primitive at index 0xFFFF, and 16384 quads would end on
    //! 65535. The largest index is 65531.
    static constexpr size_t kSlotsPerChunk=16383;

    //! How far an SDF glyph's distance field reaches past each edge of its ink, in
    //! px at the SDF atlas's base size (texels of the atlas). The field is 0.5 on
    //! the edge and changes by about 1/(2*kSdfSpread) per texel, which the text
    //! shader's anti-aliasing width is computed from.
    static constexpr int kSdfSpread=6;

    //! The slots one draw covers, kSlotsPerChunk unless GWENI_INDEX_CHUNK_SLOTS=<n>
    //! (read at construction) or setIndexChunkSlots() made it smaller. A small value
    //! makes an ordinary scene span many chunks, which draws the same pixels.
    size_t indexChunkSlots() const { return m_chunkSlots; }
    //! Sets indexChunkSlots(), clamped to [1, kSlotsPerChunk]; the atlases' draw
    //! lists are made again as the next flush() needs them, and the backend is told
    //! (indexChunkSlotsChanged). A test hook.
    void setIndexChunkSlots(size_t slots);
    //! Draws every primitive slot up to the mirror's end, each chunk whole and none
    //! skipped, as flush() did before it drew only the slots that can show. A test
    //! hook, to check the skip draws the same pixels; off by default.
    void setDrawEveryPrimitiveSlot(bool every) { m_drawEveryPrimitiveSlot=every; }
    //! A primitive chunk's ranges: a gap of fewer than kPrimitiveRangeGap hidden
    //! slots between two runs of slots that can show is drawn with them, and a
    //! chunk draws at most kMaxPrimitiveRanges ranges, the smallest gaps (the
    //! leftmost of equal ones) merged past that. Measured with gweni_bench's
    //! hidden-gaps scene (docs/architecture.md, "Only the slots that can show are
    //! drawn"): merging a gap of 1 is a gain for the glDrawElements loop
    //! and costs nothing with multi-draw, one of 2 or more a loss for both, so
    //! only 1-slot gaps are merged; the cap bounds a chunk's loop (the web's) to 16 draws.
    static constexpr size_t kPrimitiveRangeGap=2;
    static constexpr size_t kMaxPrimitiveRanges=16;
    size_t primitiveRangeGap() const { return m_primitiveRangeGap; }
    size_t maxPrimitiveRanges() const { return m_maxPrimitiveRanges; }
    //! Sets primitiveRangeGap() and maxPrimitiveRanges() (at least 1), every
    //! chunk's ranges built again at the next flush(). A test and measurement hook;
    //! GWENI_PRIMITIVE_RANGES=<gap>,<cap> sets them at construction.
    void setPrimitiveRangeLimits(size_t gap, size_t maxRanges);

    //! What every live id holds in the CPU mirrors, so two states of the UI can be
    //! compared id by id rather than as raw buffers.
    //
    //! A primitive id's entry is the bytes of its slot: its 4 vertexes (at id*4),
    //! its indexes being the static quad pattern every slot shares. A text id's
    //! entry is its hidden flag followed by the vertexes of the glyphs in use, and not where the run sits:
    //! allocateTextRun may hand the same string other slots after a free, and that
    //! draws the same. A test hook: it copies the whole mirror, so keep it out of
    //! the frame loop.
    struct MirrorSnapshot
    {
        std::map<size_t, std::vector<uint8_t>> primitives;  //!< primitive id -> slot bytes
        std::map<size_t, std::vector<uint8_t>> texts;       //!< text id -> run contents
    };

    MirrorSnapshot snapshotMirror() const;

    //! Clears the render target to color, and its depth to the far plane.
    virtual void clearTarget(const gweni::Color &color)=0;
    //! Reads the render target's top-left width x height back as RGBA8, top row
    //! first, once what was submitted has been drawn.
    virtual void readPixels(int width, int height, std::vector<uint8_t> &pixels)=0;
    //! Whether a texture handle names a texture the device still has: a test hook,
    //! to check a freed font or image destroyed its texture.
    virtual bool textureExists(TextureHandle handle) const=0;
    //! The red, green, blue and alpha bits of a texture's texels (8,0,0,0 for an R8
    //! atlas, 8,8,8,0 for an RGB8 one); false if the backend cannot tell. A test hook.
    virtual bool textureChannelSizes(TextureHandle handle, int bits[4]) const=0;
    //! Whether the device reported an error since the last call (which this
    //! clears). A test hook.
    virtual bool deviceError()=0;

protected:

    //! A primitive vertex: 16 16-bit fields, the layout a backend's vertex input
    //! reads.
    struct Vertex
    {
        uint16_t style;
        int16_t  x, y;   //device px, signed: a quad must fit in +/-32 K (cullOutOfRangeQuad)
        uint16_t z;      //0..kMaxZIndex, the projection's depth range (read in the shader as inPosition.z & 0xFFFF)
        uint16_t tx, ty; //color0 (top fill) packed rg/ba, or UV for texture (style 2, 0..65535 = 0..1)
        // SDF fields (styles 3-7); left 0 for all other styles.
        int16_t  lx, ly; //local position relative to rect center, in half-pixel units (= +/-size at the four corners, so an odd size is exact)
        uint16_t radius; //styles 4, 6, 7: corner radius in px (0..255) in the low byte, the
                         //edge mask (RoundedEdge) in the high byte; icon kind (style 5);
                         //image unit (style 2);
                         //line (style 3): LineHasPrev | LineHasNext, the neighbours it holds
        uint16_t border; //border thickness in px; line thickness in px (style 3)
        uint16_t cx, cy; //color1 (bottom fill for vertical gradient, image tint for style 2) packed rg/ba;
                         //line (style 3): its first endpoint, int16_t bits, in lx/ly's units
        uint16_t bx, by; //border color packed rg/ba; icon stroke px in bx's high byte (style 5);
                         //line (style 3): its previous point with LineHasPrev, as cx/cy, else 0
        uint16_t dx, dy; //color3 packed rg/ba: bottom edge (style 6) or checker dark (style 7);
                         //line (style 3): its next point with LineHasNext, as cx/cy, else its
                         //second endpoint (-cx/-cy: the quad is centred on the segment)
    };

    //! Style 3's radius bits: which neighbours of the segment its vertexes hold.
    enum : uint16_t
    {
        LineHasPrev=1,
        LineHasNext=2
    };

    static_assert(sizeof(Vertex) == 32, "Vertex is 16 16-bit fields; the GPU attribute layout depends on it");

    //! A clip box in pixel space (the vertexes' space, snapped like translate()):
    //! the fragment shaders keep [x0,x1) x [y0,y1) of the slot it belongs to and
    //! discard the rest. Read on the device as one RGBA16UI texel of a table
    //! kClipColumns boxes wide, slot s at (s % kClipColumns, s / kClipColumns).
    struct Box
    {
        uint16_t x0, y0;
        uint16_t x1, y1;
    };

    static_assert(sizeof(Box) == 8, "Box is one RGBA16UI texel of the clip-box tables");

    //! The clip-box tables' width in boxes, log2: 2048, so a table holds 2048
    //! slots per row and up to 2048 rows, GLES 3.0's least GL_MAX_TEXTURE_SIZE.
    static constexpr unsigned int kClipColumnsLog2=11;
    static constexpr size_t kClipColumns=size_t(1) << kClipColumnsLog2;

    //! Style-2 slots sample their texture from one of ImageUnits image units, all
    //! bound for the one primitive draw. A slot keeps its unit in its vertexes
    //! across frames, so a texture keeps its unit until freeTexture.
    static constexpr size_t ImageUnits=8;
    //! The texture each image unit holds, 0 free (imageUnit())
    TextureHandle m_imageUnits[ImageUnits]={};

    //! The two sets of slots flush() uploads and draws: the primitives' (Vertex)
    //! and the glyphs' (ftgl::glyph_vertex_t), each a vertex buffer and a clip-box
    //! table of its own.
    enum class SlotPath
    {
        Primitives,
        Text
    };

    //! A backend's init(), once it knows lcdTextUnavailable(): an LCD mode already
    //! set (GWENI_TEXT_AA at construction, a setTextAA before init()) falls back
    //! to Grayscale as setTextAA would.
    void applyLcdTextSupport();

    // ---- The backend's hooks, in the order flush() calls them ----

    //! begin(): the state the frame's draws need (blending, the depth test LESS)
    virtual void beginFrame()=0;
    //! The device buffer of a path is (re)allocated to hold `bytes`, its old
    //! contents dropped; uploadVertexRange fills it.
    virtual void allocateVertexBuffer(SlotPath path, size_t bytes)=0;
    //! Copies bytes [offset, offset+bytes) of a path's mirror (data) to its buffer.
    virtual void uploadVertexRange(SlotPath path, size_t offset, size_t bytes, const void *data)=0;
    //! The most rows a clip-box table may have (the device's largest 2D texture)
    virtual size_t maxClipBoxRows() const=0;
    //! A path's clip-box table is (re)allocated, kClipColumns boxes by `rows`,
    //! its contents dropped.
    virtual void allocateClipBoxes(SlotPath path, size_t rows)=0;
    //! Copies width x height boxes (rows of kClipColumns in the mirror, boxes
    //! pointing at the first) to the table at (x, y).
    virtual void uploadClipBoxRows(SlotPath path, size_t x, size_t y, size_t width, size_t height,
        const Box *boxes)=0;
    //! A texture for a font's atlas, its size and depth (1: R8, 3: RGB8), contents
    //! undefined until uploadAtlasRows. Called from loadFont.
    virtual TextureHandle createAtlasTexture(const texture_atlas_t &atlas)=0;
    //! Copies rows [firstRow, firstRow+rows) of the atlas's pixels (atlas.data,
    //! full-width rows of atlas.depth bytes a texel) to its texture.
    virtual void uploadAtlasRows(TextureHandle texture, const texture_atlas_t &atlas, size_t firstRow, size_t rows)=0;
    virtual void destroyAtlasTexture(TextureHandle texture)=0;
    //! A mipmapped, edge-clamped texture of a loaded image (rgba: 4 bytes a pixel,
    //! else 3, RGB sampled with alpha 1); 0 if it cannot be made.
    virtual TextureHandle createImageTexture(const SlotTextureData &image, bool rgba)=0;
    //! Called for every image's data as it is deleted, handle 0 included
    virtual void destroyImageTexture(TextureHandle texture)=0;
    //! Reads a texture's level 0 back as RGBA8 rows, row 0 first; false (rgba
    //! empty) if it cannot be read.
    virtual bool readTextureRgba(TextureHandle texture, int width, int height, std::vector<uint8_t> &rgba)=0;
    //! The primitive draws: begin, one drawPrimitiveChunk per drawn chunk in slot
    //! order, end. A chunk's draw is its ranges, in slot order, as one draw: its
    //! vertexes from its first slot's (draw.chunk*indexChunkSlots()*4), each range
    //! 2 triangles of the static quad indexes per slot from its offset.
    virtual void beginPrimitiveDraws()=0;
    virtual void drawPrimitiveChunk(const ChunkDraw &draw)=0;
    virtual void endPrimitiveDraws()=0;
    //! The text draws: begin, then for each atlas with runs to draw a
    //! beginTextAtlas (its texture and TextMode, an LCD one blended per channel)
    //! and one drawTextChunk per ChunkDraw of its list, then end
    virtual void beginTextDraws()=0;
    virtual void beginTextAtlas(const FontData &font)=0;
    virtual void drawTextChunk(const ChunkDraw &draw)=0;
    virtual void endTextDraws()=0;
    //! The end of flush(): the frame's commands go to the device
    virtual void submitFrame()=0;
    //! setIndexChunkSlots() changed the chunk size: what a backend made per chunk
    //! points at the old chunks' slots
    virtual void indexChunkSlotsChanged()=0;
    //! setViewTransform() or clearViewTransform() changed the view transform: the
    //! backend builds its projection again (viewTransform() times its own)
    virtual void viewTransformChanged()=0;

    //! Destroys every font atlas's and image's texture through the hooks and frees
    //! them. A backend's destructor calls it while its device is still there.
    void releaseResources();

    /**
        * @brief Perform actual rendering.
        */
    void flush();

    //! The effective px a font's text is drawn at: font.size*scale(). measureText
    //! measures at font.size instead, so it stays in canvas units.
    float textPx(const Font &font) const;
    //! The m_fonts key a font is loaded under at px, with the mode it is drawn in:
    //! px >= m_sdfTextMin is (face, bold) in Sdf mode, with size 0 standing for
    //! "every size"; below that it is (face, px, bold) in Coverage mode, or in
    //! m_textAA's LCD mode.
    FontKey resolveFont(const Font &font, float px) const;
    //! loadFont/ensureFont at px rather than textPx(font): measureText's font size
    Font::Status loadFont(const Font &font, float px);
    bool ensureFont(const Font &font, float px);
    //! Frees one resolved font if no owner holds it: erased if no run is laid out
    //! in it, otherwise marked freed (or, onlyIfUnused, left as it is)
    void freeFont(const FontKey &resolved, bool onlyIfUnused);

    using FontMap=std::unordered_map<FontKey, FontData, FontKeyHash>;

    //! Destroys a font's atlas texture and erases it; the iterator after it
    FontMap::iterator eraseFont(FontMap::iterator it);
    //! Destroys an image's texture and deletes its data
    void deleteTextureData(SlotTextureData *textureData);

    FontMap m_fonts; //!< keyed by resolveFont()
    //! Faces whose last load failed: ensureFont does not retry them
    std::unordered_set<FontKey, FontKeyHash> m_failedFonts;
    std::vector<SlotTextureData *> m_textureData;
    std::vector<size_t> m_freeTextureData;
    std::unordered_map<Texture, size_t> m_textures;

    std::pair<const FontKey, FontData> *m_lastFont;
    size_t m_lastTexture;

private:

    //
    //! Set of primitive slots whose CPU-side data has diverged from the device.
    //
    //! A slot is a fixed, equally sized run of elements in a buffer (4 vertexes or 6
    //! indexes per primitive id), so the set only has to remember slot numbers; flush()
    //! turns them into as few contiguous uploads as it can.
    //
    class DirtySlots
    {
    public:
        void mark(size_t slot)
        {
            if(m_marked.size() <= slot)
                m_marked.resize(slot+1, 0);
            if(m_marked[slot])
                return;
            m_marked[slot]=1;
            m_slots.push_back(slot);
        }

        //! Marks slots [first, last): the ones a mirror gained since its last
        //! upload. A new slot whose bytes are still zero is marked by no draw, but
        //! the device holds nothing defined there until it is sent.
        void markRange(size_t first, size_t last)
        {
            for(size_t slot=first; slot<last; ++slot)
                mark(slot);
        }

        bool empty() const { return m_slots.empty(); }
        size_t count() const { return m_slots.size(); }
        bool marked(size_t slot) const { return (slot < m_marked.size()) && m_marked[slot]; }
        const std::vector<size_t> &slots() const { return m_slots; }

        void clear()
        {
            for(size_t slot: m_slots)
                m_marked[slot]=0;
            m_slots.clear();
        }

        //! Calls fn(firstSlot, lastSlotExclusive) for each maximal run of dirty slots.
        template<typename Fn>
        void forEachRun(Fn &&fn)
        {
            std::sort(m_slots.begin(), m_slots.end());

            size_t i=0;

            while(i < m_slots.size())
            {
                size_t begin=m_slots[i];
                size_t end=begin+1;

                while((i+1 < m_slots.size()) && (m_slots[i+1] <= end))
                {
                    end=m_slots[i+1]+1;
                    ++i;
                }

                fn(begin, end);
                ++i;
            }
        }

    private:
        std::vector<uint8_t> m_marked;
        std::vector<size_t> m_slots;
    };

    //! Everything a primitive id owns in the CPU mirrors, as it stood before a redraw.
    struct PrimitiveSlot
    {
        Vertex vertexes[4];
        Box box;
    };

    //! Grows the CPU mirrors so the primitive's slots exist.
    void ensurePrimitiveSlots(size_t primitiveId);
    void readPrimitiveSlot(size_t primitiveId, PrimitiveSlot &slot) const;
    //! Marks only the slots whose bytes differ from `before`, so a control that redraws
    //! itself unchanged (the skin re-emits every frame) costs no upload.
    void markPrimitiveChanges(size_t primitiveId, const PrimitiveSlot &before);

    //! The current clipRegion() as a Box: its edges snapped like translate()'s, shrunk by
    //! GWENI_CLIP_DEBUG's inset, then clamped to [0, 65535] with x1 >= x0 and y1 >= y0
    //! (an empty region gives an empty box, which discards everything).
    Box makeClipBox() const;

    //! Uploads only the dirty slots of a path's vertex buffer, and the slots the
    //! mirror gained since the last upload (sentElements on), as coalesced
    //! uploadVertexRange runs. When the mirror has outgrown the device allocation
    //! (capacityElements), the buffer is re-allocated geometrically, to 1.5 times
    //! the mirror in whole slots (more than 1.5 times its old capacity), and the
    //! whole mirror sent once (a regrow, FrameStats::bufferRegrows). Clears dirty
    //! on return.
    void uploadDirty(SlotPath path, size_t &capacityElements, size_t &sentElements,
        const void *data, size_t elementSize, size_t elementCount,
        DirtySlots &dirty, size_t elementsPerSlot);
    //! The same for a path's clip-box table, kClipColumns boxes per row: the dirty
    //! slots' runs, and the boxes the mirror gained since sentBoxes, a run that
    //! crosses rows split at them (its part in the first row, the whole rows
    //! between, its part in the last). Only when the table's rows no longer hold
    //! the mirror is it re-allocated, the power of two rows at or above 1.5 times
    //! what the mirror needs, and every box sent again (a regrow). Clears dirty.
    void uploadDirtyClipBoxes(SlotPath path, size_t &rows, size_t &sentBoxes,
        const std::vector<Box> &boxes, DirtySlots &dirty);
    //! Boxes [first, last) of a path's clip-box table as uploadClipBoxRows calls,
    //! one per row piece (a partial first row, the whole rows, a partial last row),
    //! counted in FrameStats::uploadCalls/uploadBytes
    void uploadClipBoxRange(SlotPath path, const std::vector<Box> &boxes, size_t first, size_t last);
    //! Uploads the rows of every font atlas that freetype-gl rasterised glyphs into since
    //! the last upload (renderText and measureText do so lazily), then empties the band.
    void uploadDirtyAtlases();

    //! The pen x of each glyph a run lays out in fontData at px (one per glyph
    //! quad, in quad order), from the start of its line, and the line it is on in
    //! `lines` (0 for the first) where that is not null.
    void textPens(FontData &fontData, float px, const gweni::String &text, std::vector<float> &pens,
        std::vector<int> *lines);
    //! What walkPens measures of a run: its widest line's pen, unrounded, and its
    //! lines, one more than its '\n's (a trailing one is an empty last line).
    struct PenWalk
    {
        float width;
        int lines;
    };
    //! The one pen walk of a run in fontData at px, which textPens, a coverage
    //! measureText and measureCarets read: the codepoints as text_buffer_add_text
    //! steps through them (to a NUL in a coverage font), each glyph's kerning
    //! against the codepoint before and its advance, with no text_buffer_t, a '\n'
    //! taking the pen back to the line's start. Gives the widest line and the line
    //! count; appends textPens' pen of each glyph to `pens` and its line to
    //! `glyphLines`, and the width measureText gives the prefix before each
    //! codepoint (the widest line so far, rounded up) to `carets`, where they are
    //! not null.
    PenWalk walkPens(FontData &fontData, float px, const gweni::String &text,
        std::vector<float> *pens, std::vector<int> *carets, std::vector<int> *glyphLines=nullptr);
    //! At a scale() other than 1, a run measured in a coverage font (font.size below
    //! the threshold) is drawn in another font, whose advances are not the measured
    //! ones scaled: hinted, whole-pixel at font.size. shifts gets, for each drawn glyph,
    //! the x that puts it at its measured pen position scaled, so the run ends where
    //! measureText sized it and a caret placed from measureText falls between the
    //! glyphs it means; it is left empty where nothing moves. Nor is the drawn
    //! font's lineStep the measured one scaled (a coverage font truncates its
    //! descender to a whole pixel, an Sdf one rounds its step), so lineShifts gets, for each drawn glyph of a run with a '\n',
    //! the whole device px down that puts its line's baseline within half a pixel of
    //! the measured line's, scaled, relative to the first line; it is left empty
    //! where no line moves, and always for a run of one line.
    void measuredPenShifts(const gweni::Font &font, FontData &drawn, float px,
        const gweni::String &text, std::vector<float> &shifts, std::vector<float> &lineShifts);
    //! Lays text out in an Sdf font at px, in freetype-gl's convention (y up, the
    //! line's top at 0), as text_buffer_add_text does for a coverage font: metrics
    //! scaled by px/baseSize, unhinted advances, the baseline on a whole pixel.
    //! Appends 4 vertexes per glyph to `vertexes` when it is not null, and gives
    //! the size measureText reports. Rasterises the glyphs it has not seen yet.
    //! With `carets`, it gets the width measureText reports of the prefix before
    //! each codepoint step, and of the whole string last (measureCarets).
    gweni::Point layoutSdfText(FontData &fontData, float px, const gweni::String &text,
        const gweni::Color &color, std::vector<ftgl::glyph_vertex_t> *vertexes,
        std::vector<int> *carets=nullptr);
    //! The height of one line box in fontData at px, before measureText rounds it
    //! up: ascender-descender, no line gap (font->height), scaled by px/baseSize in
    //! an Sdf font. measureText, layoutSdfText and lineHeight all take it from here.
    static float lineBoxHeight(const FontData &fontData, float px);
    //! The distance from one line's baseline to the next in fontData at px, as the
    //! run is drawn: in a coverage font text_buffer_add_text's, ascender less the
    //! descender truncated to a whole pixel (text_buffer_finish_line moves the pen
    //! down by (int)descender, the next glyph by ascender), so less than
    //! lineBoxHeight; in an Sdf font layoutSdfText's, ascender-descender scaled by
    //! px/baseSize and rounded to a whole pixel. measureText's y is lineBoxHeight
    //! plus one lineStep per line after the first.
    static float lineStep(const FontData &fontData, float px);

    void setVertex(Vertex &vertex, uint8_t type, int x, int y, int z, float u=0.0f, float v=0.0f);
    //! Called once primitiveId's four vertexes are written for the device-pixel
    //! rect: if a corner of it is outside what Vertex::x/y hold (int16_t), puts
    //! all four at 0,0, a zero-area quad that draws nothing and keeps its style and
    //! other fields. A corner is never clamped: that would move it, and the
    //! interpolated UV/colour/SDF local position (and the flat half-size) with it.
    void cullOutOfRangeQuad(size_t primitiveId, const gweni::Rect &rect);
    //! Writes primitiveId's slot as one of the SDF rounded-rect quads (styles 4, 6, 7),
    //! centred in rect. color0 and color3 are given per side (left, right), because
    //! style 6 interpolates them along x; color1 and the border colour are per quad.
    //! \a edges is the RoundedEdge mask, packed over the radius (Vertex::radius);
    //! with \a reach above 0 the device rect is widened by max(1, snap(reach))
    //! px on each side \a edges leaves without a border.
    void writeRoundedQuad(size_t primitiveId, gweni::Rect rect, int zIndex, uint8_t style,
        float radius, float border, gweni::Color color0Left, gweni::Color color0Right,
        gweni::Color color1, gweni::Color color3Left, gweni::Color color3Right,
        gweni::Color borderColor, uint8_t edges=0, float reach=0.0f);
    //! The image unit (0..ImageUnits-1) a style-2 slot samples \a texture from,
    //! taking a free one the first time; ImageUnits when all are taken.
    size_t imageUnit(TextureHandle texture);

    bool m_imageUnitsFullLogged=false;

    Rect m_viewRect;
    //! setViewTransform's matrix, column-major, the identity when there is none
    float m_viewTransform[16]={ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    bool m_hasViewTransform=false;
    Color m_color;

    std::vector<Vertex> m_vertexes; //argue about vertices all you want
    std::vector<Box> m_clippingBoxes; //!< one per primitive slot

    //! Which primitive slots can show, one bit per slot: a slot can when any of its
    //! vertexes' style is not 0 (hidePrimitive and releasePrimitive set them all to
    //! 0, and a slot the mirror grew by is 0 until it is drawn). Every write that
    //! changes a slot's bytes marks it in m_dirtyVertexSlots, which the upload has
    //! to see anyway, so flush() brings the bits up to date from those slots alone
    //! (updateSlotVisibility) before the upload clears them: every draw*, a hide, a
    //! show, a release, a quad collapsed out of range, a scale or font change that
    //! redraws a slot, in O(slots changed).
    std::vector<uint64_t> m_visibleSlots;
    //! Per primitive index chunk of m_chunkSlots slots: how many of its slots can
    //! show, and its draw's ranges, built from its bits when it is drawn after a
    //! change of them (stale) and kept until the next
    struct PrimitiveChunkSlots
    {
        size_t visible=0;
        bool stale=true;
        size_t slots=0;     //!< the slots draw's ranges cover
        ChunkDraw draw;
    };
    std::vector<PrimitiveChunkSlots> m_primitiveChunks;
    bool m_drawEveryPrimitiveSlot=false;
    size_t m_primitiveRangeGap=kPrimitiveRangeGap;
    size_t m_maxPrimitiveRanges=kMaxPrimitiveRanges;
    //! setDrawEveryPrimitiveSlot's draw: one range over the whole chunk
    ChunkDraw m_wholeChunkDraw;
    //! buildPrimitiveRanges' scratch: the runs [first, end) found, and the gaps
    //! between them as (size, index) when they are too many
    std::vector<std::pair<size_t, size_t>> m_primitiveRuns;
    std::vector<std::pair<size_t, size_t>> m_primitiveGaps;
    //! Sets the slot's bit from its vertexes, and its chunk's count; a change
    //! marks the chunk's ranges stale.
    void updateSlotVisibility(size_t slot);
    //! Builds a chunk's ranges from its bits: m_chunkSlots/64 words, and the runs
    //! they hold.
    void buildPrimitiveRanges(size_t chunk);
    //! Recounts m_primitiveChunks from m_visibleSlots, for a new chunk size, every
    //! chunk's ranges stale.
    void recountPrimitiveChunks();

    //! One contiguous run of glyph quads in the text mirrors, owned by one text id.
    //
    //! A run owns the glyph slots [start, start+capacity): vertexes
    //! [start*4, (start+capacity)*4). Only the first `glyphs` of them are in use.
    //! Its font's draw list (buildTextDrawList) holds just those, and only while
    //! the run is not hidden, so the tail, a hidden run and a freed one draw
    //! nothing whatever their vertexes still hold. One run is therefore one dirty
    //! range per buffer for the upload.
    //
    //! text, font and color are what the glyphs were laid out from: renderText
    //! rebuilds the run when any of them changes, and only re-offsets it when
    //! just position or zIndex did.
    //
    //! fontData is the loaded font whose atlas the glyphs sample, and the run is on
    //! its textRuns list, which keeps that font loaded; nullptr before the first
    //! layout and once setTextAA replaced the font, and then the run draws nothing
    //! until renderText lays it out again.
    struct TextRun
    {
        size_t start=0;
        size_t capacity=0;
        size_t glyphs=0;
        gweni::Point position{0, 0};
        int zIndex=0;
        bool hidden=false;
        std::string text;
        gweni::Font font;
        float px=0.f; //!< the effective size it was laid out at, textPx(font)
        gweni::Color color;
        FontData *fontData=nullptr;
    };

    //! Gives the run at least `glyphs` slots, keeping its current ones when they
    //! already fit: best fit over the free list, splitting the remainder off, and
    //! bump-allocating past the end of the in-use slots when nothing fits. A first
    //! layout gets exactly `glyphs`; a run that outgrew its slots gets slack, a
    //! quarter more rounded up to kTextRunSlackGranule, so it grows in place next
    //! time (the tail past `glyphs` is in no draw list).
    //! textId is the run's, the owner its slots are recorded under (m_textSlotOwner).
    void allocateTextRun(size_t textId, TextRun &run, size_t glyphs);
    //! A growing run's capacity is a multiple of this many glyph slots
    static constexpr size_t kTextRunSlackGranule=8;
    //! Returns the run's slots to the free list, leaving them drawing nothing.
    void freeTextRun(TextRun &run);
    //! Adds a free block, merging it with the free blocks either side of it; a
    //! block that ends at m_textSlotsUsed lowers m_textSlotsUsed instead, so the
    //! last run grows in place and a string being typed does not leak slots.
    void addFreeTextRun(size_t start, size_t capacity);
    //! Grows the text mirrors so the first `glyphSlots` glyph slots exist.
    void ensureTextSlots(size_t glyphSlots);
    //! What the run puts in its font's draw list: (first glyph slot, glyph count),
    //! or (0, 0) for a hidden run and one with no glyphs, which draw nothing.
    static std::pair<size_t, size_t> drawnRange(const TextRun &run);
    //! The run's drawnRange was `before`: if it is not now, its font's draw list is
    //! rebuilt before the next draw. A change of font is setTextRunFont's to mark.
    void markTextDrawList(const TextRun &run, std::pair<size_t, size_t> before);
    void markTextVertexes(const TextRun &run);
    //! Writes box into the run's glyph slots in use, marking only the ones that change.
    void writeTextClipBoxes(const TextRun &run, const Box &box);
    //! Moves the text id's run from its font's draw list to fontData's (nullptr: to none).
    void setTextRunFont(size_t textId, FontData *fontData);
    //! Rebuilds fontData's drawChunks from its textRuns.
    void buildTextDrawList(FontData &fontData);
    //! Fills the first entries of drawChunks with fontData's draw list from its
    //! textRuns, reusing the entries already there and their vectors, and returns
    //! how many it filled; the entries past those are left as they were, for the
    //! caller to drop (buildTextDrawList) or keep (verifyTextDrawList).
    size_t fillTextDrawList(const FontData &fontData, std::vector<ChunkDraw> &drawChunks);
    //! In a debug build (no NDEBUG), asserts that fontData's drawChunks, not
    //! stale, are what a rebuild gives, so a run change that did not mark them
    //! really left them as they were. flush() calls it for every such font.
    void verifyTextDrawList(const FontData &fontData);
    //! verifyTextDrawList's rebuild, one scratch for every font, declared in every
    //! build so the layout of the class does not depend on NDEBUG. It is never
    //! shrunk: it keeps the largest list it has held, so verifying fonts with
    //! different chunk counts one after the other allocates nothing.
    std::vector<ChunkDraw> m_verifyDrawChunks;
    //! fillTextDrawList's (first glyph slot, glyph count) per run, kept for its capacity
    std::vector<std::pair<size_t, size_t>> m_textDrawRanges;

    std::vector<TextRun> m_textRuns;
    std::map<size_t, size_t> m_freeTextRuns; //!< start -> capacity, never adjacent to another
    size_t m_textSlotsUsed=0; //!< glyph slots below which runs are handed out
    std::vector<ftgl::glyph_vertex_t> m_textVertexes;
    std::vector<Box> m_textClipBoxes; //!< one per glyph slot
    //! The text id whose run was last given each glyph slot (kNoTextOwner: none
    //! yet), so flush() reports a written slot by text id. Set when a run gets
    //! new slots, and kept when it gives them back.
    std::vector<size_t> m_textSlotOwner;
    static constexpr size_t kNoTextOwner=~size_t(0);

    //! What the last flush() wrote, by id (writtenPrimitiveIds/writtenTextIds), and
    //! the per-text-id glyph counts it adds up, kept for their capacity
    std::vector<size_t> m_writtenPrimitiveIds;
    std::vector<WrittenText> m_writtenTextIds;
    std::vector<size_t> m_writtenTextSlotOwners;
    //! Fills m_writtenPrimitiveIds and m_writtenTextIds from the dirty sets, before
    //! the upload clears them.
    void recordWrittenIds();

    DirtySlots m_dirtyVertexSlots;
    DirtySlots m_dirtyClipSlots;
    DirtySlots m_dirtyTextVertexSlots;
    DirtySlots m_dirtyTextClipSlots;

    //element counts allocated on the device per vertex buffer, and how many of the
    //mirror's elements it holds (the mirror's size at the last upload); for the clip
    //boxes, the boxes the table holds, and its rows
    size_t m_vertexBufferCapacity=0;
    size_t m_vertexBufferSent=0;
    size_t m_clipBoxesSent=0;
    size_t m_textVertexBufferCapacity=0;
    size_t m_textVertexBufferSent=0;
    size_t m_textClipBoxesSent=0;
    size_t m_clipTextureRows=0;
    size_t m_textClipTextureRows=0;

    //per-frame counters; also reported to stdout when GWENI_UPLOAD_STATS is set
    bool m_uploadStats=false;
    FrameStats m_frameStats;

    //! GWENI_TEXT_DEBUG=<substring>: dump measureText metrics and the text mirror for
    //! strings containing <substring> (off unless the variable is set).
    const char *m_textDebug=nullptr;
    bool textDebugMatches(const gweni::String &text) const;

    //! GWENI_CLIP_DEBUG=<inset px>: shrink every clip box by the inset on each side, so
    //! the shader clipping shows on screen while the controls only set the canvas region.
    int m_clipDebugInset=0;

    //! Effective px at and above which text is drawn from its face's SDF atlas;
    //! GWENI_SDF_TEXT_MIN=<px> overrides it (0: SDF everywhere, a large value:
    //! coverage everywhere).
    float m_sdfTextMin;
    //! How coverage-range text is anti-aliased (setTextAA, GWENI_TEXT_AA)
    TextAA m_textAA=TextAA::Grayscale;
    //! The LCD modes' fallback to Grayscale has been logged (setTextAA)
    bool m_lcdFallbackLogged=false;
    //! The LCD modes' coverage boost for light text (setTextContrast, GWENI_TEXT_CONTRAST)
    float m_textContrast;
    //! A missing or unreadable face is drawn in the built-in font (setFallbackFont)
    bool m_fallbackFont=true;

    //! The slots one draw covers (indexChunkSlots())
    size_t m_chunkSlots=kSlotsPerChunk;
};


}//namespace renderer
}//namespace gweni

namespace imglib
{

template<>
inline Format format<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image)
{
    size_t channels=image.m_readData.size()/(image.width*image.height);

    if(channels == 4)
        return Format::RGBA;
    if(channels == 3)
        return Format::RGB;
    if(channels == 2)
        return Format::RA;
    if(channels == 1)
        return Format::GreyScale;
    return Format::Unknown;
}
template<>
inline Depth depth<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return Depth::Bit8; }
template<>
inline Location location<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return Location::System; } //allways system for this guy

template<>
inline size_t width<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return image.width; }
template<>
inline size_t height<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return image.height; }
template<>
inline size_t stride<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return image.width; }

template<>
inline size_t nativeId<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return (size_t)image.m_handle; }
template<>
inline uint8_t *data<gweni::renderer::SlotTextureData>(gweni::renderer::SlotTextureData &image) { return image.m_readData.data(); }
template<>
inline const uint8_t *data<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return image.m_readData.data(); }
template<>
inline size_t dataSize<gweni::renderer::SlotTextureData>(const gweni::renderer::SlotTextureData &image) { return image.m_readData.size(); }

template<>
inline bool resize<gweni::renderer::SlotTextureData>(gweni::renderer::SlotTextureData &image, Format format, Depth depth, size_t width, size_t height)
{
    size_t dataSize=sizeOfPixel(format, depth)*width*height;

    image.m_readData.resize(dataSize);
    image.width=width;
    image.height=height;
    return true;
}

}//namespace imglib

#endif //_gweni_renders_slotRenderer_h_
