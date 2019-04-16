/*New code made for the new breakpoint system.

Each breakpoint is now marked by a halfword in an array on map,
breakpoint array. Each bit means one different type of breakpoint.
Bit		Value
0 -		write
1 -		read
2 -		ARM
3 -		THUMB
4 -		Always on Write		
5 -		Always on Read
6 -		Always on ARM
7 -		Always on THUMB

Each flag is independent, and can be used as such, as well as together.
You can define a break on access (0xf), break on IO (0x3), break on 
execution(0xc) or any other combination you need. 0xf0 will always break on
any passage through the address

This structure is accompanied by two other structures for the accesses.
One of them is the Conditional structure. It's available for all types.

The breaking address header is placed at ConditionalBreak. The type_flags
indicate the type of break, in the format indicated above.
Then, it links to the several conditions needed for the break to take place.

Those conditions are then sequentially executed, and if all are true, the
break takes place.
They are composed of an address to test, a value to test against, the value's
type, the flag conditons to test and a link to the next condition to test if 
that was true. Address and value are stored as Evaluatable expressions, 
according to the already defined eval from expr.c
However, be reminded that sometimes registers and memory positions may contain
values that are not addresses at some times. If such is the case, the program 
will show garbage results or crash.

The flags are bit-based
Bit		Value
0 -		equal
1 -		greater
2 -		lesser
3 -		force Signed comparison

Bit		Value
0,1 -	addr is: 00 - byte, 01 - halfword, 10 - word, 11 - undefined, assume word
2 -		addr_is_signed
3 -		addr_is_array of values (not implemented yet)
4,5 -	00 is byte, 01 is halfword, 10 is word, 11 undefined
6 -		val_is_signed
7 -		val_is_array		(also not implemented)
Usage:
Instructions that accept this kind of implementation will follow this pattern
if <EXP_addr> <addr_type><op><val_type> <EXP_val> [<;>OR<||>OR<&&> [repeat]
if indicates that the following there is a condition. no If counts as always break
<EXP_addr> is the first value expression, that unlike what the name implies, may be
anything.
<op> is one of the comparation Operands. Full list bellow
<EXP_val> is the value to compare to.
Following can be a ||(or), that adds a second, independent break condition, or
a &&(and), that specifies that the next condition is a conjoined requisite to the break.
EX: [0x03005000] == 0x77777777 && 0x50 == [0x03005000]
	Would be impossible, but tested anyway, and make it never break due to this condition.
	[0x03005000] == 0x77777777 || 0x50 == [0x03005000]
	Would make it break when the contents of 0x03005000 were either 0x77777777 or 0x50

Ex: to implement stop only when r2 is 0xf, use
	if r2 == 0xf
	to implement stop when PC is above 0x08005000 and 0x03005000 is signed halfword 0x2500
	if pc > 0x08005000 && 0x03005000 == +0x2500
*/

/*
Full Operands lists
Op1, op2, ...,opN                       --> Meaning

== , =, eq                              --> equal

<, lt                                   --> lesser than

<=, le                                  --> less or equal to

> gt                                    --> greater than

>=, ge                                  --> greater or equal to

!=, <>, ne                              --> not equal to


valid content types
b, byte, uint8_t                        --> byte
sb, sbyte, int8_t                       --> signed byte
h,hw, halfword, uint16_t, ushort        --> halfword
sh, shw, shalfword, int16_t short       --> signed halfword
w, word, uint2_t                        --> word
sw, sword, int32_t, int                 --> signed word
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BreakpointStructures.h"
#include "remote.h"

#if (defined __WIN32__ || defined _WIN32)
#define strdup _strdup
#endif

extern bool dexp_eval(char*, uint32_t*);

//struct intToString{
//	int value;
//	char mapping[20];
//};

struct ConditionalBreak* conditionals[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

//struct intToString breakFlagMapping[] = {
//	{0x80,"Thumb"},
//	{0x40,"ARM"},
//	{0x20,"Read"},
//	{0x10,"Write"},
//	{0x8,"Thumb"},
//	{0x4,"ARM"},
//	{0x2,"Read"},
//	{0x1,"Write"},
//	{0x0,"None"}
//};
/*
struct intToString compareFlagMapping[] = {
	{0x7,"Always"},
	{0x6,"!="},
	{0x5,"<="},
	{0x4,"<"},
	{0x3,">="},
	{0x2,">"},
	{0x1,"=="},
	{0x0,"Never"}
};*/

