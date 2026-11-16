/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_fuzzymatch_h_
#define _gweni_text_fuzzymatch_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gweni
{
namespace text
{

//! The fuzzy scorer a CommandPalette ranks its commands with (fzf's and VS
//! Code's scorers are the model, kept small). Both strings are read as UTF-8
//! code points (a byte that starts no valid sequence is a code point of its
//! own, so nothing is refused); ASCII letters compare case-insensitively,
//! every other code point as it is.
//!
//! \p query matches \p text when its code points appear in \p text in order
//! (a subsequence). The score is the best over every way they can appear:
//! - each matched code point MatchScore;
//! - one at the text's start StartBonus, one starting a word (after a space
//!   or one of `:-_/.,()[]`) WordBonus, one at a camelCase hump (an upper-case
//!   ASCII letter after a lower-case one or a digit) CamelBonus;
//! - one right after the previous match ConsecutiveBonus;
//! - a gap of d code points between two matches -(GapStart+(d-1)*GapExtend);
//!   the code points before the first match cost nothing.
//! So a prefix scores above a word start, a word start above a camel hump,
//! a hump above a scattered match, and a consecutive run above the same
//! characters with gaps. An empty query matches everything with score 0.
//! The cost is O(query x text) code points.
constexpr int FuzzyMatchScore=16;
constexpr int FuzzyStartBonus=10;
constexpr int FuzzyWordBonus=8;
constexpr int FuzzyCamelBonus=7;
constexpr int FuzzyConsecutiveBonus=5;
constexpr int FuzzyGapStart=3;
constexpr int FuzzyGapExtend=1;

//! Whether \p query matches \p text (above), its score in \p score (left as
//! it was when it does not match).
GWENI_EXPORT bool fuzzyMatch(const String &query, const String &text, int &score);

//! fuzzyMatch() for one query over many texts: the query decoded once and
//! the buffers kept between texts, so a match allocates nothing once they
//! have grown to the longest text.
class GWENI_EXPORT FuzzyMatcher
{
public:
    explicit FuzzyMatcher(const String &query);

    //! As fuzzyMatch(query, text, score).
    bool match(const String &text, int &score);

private:
    std::vector<uint32_t> m_query;
    std::vector<uint32_t> m_text;
    std::vector<int> m_row;
    std::vector<int> m_next;
};

//! The indices of \p texts that \p query matches, best score first; equal
//! scores in the order of \p tieRank (lower first, when given: one value per
//! text) and then by index, so the order is the same on every run.
GWENI_EXPORT void fuzzyRank(const String &query, const std::vector<String> &texts,
    std::vector<size_t> &matches, const std::vector<int> *tieRank=nullptr);

}//namespace text
}//namespace gweni

#endif//_gweni_text_fuzzymatch_h_
