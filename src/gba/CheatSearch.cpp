#include <memory.h>
#include <stdlib.h>

#include "CheatSearch.h"

CheatSearchBlock cheatSearchBlocks[4];

CheatSearchData cheatSearchData = {
    0,
    cheatSearchBlocks
};

static bool cheatSearchEQ(uint32_t a, uint32_t b)
{
    return a == b;
}

static bool cheatSearchNE(uint32_t a, uint32_t b)
{
    return a != b;
}

static bool cheatSearchLT(uint32_t a, uint32_t b)
{
    return a < b;
}

static bool cheatSearchLE(uint32_t a, uint32_t b)
{
    return a <= b;
}

static bool cheatSearchGT(uint32_t a, uint32_t b)
{
    return a > b;
}

static bool cheatSearchGE(uint32_t a, uint32_t b)
{
    return a >= b;
}

static bool cheatSearchSignedEQ(int32_t a, int32_t b)
{
    return a == b;
}

static bool cheatSearchSignedNE(int32_t a, int32_t b)
{
    return a != b;
}

static bool cheatSearchSignedLT(int32_t a, int32_t b)
{
    return a < b;
}

static bool cheatSearchSignedLE(int32_t a, int32_t b)
{
    return a <= b;
}

static bool cheatSearchSignedGT(int32_t a, int32_t b)
{
    return a > b;
}

static bool cheatSearchSignedGE(int32_t a, int32_t b)
{
    return a >= b;
}

static bool (*cheatSearchFunc[])(uint32_t, uint32_t) = {
    cheatSearchEQ,
    cheatSearchNE,
    cheatSearchLT,
    cheatSearchLE,
    cheatSearchGT,
    cheatSearchGE
};

static bool (*cheatSearchSignedFunc[])(int32_t, int32_t) = {
    cheatSearchSignedEQ,
    cheatSearchSignedNE,
    cheatSearchSignedLT,
    cheatSearchSignedLE,
    cheatSearchSignedGT,
    cheatSearchSignedGE
};

void cheatSearchCleanup(CheatSearchData* cs)
{
    int count = cs->count;

    for (int i = 0; i < count; i++) {
        free(cs->blocks[i].saved);
        free(cs->blocks[i].bits);
    }
    cs->count = 0;
}

void cheatSearchStart(const CheatSearchData* cs)
{
    int count = cs->count;

    for (int i = 0; i < count; i++) {
        CheatSearchBlock* block = &cs->blocks[i];

        memset(block->bits, 0xff, block->size >> 3);
        memcpy(block->saved, block->data, block->size);
    }
}

int32_t cheatSearchSignedRead(uint8_t* data, int off, int size)
{
    uint32_t res = data[off++];

    switch (size) {
    case BITS_8:
        res <<= 24;
        return ((int32_t)res) >> 24;
    case BITS_16:
        res |= ((uint32_t)data[off++]) << 8;
        res <<= 16;
        return ((int32_t)res) >> 16;
    case BITS_32:
        res |= ((uint32_t)data[off++]) << 8;
        res |= ((uint32_t)data[off++]) << 16;
        res |= ((uint32_t)data[off++]) << 24;
        return (int32_t)res;
    }
    return (int32_t)res;
}

uint32_t cheatSearchRead(uint8_t* data, int off, int size)
{
    uint32_t res = data[off++];
    if (size == BITS_16)
        res |= ((uint32_t)data[off++]) << 8;
    else if (size == BITS_32) {
        res |= ((uint32_t)data[off++]) << 8;
        res |= ((uint32_t)data[off++]) << 16;
        res |= ((uint32_t)data[off++]) << 24;
    }
    return res;
}

