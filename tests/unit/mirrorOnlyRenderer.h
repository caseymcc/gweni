/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A SlotRenderer whose backend hooks do nothing, for headless tests of what
 *  SlotRenderer itself does: its CPU mirror, its packing and its frame counts
 *  are all there is. Only in a build with the OpenGL core renderer (which
 *  SlotRenderer is part of): GWENI_TEST_SLOT_RENDERER says whether it is here.
 */
#ifndef _gweni_tests_unit_mirrorOnlyRenderer_h_
#define _gweni_tests_unit_mirrorOnlyRenderer_h_

#if __has_include(<freetype-gl/freetype-gl.h>)
#include <gweni/platforms/renderers/slotRenderer.h>
#define GWENI_TEST_SLOT_RENDERER 1
#endif

#ifdef GWENI_TEST_SLOT_RENDERER

#include <cstdint>
#include <vector>

namespace test
{

//! A SlotRenderer whose backend hooks do nothing.
class MirrorOnlyRenderer: public gweni::renderer::SlotRenderer
{
public:
    using TextureHandle=gweni::renderer::TextureHandle;
    using FontData=gweni::renderer::FontData;
    using ChunkDraw=gweni::renderer::ChunkDraw;
    using SlotTextureData=gweni::renderer::SlotTextureData;

    MirrorOnlyRenderer(): SlotRenderer(paths(), gweni::Rect(0, 0, 640, 480)) {}

    void clearTarget(const gweni::Color &) override {}
    void readPixels(int, int, std::vector<uint8_t> &pixels) override { pixels.clear(); }
    bool textureExists(TextureHandle) const override { return false; }
    bool textureChannelSizes(TextureHandle, int[4]) const override { return false; }
    bool deviceError() override { return false; }

protected:
    void beginFrame() override {}
    void allocateVertexBuffer(SlotPath, size_t) override {}
    void uploadVertexRange(SlotPath, size_t, size_t, const void *) override {}
    size_t maxClipBoxRows() const override { return 2048; }
    void allocateClipBoxes(SlotPath, size_t) override {}
    void uploadClipBoxRows(SlotPath, size_t, size_t, size_t, size_t, const Box *) override {}
    TextureHandle createAtlasTexture(const texture_atlas_t &) override { return 0; }
    void uploadAtlasRows(TextureHandle, const texture_atlas_t &, size_t, size_t) override {}
    void destroyAtlasTexture(TextureHandle) override {}
    TextureHandle createImageTexture(const SlotTextureData &, bool) override { return 0; }
    void destroyImageTexture(TextureHandle) override {}
    bool readTextureRgba(TextureHandle, int, int, std::vector<uint8_t> &rgba) override
    {
        rgba.clear();
        return false;
    }
    void beginPrimitiveDraws() override {}
    void drawPrimitiveChunk(const ChunkDraw &) override {}
    void endPrimitiveDraws() override {}
    void beginTextDraws() override {}
    void beginTextAtlas(const FontData &) override {}
    void drawTextChunk(const ChunkDraw &) override {}
    void endTextDraws() override {}
    void submitFrame() override {}
    void indexChunkSlotsChanged() override {}
    void viewTransformChanged() override {}

private:
    //! Nothing is loaded, so no path is ever looked up.
    struct NoPaths: gweni::ResourcePaths
    {
        gweni::String getPath(Type, gweni::String const &name) override { return name; }
    };

    static gweni::ResourcePaths &paths()
    {
        static NoPaths noPaths;
        return noPaths;
    }
};

}//namespace test

#endif//GWENI_TEST_SLOT_RENDERER
#endif//_gweni_tests_unit_mirrorOnlyRenderer_h_
