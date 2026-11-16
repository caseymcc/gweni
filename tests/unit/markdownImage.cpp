/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A Markdown image whose bytes the app supplies later
 *  (internal::MarkdownImage, MarkdownDocument::setImageResolver, ImageHandle)
 *  on the StubRenderer, whose loads from memory decode nothing but are
 *  counted live until freed. It pins:
 *  - a paragraph that is one image (spaces aside) is a MarkdownImage, one with
 *    text too a MarkdownText with the alt text on a placeholder, as before;
 *  - its looks: with no resolver, a null handle, a handle with nothing known
 *    and a failed one, the alt text on its placeholder, the block the alt
 *    text's height; with the size known, a box of the image's size (one image
 *    px a unit) or, wider than the block, scaled down to its width keeping the
 *    aspect, the alt text in it without its placeholder; with the bytes, an
 *    ImagePanel given exactly the handle's bytes in that box, the alt text
 *    hidden; failed after that, the alt text again and the texture freed;
 *  - each notification lays out in one pass entering, besides one entry for
 *    each block below it (moved, as a paragraph that grows moves them, their
 *    subtrees not entered), as many controls with 3 blocks below the image as
 *    with 200, the next frame laying out nothing, a new size in the Box look
 *    (its alt text kept) too, and a notification that changes nothing shown
 *    (the same size again) marks nothing;
 *  - one handle shown in two documents updates both, and a deleted document's
 *    blocks stop watching it while the other goes on;
 *  - setMarkdown keeps an unchanged image block, its handle and its look,
 *    asking the resolver nothing; a changed alt text keeps the block and the
 *    handle; a new source asks again; a paragraph turned from an image into
 *    text (and back) is built anew, the old block's watch gone with it;
 *  - setImageResolver resolves the images already shown, and with a null
 *    one they show their alt text again;
 *  - deleting the document gives every texture back.
 */
#include "headlessFixture.h"

#include <gweni/controls/markdownDocument.h>
#include <gweni/controls/markdownView.h>
#include <gweni/imageHandle.h>

#include <doctest/doctest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace gweni;
using controls::MarkdownDocument;
using controls::internal::MarkdownImage;
using controls::internal::MarkdownText;
using Look=MarkdownImage::Look;

namespace
{

uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc=0xffffffffu;

    for(size_t i=0; i < size; ++i)
    {
        crc^=data[i];
        for(int bit=0; bit < 8; ++bit)
            crc=(crc >> 1)^(0xedb88320u & (0u-(crc & 1u)));
    }
    return crc^0xffffffffu;
}

void putBigEndian(std::vector<uint8_t> &bytes, uint32_t value)
{
    for(int shift=24; shift >= 0; shift-=8)
        bytes.push_back(uint8_t(value >> shift));
}

//! A PNG's signature and IHDR (all the handle reads; the stub decodes nothing)
std::vector<uint8_t> pngHeader(uint32_t width, uint32_t height)
{
    std::vector<uint8_t> bytes={0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    std::vector<uint8_t> chunk={'I', 'H', 'D', 'R'};

    putBigEndian(chunk, width);
    putBigEndian(chunk, height);
    chunk.insert(chunk.end(), {8, 6, 0, 0, 0});
    putBigEndian(bytes, 13);
    bytes.insert(bytes.end(), chunk.begin(), chunk.end());
    putBigEndian(bytes, crc32(chunk.data(), chunk.size()));
    return bytes;
}

//! The app's side: a handle per url, made on first ask, each ask counted
struct Resolver
{
    std::map<std::string, std::shared_ptr<ImageHandle>> handles;
    std::map<std::string, int> asks;

    controls::MarkdownImageResolver function()
    {
        return [this](const String &url)
        {
            ++asks[url];
            std::shared_ptr<ImageHandle> &handle=handles[url];

            if(!handle && (url != "none"))
                handle=ImageHandle::create();
            return handle;
        };
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    MarkdownDocument *document;

    explicit Scene(int width=400)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, 4000);
        document=canvas->newChild<MarkdownDocument>();
    }

    ~Scene()
    {
        delete canvas;
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 10 && frame().passes != 0; ++i)
            ;
    }

    void show(const std::string &markdown)
    {
        document->setMarkdown(markdown);
        settle();
    }

    template<typename T>
    T *block(size_t index)
    {
        T *control=dynamic_cast<T *>(document->getBlockControl(index));

        REQUIRE(control);
        return control;
    }
};

}//namespace

