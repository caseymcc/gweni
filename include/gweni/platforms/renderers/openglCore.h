#ifndef _gweni_renders_openglCore_h_
#define _gweni_renders_openglCore_h_


#include <gweni/platforms/baseRender.h>
#include <gweni/objectIdBank.h>

#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
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

struct GLTextureData: public gweni::TextureData
{
    GLTextureData()
    {}
    GLTextureData(const GLTextureData &)=delete;
    GLTextureData(GLTextureData &&other)
        : GLTextureData()
    {
        std::swap(width, other.width);
        std::swap(height, other.height);
        std::swap(readable, other.readable);
        std::swap(m_textureId, other.m_textureId);

        m_readData.swap(other.m_readData);
    }

    ~GLTextureData();

    unsigned int m_textureId{0};
    size_t m_index{std::numeric_limits<size_t>::max()};
    //                deleted_unique_ptr<unsigned char> m_ReadData;
    std::vector<unsigned char> m_readData;
};

//! How the text shader turns an atlas sample into alpha. Set per draw, so each
//! atlas's runs can use their own: today every atlas is a coverage one.
enum class GLTextMode: int
{
    Coverage=0 //!< the sample's red channel is the glyph's coverage
};

struct GLFontData
{
    GLFontData():
        markup{}, familyPath(nullptr), fontManager(nullptr), atlas(nullptr),
        textureFont(nullptr), spacing(0.f), width(0), height(0)
    {}

    GLFontData(const GLFontData &)=delete;

    GLFontData(GLFontData &&other)
        : GLFontData()
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

        spacing=other.spacing;
        width=other.width;
        height=other.height;

        mode=other.mode;
        textRuns=std::move(other.textRuns);
        drawCounts=std::move(other.drawCounts);
        drawOffsets=std::move(other.drawOffsets);
        drawListStale=other.drawListStale;
    }

    //! Deletes the atlas texture, then the manager (which owns the atlas, the
    //! texture_font_t and the cache string), then familyPath. A moved-from one
    //! holds nothing and frees nothing. Needs the GL context current.
    ~GLFontData();

    std::string faceName;
    ftgl::markup_t markup;
    char *familyPath; //!< owns the string markup.family borrows

    ftgl::font_manager_t *fontManager; //!< owned
    texture_atlas_t *atlas; //!< inside fontManager; atlas->id is a GL texture this owns
    ftgl::texture_font_t *textureFont;

    float spacing;

    int width;
    int height;

    GLTextMode mode=GLTextMode::Coverage;

    //! Text ids whose runs were laid out in this font, so flush() draws them with
    //! this atlas. OpenGLCore keeps it in step with each TextRun's fontData.
    std::unordered_set<size_t> textRuns;
    //! What flush() hands glMultiDrawElements for this atlas: the index ranges of
    //! the runs in textRuns that draw something, in slot order, adjacent ones merged.
    //! Rebuilt from textRuns when drawListStale.
    std::vector<int> drawCounts;
    std::vector<const void *> drawOffsets;
    bool drawListStale=true;
};

//
//! Renderer for [OpenGL](https://www.opengl.org/) Core Profile.
//
class GWENI_EXPORT OpenGLCore: public gweni::renderer::Base
{
    template<typename T>
    using deleted_unique_ptr=std::unique_ptr<T, std::function<void(T *)>>;

    static const wchar_t BeginCharacter=L' ';    // First Character of Wide Character Table
    static const wchar_t LastCharacter=0x2FFF;   // Last Character of Wide Character Table
    static const wchar_t NewLineCharacter=L'\n'; // New Line Character

public:

    OpenGLCore(ResourcePaths &paths, const gweni::Rect &viewRect);
    //! Releases every font, texture, buffer, vertex array and program it owns, so the
    //! GL context it was init()'d in must still be current. Another OpenGLCore in the
    //! same context keeps working: it rebinds its own objects every flush().
    virtual ~OpenGLCore();