//char* typeMapping[] = {"'uint8_t","'uint16_t","'uint32_t","'uint32_t","'int8_t","'int16_t","'int32_t","'int32_t"};
//
//char* compareFlagMapping[] = {"Never","==",">",">=","<","<=","!=","<=>"};

//Constructors
//case '*':	flag = 0xf;	break;
//case 't':	flag = 0x8;	break; // thumb
//case 'a':	flag = 0x4;	break; // arm
//case 'x':	flag = 0xC;	break;
//case 'r':	flag = 0x2;	break; // mem read
//case 'w':	flag = 0x1;	break; // mem write
//case 'i':	flag = 0x3;	break;
struct ConditionalBreak* addConditionalBreak(uint32_t address, uint8_t flag)
{
    uint8_t condIndex = address >> 24;
    struct ConditionalBreak* cond = NULL;
    BreakSet((&map[condIndex])->breakPoints, address & (&map[condIndex])->mask, ((flag & 0xf) | (flag >> 4)));
    if (flag & 0xf0) {
        struct ConditionalBreak* base = conditionals[condIndex];
        struct ConditionalBreak* prev = conditionals[condIndex];
        if (conditionals[condIndex]) {
            while ((base) && (address >= base->break_address)) {
                if (base->break_address == address) {
                    if (base->type_flags & 0xf0) {
                        base->type_flags |= (flag & 0xf0);
                        flag &= 0xf;
                        cond = base;
                        goto addCB_nextHandle;
                    } else {
                        goto condCreateForFlagAlways;
                    }
                } else {
                    prev = base;
                    base = base->next;
                }
            }
        }
    condCreateForFlagAlways:
        cond = (struct ConditionalBreak*)malloc(sizeof(struct ConditionalBreak));
        cond->break_address = address;
        cond->type_flags = flag & 0xf0;
        cond->next = base;
        cond->firstCond = NULL;
        if (prev == conditionals[condIndex])
            conditionals[condIndex] = cond;
        else
            prev->next = cond;
    }
    flag &= 0xf;

addCB_nextHandle:
    if (flag == 0)
        return cond;

    cond = (struct ConditionalBreak*)malloc(sizeof(struct ConditionalBreak));
    cond->break_address = address;
    cond->type_flags = flag;
    cond->next = NULL;
    cond->firstCond = NULL;
    if (conditionals[condIndex] == NULL) {
        conditionals[condIndex] = cond;
        return cond;
    } else {
        struct ConditionalBreak *curr, *prev;
        curr = conditionals[condIndex];
        prev = conditionals[condIndex];
        while (curr) {
            if (curr->break_address > address) {
                if (prev == curr) {
                    conditionals[condIndex] = cond;
                } else {
                    prev->next = cond;
                }
                cond->next = curr;
                return cond;
            }
            prev = curr;
            curr = prev->next;
        }
        prev->next = cond;
        return cond;
    }
}

void addCondition(struct ConditionalBreak* base, struct ConditionalBreakNode* toAdd)
{
    if (base->firstCond) {
        struct ConditionalBreakNode *curr, *prev;
        curr = base->firstCond;
        prev = base->firstCond;
        while (curr) {
            prev = curr;
            curr = curr->next;
        }
        prev->next = toAdd;
    } else {
        base->firstCond = toAdd;
    }
}