TEST_CASE("Markdown image: an image paragraph is a MarkdownImage, one among text is not")
{
    Scene scene;

    scene.show("![alt one](a.png)\n\nText ![alt two](b.png) after.\n\n  ![](c.png)  \n");

    MarkdownImage *image=scene.block<MarkdownImage>(0);

    CHECK(scene.block<MarkdownText>(1)->getText() == "Text alt two after.");
    CHECK(scene.block<MarkdownImage>(2)->getAltText()->getText() == "image");
    CHECK(image->getSource() == "a.png");
    CHECK(image->getAltText()->getText() == "alt one");
    // no resolver: the alt text on its placeholder, the block its height
    CHECK((image->getLook() == Look::Alt));
    CHECK((image->getHandle() == nullptr));
    CHECK((image->getAltText()->getSpans().at(0).style & controls::RichSpan::Placeholder));
    CHECK(image->getHeight() == image->getAltText()->getHeight());
    CHECK(image->getHeight() > 0);
    CHECK(image->getImage() == nullptr);
}

TEST_CASE("Markdown image: none, size known, bytes and failed")
{
    Scene scene;
    Resolver resolver;
    const size_t textures=scene.renderer.loadedTextureCount();

    scene.document->setImageResolver(resolver.function());
    scene.show("Before.\n\n![the alt](attachment:1)\n\n![wide](attachment:2)\n\n![gone](none)\n\nAfter.\n");

    MarkdownImage *image=scene.block<MarkdownImage>(1);
    MarkdownImage *wide=scene.block<MarkdownImage>(2);
    MarkdownImage *none=scene.block<MarkdownImage>(3);
    std::shared_ptr<ImageHandle> handle=resolver.handles["attachment:1"];
    const int altHeight=image->getHeight();

    REQUIRE((handle != nullptr));
    CHECK((image->getHandle() == handle));
    CHECK(resolver.asks["attachment:1"] == 1);
    // a null handle, and one with nothing known: the alt text
    CHECK((none->getHandle() == nullptr));
    CHECK((none->getLook() == Look::Alt));
    CHECK((image->getLook() == Look::Alt));

    // the size: a box of it, the alt text inside without its placeholder
    handle->setSize(200, 100);
    scene.settle();
    CHECK((image->getLook() == Look::Box));
    CHECK(image->getBoxSize().width == 200);
    CHECK(image->getBoxSize().height == 100);
    CHECK(image->getHeight() == 100);
    CHECK(image->getPreferredSize().width == 200);
    CHECK(image->getAltText()->visible());
    CHECK(image->getAltText()->getText() == "the alt");
    CHECK((image->getAltText()->getSpans().at(0).style & controls::RichSpan::Placeholder) == 0);
    CHECK(image->getAltText()->getX() == MarkdownImage::AltInset);

    // wider than the block: scaled down to its width
    resolver.handles["attachment:2"]->setSize(1000, 300);
    scene.settle();
    CHECK(wide->getWidth() == 400);
    CHECK(wide->getBoxSize().width == 400);
    CHECK(wide->getBoxSize().height == 120);
    CHECK(wide->getHeight() == 120);

    // the bytes: the panel given them, in the box
    const std::vector<uint8_t> png=pngHeader(300, 150);

    handle->setBytes(png);
    scene.settle();
    CHECK((image->getLook() == Look::Image));
    REQUIRE(image->getImage());
    CHECK(image->getImage()->visible());
    CHECK_FALSE(image->getAltText()->visible());
    CHECK(image->getImage()->getWidth() == 300);
    CHECK(image->getImage()->getHeight() == 150);
    CHECK(image->getHeight() == 150);
    REQUIRE(scene.renderer.loadedTextureCount() == textures+1);
    CHECK(scene.renderer.textureBytes.rbegin()->second == png);

    // failed after: the alt text again, the texture given back
    handle->setFailed();
    scene.settle();
    CHECK((image->getLook() == Look::Alt));
    CHECK(image->getAltText()->visible());
    CHECK_FALSE(image->getImage()->visible());
    CHECK((image->getAltText()->getSpans().at(0).style & controls::RichSpan::Placeholder));
    CHECK(image->getHeight() == altHeight);
    CHECK(scene.renderer.loadedTextureCount() == textures);
}

