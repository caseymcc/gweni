/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::MarkdownView and the Markdown theme on the StubRenderer. It pins:
 *  - the view lays its document out at its viewport's width, the vertical bar
 *    shown and disabled while the document fits, enabled and ranged over
 *    the document less the viewport when it does not, each in one layout
 *    pass on the frame the markdown is set and on a resize;
 *  - the wheel scrolls the view, a vertical wheel over a scrolled code block
 *    included;
 *  - a press and a release on a link call the view's onLinkClicked (and the
 *    document's) once with its url;
 *  - the skin's Markdown metrics (block and tight spacing, list indent,
 *    heading sizes) are what the document lays out with, a change taking
 *    effect through ThemeEditor::applyToCanvas(canvas, true);
 *  - the textured skin's theme names them: the seven Markdown surfaces in
 *    tokyonight::surfaceRegistry() and the metrics in metricRegistry(), and
 *    a theme file's "metrics" sets them.
 */
#include "headlessFixture.h"

#include <gweni/controls/markdownView.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/tools/themeEditor.h>

#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace gweni;
using controls::MarkdownDocument;
using controls::MarkdownView;
using controls::internal::MarkdownCodeBlock;
using controls::internal::MarkdownList;
using controls::internal::MarkdownListItem;
using controls::internal::MarkdownText;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    MarkdownView *view;

    explicit Scene(int width=400, int height=300)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, height);
        view=canvas->newChild<MarkdownView>();
        view->setDock(DockPosition::Center);
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
};

std::string paragraphs(int count)
{
    std::string markdown;

    for(int i=0; i < count; ++i)
        markdown+="Paragraph "+std::to_string(i)+" of the view, a line of words that wraps when narrow.\n\n";
    return markdown;
}

}//namespace

TEST_CASE("Markdown view: the document at the viewport's width, the bar disabled while it fits")
{
    Scene scene;

    scene.settle();
    scene.view->setMarkdown("# Short\n\nOne paragraph.\n");
    CHECK(scene.frame().passes == 1);

    MarkdownDocument *document=scene.view->getDocument();
    controls::BaseScrollBar *bar=scene.view->getScrollBar();

    REQUIRE(bar);
    CHECK(document->getWidth() == scene.view->getViewport().w);
    CHECK(bar->visible());
    CHECK(bar->isDisabled());
    CHECK(scene.view->getMaxVerticalScrollOffset() == 0);

    // taller than the view: the bar ranges over it, laid out in one pass
    scene.view->setMarkdown(paragraphs(40));
    CHECK(scene.frame().passes == 1);
    CHECK(!bar->isDisabled());
    CHECK(document->getWidth() == scene.view->getViewport().w);
    CHECK(document->getHeight() == document->getPreferredSize().height);
    CHECK(scene.view->getMaxVerticalScrollOffset() == document->getHeight()-scene.view->getViewport().h);
    CHECK(scene.view->getMaxVerticalScrollOffset() > 0);

    // a narrower view: the paragraphs wrap taller, in one pass
    const int before=document->getHeight();

    scene.canvas->setSize(250, 300);
    CHECK(scene.frame().passes == 1);
    CHECK(document->getWidth() == scene.view->getViewport().w);
    CHECK(document->getHeight() > before);
    CHECK(scene.frame().passes == 0);
}

TEST_CASE("Markdown view: the wheel scrolls it, over a scrolled code block too")
{
    Scene scene;
    const std::string code="```\nint a_line_of_code_much_too_wide_for_the_view = 1; // and more and more and more and more and more and more\n```\n\n";

    scene.view->setMarkdown(code+paragraphs(40));
    scene.settle();

    CHECK(scene.view->onMouseWheeled(-120));
    scene.settle();

    const int scrolled=scene.view->getVerticalScrollOffset();

    CHECK(scrolled > 0);

    MarkdownCodeBlock *block=dynamic_cast<MarkdownCodeBlock *>(scene.view->getDocument()->getBlockControl(0));

    REQUIRE(block);
    REQUIRE(block->getScroll());
    REQUIRE(block->getScroll()->getMaxHorizontalScrollOffset() > 0);
    // over the code: the view's, the code not moved sideways
    CHECK(block->getScroll()->onMouseWheeled(-120));
    scene.settle();
    CHECK(scene.view->getVerticalScrollOffset() > scrolled);
    CHECK(block->getScroll()->getHorizontalScrollOffset() == 0);
}