    void init() override;
    void setViewRect(const gweni::Rect &viewRect) override;

    void begin() override;
    void end() override;

    void setDrawColor(gweni::Color color) override;
    void drawFilledRect(size_t primitiveId, gweni::Rect rect, int zIndex) override;
    void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border,
        gweni::Color fillTop, gweni::Color fillBottom, gweni::Color borderColor, int zIndex) override;
    void drawBilinearRect(size_t primitiveId, gweni::Rect rect, int zIndex,
        gweni::Color c00, gweni::Color c10, gweni::Color c01, gweni::Color c11,
        float radius, float border, gweni::Color borderColor) override;
    void drawCheckerRect(size_t primitiveId, gweni::Rect rect, int zIndex,
        gweni::Color fill, gweni::Color light, gweni::Color dark,
        float radius, float border, gweni::Color borderColor) override;
    void drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction,
        gweni::Color color, int zIndex) override;

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

    gweni::Point measureText(const gweni::Font &font, const gweni::String &text) override;

    // Resource Loader
    gweni::Font::Status loadFont(const gweni::Font &font) override;
    void freeFont(const gweni::Font &font) override;
    bool ensureFont(const gweni::Font &font) override;

    Texture::Status loadTexture(gweni::Texture &texture) override;
    void freeTexture(const gweni::Texture &texture) override;
    TextureData getTextureData(const gweni::Texture &texture) const override;
    bool ensureTexture(const gweni::Texture &texture) override;
    GLTextureData *getFreeTextureData();

    //! GL name of a loaded texture, 0 if it is not loaded
    unsigned int getTextureName(const gweni::Texture &texture) const;
    //! GL name of a loaded font's glyph atlas, 0 if the font is not loaded
    unsigned int getFontAtlasName(const gweni::Font &font) const;
    //! Fonts loaded and not freed yet
    size_t getLoadedFontCount() const { return m_fonts.size(); }

    //! The GL objects init() created (0 before init()), so a test can check that the
    //! destructor deletes them.
    struct GLObjectNames
    {
        std::vector<unsigned int> buffers;
        std::vector<unsigned int> vertexArrays;
        std::vector<unsigned int> textures; //!< the clip-box buffer textures
        std::vector<unsigned int> programs;
    };

    GLObjectNames getGLObjectNames() const;

protected:// Resourses



    std::unordered_map<Font, GLFontData> m_fonts;
    //! Faces whose last load failed: ensureFont does not retry them
    std::unordered_set<Font> m_failedFonts;
    std::vector<GLTextureData *> m_textureData;
    std::vector<size_t> m_freeTextureData;
    std::unordered_map<Texture, size_t> m_textures;
    
    std::pair<const Font, GLFontData> *m_lastFont;
//    std::pair<const Texture, GLTextureData> *m_lastTexture;
    size_t m_lastTexture;