void cheatSearch(const CheatSearchData* cs, int compare, int size,
    bool isSigned)
{
    if (compare < 0 || compare > SEARCH_GE)
        return;
    int inc = 1;
    if (size == BITS_16)
        inc = 2;
    else if (size == BITS_32)
        inc = 4;

    if (isSigned) {
        bool (*func)(int32_t, int32_t) = cheatSearchSignedFunc[compare];

        for (int i = 0; i < cs->count; i++) {
            CheatSearchBlock* block = &cs->blocks[i];
            int size2 = block->size;
            uint8_t* bits = block->bits;
            uint8_t* data = block->data;
            uint8_t* saved = block->saved;

            for (int j = 0; j < size2; j += inc) {
                if (IS_BIT_SET(bits, j)) {
                    int32_t a = cheatSearchSignedRead(data, j, size);
                    int32_t b = cheatSearchSignedRead(saved, j, size);

                    if (!func(a, b)) {
                        CLEAR_BIT(bits, j);
                        if (size == BITS_16)
                            CLEAR_BIT(bits, j + 1);
                        if (size == BITS_32) {
                            CLEAR_BIT(bits, j + 2);
                            CLEAR_BIT(bits, j + 3);
                        }
                    }
                }
            }
        }
    } else {
        bool (*func)(uint32_t, uint32_t) = cheatSearchFunc[compare];

        for (int i = 0; i < cs->count; i++) {
            CheatSearchBlock* block = &cs->blocks[i];
            int size2 = block->size;
            uint8_t* bits = block->bits;
            uint8_t* data = block->data;
            uint8_t* saved = block->saved;

            for (int j = 0; j < size2; j += inc) {
                if (IS_BIT_SET(bits, j)) {
                    uint32_t a = cheatSearchRead(data, j, size);
                    uint32_t b = cheatSearchRead(saved, j, size);

                    if (!func(a, b)) {
                        CLEAR_BIT(bits, j);
                        if (size == BITS_16)
                            CLEAR_BIT(bits, j + 1);
                        if (size == BITS_32) {
                            CLEAR_BIT(bits, j + 2);
                            CLEAR_BIT(bits, j + 3);
                        }
                    }
                }
            }
        }
    }
}

void cheatSearchValue(const CheatSearchData* cs, int compare, int size,
    bool isSigned, uint32_t value)
{
    if (compare < 0 || compare > SEARCH_GE)
        return;
    int inc = 1;
    if (size == BITS_16)
        inc = 2;
    else if (size == BITS_32)
        inc = 4;

    if (isSigned) {
        bool (*func)(int32_t, int32_t) = cheatSearchSignedFunc[compare];

        for (int i = 0; i < cs->count; i++) {
            CheatSearchBlock* block = &cs->blocks[i];
            int size2 = block->size;
            uint8_t* bits = block->bits;
            uint8_t* data = block->data;

            for (int j = 0; j < size2; j += inc) {
                if (IS_BIT_SET(bits, j)) {
                    int32_t a = cheatSearchSignedRead(data, j, size);
                    int32_t b = (int32_t)value;

                    if (!func(a, b)) {
                        CLEAR_BIT(bits, j);
                        if (size == BITS_16)
                            CLEAR_BIT(bits, j + 1);
                        if (size == BITS_32) {
                            CLEAR_BIT(bits, j + 2);
                            CLEAR_BIT(bits, j + 3);
                        }
                    }
                }
            }
        }
    } else {
        bool (*func)(uint32_t, uint32_t) = cheatSearchFunc[compare];

        for (int i = 0; i < cs->count; i++) {
            CheatSearchBlock* block = &cs->blocks[i];
            int size2 = block->size;
            uint8_t* bits = block->bits;
            uint8_t* data = block->data;

            for (int j = 0; j < size2; j += inc) {
                if (IS_BIT_SET(bits, j)) {
                    uint32_t a = cheatSearchRead(data, j, size);

                    if (!func(a, value)) {
                        CLEAR_BIT(bits, j);
                        if (size == BITS_16)
                            CLEAR_BIT(bits, j + 1);
                        if (size == BITS_32) {
                            CLEAR_BIT(bits, j + 2);
                            CLEAR_BIT(bits, j + 3);
                        }
                    }
                }
            }
        }
    }
}

int cheatSearchGetCount(const CheatSearchData* cs, int size)
{
    int res = 0;
    int inc = 1;
    if (size == BITS_16)
        inc = 2;
    else if (size == BITS_32)
        inc = 4;

    for (int i = 0; i < cs->count; i++) {
        CheatSearchBlock* block = &cs->blocks[i];

        int size2 = block->size;
        uint8_t* bits = block->bits;
        for (int j = 0; j < size2; j += inc) {
            if (IS_BIT_SET(bits, j))
                res++;
        }
    }
    return res;
}

void cheatSearchUpdateValues(const CheatSearchData* cs)
{
    for (int i = 0; i < cs->count; i++) {
        CheatSearchBlock* block = &cs->blocks[i];

        memcpy(block->saved, block->data, block->size);
    }
}