//destructors
void freeConditionalBreak(struct ConditionalBreak* toFree)
{
    struct ConditionalBreakNode* freeMe;
    while (toFree->firstCond) {
        freeMe = toFree->firstCond;
        toFree->firstCond = toFree->firstCond->next;
        free(freeMe);
    }
    free(toFree);
}

void freeConditionalNode(struct ConditionalBreakNode* toDel)
{
    if (toDel->next)
        freeConditionalNode(toDel->next);
    if (toDel->address)
        free(toDel->address);
    if (toDel->value)
        free(toDel->value);
    free(toDel);
}

void freeAllConditionals()
{
    for (int i = 0; i < 16; i++) {
        while (conditionals[i]) {
            struct ConditionalBreak* tmp = conditionals[i];
            conditionals[i] = conditionals[i]->next;
            freeConditionalBreak(tmp);
        }
    }
}

int removeConditionalBreak(struct ConditionalBreak* toDelete)
{
    if (toDelete) {
        uint8_t condIndex = toDelete->break_address >> 24;
        struct ConditionalBreak* base = conditionals[condIndex];
        struct ConditionalBreak* prev = conditionals[condIndex];
        while (base) {
            if (base == toDelete) {
                if (base == prev) {
                    conditionals[condIndex] = base->next;
                } else {
                    prev->next = base->next;
                }
                freeConditionalBreak(toDelete);
                return 0;
            }
            prev = base;
            base = base->next;
        }
        return -1; //failed to remove
    }
    return -2; //delete failed because container was not there
}

/*
int removeConditionalBreak(uint32_t address, uint8_t num, uint8_t flag){
	uint8_t condIndex = address>>24;
	struct ConditionalBreak* base = conditionals[condIndex];
	struct ConditionalBreak* prev = conditionals[condIndex];
	uint8_t counter = 0;
	while(base){
		if(base->break_address > address)
			return -2;	//failed to remove
		if(base->break_address == address){
			counter++;
		}
		if(counter == num){
			base->type_flags &= ~flag;
			if(base->type_flags == 0){
				if(prev == base){
					conditionals[condIndex] = base->next;
				}else{
					prev->next = base->next;
				}
				freeConditionalBreak(base);
				return 0;
			}
			return 1;
		}
		prev = base;
		base = base->next;
	}
	return -2;
}

bool removeCondition(struct ConditionalBreak* base, struct ConditionalBreakNode* toDel){
	if(base->firstCond){
		if(toDel == base->firstCond){
			base->firstCond = toDel->next;
			freeConditionalNode(toDel);
			return true;
		}
		struct ConditionalBreakNode* curr, *prev;
		curr = base->firstCond;
		prev = base->firstCond;
		while(curr){
			if(curr == toDel){
				prev->next = curr->next;
				freeConditionalNode(toDel);
				return true;
			}
			prev = curr;
			curr = curr->next;
		}
	}
	freeConditionalNode(toDel);
	return false;
}

bool removeCondition(uint32_t address, uint8_t flags, uint8_t num){
	struct ConditionalBreak* base  = conditionals[address>>24];
	while(base && base->break_address < address){
		base = base->next;
	}
	if(base && base->break_address == address){
		struct ConditionalBreakNode* curr = base->firstCond; 
		for(int i = 0; i < num; i++){
			if(!curr && ((!base) || (!base->next))){
				return false;
			}if(!curr){
				while(!curr){
				base = base->next;
				if(base->break_address > address)
					return false;
				
				curr = base->firstCond;
			}
		}
	}
	}
}*/

/*
int removeAllConditions(uint32_t address, uint8_t flags){
	struct ConditionalBreak* base  = conditionals[address>>24];
	while(base && base->break_address < address){
		base = base->next;
	}
	struct ConditionalBreak* curr = base;
	while(base && base->break_address == address){
		base->type_flags &= ~flags;
		if(!base->type_flags){
			curr = base;
			base = base->next;
			removeCondition(base);
		}
	}
	return true;
}*/

