/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::parseMarkdown, md4c's CommonMark with GFM tables, strikethrough and
 *  autolinks as gweni's block model, the blocks written out by hand
 *  (markdownTesting.h's notation). It pins, element by element:
 *  - headings 1 to 6 (ATX and setext) with their spans;
 *  - a paragraph's emphasis, strong, both, strikethrough and inline code as
 *    style bits, links, autolinks (<...>, a bare URL, www. given "http://",
 *    an e-mail address given "mailto:", trailing ?!.,:*_~ left out of an
 *    autolink as GFM says), a soft break as ' ', a hard break
 *    (two spaces or a '\') as '\n', and runs of one style merged;
 *  - entities: the common names and numeric ones decoded, U+FFFD for an
 *    invalid codepoint and NUL, an unknown name kept as written;
 *  - an image as a span of its flattened alt text and src (inside a link,
 *    with the link's url);
 *  - lists: bullet marker, ordered start and delimiter, tight and loose, a
 *    tight item's text an implicit Paragraph (with a block between two of
 *    them), nested to any depth, a new delimiter starting a new list;
 *  - block quotes, nested, holding other blocks;
 *  - rules; fenced code (language, the text without its last '\n') and
 *    indented code; GFM tables (the header row, each column's alignment on
 *    the table and on its cells, a short row filled with empty cells);
 *  - raw HTML, a block and inline, kept as the literal text it was written
 *    as, and task lists (not enabled) left as text, a footnote read as
 *    CommonMark reads it (a link reference definition and a link);
 *  - every block's hash: the same for the same block, different for another
 *    type, attribute, text, style, url or nesting, the same at any position
 *    in the document, and pinned to its value so every target (wasm32's
 *    32-bit size_t included) hashes alike.
 */
#include "markdownTesting.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using gweni::text::MarkdownBlock;
using gweni::text::parseMarkdown;
using test::describeMarkdown;

TEST_CASE("Markdown blocks: headings 1 to 6, ATX and setext")
{
    CHECK(describeMarkdown("# One\n## Two\n### Three\n#### Four\n##### Five\n###### Six *it*\n") ==
        "Heading 1 'One'\n"
        "Heading 2 'Two'\n"
        "Heading 3 'Three'\n"
        "Heading 4 'Four'\n"
        "Heading 5 'Five'\n"
        "Heading 6 'Six ' I'it'\n");
    CHECK(describeMarkdown("Title\n=====\n\nSub\n---\n") ==
        "Heading 1 'Title'\n"
        "Heading 2 'Sub'\n");
    // seven #s is no heading
    CHECK(describeMarkdown("####### seven\n") == "Paragraph '####### seven'\n");
}

TEST_CASE("Markdown blocks: a paragraph's inline styles")
{
    CHECK(describeMarkdown("a *em* _em_ **strong** __strong__ ***both*** ~~gone~~ ~one~ `co  de` **b `c` b**\n") ==
        "Paragraph 'a ' I'em' ' ' I'em' ' ' B'strong' ' ' B'strong' ' ' BI'both' ' ' S'gone' ' ' S'one' ' ' "
        "C'co  de' ' ' B'b ' BC'c' B' b'\n");
    // runs of one style merge into one span
    CHECK(describeMarkdown("plain *a* *b*\n") == "Paragraph 'plain ' I'a' ' ' I'b'\n");
    CHECK(describeMarkdown("*a **b** c*\n") == "Paragraph I'a ' BI'b' I' c'\n");
}

TEST_CASE("Markdown blocks: links and autolinks")
{
    CHECK(describeMarkdown("[text *em*](http://x.org \"title\") [ref][r]\n\n[r]: /path\n") ==
        "Paragraph 'text '<http://x.org> I'em'<http://x.org> ' ' 'ref'</path>\n");
    CHECK(describeMarkdown("<http://auto.org/a> <me@x.org> https://bare.org/p?q=1 www.w.org x@y.com\n") ==
        "Paragraph 'http://auto.org/a'<http://auto.org/a> ' ' 'me@x.org'<mailto:me@x.org> ' ' "
        "'https://bare.org/p?q=1'<https://bare.org/p?q=1> ' ' 'www.w.org'<http://www.w.org> ' ' "
        "'x@y.com'<mailto:x@y.com>\n");
    // trailing punctuation is no part of an autolink (GFM), a link's own is
    CHECK(describeMarkdown("see https://example.com/docs. or https://x.org/a?! and [b](http://y.org/c.)\n") ==
        "Paragraph 'see ' 'https://example.com/docs'<https://example.com/docs> '. or ' "
        "'https://x.org/a'<https://x.org/a> '?! and ' 'b'<http://y.org/c.>\n");
    // an entity in a url is decoded
    CHECK(describeMarkdown("[a](/q?x=1&amp;y=2)\n") == "Paragraph 'a'</q?x=1&y=2>\n");
}

TEST_CASE("Markdown blocks: soft and hard breaks")
{
    CHECK(describeMarkdown("one\ntwo  \nthree\\\nfour\n") == "Paragraph 'one two\nthree\nfour'\n");
}

TEST_CASE("Markdown blocks: entities")
{
    CHECK(describeMarkdown("&amp; &lt;&gt; &copy; &mdash; &#65; &#x1F600; &#0; &#x110000; &bogus;\n") ==
        "Paragraph '& <> \xC2\xA9 \xE2\x80\x94 A \xF0\x9F\x98\x80 \xEF\xBF\xBD \xEF\xBF\xBD &bogus;'\n");
    // a NUL in the text is U+FFFD too
    CHECK(describeMarkdown(std::string("a\0b\n", 4)) == "Paragraph 'a\xEF\xBF\xBD" "b'\n");
}

TEST_CASE("Markdown blocks: images")
{
    CHECK(describeMarkdown("![alt *em* `c`](img.png \"t\") [![in](i.png)](u) ![](none.png)\n") ==
        "Paragraph !'alt em c'(img.png) ' ' !'in'(i.png)<u> ' ' !''(none.png)\n");
    // a nested image's alt text joins the outer one's
    CHECK(describeMarkdown("![a ![b](x.png) c](y.png)\n") == "Paragraph !'a b c'(y.png)\n");
}

TEST_CASE("Markdown blocks: lists, tight and loose, nested")
{
    CHECK(describeMarkdown("- one\n- two\n  1. a\n  2) b\n     * deep\n       + deeper\n- three\n") ==
        "List bullet '-' tight\n"
        "  ListItem\n"
        "    Paragraph 'one'\n"
        "  ListItem\n"
        "    Paragraph 'two'\n"
        "    List ordered 1 '.' tight\n"
        "      ListItem\n"
        "        Paragraph 'a'\n"
        "    List ordered 2 ')' tight\n"
        "      ListItem\n"
        "        Paragraph 'b'\n"
        "        List bullet '*' tight\n"
        "          ListItem\n"
        "            Paragraph 'deep'\n"
        "            List bullet '+' tight\n"
        "              ListItem\n"
        "                Paragraph 'deeper'\n"
        "  ListItem\n"
        "    Paragraph 'three'\n");
    CHECK(describeMarkdown("3. x\n\n4. y\n\n   more\n") ==
        "List ordered 3 '.' loose\n"
        "  ListItem\n"
        "    Paragraph 'x'\n"
        "  ListItem\n"
        "    Paragraph 'y'\n"
        "    Paragraph 'more'\n");
    // a block between two runs of a tight item's text makes two paragraphs
    CHECK(describeMarkdown("- a\n  ```\n  c\n  ```\n  b\n") ==
        "List bullet '-' tight\n"
        "  ListItem\n"
        "    Paragraph 'a'\n"
        "    CodeBlock fenced \"c\"\n"
        "    Paragraph 'b'\n");
    // an empty item has no paragraph
    CHECK(describeMarkdown("-\n- *x*\n") ==
        "List bullet '-' tight\n"
        "  ListItem\n"
        "  ListItem\n"
        "    Paragraph I'x'\n");
}

TEST_CASE("Markdown blocks: block quotes, nested")
{
    CHECK(describeMarkdown("> quote\n> > nested\n> > > three\n>\n> - li\n> # h\n") ==
        "Quote\n"
        "  Paragraph 'quote'\n"
        "  Quote\n"
        "    Paragraph 'nested'\n"
        "    Quote\n"
        "      Paragraph 'three'\n"
        "  List bullet '-' tight\n"
        "    ListItem\n"
        "      Paragraph 'li'\n"
        "  Heading 1 'h'\n");
    // a lazy continuation line stays in the quote's paragraph
    CHECK(describeMarkdown("> a\nb\n") == "Quote\n  Paragraph 'a b'\n");
}

TEST_CASE("Markdown blocks: rules")
{
    CHECK(describeMarkdown("---\n***\n___\npara\n\n- - -\n") ==
        "Rule\n"
        "Rule\n"
        "Rule\n"
        "Paragraph 'para'\n"
        "Rule\n");
}

TEST_CASE("Markdown blocks: fenced and indented code")
{
    CHECK(describeMarkdown("```c++ extra words\nint x;\n\n  y &amp; *z*\n```\n\n~~~\ntilde\n~~~\n\n    indented\n      code\n") ==
        "CodeBlock fenced c++ \"int x;\n\n  y &amp; *z*\"\n"
        "CodeBlock fenced \"tilde\"\n"
        "CodeBlock indented \"indented\n  code\"\n");
    // an empty fenced block has no text
    CHECK(describeMarkdown("```\n```\n") == "CodeBlock fenced \"\"\n");
}

TEST_CASE("Markdown blocks: GFM tables with column alignment")
{
    CHECK(describeMarkdown("| A | B | C | D |\n|---|--:|:-:|:--|\n| 1 | *2* | `3` | |\n| x |\n") ==
        "Table - right center left\n"
        "  TableRow header\n"
        "    TableCell 'A'\n"
        "    TableCell right 'B'\n"
        "    TableCell center 'C'\n"
        "    TableCell left 'D'\n"
        "  TableRow\n"
        "    TableCell '1'\n"
        "    TableCell right I'2'\n"
        "    TableCell center C'3'\n"
        "    TableCell left\n"
        "  TableRow\n"
        "    TableCell 'x'\n"
        "    TableCell right\n"
        "    TableCell center\n"
        "    TableCell left\n");
    // a table with no body rows, and an escaped pipe in a cell
    CHECK(describeMarkdown("a | b\n--|--\n\nc \\| d\n") ==
        "Table - -\n"
        "  TableRow header\n"
        "    TableCell 'a'\n"
        "    TableCell 'b'\n"
        "Paragraph 'c | d'\n");
}

TEST_CASE("Markdown blocks: raw HTML is literal text")
{
    CHECK(describeMarkdown("<div>\n*not em*\n</div>\n\nan <span class=\"x\">inline</span> tag &amp; <!-- c -->\n") ==
        "Html \"<div>\n*not em*\n</div>\"\n"
        "Paragraph 'an <span class=\"x\">inline</span> tag & <!-- c -->'\n");
    // inline HTML inside a style keeps the style
    CHECK(describeMarkdown("**<b>x</b>**\n") == "Paragraph B'<b>x</b>'\n");
}

TEST_CASE("Markdown blocks: syntax not enabled stays text")
{
    // task-list boxes are text; with footnotes off, [^1]: is a link
    // reference definition as CommonMark reads it, and [^1] a link to it
    CHECK(describeMarkdown("- [ ] task\n- [x] done\n\nref[^1]\n\n[^1]: note\n") ==
        "List bullet '-' tight\n"
        "  ListItem\n"
        "    Paragraph '[ ] task'\n"
        "  ListItem\n"
        "    Paragraph '[x] done'\n"
        "Paragraph 'ref' '^1'<note>\n");
    CHECK(describeMarkdown("") == "");
}

TEST_CASE("Markdown blocks: hashes follow type, attributes, content and nesting")
{
    auto hashOf=[](const std::string &markdown)
    {
        std::vector<MarkdownBlock> blocks=parseMarkdown(markdown);

        REQUIRE(blocks.size() == 1);
        return blocks[0].hash;
    };

    // the same block at another position hashes alike
    std::vector<MarkdownBlock> blocks=parseMarkdown("para\n\n# h\n\npara\n");

    REQUIRE(blocks.size() == 3);
    CHECK(blocks[0].hash == blocks[2].hash);
    CHECK(blocks[0] == blocks[2]);
    CHECK(blocks[0] != blocks[1]);

    const std::vector<std::string> differing=
    {
        "para", "para2", "*para*", "**para**", "`para`", "[para](a)", "[para](b)",
        "![para](a)", "# para", "## para", "> para", "> > para", "- para", "* para",
        "1. para", "2. para", "1) para", "- para\n\n- b", "- para\n- b", "- a\n  - para",
        "- a\n  - para\n    - x", "```\npara\n```", "```js\npara\n```", "    para",
        "<div>para</div>", "---", "| a |\n|---|", "| a |\n|:--|", "| a |\n|---|\n| b |"
    };
    std::vector<uint64_t> hashes;

    for(const std::string &markdown:differing)
    {
        uint64_t hash=hashOf(markdown);

        INFO(markdown);
        for(uint64_t other:hashes)
            CHECK(hash != other);
        hashes.push_back(hash);
    }

    // FNV-1a over fixed-width fields: the same values on every target
    std::vector<MarkdownBlock> pinned=parseMarkdown("# Hi *there*\n\n- a\n  - b\n");

    REQUIRE(pinned.size() == 2);
    CHECK(pinned[0].hash == 0x84d75d8d669cd0d5ull);
    CHECK(pinned[1].hash == 0x8ea7bc5c61af05f5ull);
    CHECK(pinned[1].children[0].children[1].hash == 0xd6d1194efc74966full);
}
