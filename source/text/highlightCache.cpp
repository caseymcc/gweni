/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/highlightCache.h>

#include <algorithm>

namespace gweni
{
namespace text
{

namespace
{

const std::vector<Token> &noTokens()
{
    static const std::vector<Token> none;

    return none;
}

//! The capacity for \p needed entries: a power of two, so a line inserted
//! after a new text or past a power of two moves the entries once, and the
//! lines inserted and removed below one move none (as the Document's own vector
//! of lines grows)
size_t roomFor(size_t needed)
{
    size_t capacity=16;

    while(capacity < needed)
        capacity*=2;
    return capacity;
}

}//namespace

HighlightCache::HighlightCache()
{}

HighlightCache::~HighlightCache()
{
    attach(nullptr);
}

void HighlightCache::attach(Document *document)
{
    if(m_observed)
        m_observed->removeObserver(m_observerId);
    m_observed=document;
    m_observerId=0;
    if(document)
        m_observerId=document->addObserver([this](const LineChange &change) { linesChanged(change); });
    setDocument(document);
}

void HighlightCache::setDocument(const Document *document)
{
    m_document=document;
    reset();
}

void HighlightCache::setTokenizer(const Tokenizer *tokenizer)
{
    m_tokenizer=tokenizer;
    reset();
}

// Without a tokenizer it keeps no entries, so an editor with no language
// pays nothing per line.
void HighlightCache::reset()
{
    m_entries.clear();
    if(!m_tokenizer)
        m_entries.shrink_to_fit();
    else if(m_document)
    {
        m_entries.reserve(roomFor(m_document->lineCount()));
        m_entries.resize(m_document->lineCount());
    }
    m_frontier=0;
}

void HighlightCache::setVisibleLines(size_t first, size_t count)
{
    m_visibleFirst=first;
    m_visibleCount=count;
}

void HighlightCache::linesChanged(const LineChange &change)
{
    if(!m_document)
        return;
    if(!m_tokenizer)
    {
        // no entries to shift, but the window moves with its lines all the same
        if(m_visibleFirst >= change.first+change.removed)
            m_visibleFirst=m_visibleFirst+change.inserted-change.removed;
        return;
    }

    const size_t first=std::min(change.first, m_entries.size());
    const size_t removed=std::min(change.removed, m_entries.size()-first);
    const size_t kept=std::min(removed, change.inserted);

    // The replaced lines that are still there keep their entries (and their
    // tokens' capacity), stale; the rest are erased or inserted. The entries
    // after them move and are not tokenized again.
    for(size_t i=0; i < kept; ++i)
        m_entries[first+i].stale=true;
    if(removed > kept)
        m_entries.erase(m_entries.begin()+first+kept, m_entries.begin()+first+removed);
    else if(change.inserted > kept)
    {
        const size_t needed=m_entries.size()+change.inserted-kept;

        if(needed > m_entries.capacity())
            m_entries.reserve(roomFor(needed));
        m_entries.insert(m_entries.begin()+first+kept, change.inserted-kept, Entry());
    }

    // should the document and the changes disagree, the document wins
    if(m_entries.size() != m_document->lineCount())
    {
        reset();
        return;
    }

    m_frontier=std::min(m_frontier, first);

    // A window below the lines replaced moves with its lines (as a view
    // anchored on them does), so the pass reaches the lines on screen after
    // the edit, not those at the old numbers
    if(m_visibleFirst >= first+removed)
        m_visibleFirst=m_visibleFirst+change.inserted-removed;

    if((m_visibleCount == 0) || m_entries.empty())
        return;

    const size_t lastVisible=std::min(m_visibleFirst+m_visibleCount, m_entries.size())-1;

    if((m_visibleFirst >= m_entries.size()) || (first > lastVisible))
        return;

    m_changed.clear();
    freshen(lastVisible, true);
    if(m_changeHandler && !m_changed.empty())
        m_changeHandler(m_changed);
}

void HighlightCache::freshen(size_t last, bool record)
{
    if(!m_tokenizer || m_entries.empty())
        return;
    last=std::min(last, m_entries.size()-1);
    if(m_frontier > last)
        return;

    TokenState previous=(m_frontier == 0)?m_tokenizer->initialState():m_entries[m_frontier-1].end;

    for(size_t i=m_frontier; i <= last; ++i)
    {
        Entry &entry=m_entries[i];

        if(entry.stale || (entry.start != previous))
        {
            if(record)
                m_previous.swap(entry.tokens);
            entry.tokens.clear();
            entry.start=previous;
            entry.end=m_tokenizer->tokenize(m_document->line(i), previous, entry.tokens);
            entry.stale=false;
            ++m_tokenizedLines;
            if(record && (m_previous != entry.tokens))
                m_changed.push_back(i);
        }
        previous=entry.end;
    }
    m_frontier=last+1;
}

const std::vector<Token> &HighlightCache::tokens(size_t line)
{
    if(!m_tokenizer || (line >= m_entries.size()))
        return noTokens();
    freshen(line, false);
    return m_entries[line].tokens;
}

TokenState HighlightCache::endState(size_t line)
{
    if(!m_tokenizer || (line >= m_entries.size()))
        return TokenState();
    freshen(line, false);
    return m_entries[line].end;
}

bool HighlightCache::isFresh(size_t line) const
{
    if(!m_tokenizer || (line >= m_entries.size()))
        return false;
    if(line < m_frontier)
        return true;

    TokenState previous=(m_frontier == 0)?m_tokenizer->initialState():m_entries[m_frontier-1].end;

    for(size_t i=m_frontier; i <= line; ++i)
    {
        const Entry &entry=m_entries[i];

        if(entry.stale || (entry.start != previous))
            return false;
        previous=entry.end;
    }
    return true;
}

}//namespace text
}//namespace gweni