void recountFlagsForAddress(uint32_t address)
{
    struct ConditionalBreak* base = conditionals[address >> 24];
    uint8_t flags = 0;
    while (base) {
        if (base->break_address < address) {
            base = base->next;
        } else {
            if (base->break_address == address) {
                flags |= base->type_flags;
            } else {
                BreakClear((&map[address >> 24])->breakPoints, address & (&map[address >> 24])->mask, 0xff);
                BreakSet((&map[address >> 24])->breakPoints, address & (&map[address >> 24])->mask, ((flags >> 4) | (flags & 0x8)));
                return;
            }
            base = base->next;
        }
    }
    BreakClear((&map[address >> 24])->breakPoints, address & (&map[address >> 24])->mask, 0xff);
    BreakSet((&map[address >> 24])->breakPoints, address & (&map[address >> 24])->mask, ((flags >> 4) | (flags & 0x8)));
}

//Removers
int removeConditionalBreakNo(uint32_t addrNo, uint8_t number)
{
    if (conditionals[addrNo >> 24]) {
        struct ConditionalBreak* base = conditionals[addrNo >> 24];
        struct ConditionalBreak* curr = conditionals[addrNo >> 24];
        while (curr->break_address < addrNo) {
            base = curr;
            curr = curr->next;
        }
        if (curr->break_address == addrNo) {
            if (number == 1) {
                if (base == curr) {
                    conditionals[addrNo >> 24] = curr->next;
                    freeConditionalBreak(curr);
                } else {
                    base->next = curr->next;
                    freeConditionalBreak(curr);
                }
                recountFlagsForAddress(addrNo);
                return 0;
            } else {
                int count = 1;
                while (curr && (curr->break_address == addrNo)) {
                    if (count == number) {
                        base->next = curr->next;
                        freeConditionalBreak(curr);
                        recountFlagsForAddress(addrNo);
                        return 1;
                    }
                    count++;
                    base = curr;
                    curr = curr->next;
                }
                return -1;
            }
        }
    }
    return -2;
}

int removeFlagFromConditionalBreakNo(uint32_t addrNo, uint8_t number, uint8_t flag)
{
    if (conditionals[addrNo >> 24]) {
        struct ConditionalBreak* base = conditionals[addrNo >> 24];
        struct ConditionalBreak* curr = conditionals[addrNo >> 24];
        while (curr->break_address < addrNo) {
            base = curr;
            curr = curr->next;
        }
        if (curr->break_address == addrNo) {
            if (number == 1) {
                curr->type_flags &= ~flag;
                if (curr->type_flags == 0) {
                    if (base == curr) {
                        conditionals[addrNo >> 24] = curr->next;
                        freeConditionalBreak(curr);
                    } else {
                        base->next = curr->next;
                        freeConditionalBreak(curr);
                    }
                }
                recountFlagsForAddress(addrNo);
                return 0;
            } else {
                int count = 1;
                while (curr && (curr->break_address == addrNo)) {
                    if (count == number) {
                        curr->type_flags &= ~flag;
                        if (!curr->type_flags) {
                            base->next = curr->next;
                            freeConditionalBreak(curr);
                            recountFlagsForAddress(addrNo);
                            return 1;
                        }
                        recountFlagsForAddress(addrNo);
                        return 0;
                    }
                    count++;
                    base = curr;
                    curr = curr->next;
                }
                return -1;
            }
        }
    }
    return -2;
}

int removeConditionalWithAddress(uint32_t address)
{
    uint8_t addrNo = address >> 24;
    if (conditionals[addrNo] != NULL) {
        struct ConditionalBreak* base = conditionals[addrNo];
        struct ConditionalBreak* curr = conditionals[addrNo];
        uint8_t count = 0;
        uint8_t flags = 0;
        while (curr && address >= curr->break_address) {
            if (curr->break_address == address) {
                base->next = curr->next;
                flags |= curr->type_flags;
                freeConditionalBreak(curr);
                curr = base->next;
                count++;
            } else {
                base = curr;
                curr = curr->next;
            }
        }
        BreakClear((&map[address >> 24])->breakPoints, address & (&map[address >> 24])->mask, ((flags & 0xf) | (flags >> 4)));
        return count;
    }
    return -2;
}

