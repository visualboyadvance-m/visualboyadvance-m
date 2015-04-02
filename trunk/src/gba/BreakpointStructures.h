#ifndef VBA_BKS_H
#define VBA_BKS_H

#include "../common/Types.h"

#define readWord(addr) \
  ((map[(addr)>>24].address[(addr) & map[(addr)>>24].mask])+\
  ((map[(addr+1)>>24].address[(addr+1) & map[(addr+1)>>24].mask])<<8)+\
  ((map[(addr+2)>>24].address[(addr+2) & map[(addr+2)>>24].mask])<<16)+\
  ((map[(addr+3)>>24].address[(addr+3) & map[(addr+3)>>24].mask])<<24))

#define readHalfWord(addr) \
 ((&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask])+\
 ((&map[(addr+1)>>24].address[(addr+1) & map[(addr+1)>>24].mask])<<8))

#define readByte(addr) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]


struct ConditionalBreakNode{
	char* address;
	char* value;
	u8 cond_flags;
	u8 exp_type_flags;
	struct ConditionalBreakNode* next;
};

struct ConditionalBreak{
	u32 break_address;
	u8 type_flags;
	struct ConditionalBreakNode* firstCond;
	struct ConditionalBreak* next;
};

extern struct ConditionalBreak* conditionals[16]; 

//conditional break manipulators
//case '*':	flag = 0xf;	break;
//case 't':	flag = 0x8;	break; // thumb
//case 'a':	flag = 0x4;	break; // arm
//case 'x':	flag = 0xC;	break;
//case 'r':	flag = 0x2;	break; // mem read
//case 'w':	flag = 0x1;	break; // mem write
//case 'i':	flag = 0x3;	break;
struct ConditionalBreak* addConditionalBreak(u32 address, u8 flag);

int removeConditionalBreakNo(u32 address, u8 number);
int removeFlagFromConditionalBreakNo(u32 address, u8 number, u8 flag);
int removeConditionalWithAddress(u32 address);
int removeConditionalWithFlag(u8 flag, bool orMode);
int removeConditionalWithAddressAndFlag(u32 address, u8 flag, bool orMode);
//void freeConditionalBreak(struct ConditionalBreak* toFree);

void addCondition(struct ConditionalBreak* base, struct ConditionalBreakNode* toAdd);
//bool removeCondition(struct ConditionalBreak* base, struct ConditionalBreakNode* toDel);
//bool removeCondition(u32 address, u8 flags, u8 num);

void freeConditionalNode(struct ConditionalBreakNode* toDel);


void parseAndCreateConditionalBreaks(u32 address, u8 flags, char** exp, int n);

bool isCorrectBreak(struct ConditionalBreak* toTest, u8 accessType);
bool doesBreak(u32 address, u8 allowedFlags);
bool doBreak(struct ConditionalBreak* toTest);

//printing the structure(AKA list Breaks)
//void printConditionalBreak(struct ConditionalBreak* toPrint, bool printAddress);
//void printAllConditionals();
//u8 printConditionalsFromAddress(u32 address);
//void printAllFlagConditionals(u8 flag, bool orMode);
//void printAllFlagConditionalsWithAddress(u32 address, u8 flag, bool orMode);
#endif