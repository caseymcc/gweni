/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A MarkdownDocument's table edited through setMarkdown draws what the same
 *  text built fresh draws, by the textured skin offscreen on the backend
 *  under test at scale 1 and 2: a cell grown so its column's lines move while
 *  the table keeps its bounds (the table's grid and header fill redrawn at
 *  the new edges, not left at the old ones), byte for byte over the whole
 *  view, and the edit changing the frame from before it.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/markdownDocument.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <string>
#include <vector>

namespace
{

using gweni::Color;

struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;

    explicit Scene(float scale)
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(viewSize.x/scale), static_cast<int>(viewSize.y/scale));
    }

    ~Scene()
    {
        delete canvas;
        delete skin;
        gweni::test::gpuRenderer().setScale(1.0f);

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    std::vector<uint8_t> settle()
    {
        std::vector<uint8_t> pixels;

        for(int i=0; i < 4; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(pixels);
        return pixels;
    }
};

size_t differences(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    size_t count=0;

    for(size_t i=0; i < a.size() && i < b.size(); ++i)
    {
        if(a[i] != b[i])
            ++count;
    }
    return count;
}

const std::string before="| a | b |\n|---|---|\n| c | d |\n";
const std::string after="| a much longer cell | b |\n|---|---|\n| c | d |\n";

}//namespace

TEST_CASE("Markdown table edit: a cell grown through setMarkdown draws what the text built fresh draws")
{
    for(float scale : {1.0f, 2.0f})
    {
        CAPTURE(scale);

        std::vector<uint8_t> fresh;

        {
            Scene scene(scale);
            gweni::controls::MarkdownDocument *document=scene.canvas->newChild<gweni::controls::MarkdownDocument>();

            document->setMarkdown(after);
            fresh=scene.settle();
        }

        Scene scene(scale);
        gweni::controls::MarkdownDocument *document=scene.canvas->newChild<gweni::controls::MarkdownDocument>();

        document->setMarkdown(before);

        const std::vector<uint8_t> first=scene.settle();

        document->setMarkdown(after);

        const std::vector<uint8_t> edited=scene.settle();

        REQUIRE(edited.size() == fresh.size());
        CHECK(differences(first, edited) > 0);
        CHECK(differences(edited, fresh) == 0);
    }
}