int removeConditionalWithFlag(uint8_t flag, bool orMode)
{
    for (uint8_t addrNo = 0; addrNo < 16; addrNo++) {
        if (conditionals[addrNo] != NULL) {
            struct ConditionalBreak* base = conditionals[addrNo];
            struct ConditionalBreak* curr = conditionals[addrNo];
            uint8_t count = 0;
            while (curr) {
                if (((curr->type_flags & flag) == curr->type_flags) || (orMode && (curr->type_flags & flag))) {
                    curr->type_flags &= ~flag;
                    BreakClear((&map[addrNo])->breakPoints, curr->break_address & (&map[addrNo])->mask, ((flag & 0xf) | (flag >> 4)));
                    if (curr->type_flags == 0) {
                        if (base == conditionals[addrNo]) {
                            conditionals[addrNo] = curr->next;
                            base = curr->next;
                            curr = base;
                        } else {
                            base->next = curr->next;
                            freeConditionalBreak(curr);
                            curr = base->next;
                        }
                        count++;
                    } else {
                        base = curr;
                        curr = curr->next;
                    }
                } else {
                    base = curr;
                    curr = curr->next;
                }
            }
            return count;
        }
    }
    return -2;
}

int removeConditionalWithAddressAndFlag(uint32_t address, uint8_t flag, bool orMode)
{
    uint8_t addrNo = address >> 24;
    if (conditionals[addrNo] != NULL) {
        struct ConditionalBreak* base = conditionals[addrNo];
        struct ConditionalBreak* curr = conditionals[addrNo];
        uint8_t count = 0;
        while (curr && address >= curr->break_address) {
            if ((curr->break_address == address) && (((curr->type_flags & flag) == curr->type_flags) || (orMode && (curr->type_flags & flag)))) {
                curr->type_flags &= ~flag;
                BreakClear((&map[address >> 24])->breakPoints, curr->break_address & (&map[address >> 24])->mask, ((flag & 0xf) | (flag >> 4)));
                if (curr->type_flags == 0) {
                    if (curr == conditionals[addrNo]) {
                        conditionals[addrNo] = curr->next;
                        freeConditionalBreak(curr);
                        curr = conditionals[addrNo];
                    } else {
                        base->next = curr->next;
                        freeConditionalBreak(curr);
                        curr = base->next;
                    }
                    count++;
                } else {
                    base = curr;
                    curr = curr->next;
                }
            } else {
                base = curr;
                curr = curr->next;
            }
        }
        return count;
    }
    return -2;
}

