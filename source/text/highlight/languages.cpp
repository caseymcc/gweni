/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "languages.h"

namespace gweni
{
namespace text
{
namespace highlight
{

std::vector<std::pair<String, std::unique_ptr<Tokenizer>>> builtinLanguages()
{
    std::vector<std::pair<String, std::unique_ptr<Tokenizer>>> languages;

    languages.emplace_back("yaml", makeYamlTokenizer());
    languages.emplace_back("json", makeJsonTokenizer());
    languages.emplace_back("diff", makeDiffTokenizer());
    languages.emplace_back("bash", makeBashTokenizer());

    // markdown+jinja's host is the registry's markdown, which the registry
    // keeps alive as long as it keeps the overlay
    std::unique_ptr<Tokenizer> markdown=makeMarkdownTokenizer();
    std::unique_ptr<Tokenizer> jinja=makeJinjaOverlay(*markdown);

    languages.emplace_back("markdown", std::move(markdown));
    languages.emplace_back("markdown+jinja", std::move(jinja));

    return languages;
}

}//namespace highlight
}//namespace text
}//namespace gweni
