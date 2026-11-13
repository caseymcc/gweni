/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/tokenizer.h>

#include "highlight/languages.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <mutex>

namespace gweni
{
namespace text
{

const char *tokenKindName(TokenKind kind)
{
    switch(kind)
    {
    case TokenKind::Plain: return "plain";
    case TokenKind::Keyword: return "keyword";
    case TokenKind::String: return "string";
    case TokenKind::Number: return "number";
    case TokenKind::Comment: return "comment";
    case TokenKind::Key: return "key";
    case TokenKind::Punctuation: return "punctuation";
    case TokenKind::Heading: return "heading";
    case TokenKind::Emphasis: return "emphasis";
    case TokenKind::Strong: return "strong";
    case TokenKind::Code: return "code";
    case TokenKind::Link: return "link";
    case TokenKind::Template: return "template";
    case TokenKind::Added: return "added";
    case TokenKind::Removed: return "removed";
    case TokenKind::Hunk: return "hunk";
    case TokenKind::Meta: return "meta";
    case TokenKind::Count: break;
    }
    return "";
}

namespace
{

struct Registry
{
    std::mutex mutex;
    std::map<String, Tokenizer *> languages;
    //! every tokenizer ever registered, so a pointer handed out stays valid
    std::vector<std::unique_ptr<Tokenizer>> owned;
    bool builtinsAdded=false;
};

Registry &registry()
{
    static Registry instance;

    return instance;
}

//! Registers the built-ins on the first use; the caller holds the mutex.
void addBuiltins(Registry &reg)
{
    if(reg.builtinsAdded)
        return;
    reg.builtinsAdded=true;
    for(auto &language:highlight::builtinLanguages())
    {
        // a name the app registered first keeps the app's
        if(reg.languages.count(language.first) != 0)
            continue;
        reg.languages[language.first]=language.second.get();
        reg.owned.push_back(std::move(language.second));
    }
}

bool endsWith(const String &text, const char *suffix)
{
    const size_t length=strlen(suffix);

    return (text.size() >= length) && (text.compare(text.size()-length, length, suffix) == 0);
}

}//namespace

const Tokenizer *findLanguage(const String &name)
{
    Registry &reg=registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    addBuiltins(reg);

    const auto it=reg.languages.find(name);

    return (it == reg.languages.end())?nullptr:it->second;
}

void registerLanguage(const String &name, std::unique_ptr<Tokenizer> tokenizer)
{
    Registry &reg=registry();
    std::lock_guard<std::mutex> lock(reg.mutex);

    addBuiltins(reg);
    if(!tokenizer)
    {
        reg.languages.erase(name);
        return;
    }
    reg.languages[name]=tokenizer.get();
    reg.owned.push_back(std::move(tokenizer));
}

std::vector<String> languageNames()
{
    Registry &reg=registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    std::vector<String> names;

    addBuiltins(reg);
    for(const auto &language:reg.languages)
        names.push_back(language.first);
    return names;
}

String languageForFileName(const String &fileName)
{
    // longest suffix first: *.md.j2 before *.md
    static const struct
    {
        const char *suffix;
        const char *language;
    } suffixes[]=
    {
        {".md.j2", "markdown+jinja"},
        {".yaml", "yaml"},
        {".yml", "yaml"},
        {".json", "json"},
        {".md", "markdown"},
        {".diff", "diff"},
        {".patch", "diff"},
        {".sh", "bash"}
    };
    String lower(fileName);

    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for(const auto &entry:suffixes)
    {
        // a bare suffix (".yaml") is a hidden file's name, not a yaml file
        if(endsWith(lower, entry.suffix) && (lower.size() > strlen(entry.suffix)) &&
            (lower[lower.size()-strlen(entry.suffix)-1] != '/'))
            return entry.language;
    }
    return String();
}

}//namespace text
}//namespace gweni
