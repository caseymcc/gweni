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

    return languages;
}

}//namespace highlight
}//namespace text
}//namespace gweni
