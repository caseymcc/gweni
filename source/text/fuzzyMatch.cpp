/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/text/fuzzyMatch.h>

#include <algorithm>
#include <climits>
#include <cstdint>

namespace gweni
{
namespace text
{

namespace
{

// A byte that starts no valid sequence: past every code point, so it equals
// only the same byte.
constexpr uint32_t InvalidByte=0x110000;
constexpr int NoMatch=INT_MIN/4;

// \p text as code points: a valid 2..4 byte sequence one, any other byte one
// of its own (InvalidByte+byte).
void decode(const String &text, std::vector<uint32_t> &points)
{
    points.clear();

    const size_t size=text.size();
    size_t i=0;

    while(i < size)
    {
        const uint8_t lead=static_cast<uint8_t>(text[i]);
        size_t length=0;
        uint32_t point=0;

        if(lead < 0x80)
        {
            points.push_back(lead);
            ++i;
            continue;
        }
        else if((lead & 0xe0) == 0xc0)
        {
            length=2;
            point=lead & 0x1f;
        }
        else if((lead & 0xf0) == 0xe0)
        {
            length=3;
            point=lead & 0x0f;
        }
        else if((lead & 0xf8) == 0xf0)
        {
            length=4;
            point=lead & 0x07;
        }

        bool valid=(length > 0) && (i+length <= size);

        for(size_t k=1; valid && (k < length); ++k)
        {
            const uint8_t next=static_cast<uint8_t>(text[i+k]);

            if((next & 0xc0) != 0x80)
                valid=false;
            else
                point=(point << 6) | (next & 0x3f);
        }

        if(valid)
        {
            points.push_back(point);
            i+=length;
        }
        else
        {
            points.push_back(InvalidByte+lead);
            ++i;
        }
    }
}

uint32_t folded(uint32_t point)
{
    return ((point >= 'A') && (point <= 'Z'))?point-'A'+'a':point;
}

bool isUpper(uint32_t point)
{
    return (point >= 'A') && (point <= 'Z');
}

bool isLowerOrDigit(uint32_t point)
{
    return ((point >= 'a') && (point <= 'z')) || ((point >= '0') && (point <= '9'));
}

bool isSeparator(uint32_t point)
{
    switch(point)
    {
    case ' ': case ':': case '-': case '_': case '/': case '.': case ',':
    case '(': case ')': case '[': case ']':
        return true;
    default:
        return false;
    }
}

// The bonus of a match at text code point \p at.
int bonusAt(const std::vector<uint32_t> &text, size_t at)
{
    if(at == 0)
        return FuzzyStartBonus;

    const uint32_t before=text[at-1];

    if(isSeparator(before))
        return FuzzyWordBonus;
    if(isUpper(text[at]) && isLowerOrDigit(before))
        return FuzzyCamelBonus;
    return 0;
}

// The best score of \p query (folded code points) in \p text (its code
// points as they are), or NoMatch. One row of the table per query code
// point: row[j] the best score with that query point matched at text point
// j; a gap's penalty is affine, so the best over every earlier match is kept
// as a running value instead of a scan.
int bestScore(const std::vector<uint32_t> &query, const std::vector<uint32_t> &text,
    std::vector<int> &row, std::vector<int> &next)
{
    const size_t n=text.size();
    const size_t m=query.size();

    if(m == 0)
        return 0;
    if(m > n)
        return NoMatch;

    row.assign(n, NoMatch);

    for(size_t j=0; j < n; ++j)
    {
        if(folded(text[j]) == query[0])
            row[j]=FuzzyMatchScore+bonusAt(text, j);
    }

    for(size_t i=1; i < m; ++i)
    {
        next.assign(n, NoMatch);

        // the best of row[k]-(GapStart+(j-k-2)*GapExtend) over k <= j-2
        int gapped=NoMatch;

        for(size_t j=i; j < n; ++j)
        {
            if(j >= 2)
            {
                const int from=(row[j-2] == NoMatch)?NoMatch:row[j-2]-FuzzyGapStart;
                const int carried=(gapped == NoMatch)?NoMatch:gapped-FuzzyGapExtend;

                gapped=std::max(from, carried);
            }

            if(folded(text[j]) != query[i])
                continue;

            int before=gapped;

            if(row[j-1] != NoMatch)
                before=std::max(before, row[j-1]+FuzzyConsecutiveBonus);

            if(before != NoMatch)
                next[j]=before+FuzzyMatchScore+bonusAt(text, j);
        }
        row.swap(next);
    }

    return *std::max_element(row.begin(), row.end());
}

void decodeQuery(const String &query, std::vector<uint32_t> &points)
{
    decode(query, points);

    for(uint32_t &point : points)
        point=folded(point);
}

}//namespace

bool fuzzyMatch(const String &query, const String &text, int &score)
{
    std::vector<uint32_t> queryPoints;
    std::vector<uint32_t> textPoints;
    std::vector<int> row;
    std::vector<int> next;

    decodeQuery(query, queryPoints);
    decode(text, textPoints);

    const int best=bestScore(queryPoints, textPoints, row, next);

    if(best == NoMatch)
        return false;

    score=best;
    return true;
}

void fuzzyRank(const String &query, const std::vector<String> &texts, std::vector<size_t> &matches,
    const std::vector<int> *tieRank)
{
    std::vector<uint32_t> queryPoints;
    std::vector<uint32_t> textPoints;
    std::vector<int> row;
    std::vector<int> next;
    std::vector<int> scores(texts.size(), NoMatch);

    decodeQuery(query, queryPoints);
    matches.clear();

    for(size_t i=0; i < texts.size(); ++i)
    {
        decode(texts[i], textPoints);
        scores[i]=bestScore(queryPoints, textPoints, row, next);

        if(scores[i] != NoMatch)
            matches.push_back(i);
    }

    std::sort(matches.begin(), matches.end(), [&](size_t a, size_t b)
        {
            if(scores[a] != scores[b])
                return scores[a] > scores[b];

            if(tieRank)
            {
                const int rankA=(*tieRank)[a];
                const int rankB=(*tieRank)[b];

                if(rankA != rankB)
                    return rankA < rankB;
            }
            return a < b;
        });
}

}//namespace text
}//namespace gweni