public:

    bool initializeContext(gweni::WindowProvider *window) override;
    bool shutdownContext(gweni::WindowProvider *window) override;
    bool presentContext(gweni::WindowProvider *window) override;
    bool resizedContext(gweni::WindowProvider *window, int w, int h) override;
    bool beginContext(gweni::WindowProvider *window) override;
    bool endContext(gweni::WindowProvider *window) override;

    void *m_context;

    //! What the last flush() actually did. Reading it is how the "only what changed
    //! is uploaded" claim gets checked from outside the renderer.
    struct FrameStats
    {
        size_t frameIndex=0;            //!< frames flushed so far
        size_t primitiveSlotsWritten=0; //!< primitive slots whose bytes changed this frame
        size_t textSlotsWritten=0;      //!< glyph slots whose vertexes or clip box changed this frame
        size_t uploadCalls=0;           //!< glBufferData/glBufferSubData calls
        size_t uploadBytes=0;           //!< bytes those calls sent
        size_t mirrorBytes=0;           //!< size of the whole CPU mirror, for comparison
        size_t drawCalls=0;             //!< glDrawElements and glMultiDrawElements calls
        size_t textDraws=0;             //!< of those, the text draws: one per atlas with runs to draw
        size_t atlasUploadCalls=0;      //!< glTexSubImage2D calls for font atlas row bands
        size_t atlasUploadBytes=0;      //!< bytes those calls sent
    };

    const FrameStats &getFrameStats() const { return m_frameStats; }

    //! What every live id holds in the CPU mirrors, so two states of the UI can be
    //! compared id by id rather than as raw buffers.
    //
    //! A primitive id's entry is the bytes of its slot: its 4 vertexes (at id*4)
    //! followed by its 6 indexes (at id*6). A text id's entry is its hidden flag
    //! followed by the vertexes of the glyphs in use, and not where the run sits:
    //! allocateTextRun may hand the same string other slots after a free, and that
    //! draws the same. A test hook: it copies the whole mirror, so keep it out of
    //! the frame loop.
    struct MirrorSnapshot
    {
        std::map<size_t, std::vector<uint8_t>> primitives;  //!< primitive id -> slot bytes
        std::map<size_t, std::vector<uint8_t>> texts;       //!< text id -> run contents
    };

    MirrorSnapshot snapshotMirror() const;