TEST_CASE("Markdown view: a link's click calls onLinkClicked with its url")
{
    Scene scene;
    std::vector<std::string> viewUrls;
    std::vector<std::string> documentUrls;

    struct Listener: public event::Handler
    {
        std::vector<std::string> *urls=nullptr;

        void onLink(event::Info info)
        {
            urls->push_back(info.string);
        }
    } viewListener, documentListener;

    viewListener.urls=&viewUrls;
    documentListener.urls=&documentUrls;
    scene.view->onLinkClicked.add(&viewListener, &Listener::onLink);
    scene.view->getDocument()->onLinkClicked.add(&documentListener, &Listener::onLink);

    scene.view->setMarkdown("Intro.\n\n- see [the docs](https://example.com/docs) here\n");
    scene.settle();

    MarkdownList *list=dynamic_cast<MarkdownList *>(scene.view->getDocument()->getBlockControl(1));

    REQUIRE(list);

    MarkdownListItem *item=dynamic_cast<MarkdownListItem *>(list->getBlockControl(0));

    REQUIRE(item);

    MarkdownText *text=dynamic_cast<MarkdownText *>(item->getBlockControl(0));

    REQUIRE(text);

    // the link's piece: the second
    const text::RichPiece &piece=text->getLayout().pieces().at(1);
    const Rect box=piece.box();
    const Point at=text->localPosToCanvas(Point(text->contentOrigin().x+box.x+box.w/2,
        text->contentOrigin().y+box.y+box.h/2));

    scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    scene.frame();
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    scene.canvas->inputMouseButton(0, false);
    scene.frame();

    CHECK(viewUrls == std::vector<std::string>{ "https://example.com/docs" });
    CHECK(documentUrls == std::vector<std::string>{ "https://example.com/docs" });
}

TEST_CASE("Markdown view: the skin's Markdown metrics, a change applied through the theme editor")
{
    Scene scene;

    scene.view->setMarkdown("# Title\n\nBody.\n\n- a\n- b\n");
    scene.settle();

    MarkdownDocument *document=scene.view->getDocument();
    MarkdownText *title=dynamic_cast<MarkdownText *>(document->getBlockControl(0));
    MarkdownList *list=dynamic_cast<MarkdownList *>(document->getBlockControl(2));

    REQUIRE(title);
    REQUIRE(list);

    controls::Base *body=document->getBlockControl(1);
    MarkdownListItem *a=dynamic_cast<MarkdownListItem *>(list->getBlockControl(0));
    MarkdownListItem *b=dynamic_cast<MarkdownListItem *>(list->getBlockControl(1));

    REQUIRE(a);
    REQUIRE(b);
    CHECK(title->getFont().size == doctest::Approx(float(scene.skin.Metrics.MarkdownHeading1)));
    CHECK(body->getY() == title->getHeight()+scene.skin.Metrics.MarkdownBlockSpacing);
    CHECK(b->getY() == a->getHeight()+scene.skin.Metrics.MarkdownTightSpacing);
    CHECK(a->getBlockControl(0)->getX() == scene.skin.Metrics.MarkdownListIndent);

    scene.skin.Metrics.MarkdownHeading1=30;
    scene.skin.Metrics.MarkdownBlockSpacing=25;
    scene.skin.Metrics.MarkdownTightSpacing=7;
    scene.skin.Metrics.MarkdownListIndent=40;
    tools::ThemeEditor::applyToCanvas(scene.canvas, true);
    scene.settle();

    CHECK(title->getFont().size == doctest::Approx(30.0f));
    CHECK(body->getY() == title->getHeight()+25);
    CHECK(b->getY() == a->getHeight()+7);
    CHECK(a->getBlockControl(0)->getX() == 40);
    CHECK(a->getMarker()->getX()+a->getMarker()->getWidth() == 40-MarkdownListItem::MarkerGap);
}

TEST_CASE("Markdown theme: the surfaces and metrics a theme names, a file's metrics applied")
{
    const auto surfaces=skin::textured::tokyonight::surfaceRegistry();

    for(const char *name:{ "MarkdownCode", "MarkdownCodeBlock", "MarkdownQuoteBar", "MarkdownRule",
        "MarkdownTableHeader", "MarkdownTableBorder", "MarkdownImagePlaceholder" })
    {
        INFO(name);
        CHECK(surfaces.count(name) == 1);
    }

    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin(&renderer);
    const auto metrics=skin.metricRegistry();

    for(const char *name:{ "MarkdownBlockSpacing", "MarkdownTightSpacing", "MarkdownListIndent",
        "MarkdownHeading1", "MarkdownHeading2", "MarkdownHeading3", "MarkdownHeading4",
        "MarkdownHeading5", "MarkdownHeading6" })
    {
        INFO(name);
        CHECK(metrics.count(name) == 1);
    }

    const std::string path="markdownViewTheme.json";

    {
        std::ofstream file(path);

        file<<"{\"metrics\": {\"MarkdownBlockSpacing\": 16, \"MarkdownListIndent\": 30, \"MarkdownHeading2\": 28}}";
    }
    CHECK(skin.loadThemeFile(path));
    std::remove(path.c_str());
    CHECK(skin.Metrics.MarkdownBlockSpacing == 16);
    CHECK(skin.Metrics.MarkdownListIndent == 30);
    CHECK(skin.Metrics.MarkdownHeading2 == 28);
    CHECK(skin.Metrics.MarkdownTightSpacing == decltype(skin::Base::Metrics){}.MarkdownTightSpacing);
}
