/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_tokenizer_h_
#define _gweni_text_tokenizer_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace gweni
{
namespace text
{

//! What a token is, for its colour: a fixed set every language maps onto, so
//! a theme gives each kind one colour whatever the language.
enum class TokenKind: uint8_t
{
    Plain,
    Keyword,
    String,
    Number,
    Comment,
    Key,
    Punctuation,
    Heading,
    Emphasis,
    Strong,
    Code,
    Link,
    Template,
    Added,
    Removed,
    Hunk,
    Meta,
    Count //!< not a kind: how many there are
};

//! The kind's name in lower case ("keyword", "string", ...), "" for Count.
GWENI_EXPORT const char *tokenKindName(TokenKind kind);

//! One token of a line: bytes [begin, end) of it, and their kind.
struct Token
{
    uint32_t begin=0;
    uint32_t end=0;
    TokenKind kind=TokenKind::Plain;

    bool operator==(const Token &other) const
    {
        return (begin == other.begin) && (end == other.end) && (kind == other.kind);
    }
    bool operator!=(const Token &other) const { return !(*this == other); }
};

//! What a tokenizer carries from the end of one line to the start of the
//! next: an open block string, comment, fence or heredoc, say. A small value,
//! compared to tell whether an edit changed what the lines after it start in.
//! A language uses the words as it likes (all zero is its usual start); one
//! nested in another (a fence's language in Markdown, the host under Jinja)
//! keeps its state in the words after the outer one's (nested(), inner()), so
//! a language another nests should leave its last words zero: Jinja over
//! Markdown over a fence's language leaves that language Words-2 of them.
struct TokenState
{
    static constexpr size_t Words=6;

    std::array<uint32_t, Words> words{};

    bool operator==(const TokenState &other) const { return words == other.words; }
    bool operator!=(const TokenState &other) const { return !(*this == other); }

    //! A state whose first word is \p outer and whose other words are the
    //! first Words-1 of \p inner (an inner state using its last word cannot nest).
    static TokenState nested(uint32_t outer, const TokenState &inner)
    {
        TokenState state;

        state.words[0]=outer;
        for(size_t i=1; i < Words; ++i)
            state.words[i]=inner.words[i-1];
        return state;
    }
    //! The state nested() put after the first word.
    TokenState inner() const
    {
        TokenState state;

        for(size_t i=1; i < Words; ++i)
            state.words[i-1]=words[i];
        return state;
    }
};

//! A language's highlighter: one line at a time, from the state the line
//! starts in. Each line is tokenized alone (a tokenizer keeps no state of its
//! own between calls), so a cache can redo any line from its start state.
class GWENI_EXPORT Tokenizer
{
public:
    virtual ~Tokenizer()=default;

    //! The state the first line starts in.
    virtual TokenState initialState() const { return TokenState(); }

    //! Appends the tokens of \p line (no '\n' in it), which starts in \p start,
    //! to \p tokens: in ascending order, not overlapping, inside the line (a
    //! byte in no token is Plain, and a Plain token need not be given); and
    //! returns the state the next line starts in.
    virtual TokenState tokenize(const String &line, const TokenState &start,
        std::vector<Token> &tokens) const=0;
};

//! The tokenizer registered under \p name ("yaml", "markdown+jinja", ...),
//! nullptr if none is. The built-in languages are registered before the
//! first lookup.
GWENI_EXPORT const Tokenizer *findLanguage(const String &name);

//! Registers \p tokenizer under \p name, replacing one registered under it
//! before (the registry owns it; a null one removes the name). A tokenizer
//! found before stays alive until the process ends, so a pointer
//! findLanguage() gave never dangles.
GWENI_EXPORT void registerLanguage(const String &name, std::unique_ptr<Tokenizer> tokenizer);

//! Every registered name, sorted.
GWENI_EXPORT std::vector<String> languageNames();

//! The language a file of this name is in, by its suffix, whatever the case:
//! "yaml" for *.yaml and *.yml, "json" for *.json, "markdown" for *.md,
//! "markdown+jinja" for *.md.j2, "diff" for *.diff and *.patch, "bash" for
//! *.sh; "" for any other. \p fileName may hold a path.
GWENI_EXPORT String languageForFileName(const String &fileName);

//! Jinja over \p host (which must outlive it): the tags {{ ... }}, {% ... %}
//! and {# ... #}, each of which may run over lines, are Template (a {% %}'s
//! statement name a Keyword, a quoted string in a tag a String) and Comment,
//! and \p host tokenizes the line around them as if each tag were a word of
//! as many letters, so the Markdown around a tag (the **...** holding it, the
//! heading it is in) reads as without it. A line wholly inside tags leaves the
//! host's state as it was. The registry's "markdown+jinja" is Jinja over its
//! "markdown".
GWENI_EXPORT std::unique_ptr<Tokenizer> makeJinjaOverlay(const Tokenizer &host);

}//namespace text
}//namespace gweni

#endif//_gweni_text_tokenizer_h_