private:

    uint32_t loadShaders(const char *vertex, const char *fragment);

    /**
        * @brief Perform actual rendering.
        */
    void flush();

    /**
        * @brief Add vertex to buffer.
        * @param x
        * @param y
        * @param u
        * @param v
        */
    struct Vertex
    {
        uint16_t style;
        uint16_t x, y, z;
        uint16_t tx, ty; //color0 (top fill) packed rg/ba, or UV for texture (style 2)
        // SDF rounded-rect fields (styles 4, 6, 7); left 0 for all other styles.
        int16_t  lx, ly; //local position relative to rect center (= +/-halfSize at the four corners)
        uint16_t radius; //corner radius in px
        uint16_t border; //border thickness in px
        uint16_t cx, cy; //color1 (bottom fill for vertical gradient) packed rg/ba
        uint16_t bx, by; //border color packed rg/ba
        uint16_t dx, dy; //color3 packed rg/ba: bottom edge (style 6) or checker dark (style 7)
//        glm::vec3 pos;
//        glm::vec2 uv;
//        glm::vec4 color;
    };

    static_assert(sizeof(Vertex) == 32, "Vertex is 16 16-bit fields; the GPU attribute layout depends on it");

    //! A clip box in pixel space (the vertexes' space, scaled like translate()):
    //! the fragment shaders keep [x0,x1) x [y0,y1) of the slot it belongs to and
    //! discard the rest. Read on the GPU as one RGBA16UI texel of a buffer texture.
    struct Box
    {
        uint16_t x0, y0;
        uint16_t x1, y1;
    };

    static_assert(sizeof(Box) == 8, "Box is one GL_RGBA16UI texel of the clip buffer textures");

    //
    //! Set of primitive slots whose CPU-side data has diverged from the GPU buffer.
    //
    //! A slot is a fixed, equally sized run of elements in a buffer (4 vertexes or 6
    //! indexes per primitive id), so the set only has to remember slot numbers; flush()
    //! turns them into as few contiguous glBufferSubData ranges as it can.
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
        uint16_t indexes[6];
        Box box;
    };

    //! Grows the CPU mirrors so the primitive's slots exist.
    void ensurePrimitiveSlots(size_t primitiveId);
    void readPrimitiveSlot(size_t primitiveId, PrimitiveSlot &slot) const;
    //! Marks only the slots whose bytes differ from `before`, so a control that redraws
    //! itself unchanged (the skin re-emits every frame) costs no upload.
    void markPrimitiveChanges(size_t primitiveId, const PrimitiveSlot &before);

    //! The current clipRegion() as a Box: scaled by m_scale like translate(), shrunk by
    //! GWENI_CLIP_DEBUG's inset, then clamped to [0, 65535] with x1 >= x0 and y1 >= y0
    //! (an empty region gives an empty box, which discards everything).
    Box makeClipBox() const;

    //! Uploads only the dirty slots of a buffer, growing (and fully re-uploading) it when the
    //! mirror has outgrown the device allocation. Clears dirty on return.
    void uploadDirty(unsigned int target, uint32_t buffer, size_t &capacityElements,
        const void *data, size_t elementSize, size_t elementCount,
        DirtySlots &dirty, size_t elementsPerSlot);
    //! Uploads the rows of every font atlas that freetype-gl rasterised glyphs into since
    //! the last upload (renderText and measureText do so lazily), then empties the band.
    void uploadDirtyAtlases();

    void setVertex(Vertex &vertex, uint8_t type, int x, int y, int z, float u=0.0f, float v=0.0f);
    //! Writes primitiveId's slot as one of the SDF rounded-rect quads (styles 4, 6, 7),
    //! centred in rect. color0 and color3 are given per side (left, right), because
    //! style 6 interpolates them along x; color1 and the border colour are per quad.
    void writeRoundedQuad(size_t primitiveId, gweni::Rect rect, int zIndex, uint8_t style,
        float radius, float border, gweni::Color color0Left, gweni::Color color0Right,
        gweni::Color color1, gweni::Color color3Left, gweni::Color color3Right,
        gweni::Color borderColor);
    void addVert(int x, int y, float u=0.0f, float v=0.0f);
    void setTexture(unsigned int texture);

    Rect m_viewRect;
    Color m_color;
    unsigned int m_currentTexture;
    glm::uvec2 m_currentTextureSize;
    unsigned char m_texturesOn;
    std::vector<glm::uvec2> m_textureSizes;

    std::vector<uint16_t> m_indexes; //argue about indices all you want
    std::vector<Vertex> m_vertexes; //argue about vertices all you want
    std::vector<Box> m_clippingBoxes; //!< one per primitive slot

    //! One contiguous run of glyph quads in the text mirrors, owned by one text id.
    //
    //! A run owns the glyph slots [start, start+capacity): vertexes
    //! [start*4, (start+capacity)*4) and indexes [start*6, (start+capacity)*6).
    //! Only the first `glyphs` of them are in use; the tail, and the whole run
    //! while it is hidden, carries degenerate (all-zero) indexes so it draws
    //! nothing. One run is therefore one dirty range per buffer for the upload.
    //
    //! text, font and color are what the glyphs were laid out from: renderText
    //! rebuilds the run when any of them changes, and only re-offsets it when
    //! just position or zIndex did.
    //
    //! fontData is the loaded font whose atlas the glyphs sample, and the run is on
    //! its textRuns list; nullptr before the first layout and once that font is
    //! freed, and then the run draws nothing until renderText lays it out again.
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
        gweni::Color color;
        GLFontData *fontData=nullptr;
    };

    //! Gives the run at least `glyphs` slots, keeping its current ones when they
    //! already fit: best fit over the free list, splitting the remainder off, and
    //! bump-allocating past the end of the in-use slots when nothing fits.
    void allocateTextRun(TextRun &run, size_t glyphs);
    //! Returns the run's slots to the free list, leaving them drawing nothing.
    void freeTextRun(TextRun &run);
    //! Adds a free block, merging it with the free blocks either side of it; a
    //! block that ends at m_textSlotsUsed lowers m_textSlotsUsed instead, so the
    //! last run grows in place and a string being typed does not leak slots.
    void addFreeTextRun(size_t start, size_t capacity);
    //! Grows the text mirrors so the first `glyphSlots` glyph slots exist.
    void ensureTextSlots(size_t glyphSlots);
    //! (Re)writes the run's whole index range and marks it dirty.
    void writeTextIndexes(const TextRun &run);
    void markTextVertexes(const TextRun &run);
    //! Writes box into the run's glyph slots in use, marking only the ones that change.
    void writeTextClipBoxes(const TextRun &run, const Box &box);
    //! Moves the text id's run from its font's draw list to fontData's (nullptr: to none).
    void setTextRunFont(size_t textId, GLFontData *fontData);
    //! Rebuilds fontData's drawCounts/drawOffsets from its textRuns.
    void buildTextDrawList(GLFontData &fontData);

    std::vector<TextRun> m_textRuns;
    std::map<size_t, size_t> m_freeTextRuns; //!< start -> capacity, never adjacent to another
    size_t m_textSlotsUsed=0; //!< glyph slots below which runs are handed out
    //! 32-bit, unlike the primitive indexes: a glyph slot is 4 vertexes, so 16 bits
    //! would cap the text mirror at 16384 glyph slots and wrap silently past it.
    std::vector<uint32_t> m_textIndexes;
    std::vector<ftgl::glyph_vertex_t> m_textVertexes;
    std::vector<Box> m_textClipBoxes; //!< one per glyph slot

    DirtySlots m_dirtyVertexSlots;
    DirtySlots m_dirtyIndexSlots;
    DirtySlots m_dirtyClipSlots;
    DirtySlots m_dirtyTextVertexSlots;
    DirtySlots m_dirtyTextIndexSlots;
    DirtySlots m_dirtyTextClipSlots;

    //element counts currently allocated on the device, per buffer
    size_t m_vertexBufferCapacity=0;
    size_t m_indexBufferCapacity=0;
    size_t m_clipBufferCapacity=0;
    size_t m_textVertexBufferCapacity=0;
    size_t m_textIndexBufferCapacity=0;
    size_t m_textClipBufferCapacity=0;

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

    glm::mat4 m_projectionMatrix;

    uint32_t m_program=0;
    uint32_t m_activeProgram;

    uint32_t m_projectionUniform;
    uint32_t m_textureSizeUniform;
    uint32_t m_clipBoxesUniform;

    uint32_t m_vertexArray=0;
    uint32_t m_vertexBuffer=0;
    uint32_t m_indexBuffer=0;
    uint32_t m_clipBuffer=0;
    uint32_t m_clipTexture=0; //!< GL_TEXTURE_BUFFER view of m_clipBuffer

