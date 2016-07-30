// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/Port.h"
#include "../gba/GBA.h"
#include "../gba/elf.h"
#include "exprNode.h"

#ifndef __GNUC__
#define strdup _strdup
#endif

extern char* yytext;

#define debuggerReadMemory(addr) \
    READ32LE((&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

const void* exprNodeCleanUpList[100];
int exprNodeCleanUpCount = 0;
Type exprNodeType = { 0, TYPE_base, "int", DW_ATE_signed, 4, 0, { 0 }, 0 };

void exprNodeClean(const void* m)
{
    exprNodeCleanUpList[exprNodeCleanUpCount++] = m;
}

void exprNodeCleanUp()
{
    for (int i = 0; i < exprNodeCleanUpCount; i++) {
        free((void*)exprNodeCleanUpList[i]);
    }
    exprNodeCleanUpCount = 0;
}

Node* exprNodeIdentifier()
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    n->name = strdup(yytext);

    exprNodeClean(n->name);
    exprNodeClean(n);

    n->print = exprNodeIdentifierPrint;
    n->resolve = exprNodeIdentifierResolve;
    return n;
}

bool exprNodeIdentifierResolve(Node* n, Function* f, CompileUnit* u)
{
    Object* o;
    if (elfGetObject(n->name, f, u, &o)) {
        n->type = o->type;
        n->location = elfDecodeLocation(f, o->location, &n->locType);
        return true;
    } else {
        printf("Object %s not found\n", n->name);
    }
    return false;
}

void exprNodeIdentifierPrint(Node* n)
{
    printf("%s", n->name);
}

Node* exprNodeNumber()
{
    Node* n = (Node*)calloc(1, sizeof(Node));

    exprNodeClean(n);
    n->location = atoi(yytext);
    n->type = &exprNodeType;
    n->locType = LOCATION_value;
    n->print = exprNodeNumberPrint;
    n->resolve = exprNodeNumberResolve;
    return n;
}

bool exprNodeNumberResolve(Node* n, Function* f, CompileUnit* u)
{
    return true;
}

void exprNodeNumberPrint(Node* n)
{
    printf("%d", n->location);
}

Node* exprNodeStar(Node* exp)
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    exprNodeClean(n);

    n->expression = exp;

    n->print = exprNodeStarPrint;
    n->resolve = exprNodeStarResolve;
    return n;
}

bool exprNodeStarResolve(Node* n, Function* f, CompileUnit* u)
{
    if (n->expression->resolve(n->expression, f, u)) {
        if (n->expression->type->type == TYPE_pointer) {
            n->location = n->expression->location;
            if (n->expression->locType == LOCATION_memory) {
                n->location = debuggerReadMemory(n->location);
            } else if (n->expression->locType == LOCATION_register) {
                n->location = reg[n->expression->location].I;
            } else {
                n->location = n->expression->location;
            }
            n->type = n->expression->type->pointer;
            n->locType = LOCATION_memory;
            return true;
        } else {
            printf("Object is not of pointer type\n");
        }
    }
    return false;
}

void exprNodeStarPrint(Node* n)
{
    printf("*");
    n->expression->print(n->expression);
}

Node* exprNodeDot(Node* exp, Node* ident)
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    exprNodeClean(n);

    n->expression = exp;
    n->name = ident->name;

    n->print = exprNodeDotPrint;
    n->resolve = exprNodeDotResolve;
    return n;
}

bool exprNodeDotResolve(Node* n, Function* f, CompileUnit* u)
{
    if (n->expression->resolve(n->expression, f, u)) {
        TypeEnum tt = n->expression->type->type;

        if (tt == TYPE_struct || tt == TYPE_union) {
            uint32_t loc = n->expression->location;
            Type* t = n->expression->type;
            int count = t->structure->memberCount;
            int i = 0;
            while (i < count) {
                Member* m = &t->structure->members[i];
                if (strcmp(m->name, n->name) == 0) {
                    // found member
                    n->type = m->type;
                    if (tt == TYPE_struct) {
                        n->location = elfDecodeLocation(f, m->location, &n->locType,
                            loc);
                        n->objLocation = loc;
                    } else {
                        n->location = loc;
                        n->locType = n->expression->locType;
                        n->objLocation = loc;
                    }
                    n->member = m;
                    return true;
                }
                i++;
            }
            printf("Member %s not found\n", n->name);
        } else {
            printf("Object is not of structure type\n");
        }
    }
    return false;
}

void exprNodeDotPrint(Node* n)
{
    n->expression->print(n->expression);
    printf(".%s", n->name);
}

Node* exprNodeArrow(Node* exp, Node* ident)
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    exprNodeClean(n);

    n->expression = exp;
    n->name = ident->name;

    n->print = exprNodeArrowPrint;
    n->resolve = exprNodeArrowResolve;
    return n;
}