//true creating code for a given expression.
//It assumes an if was found, and that all up to the if was removed.
//receives an array of chars following the pattern:
//{'<expType>,<exp>,<op>,'<expType>,<exp>,<And_symbol|or_symbol>, (repeats)
//<expType is optional, and always assumes int-compare as default.
uint8_t parseExpressionType(char* type);
uint8_t parseConditionOperand(char* type);
void parseAndCreateConditionalBreaks(uint32_t address, uint8_t flags, char** exp, int n)
{
    struct ConditionalBreak* workBreak = addConditionalBreak(address, flags);
    flags &= 0xf;
    if (!flags)
        return;
    struct ConditionalBreakNode* now = (struct ConditionalBreakNode*)malloc(sizeof(struct ConditionalBreakNode));
    struct ConditionalBreakNode* toAdd = now;
    for (int i = 0; i < n; i++) {
        now->next = 0;
        now->exp_type_flags = 0;
        if (exp[i][0] == '\'') {
            now->exp_type_flags |= parseExpressionType(&exp[i][1]);
            i++;
            if (i >= n)
                goto fail;
        } else {
            now->exp_type_flags |= 6; //assume signed word
        }
        now->address = strdup(exp[i]);
        i++;
        if (i >= n)
            goto fail;
        char* operandName = exp[i];
        now->cond_flags = parseConditionOperand(exp[i]);
        i++;
        if (i >= n)
            goto fail;

        if (exp[i][0] == '\'') {
            now->exp_type_flags |= (parseExpressionType(&exp[i][1]) << 4);
            i++;
            if (i >= n)
                goto fail;
        } else {
            now->exp_type_flags |= 0x60; //assume signed word
        }
        now->value = strdup(exp[i]);
        i++;
        uint32_t val;
        if (!dexp_eval(now->value, &val) || !dexp_eval(now->address, &val)) {
            printf("Invalid expression.\n");
            if (workBreak)
                removeConditionalBreak(workBreak);
            return;
        }
        if (i < n) {
            if (strcmp(exp[i], "&&") == 0) {
                now = (struct ConditionalBreakNode*)malloc(sizeof(struct ConditionalBreakNode));
                toAdd->next = now;
            } else if (strcmp(exp[i], "||") == 0) {
                addCondition(workBreak, toAdd);
                printf("Added break on address %08x, with condition:\n%s %s %s\n", address, now->address, operandName, now->value);
                workBreak = addConditionalBreak(address, flags);
                now = (struct ConditionalBreakNode*)malloc(sizeof(struct ConditionalBreakNode));
                toAdd = now;
            }
        } else {
            addCondition(workBreak, toAdd);
            printf("Added break on address %08x, ending with condition: \n%s %s %s\n", address, now->address, operandName, now->value);
            return;
        }
    }
    return;

fail:
    printf("Not enough material (expressions) to work with.");
    removeConditionalBreak(workBreak);
    return;
}

//aux
uint8_t parseExpressionType(char* given_type)
{
    uint8_t flags = 0;
    //for such a small string, pays off to convert first
    char* type = strdup(given_type);
    for (int i = 0; type[i] != '\0'; i++) {
        type[i] = toupper(type[i]);
    }
    if ((type[0] == 'S') || type[0] == 'U') {
        flags |= (4 - ((type[0] - 'S') << 1));
        type++;
        if (type[0] == 'H') {
            type[0] = '1';
            type[1] = '6';
            type[2] = '\0';
        } else if (type[0] == 'W') {
            type[0] = '3';
            type[1] = '2';
            type[2] = '\0';
        } else if (type[0] == 'B') {
            type[0] = '8';
            type[1] = '\0';
        }
        int size;
        sscanf(type, "%d", &size);
        size = (size >> 3) - 1;
        flags |= (size >= 2 ? 2 : ((uint8_t)size));
        free(type);
        return flags;
    }
    if (strcmp(type, "INT") == 0) {
        flags = 6;
    }
    if (type[0] == 'H') {
        flags = 1;
    } else if (type[0] == 'W') {
        flags = 2;
    } else if (type[0] == 'B') {
        flags = 0;
    } else {
        flags = 6;
    }
    free(type);
    return flags;
}

uint8_t parseConditionOperand(char* type)
{
    uint8_t flag = 0;
    if (toupper(type[0]) == 'S') {
        flag = 8;
        type++;
    }
    switch (type[0]) {
    case '!':
        if (type[1] == '=' || type[1] == '\0')
            return flag | 0x6;
        break;
    case '=':
        if (type[1] == '=' || type[1] == '\0')
            return flag | 0x1;
        break;
    case '<':
        if (type[1] == '>')
            return flag | 0x6;
        if (type[1] == '=')
            return flag | 0x5;
        if (type[1] == '\0')
            return flag | 0x4;
        break;
    case '>':
        if (type[1] == '<')
            return flag | 0x6;
        if (type[1] == '=')
            return flag | 0x3;
        if (type[1] == '\0')
            return flag | 0x2;
        break;
    default:;
    }
    switch (tolower(type[0])) {
    case 'l':
        if (tolower(type[1]) == 'e')
            return flag | 0x5;
        if (tolower(type[1]) == 't')
            return flag | 0x4;
        break;
    case 'g':
        if (tolower(type[1]) == 'e')
            return flag | 0x3;
        if (tolower(type[1]) == 't')
            return flag | 0x2;
	break;

    case 'e':
        if (tolower(type[1]) == 'q')
            return flag | 0x1;
        if (type[1] == '\0')
            return flag | 0x1;
	break;
    case 'n':
        if (tolower(type[1]) == 'e')
            return flag | 0x6;
    default:
	break;
    }
    return flag;
}