TEST_CASE("Markdown image: a notification lays out its block, the blocks below only moved")
{
    struct Counts
    {
        size_t sizeVisited=0;
        size_t resizeVisited=0;
        size_t bytesVisited=0;
    };

    auto measure=[](int below)
    {
        Scene scene;
        Resolver resolver;
        std::string markdown="Above.\n\n![picture](p.png)\n\n";

        for(int i=0; i < below; ++i)
            markdown+="Paragraph number "+std::to_string(1000+i)+".\n\n";
        scene.document->setImageResolver(resolver.function());
        scene.show(markdown);

        std::shared_ptr<ImageHandle> handle=resolver.handles["p.png"];
        controls::Base *last=scene.document->getBlockControl(scene.document->getBlockCount()-1);
        const int lastY=last->getY();
        const int altHeight=scene.document->getBlockControl(1)->getHeight();
        Counts counts;

        handle->setSize(120, 80);

        const controls::LayoutStats &sized=scene.frame();

        CHECK(sized.passes == 1);
        counts.sizeVisited=sized.visited;
        CHECK(last->getY() == lastY+80-altHeight);
        CHECK(scene.frame().passes == 0);

        // the same size again changes nothing shown: nothing marked
        handle->setSize(120, 80);
        CHECK(scene.frame().passes == 0);

        // another size, the look kept: its block laid out again alone
        handle->setSize(120, 70);

        const controls::LayoutStats &resized=scene.frame();

        CHECK(resized.passes == 1);
        counts.resizeVisited=resized.visited;
        CHECK(last->getY() == lastY+70-altHeight);
        CHECK(scene.frame().passes == 0);

        handle->setBytes(pngHeader(120, 90));

        const controls::LayoutStats &bytes=scene.frame();

        CHECK(bytes.passes == 1);
        counts.bytesVisited=bytes.visited;
        CHECK(last->getY() == lastY+90-altHeight);
        CHECK(scene.frame().passes == 0);
        return counts;
    };

    const Counts small=measure(3);
    const Counts large=measure(200);

    // each block below entered once, to be moved (its subtree not), as a
    // paragraph that grows moves them; the rest the same at both counts
    CHECK(small.sizeVisited > 3);
    CHECK(small.sizeVisited-3 == large.sizeVisited-200);
    CHECK(small.resizeVisited > 3);
    CHECK(small.resizeVisited-3 == large.resizeVisited-200);
    CHECK(small.bytesVisited > 3);
    CHECK(small.bytesVisited-3 == large.bytesVisited-200);
}