//    ftgl::font_manager_t *m_fontManager;

    uint32_t m_textProgram=0;
    uint32_t m_textProjectionUniform;
    uint32_t m_textClipBoxesUniform;
    uint32_t m_textModeUniform; //!< the GLTextMode of the atlas being drawn

    uint32_t m_textVertexArray=0;
    uint32_t m_textVertexBuffer=0;
    uint32_t m_textIndexBuffer=0;
    uint32_t m_textClipBuffer=0;
    uint32_t m_textClipTexture=0; //!< GL_TEXTURE_BUFFER view of m_textClipBuffer

    uint16_t m_zIndex;
};


}//namespace renderer
}//namespace gweni

namespace imglib
{

template<>
inline Format format<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image)
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
inline Depth depth<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return Depth::Bit8; }
template<>
inline Location location<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return Location::System; } //allways system for this guy

template<>
inline size_t width<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.width; }
template<>
inline size_t height<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.height; }
template<>
inline size_t stride<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.width; }

template<>
inline size_t nativeId<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return (size_t)image.m_textureId; }
template<>
inline uint8_t *data<gweni::renderer::GLTextureData>(gweni::renderer::GLTextureData &image) { return image.m_readData.data(); }
template<>
inline const uint8_t *data<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.m_readData.data(); }
template<>
inline size_t dataSize<gweni::renderer::GLTextureData>(const gweni::renderer::GLTextureData &image) { return image.m_readData.size(); }

template<>
inline bool resize<gweni::renderer::GLTextureData>(gweni::renderer::GLTextureData &image, Format format, Depth depth, size_t width, size_t height)
{
    size_t dataSize=sizeOfPixel(format, depth)*width*height;

    image.m_readData.resize(dataSize);
    image.width=width;
    image.height=height;
    return true;
}

}//namespace imglib

#endif //_gweni_renders_openglCore_h_
