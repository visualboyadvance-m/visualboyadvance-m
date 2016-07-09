#ifndef CHEATSEARCH_H
#define CHEATSEARCH_H

#include "../System.h"

struct CheatSearchBlock {
    int size;
    uint32_t offset;
    uint8_t* bits;
    uint8_t* data;
    uint8_t* saved;
};

struct CheatSearchData {
    int count;
    CheatSearchBlock* blocks;
};

enum { SEARCH_EQ,
    SEARCH_NE,
    SEARCH_LT,
    SEARCH_LE,
    SEARCH_GT,
    SEARCH_GE };

enum { BITS_8,
    BITS_16,
    BITS_32 };

#define SET_BIT(bits, off) (bits)[(off) >> 3] |= (1 << ((off)&7))

#define CLEAR_BIT(bits, off) (bits)[(off) >> 3] &= ~(1 << ((off)&7))

#define IS_BIT_SET(bits, off) (bits)[(off) >> 3] & (1 << ((off)&7))

extern CheatSearchData cheatSearchData;

void cheatSearchCleanup(CheatSearchData* cs);
void cheatSearchStart(const CheatSearchData* cs);
void cheatSearch(const CheatSearchData* cs, int compare, int size, bool isSigned);
void cheatSearchValue(const CheatSearchData* cs, int compare, int size, bool isSigned, uint32_t value);
int cheatSearchGetCount(const CheatSearchData* cs, int size);
void cheatSearchUpdateValues(const CheatSearchData* cs);
int32_t cheatSearchSignedRead(uint8_t* data, int off, int size);
uint32_t cheatSearchRead(uint8_t* data, int off, int size);

#endif // CHEATSEARCH_H
