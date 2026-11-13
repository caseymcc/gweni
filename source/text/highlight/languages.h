/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

//! The built-in languages, one file each in this directory: each file's
//! make*Tokenizer() makes its tokenizer, and builtinLanguages() lists them for
//! the registry (findLanguage()), which registers them before its first lookup.

#ifndef _gweni_text_highlight_languages_h_
#define _gweni_text_highlight_languages_h_

#include <gweni/text/tokenizer.h>

#include <memory>
#include <utility>
#include <vector>

namespace gweni
{
namespace text
{
namespace highlight
{

std::unique_ptr<Tokenizer> makeYamlTokenizer();
std::unique_ptr<Tokenizer> makeJsonTokenizer();
std::unique_ptr<Tokenizer> makeDiffTokenizer();
std::unique_ptr<Tokenizer> makeBashTokenizer();

//! Every built-in language, by its registry name.
std::vector<std::pair<String, std::unique_ptr<Tokenizer>>> builtinLanguages();

}//namespace highlight
}//namespace text
}//namespace gweni

#endif//_gweni_text_highlight_languages_h_