bool exprNodeArrowResolve(Node* n, Function* f, CompileUnit* u)
{
    if (n->expression->resolve(n->expression, f, u)) {
        TypeEnum tt = n->expression->type->type;
        if (tt != TYPE_pointer) {
            printf("Object not of pointer type\n");
            return false;
        }
        tt = n->expression->type->pointer->type;

        if (tt == TYPE_struct || tt == TYPE_union) {
            uint32_t loc = debuggerReadMemory(n->expression->location);
            Type* t = n->expression->type->pointer;
            int count = t->structure->memberCount;
            int i = 0;
            while (i < count) {
                Member* m = &t->structure->members[i];
                if (strcmp(m->name, n->name) == 0) {
                    // found member
                    n->type = m->type;
                    if (tt == TYPE_struct) {
                        n->location = elfDecodeLocation(f, m->location, &n->locType,
                            loc);
                        n->objLocation = loc;
                    } else {
                        n->location = loc;
                        n->objLocation = loc;
                    }
                    n->locType = LOCATION_memory;
                    n->member = m;
                    return true;
                }
                i++;
            }
            printf("Member %s not found\n", n->name);
        } else {
            printf("Object is not of structure type\n");
        }
    }
    return false;
}

void exprNodeArrowPrint(Node* n)
{
    n->expression->print(n->expression);
    printf("->%s", n->name);
}

Node* exprNodeAddr(Node* exp)
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    exprNodeClean(n);

    n->expression = exp;

    n->print = exprNodeAddrPrint;
    n->resolve = exprNodeAddrResolve;
    return n;
}

bool exprNodeAddrResolve(Node* n, Function* f, CompileUnit* u)
{
    if (n->expression->resolve(n->expression, f, u)) {
        if (n->expression->locType == LOCATION_memory) {
            n->location = n->expression->location;
            n->locType = LOCATION_value;
            n->type = &exprNodeType;
        } else if (n->expression->locType == LOCATION_register) {
            printf("Value is in register %d\n", n->expression->location);
        } else {
            printf("Direct value is %d\n", n->location);
        }
        return true;
    }
    return false;
}

void exprNodeAddrPrint(Node* n)
{
    printf("*");
    n->expression->print(n->expression);
}

Node* exprNodeSizeof(Node* exp)
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    exprNodeClean(n);

    n->expression = exp;

    n->print = exprNodeSizeofPrint;
    n->resolve = exprNodeSizeofResolve;
    return n;
}

bool exprNodeSizeofResolve(Node* n, Function* f, CompileUnit* u)
{
    if (n->expression->resolve(n->expression, f, u)) {
        n->location = n->expression->type->size;
        n->locType = LOCATION_value;
        n->type = &exprNodeType;
        return true;
    }
    return false;
}

void exprNodeSizeofPrint(Node* n)
{
    printf("sizeof(");
    n->expression->print(n->expression);
    printf(")");
}

Node* exprNodeArray(Node* exp, Node* number)
{
    Node* n = (Node*)calloc(1, sizeof(Node));
    exprNodeClean(n);

    n->expression = exp;
    n->value = number->location;

    n->print = exprNodeArrayPrint;
    n->resolve = exprNodeArrayResolve;
    return n;
}

int exprNodeGetSize(Array* a, int index)
{
    index++;
    if (index == a->maxBounds) {
        return a->type->size;
    } else {
        int size = a->bounds[a->maxBounds - 1] * a->type->size;

        for (int i = index; i < a->maxBounds - 1; i++) {
            size *= a->bounds[i];
        }
        return size;
    }
}

bool exprNodeArrayResolve(Node* n, Function* f, CompileUnit* u)
{
    if (n->expression->resolve(n->expression, f, u)) {
        TypeEnum tt = n->expression->type->type;
        if (tt != TYPE_array && tt != TYPE_pointer) {
            printf("Object not of array or pointer type\n");
            return false;
        }

        if (tt == TYPE_array) {
            Array* a = n->expression->type->array;

            uint32_t loc = n->expression->location;
            Type* t = a->type;
            if (a->maxBounds > 1) {
                int index = n->expression->index;

                if (index == a->maxBounds) {
                    printf("Too many indices for array\n");
                    return false;
                }

                if ((index + 1) < a->maxBounds) {
                    n->type = n->expression->type;
                    n->index = index + 1;
                    n->locType = LOCATION_memory;
                    n->location = n->expression->location + n->value * exprNodeGetSize(a, index);
                    return true;
                }
            }
            n->type = t;
            n->location = loc + n->value * t->size;
            n->locType = LOCATION_memory;
        } else {
            Type* t = n->expression->type->pointer;
            uint32_t loc = n->expression->location;
            if (n->expression->locType == LOCATION_register)
                loc = reg[loc].I;
            else
                loc = debuggerReadMemory(loc);
            n->type = t;
            n->location = loc + n->value * t->size;
            n->locType = LOCATION_memory;
        }
        return true;
    }
    return false;
}

void exprNodeArrayPrint(Node* n)
{
    n->expression->print(n->expression);
    printf("[%d]", n->value);
}