uint32_t calculateFinalValue(char* expToEval, uint8_t type_of_flags)
{
    uint32_t val;
    if (!dexp_eval(expToEval, &val)) {
        printf("Invalid expression.\n");
        return 0;
    }
    if (type_of_flags & 0x4) {
        switch (type_of_flags & 0x3) {
        case 0:
            return (int8_t)(val & 0xff);
        case 1:
            return (int16_t)(val & 0xffff);
        default:
            return (int)val;
        }
    } else {
        switch (type_of_flags & 0x3) {
        case 0:
            return (uint8_t)(val & 0xff);
        case 1:
            return (uint16_t)(val & 0xffff);
        default:
            return val;
        }
    }
}

//check for execution
bool isCorrectBreak(struct ConditionalBreak* toTest, uint8_t accessType)
{

    return (toTest->type_flags & accessType);
}

bool doBreak(struct ConditionalBreak* toTest)
{
    struct ConditionalBreakNode* toExamine = toTest->firstCond;

    bool globalVeredict = true;
    bool veredict = false;
    while (toExamine && globalVeredict) {
        uint32_t address = calculateFinalValue(toExamine->address, toExamine->exp_type_flags & 0xf);
        uint32_t value = calculateFinalValue(toExamine->value, toExamine->exp_type_flags >> 4);
        if ((toExamine->cond_flags & 0x7) != 0) {
            veredict = veredict || ((toExamine->cond_flags & 1) ? (address == value) : false);
            veredict = veredict || ((toExamine->cond_flags & 4) ? ((toExamine->cond_flags & 8) ? ((int)address < (int)value) : (address < value)) : false);
            veredict = veredict || ((toExamine->cond_flags & 2) ? ((toExamine->cond_flags & 8) ? ((int)address > (int)value) : (address > value)) : false);
        }
        toExamine = toExamine->next;
        globalVeredict = veredict && globalVeredict;
    }
    return globalVeredict;
}

bool doesBreak(uint32_t address, uint8_t allowedFlags)
{
    uint8_t addrNo = address >> 24;
    if (conditionals[addrNo]) {
        struct ConditionalBreak* base = conditionals[addrNo];
        while (base && base->break_address < address) {
            base = base->next;
        }
        while (base && base->break_address == address) {
            if (base->type_flags & allowedFlags & 0xf0) {
                return true;
            }
            if (isCorrectBreak(base, allowedFlags) && doBreak(base)) {
                return true;
            }
            base = base->next;
        }
    }
    return false;
}

/*
//Test main
int main (int argc, char** args){
	char* argv[] = {"r7","s==","0", "||","'byte","0x25","ge","'hword","0x0001"};
	parseAndCreateConditionalBreaks(0x0203fd00, 0x1f, argv, 9);
	parseAndCreateConditionalBreaks(0x0203fd00, 0x10, argv, 9);
	parseAndCreateConditionalBreaks(0x0203fd00, 0x40, argv, 9);
	parseAndCreateConditionalBreaks(0x0203fd04, 0x04, argv, 9);
	printAllFlagConditionals(0xff, true);
	removeFlagFromConditionalBreakNo(0x0203fd00, 0x3,0xff);
	printf("after\n");
	printAllFlagConditionals(0xff, true);
	
	getchar();
}
*/