TEST_CASE("Markdown image: one handle in two documents")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);
    std::shared_ptr<ImageHandle> handle=ImageHandle::create();
    auto resolver=[handle](const String &) { return handle; };

    canvas->setSize(400, 2000);

    MarkdownDocument *first=canvas->newChild<MarkdownDocument>();
    controls::MarkdownView *view=canvas->newChild<controls::MarkdownView>();
    MarkdownDocument *second=view->getDocument();

    first->setImageResolver(resolver);
    view->setImageResolver(resolver);
    first->setMarkdown("![a](x)\n\n![b](x)\n");
    second->setMarkdown("![c](x)\n");
    // three blocks watch it
    CHECK(handle->watchCount() == 3);

    handle->setSize(50, 40);
    for(controls::Base *control:{ first->getBlockControl(0), first->getBlockControl(1),
        second->getBlockControl(0) })
        CHECK((static_cast<MarkdownImage *>(control)->getLook() == Look::Box));

    // the first document deleted: the second goes on
    first->delayedDelete();
    canvas->updateCanvas();
    canvas->renderCanvas();
    CHECK(handle->watchCount() == 1);
    handle->setBytes(pngHeader(10, 10));
    CHECK((static_cast<MarkdownImage *>(second->getBlockControl(0))->getLook() == Look::Image));
    CHECK(renderer.loadedTextureCount() == 1);

    delete canvas;
    CHECK(handle->watchCount() == 0);
    CHECK(renderer.loadedTextureCount() == 0);
}

TEST_CASE("Markdown image: setMarkdown keeps an unchanged image block and its handle")
{
    Scene scene;
    Resolver resolver;

    scene.document->setImageResolver(resolver.function());
    scene.show("Intro.\n\n![alt](a.png)\n");

    MarkdownImage *image=scene.block<MarkdownImage>(1);
    std::shared_ptr<ImageHandle> handle=resolver.handles["a.png"];

    handle->setSize(60, 30);
    scene.settle();

    // another block edited: the image block and its look kept, nothing asked
    scene.show("Intro edited.\n\n![alt](a.png)\n");
    CHECK((scene.block<MarkdownImage>(1) == image));
    CHECK((image->getHandle() == handle));
    CHECK((image->getLook() == Look::Box));
    CHECK(resolver.asks["a.png"] == 1);

    // its alt text changed: the same block and handle, the new alt in the box
    scene.show("Intro edited.\n\n![other alt](a.png)\n");
    CHECK((scene.block<MarkdownImage>(1) == image));
    CHECK((image->getHandle() == handle));
    CHECK(resolver.asks["a.png"] == 1);
    CHECK(image->getAltText()->getText() == "other alt");
    CHECK((image->getAltText()->getSpans().at(0).style & controls::RichSpan::Placeholder) == 0);

    // a new source: asked for, the old handle no longer watched
    scene.show("Intro edited.\n\n![other alt](b.png)\n");
    CHECK((scene.block<MarkdownImage>(1) == image));
    CHECK(resolver.asks["b.png"] == 1);
    CHECK((image->getHandle() == resolver.handles["b.png"]));
    CHECK(handle->watchCount() == 0);
    CHECK((image->getLook() == Look::Alt));

    // turned into text: a MarkdownText built, the image block deleted
    std::shared_ptr<ImageHandle> second=resolver.handles["b.png"];

    scene.show("Intro edited.\n\nNow text.\n");
    CHECK(scene.block<MarkdownText>(1)->getText() == "Now text.");
    CHECK(second->watchCount() == 0);

    // and back: a new image block
    scene.show("Intro edited.\n\n![alt](b.png)\n");
    CHECK((scene.block<MarkdownImage>(1)->getHandle() == second));
}

TEST_CASE("Markdown image: setImageResolver resolves the images shown")
{
    Scene scene;
    Resolver resolver;

    scene.show("![alt](a.png)\n");

    MarkdownImage *image=scene.block<MarkdownImage>(0);

    CHECK((image->getHandle() == nullptr));
    resolver.handles["a.png"]=ImageHandle::create();
    resolver.handles["a.png"]->setSize(30, 20);
    scene.document->setImageResolver(resolver.function());
    CHECK((image->getHandle() == resolver.handles["a.png"]));
    scene.settle();
    CHECK((image->getLook() == Look::Box));
    CHECK(image->getHeight() == 20);

    scene.document->setImageResolver(nullptr);
    CHECK((image->getHandle() == nullptr));
    scene.settle();
    CHECK((image->getLook() == Look::Alt));
    CHECK(resolver.handles["a.png"]->watchCount() == 0);
}
