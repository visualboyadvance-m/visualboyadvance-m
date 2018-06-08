#ifndef VBA_BKS_H
#define VBA_BKS_H

#include "../common/Types.h"

#define readWord(addr) \
    ((map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]) + ((map[(addr + 1) >> 24].address[(addr + 1) & map[(addr + 1) >> 24].mask]) << 8) + ((map[(addr + 2) >> 24].address[(addr + 2) & map[(addr + 2) >> 24].mask]) << 16) + ((map[(addr + 3) >> 24].address[(addr + 3) & map[(addr + 3) >> 24].mask]) << 24))

#define readHalfWord(addr) \
    ((&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]) + ((&map[(addr + 1) >> 24].address[(addr + 1) & map[(addr + 1) >> 24].mask]) << 8))

#define readByte(addr) map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]

struct ConditionalBreakNode {
    char* address;
    char* value;
    uint8_t cond_flags;
    uint8_t exp_type_flags;
    struct ConditionalBreakNode* next;
};

struct ConditionalBreak {
    uint32_t break_address;
    uint8_t type_flags;
    struct ConditionalBreakNode* firstCond;
    struct ConditionalBreak* next;
};

extern struct ConditionalBreak* conditionals[16];

// conditional break manipulators
// case '*':	flag = 0xf;	break;
// case 't':	flag = 0x8;	break; // thumb
// case 'a':	flag = 0x4;	break; // arm
// case 'x':	flag = 0xC;	break;
// case 'r':	flag = 0x2;	break; // mem read
// case 'w':	flag = 0x1;	break; // mem write
// case 'i':	flag = 0x3;	break;
struct ConditionalBreak* addConditionalBreak(uint32_t address, uint8_t flag);

int removeConditionalBreakNo(uint32_t address, uint8_t number);
int removeFlagFromConditionalBreakNo(uint32_t address, uint8_t number, uint8_t flag);
int removeConditionalWithAddress(uint32_t address);
int removeConditionalWithFlag(uint8_t flag, bool orMode);
int removeConditionalWithAddressAndFlag(uint32_t address, uint8_t flag, bool orMode);
// void freeConditionalBreak(struct ConditionalBreak* toFree);

void addCondition(struct ConditionalBreak* base, struct ConditionalBreakNode* toAdd);
// bool removeCondition(struct ConditionalBreak* base, struct ConditionalBreakNode* toDel);
// bool removeCondition(uint32_t address, uint8_t flags, uint8_t num);

void freeConditionalNode(struct ConditionalBreakNode* toDel);

void parseAndCreateConditionalBreaks(uint32_t address, uint8_t flags, char** exp, int n);

bool isCorrectBreak(struct ConditionalBreak* toTest, uint8_t accessType);
bool doesBreak(uint32_t address, uint8_t allowedFlags);
bool doBreak(struct ConditionalBreak* toTest);

// printing the structure(AKA list Breaks)
// void printConditionalBreak(struct ConditionalBreak* toPrint, bool printAddress);
// void printAllConditionals();
// uint8_t printConditionalsFromAddress(uint32_t address);
// void printAllFlagConditionals(uint8_t flag, bool orMode);
// void printAllFlagConditionalsWithAddress(uint32_t address, uint8_t flag, bool orMode);
#endif
