#include "core/gba/gbaElf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>
#include <string>
#include <vector>

#include <elfio/elfio.hpp>

#include "core/base/message.h"
#include "core/base/port.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"

#define elfReadMemory(addr) \
    READ32LE((&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

#define DW_TAG_array_type 0x01
#define DW_TAG_enumeration_type 0x04
#define DW_TAG_formal_parameter 0x05
#define DW_TAG_label 0x0a
#define DW_TAG_lexical_block 0x0b
#define DW_TAG_member 0x0d
#define DW_TAG_pointer_type 0x0f
#define DW_TAG_reference_type 0x10
#define DW_TAG_compile_unit 0x11
#define DW_TAG_structure_type 0x13
#define DW_TAG_subroutine_type 0x15
#define DW_TAG_typedef 0x16
#define DW_TAG_union_type 0x17
#define DW_TAG_unspecified_parameters 0x18
#define DW_TAG_inheritance 0x1c
#define DW_TAG_inlined_subroutine 0x1d
#define DW_TAG_subrange_type 0x21
#define DW_TAG_base_type 0x24
#define DW_TAG_const_type 0x26
#define DW_TAG_enumerator 0x28
#define DW_TAG_subprogram 0x2e
#define DW_TAG_variable 0x34
#define DW_TAG_volatile_type 0x35

#define DW_AT_sibling 0x01
#define DW_AT_location 0x02
#define DW_AT_name 0x03
#define DW_AT_byte_size 0x0b
#define DW_AT_bit_offset 0x0c
#define DW_AT_bit_size 0x0d
#define DW_AT_stmt_list 0x10
#define DW_AT_low_pc 0x11
#define DW_AT_high_pc 0x12
#define DW_AT_language 0x13
#define DW_AT_compdir 0x1b
#define DW_AT_const_value 0x1c
#define DW_AT_containing_type 0x1d
#define DW_AT_inline 0x20
#define DW_AT_producer 0x25
#define DW_AT_prototyped 0x27
#define DW_AT_upper_bound 0x2f
#define DW_AT_abstract_origin 0x31
#define DW_AT_accessibility 0x32
#define DW_AT_artificial 0x34
#define DW_AT_data_member_location 0x38
#define DW_AT_decl_file 0x3a
#define DW_AT_decl_line 0x3b
#define DW_AT_declaration 0x3c
#define DW_AT_encoding 0x3e
#define DW_AT_external 0x3f
#define DW_AT_frame_base 0x40
#define DW_AT_macro_info 0x43
#define DW_AT_specification 0x47
#define DW_AT_type 0x49
#define DW_AT_virtuality 0x4c
#define DW_AT_vtable_elem_location 0x4d
// DWARF 2.1/3.0 extensions
#define DW_AT_entry_pc 0x52
#define DW_AT_ranges 0x55
// ARM Compiler extensions
#define DW_AT_proc_body 0x2000
#define DW_AT_save_offset 0x2001
#define DW_AT_user_2002 0x2002
// MIPS extensions
#define DW_AT_MIPS_linkage_name 0x2007

#define DW_FORM_addr 0x01
#define DW_FORM_data2 0x05
#define DW_FORM_data4 0x06
#define DW_FORM_string 0x08
#define DW_FORM_block 0x09
#define DW_FORM_block1 0x0a
#define DW_FORM_data1 0x0b
#define DW_FORM_flag 0x0c
#define DW_FORM_sdata 0x0d
#define DW_FORM_strp 0x0e
#define DW_FORM_udata 0x0f
#define DW_FORM_ref_addr 0x10
#define DW_FORM_ref4 0x13
#define DW_FORM_ref_udata 0x15
#define DW_FORM_indirect 0x16

#define DW_OP_addr 0x03
#define DW_OP_plus_uconst 0x23
#define DW_OP_reg0 0x50
#define DW_OP_reg1 0x51
#define DW_OP_reg2 0x52
#define DW_OP_reg3 0x53
#define DW_OP_reg4 0x54
#define DW_OP_reg5 0x55
#define DW_OP_reg6 0x56
#define DW_OP_reg7 0x57
#define DW_OP_reg8 0x58
#define DW_OP_reg9 0x59
#define DW_OP_reg10 0x5a
#define DW_OP_reg11 0x5b
#define DW_OP_reg12 0x5c
#define DW_OP_reg13 0x5d
#define DW_OP_reg14 0x5e
#define DW_OP_reg15 0x5f
#define DW_OP_fbreg 0x91

#define DW_LNS_extended_op 0x00
#define DW_LNS_copy 0x01
#define DW_LNS_advance_pc 0x02
#define DW_LNS_advance_line 0x03
#define DW_LNS_set_file 0x04
#define DW_LNS_set_column 0x05
#define DW_LNS_negate_stmt 0x06
#define DW_LNS_set_basic_block 0x07
#define DW_LNS_const_add_pc 0x08
#define DW_LNS_fixed_advance_pc 0x09

#define DW_LNE_end_sequence 0x01
#define DW_LNE_set_address 0x02
#define DW_LNE_define_file 0x03

#define DW_CFA_advance_loc 0x01
#define DW_CFA_offset 0x02
#define DW_CFA_restore 0x03
#define DW_CFA_set_loc 0x01
#define DW_CFA_advance_loc1 0x02
#define DW_CFA_advance_loc2 0x03
#define DW_CFA_advance_loc4 0x04
#define DW_CFA_offset_extended 0x05
#define DW_CFA_restore_extended 0x06
#define DW_CFA_undefined 0x07
#define DW_CFA_same_value 0x08
#define DW_CFA_register 0x09
#define DW_CFA_remember_state 0x0a
#define DW_CFA_restore_state 0x0b
#define DW_CFA_def_cfa 0x0c
#define DW_CFA_def_cfa_register 0x0d
#define DW_CFA_def_cfa_offset 0x0e
#define DW_CFA_nop 0x00

#define CASE_TYPE_TAG             \
    case DW_TAG_const_type:       \
    case DW_TAG_volatile_type:    \
    case DW_TAG_pointer_type:     \
    case DW_TAG_base_type:        \
    case DW_TAG_array_type:       \
    case DW_TAG_structure_type:   \
    case DW_TAG_union_type:       \
    case DW_TAG_typedef:          \
    case DW_TAG_subroutine_type:  \
    case DW_TAG_enumeration_type: \
    case DW_TAG_enumerator:       \
    case DW_TAG_reference_type

struct ELFcie {
    ELFcie* next;
    uint32_t offset;
    uint8_t* augmentation;
    uint32_t codeAlign;
    int32_t dataAlign;
    int returnAddress;
    uint8_t* data;
    uint32_t dataLen;
};

struct ELFfde {
    ELFcie* cie;
    uint32_t address;
    uint32_t end;
    uint8_t* data;
    uint32_t dataLen;
};

enum ELFRegMode {
    REG_NOT_SET,
    REG_OFFSET,
    REG_REGISTER
};

struct ELFFrameStateRegister {
    ELFRegMode mode;
    int reg;
    int32_t offset;
};

struct ELFFrameStateRegisters {
    ELFFrameStateRegister regs[16];
    ELFFrameStateRegisters* previous;
};

enum ELFCfaMode {
    CFA_NOT_SET,
    CFA_REG_OFFSET
};

struct ELFFrameState {
    ELFFrameStateRegisters registers;

    ELFCfaMode cfaMode;
    int cfaRegister;
    int32_t cfaOffset;

    uint32_t pc;

    int dataAlign;
    int codeAlign;
    int returnAddress;
};

Symbol* elfSymbols = NULL;
char* elfSymbolsStrTab = NULL;
int elfSymbolsCount = 0;

// Replacement for the old raw-pointer section table. Entries are populated
// from an ELFIO::elfio reader at load time.
struct ElfSection {
    std::string    name;
    uint32_t       type;
    uint32_t       flags;
    uint32_t       addr;
    uint32_t       size;
    uint32_t       link;
    uint32_t       info;
    const uint8_t* bytes;  // points into elfio-owned section data, or nullptr
};

static ELFIO::elfio      g_elfReader;
static std::vector<ElfSection> g_elfSections;

// All ELF container state now lives in g_elfReader / g_elfSections. No more
// raw file-buffer globals — the legacy elfSectionHeaders[] / elfFileData were
// never read outside this file.

CompileUnit* elfCompileUnits = NULL;
DebugInfo* elfDebugInfo = NULL;
char* elfDebugStrings = NULL;

ELFcie* elfCies = NULL;
ELFfde** elfFdes = NULL;
int elfFdeCount = 0;

CompileUnit* elfCurrentUnit = NULL;

uint32_t elfRead4Bytes(uint8_t*);
uint16_t elfRead2Bytes(uint8_t*);

CompileUnit* elfGetCompileUnit(uint32_t addr)
{
    if (elfCompileUnits) {
        CompileUnit* unit = elfCompileUnits;
        while (unit) {
            if (unit->lowPC) {
                if (addr >= unit->lowPC && addr < unit->highPC)
                    return unit;
            } else {
                ARanges* r = unit->ranges;
                if (r) {
                    int count = r->count;
                    for (int j = 0; j < count; j++) {
                        if (addr >= r->ranges[j].lowPC && addr < r->ranges[j].highPC)
                            return unit;
                    }
                }
            }
            unit = unit->next;
        }
    }
    return NULL;
}

const char* elfGetAddressSymbol(uint32_t addr)
{
    static char buffer[256];		//defining globalscope here just feels so wrong

    CompileUnit* unit = elfGetCompileUnit(addr);
    // found unit, need to find function
    if (unit) {
        Function* func = unit->functions;
        while (func) {
            if (addr >= func->lowPC && addr < func->highPC) {
                int offset = addr - func->lowPC;
                const char* name = func->name;
                if (!name)
                    name = "";
                if (offset)
                    snprintf(buffer, 256, "%s+%d", name, offset);
                else {
#if __STDC_WANT_SECURE_LIB__
                    strncpy_s(buffer, sizeof(buffer), name, 255); //strncpy_s does not allways append a '\0'
#else
                    strncpy(buffer, name, 255);		//strncpy does not allways append a '\0'
#endif
		    buffer[255] = '\0';
		}
                return buffer;
            }
            func = func->next;
        }
    }

    if (elfSymbolsCount) {
        for (int i = 0; i < elfSymbolsCount; i++) {
            Symbol* s = &elfSymbols[i];
            if ((addr >= s->value) && addr < (s->value + s->size)) {
                int offset = addr - s->value;
                const char* name = s->name;
                if (name == NULL)
                    name = "";
                if (offset)
                    snprintf(buffer, 256,"%s+%d", name, addr - s->value);
                else {
#if __STDC_WANT_SECURE_LIB__
                    strncpy_s(buffer, sizeof(buffer), name, 255);
#else
                    strncpy(buffer, name, 255);
#endif
		    buffer[255] = '\0';
		}
                return buffer;
            } else if (addr == s->value) {
                if (s->name) {
#if __STDC_WANT_SECURE_LIB__
                    strncpy_s(buffer, sizeof(buffer), s->name, 255);
#else
                    strncpy(buffer, s->name, 255);
#endif
		    buffer[255] = '\0';
		} else
#if __STDC_WANT_SECURE_LIB__
                    strcpy_s(buffer, sizeof(buffer), "");
#else
                    strcpy(buffer, "");
#endif
                return buffer;
            }
        }
    }
    return "";
}

bool elfFindLineInModule(uint32_t* addr, const char* name, int line)
{
    CompileUnit* unit = elfCompileUnits;

    while (unit) {
        if (unit->lineInfoTable) {
            int i;
            int count = unit->lineInfoTable->fileCount;
            char* found = NULL;
            for (i = 0; i < count; i++) {
                if (strcmp(name, unit->lineInfoTable->files[i]) == 0) {
                    found = unit->lineInfoTable->files[i];
                    break;
                }
            }
            // found a matching filename... try to find line now
            if (found) {
                LineInfoItem* table = unit->lineInfoTable->lines;
                count = unit->lineInfoTable->number;
                for (i = 0; i < count; i++) {
                    if (table[i].file == found && table[i].line == line) {
                        *addr = table[i].address;
                        return true;
                    }
                }
                // we can only find a single match
                return false;
            }
        }
        unit = unit->next;
    }
    return false;
}

int elfFindLine(CompileUnit* unit, Function* /* func */, uint32_t addr, const char** f)
{
    int currentLine = -1;
    if (unit->hasLineInfo) {
        int count = unit->lineInfoTable->number;
        LineInfoItem* table = unit->lineInfoTable->lines;
        int i;
        for (i = 0; i < count; i++) {
            if (addr <= table[i].address)
                break;
        }
        if (i == count)
            i--;
        *f = table[i].file;
        currentLine = table[i].line;
    }
    return currentLine;
}

bool elfFindLineInUnit(uint32_t* addr, CompileUnit* unit, int line)
{
    if (unit->hasLineInfo) {
        int count = unit->lineInfoTable->number;
        LineInfoItem* table = unit->lineInfoTable->lines;
        int i;
        for (i = 0; i < count; i++) {
            if (line == table[i].line) {
                *addr = table[i].address;
                return true;
            }
        }
    }
    return false;
}

bool elfGetCurrentFunction(uint32_t addr, Function** f, CompileUnit** u)
{
    CompileUnit* unit = elfGetCompileUnit(addr);
    // found unit, need to find function
    if (unit) {
        Function* func = unit->functions;
        while (func) {
            if (addr >= func->lowPC && addr < func->highPC) {
                *f = func;
                *u = unit;
                return true;
            }
            func = func->next;
        }
    }
    return false;
}

bool elfGetObject(const char* name, Function* f, CompileUnit* u, Object** o)
{
    if (f && u) {
        Object* v = f->variables;

        while (v) {
            if (strcmp(name, v->name) == 0) {
                *o = v;
                return true;
            }
            v = v->next;
        }
        v = f->parameters;
        while (v) {
            if (strcmp(name, v->name) == 0) {
                *o = v;
                return true;
            }
            v = v->next;
        }
        v = u->variables;
        while (v) {
            if (strcmp(name, v->name) == 0) {
                *o = v;
                return true;
            }
            v = v->next;
        }
    }

    CompileUnit* c = elfCompileUnits;

    while (c) {
        if (c != u) {
            Object* v = c->variables;
            while (v) {
                if (strcmp(name, v->name) == 0) {
                    *o = v;
                    return true;
                }
                v = v->next;
            }
        }
        c = c->next;
    }

    return false;
}

const char* elfGetSymbol(int i, uint32_t* value, uint32_t* size, int* type)
{
    if (i < elfSymbolsCount) {
        Symbol* s = &elfSymbols[i];
        *value = s->value;
        *size = s->size;
        *type = s->type;
        return s->name;
    }
    return NULL;
}

bool elfGetSymbolAddress(const char* sym, uint32_t* addr, uint32_t* size, int* type)
{
    if (elfSymbolsCount) {
        for (int i = 0; i < elfSymbolsCount; i++) {
            Symbol* s = &elfSymbols[i];
            if (strcmp(sym, s->name) == 0) {
                *addr = s->value;
                *size = s->size;
                *type = s->type;
                return true;
            }
        }
    }
    return false;
}

ELFfde* elfGetFde(uint32_t address)
{
    if (elfFdes) {
        int i;
        for (i = 0; i < elfFdeCount; i++) {
            if (address >= elfFdes[i]->address && address < elfFdes[i]->end) {
                return elfFdes[i];
            }
        }
    }

    return NULL;
}

void elfExecuteCFAInstructions(ELFFrameState* state, uint8_t* data, uint32_t len,
    uint32_t pc)
{
    uint8_t* end = data + len;
    int _bytes;
    int _reg;
    ELFFrameStateRegisters* fs;

    while (data < end && state->pc < pc) {
        uint8_t op = *data++;

        switch (op >> 6) {
        case DW_CFA_advance_loc:
            state->pc += (op & 0x3f) * state->codeAlign;
            break;
        case DW_CFA_offset:
            _reg = op & 0x3f;
            state->registers.regs[_reg].mode = REG_OFFSET;
            state->registers.regs[_reg].offset = state->dataAlign * (int32_t)elfReadLEB128(data, &_bytes);
            data += _bytes;
            break;
        case DW_CFA_restore:
            // we don't care much about the other possible settings,
            // so just setting to unset is enough for now
            state->registers.regs[op & 0x3f].mode = REG_NOT_SET;
            break;
        case 0:
            switch (op & 0x3f) {
            case DW_CFA_nop:
                break;
            case DW_CFA_advance_loc1:
                state->pc += state->codeAlign * (*data++);
                break;
            case DW_CFA_advance_loc2:
                state->pc += state->codeAlign * elfRead2Bytes(data);
                data += 2;
                break;
            case DW_CFA_advance_loc4:
                state->pc += state->codeAlign * elfRead4Bytes(data);
                data += 4;
                break;
            case DW_CFA_offset_extended:
                _reg = elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->registers.regs[_reg].mode = REG_OFFSET;
                state->registers.regs[_reg].offset = state->dataAlign * (int32_t)elfReadLEB128(data, &_bytes);
                data += _bytes;
                break;
            case DW_CFA_restore_extended:
            case DW_CFA_undefined:
            case DW_CFA_same_value:
                _reg = elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->registers.regs[_reg].mode = REG_NOT_SET;
                break;
            case DW_CFA_register:
                _reg = elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->registers.regs[_reg].mode = REG_REGISTER;
                state->registers.regs[_reg].reg = elfReadLEB128(data, &_bytes);
                data += _bytes;
                break;
            case DW_CFA_remember_state:
                fs = (ELFFrameStateRegisters*)calloc(1,
                    sizeof(ELFFrameStateRegisters));
                memcpy(fs, &state->registers, sizeof(ELFFrameStateRegisters));
                state->registers.previous = fs;
                break;
            case DW_CFA_restore_state:
                if (state->registers.previous == NULL) {
                    printf("Error: previous frame state is NULL.\n");
                    return;
                }
                fs = state->registers.previous;
                memcpy(&state->registers, fs, sizeof(ELFFrameStateRegisters));
                free(fs);
                break;
            case DW_CFA_def_cfa:
                state->cfaRegister = elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->cfaOffset = (int32_t)elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->cfaMode = CFA_REG_OFFSET;
                break;
            case DW_CFA_def_cfa_register:
                state->cfaRegister = elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->cfaMode = CFA_REG_OFFSET;
                break;
            case DW_CFA_def_cfa_offset:
                state->cfaOffset = (int32_t)elfReadLEB128(data, &_bytes);
                data += _bytes;
                state->cfaMode = CFA_REG_OFFSET;
                break;
            default:
                printf("Unknown CFA opcode %08x\n", op);
                return;
            }
            break;
        default:
            printf("Unknown CFA opcode %08x\n", op);
            return;
        }
    }
}

ELFFrameState* elfGetFrameState(ELFfde* fde, uint32_t address)
{
    ELFFrameState* state = (ELFFrameState*)calloc(1, sizeof(ELFFrameState));
    state->pc = fde->address;
    state->dataAlign = fde->cie->dataAlign;
    state->codeAlign = fde->cie->codeAlign;
    state->returnAddress = fde->cie->returnAddress;

    elfExecuteCFAInstructions(state,
        fde->cie->data,
        fde->cie->dataLen,
        0xffffffff);
    elfExecuteCFAInstructions(state,
        fde->data,
        fde->dataLen,
        address);

    return state;
}

void elfPrintCallChain(uint32_t address)
{
    int count = 1;

    reg_pair regs[15];
    reg_pair newRegs[15];

    memcpy(&regs[0], &reg[0], sizeof(reg_pair) * 15);

    while (count < 20) {
        const char* addr = elfGetAddressSymbol(address);
        if (*addr == 0)
            addr = "???";

        printf("%08x %s\n", address, addr);

        ELFfde* fde = elfGetFde(address);

        if (fde == NULL) {
            break;
        }

        ELFFrameState* state = elfGetFrameState(fde, address);

        if (!state) {
            break;
        }

        if (state->cfaMode == CFA_REG_OFFSET) {
            memcpy(&newRegs[0], &regs[0], sizeof(reg_pair) * 15);
            uint32_t _addr = 0;
            for (int i = 0; i < 15; i++) {
                ELFFrameStateRegister* r = &state->registers.regs[i];

                switch (r->mode) {
                case REG_NOT_SET:
                    newRegs[i].I = regs[i].I;
                    break;
                case REG_OFFSET:
                    newRegs[i].I = elfReadMemory(regs[state->cfaRegister].I + state->cfaOffset + r->offset);
                    break;
                case REG_REGISTER:
                    newRegs[i].I = regs[r->reg].I;
                    break;
                default:
                    printf("Unknown register mode: %d\n", r->mode);
                    break;
                }
            }
            memcpy(regs, newRegs, sizeof(reg_pair) * 15);
            _addr = newRegs[14].I;
            _addr &= 0xfffffffe;
            address = _addr;
            count++;
        } else {
            printf("CFA not set\n");
            break;
        }
        if (state->registers.previous) {
            ELFFrameStateRegisters* prev = state->registers.previous;

            while (prev) {
                ELFFrameStateRegisters* p = prev->previous;
                free(prev);
                prev = p;
            }
        }
        free(state);
    }
}

uint32_t elfDecodeLocation(Function* f, ELFBlock* o, LocationType* type, uint32_t base)
{
    uint32_t framebase = 0;
    if (f && f->frameBase) {
        ELFBlock* b = f->frameBase;
        switch (*b->data) {
        case DW_OP_reg0:
        case DW_OP_reg1:
        case DW_OP_reg2:
        case DW_OP_reg3:
        case DW_OP_reg4:
        case DW_OP_reg5:
        case DW_OP_reg6:
        case DW_OP_reg7:
        case DW_OP_reg8:
        case DW_OP_reg9:
        case DW_OP_reg10:
        case DW_OP_reg11:
        case DW_OP_reg12:
        case DW_OP_reg13:
        case DW_OP_reg14:
        case DW_OP_reg15:
            framebase = reg[*b->data - 0x50].I;
            break;
        default:
            fprintf(stderr, "Unknown frameBase %02x\n", *b->data);
            break;
        }
    }

    ELFBlock* loc = o;
    uint32_t location = 0;
    int _bytes = 0;
    if (loc) {
        switch (*loc->data) {
        case DW_OP_addr:
            location = elfRead4Bytes(loc->data + 1);
            *type = LOCATION_memory;
            break;
        case DW_OP_plus_uconst:
            location = base + elfReadLEB128(loc->data + 1, &_bytes);
            *type = LOCATION_memory;
            break;
        case DW_OP_reg0:
        case DW_OP_reg1:
        case DW_OP_reg2:
        case DW_OP_reg3:
        case DW_OP_reg4:
        case DW_OP_reg5:
        case DW_OP_reg6:
        case DW_OP_reg7:
        case DW_OP_reg8:
        case DW_OP_reg9:
        case DW_OP_reg10:
        case DW_OP_reg11:
        case DW_OP_reg12:
        case DW_OP_reg13:
        case DW_OP_reg14:
        case DW_OP_reg15:
            location = *loc->data - 0x50;
            *type = LOCATION_register;
            break;
        case DW_OP_fbreg: {
            int bytes;
            int32_t off = elfReadSignedLEB128(loc->data + 1, &bytes);
            location = framebase + off;
            *type = LOCATION_memory;
        } break;
        default:
            fprintf(stderr, "Unknown location %02x\n", *loc->data);
            break;
        }
    }
    return location;
}

uint32_t elfDecodeLocation(Function* f, ELFBlock* o, LocationType* type)
{
    return elfDecodeLocation(f, o, type, 0);
}

// reading function

uint32_t elfRead4Bytes(uint8_t* data)
{
    uint32_t value = *data++;
    value |= (*data++ << 8);
    value |= (*data++ << 16);
    value |= (*data << 24);
    return value;
}

uint16_t elfRead2Bytes(uint8_t* data)
{
    uint16_t value = *data++;
    value |= (*data << 8);
    return value;
}

char* elfReadString(uint8_t* data, int* bytesRead)
{
    if (*data == 0) {
        *bytesRead = 1;
        return NULL;
    }
    *bytesRead = (int)strlen((char*)data) + 1;
    return (char*)data;
}

int32_t elfReadSignedLEB128(uint8_t* data, int* bytesRead)
{
    int32_t result = 0;
    int shift = 0;
    int count = 0;

    uint8_t byte;
    do {
        byte = *data++;
        count++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    if ((shift < 32) && (byte & 0x40))
        result |= -(1 << shift);
    *bytesRead = count;
    return result;
}

uint32_t elfReadLEB128(uint8_t* data, int* bytesRead)
{
    uint32_t result = 0;
    int shift = 0;
    int count = 0;
    uint8_t byte;
    do {
        byte = *data++;
        count++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    *bytesRead = count;
    return result;
}

// Section bytes come from elfio-owned storage.
uint8_t* elfReadSection(ElfSection* sh)
{
    if (sh == NULL || sh->bytes == NULL)
        return NULL;
    return const_cast<uint8_t*>(sh->bytes);
}

ElfSection* elfGetSectionByName(const char* name)
{
    if (name == NULL)
        return NULL;
    for (auto& s : g_elfSections) {
        if (s.name == name)
            return &s;
    }
    return NULL;
}

ElfSection* elfGetSectionByNumber(int number)
{
    if (number >= 0 && number < static_cast<int>(g_elfSections.size()))
        return &g_elfSections[number];
    return NULL;
}

CompileUnit* elfGetCompileUnitForData(uint8_t* data)
{
    uint8_t* end = elfCurrentUnit->top + 4 + elfCurrentUnit->length;

    if (data >= elfCurrentUnit->top && data < end)
        return elfCurrentUnit;

    CompileUnit* unit = elfCompileUnits;

    while (unit) {
        end = unit->top + 4 + unit->length;

        if (data >= unit->top && data < end)
            return unit;

        unit = unit->next;
    }

    fprintf(stderr, "Warning: cannot find reference to compile unit at offset %08x\n",
        (int)(data - elfDebugInfo->infodata));
    return NULL;
}

uint8_t* elfReadAttribute(uint8_t* data, ELFAttr* attr)
{
    int bytes;
    int form = attr->form;
start:
    switch (form) {
    case DW_FORM_addr:
        attr->value = elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_data2:
        attr->value = elfRead2Bytes(data);
        data += 2;
        break;
    case DW_FORM_data4:
        attr->value = elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_string:
        attr->string = (char*)data;
        data += strlen(attr->string) + 1;
        break;
    case DW_FORM_strp:
        attr->string = elfDebugStrings + elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_block:
        attr->block = (ELFBlock*)malloc(sizeof(ELFBlock));
        attr->block->length = elfReadLEB128(data, &bytes);
        data += bytes;
        attr->block->data = data;
        data += attr->block->length;
        break;
    case DW_FORM_block1:
        attr->block = (ELFBlock*)malloc(sizeof(ELFBlock));
        attr->block->length = *data++;
        attr->block->data = data;
        data += attr->block->length;
        break;
    case DW_FORM_data1:
        attr->value = *data++;
        break;
    case DW_FORM_flag:
        attr->flag = (*data++) ? true : false;
        break;
    case DW_FORM_sdata:
        attr->value = elfReadSignedLEB128(data, &bytes);
        data += bytes;
        break;
    case DW_FORM_udata:
        attr->value = elfReadLEB128(data, &bytes);
        data += bytes;
        break;
    case DW_FORM_ref_addr:
        attr->value = (uint32_t)((elfDebugInfo->infodata + elfRead4Bytes(data)) - elfGetCompileUnitForData(data)->top);
        data += 4;
        break;
    case DW_FORM_ref4:
        attr->value = elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_ref_udata:
        attr->value = (uint32_t)((elfDebugInfo->infodata + (elfGetCompileUnitForData(data)->top - elfDebugInfo->infodata) + elfReadLEB128(data, &bytes)) - elfCurrentUnit->top);
        data += bytes;
        break;
    case DW_FORM_indirect:
        form = elfReadLEB128(data, &bytes);
        data += bytes;
        goto start;
    default:
        fprintf(stderr, "Warning: unsupported DWARF FORM %02x — debug info skipped\n", form);
        attr->value = 0;
        break;
    }
    return data;
}

ELFAbbrev* elfGetAbbrev(ELFAbbrev** table, uint32_t number)
{
    int hash = number % 121;

    ELFAbbrev* abbrev = table[hash];

    while (abbrev) {
        if (abbrev->number == number)
            return abbrev;
        abbrev = abbrev->next;
    }
    return NULL;
}

ELFAbbrev** elfReadAbbrevs(uint8_t* data, uint32_t offset)
{
    data += offset;
    ELFAbbrev** abbrevs = (ELFAbbrev**)calloc(sizeof(ELFAbbrev*) * 121, 1);
    int bytes = 0;
    uint32_t number = elfReadLEB128(data, &bytes);
    data += bytes;
    while (number) {
        ELFAbbrev* abbrev = (ELFAbbrev*)calloc(1, sizeof(ELFAbbrev));

        // read tag information
        abbrev->number = number;
        abbrev->tag = elfReadLEB128(data, &bytes);
        data += bytes;
        abbrev->hasChildren = *data++ ? true : false;

        // read attributes
        int name = elfReadLEB128(data, &bytes);
        data += bytes;
        int form = elfReadLEB128(data, &bytes);
        data += bytes;

        while (name) {
            if ((abbrev->numAttrs % 4) == 0) {
                abbrev->attrs = (ELFAttr*)realloc(abbrev->attrs,
                    (abbrev->numAttrs + 4) * sizeof(ELFAttr));
            }
            abbrev->attrs[abbrev->numAttrs].name = name;
            abbrev->attrs[abbrev->numAttrs++].form = form;

            name = elfReadLEB128(data, &bytes);
            data += bytes;
            form = elfReadLEB128(data, &bytes);
            data += bytes;
        }

        int hash = number % 121;
        abbrev->next = abbrevs[hash];
        abbrevs[hash] = abbrev;

        number = elfReadLEB128(data, &bytes);
        data += bytes;

        if (elfGetAbbrev(abbrevs, number) != NULL)
            break;
    }

    return abbrevs;
}

void elfParseCFA()
{
    ElfSection* h = elfGetSectionByName(".debug_frame");

    if (h == NULL) {
        return;
    }

    uint8_t* data = elfReadSection(h);
    if (data == NULL)
        return;

    uint8_t* topOffset = data;

    uint8_t* end = data + h->size;

    ELFcie* cies = NULL;

    while (data < end) {
        uint32_t offset = (uint32_t)(data - topOffset);
        uint32_t len = elfRead4Bytes(data);
        data += 4;

        uint8_t* dataEnd = data + len;

        uint32_t id = elfRead4Bytes(data);
        data += 4;

        if (id == 0xffffffff) {
            // skip version
            (*data)++;

            ELFcie* cie = (ELFcie*)calloc(1, sizeof(ELFcie));

            cie->next = cies;
            cies = cie;

            cie->offset = offset;

            cie->augmentation = data;
            while (*data)
                data++;
            data++;

            if (*cie->augmentation) {
                // Modern toolchains emit CIE augmentations (e.g. "zR") that
                // this parser doesn't decode. Skipping .debug_frame entirely
                // is safe: CFA info is only used by the optional debugger
                // for frame-base lookups and is not required to run the ROM.
                fprintf(stderr,
                    "Warning: .debug_frame augmentation \"%s\" not supported — CFA info skipped\n",
                    cie->augmentation);
                ELFcie* c = cies;
                while (c) {
                    ELFcie* nxt = c->next;
                    free(c);
                    c = nxt;
                }
                return;
            }

            int bytes;
            cie->codeAlign = elfReadLEB128(data, &bytes);
            data += bytes;

            cie->dataAlign = elfReadSignedLEB128(data, &bytes);
            data += bytes;

            cie->returnAddress = *data++;

            cie->data = data;
            cie->dataLen = (uint32_t)(dataEnd - data);
        } else {
            ELFfde* fde = (ELFfde*)calloc(1, sizeof(ELFfde));

            ELFcie* cie = cies;

            while (cie != NULL) {
                if (cie->offset == id)
                    break;
                cie = cie->next;
            }

            if (!cie) {
                fprintf(stderr, "Warning: cannot find CIE %08x — skipping FDE\n", id);
                free(fde);
                data = dataEnd;
                continue;
            }

            fde->cie = cie;

            fde->address = elfRead4Bytes(data);
            data += 4;

            fde->end = fde->address + elfRead4Bytes(data);
            data += 4;

            fde->data = data;
            fde->dataLen = (uint32_t)(dataEnd - data);

            if ((elfFdeCount % 10) == 0) {
                elfFdes = (ELFfde**)realloc(elfFdes, (elfFdeCount + 10) * sizeof(ELFfde*));
            }
            elfFdes[elfFdeCount++] = fde;
        }
        data = dataEnd;
    }

    elfCies = cies;
}

void elfAddLine(LineInfo* l, uint32_t a, int file, int line, int* max)
{
    if (l->number == *max) {
        *max += 1000;
        l->lines = (LineInfoItem*)realloc(l->lines, *max * sizeof(LineInfoItem));
    }
    LineInfoItem* li = &l->lines[l->number];
    li->file = l->files[file - 1];
    li->address = a;
    li->line = line;
    l->number++;
}

void elfParseLineInfo(CompileUnit* unit)
{
    ElfSection* h = elfGetSectionByName(".debug_line");
    if (h == NULL) {
        fprintf(stderr, "No line information found\n");
        return;
    }
    LineInfo* l = unit->lineInfoTable = (LineInfo*)calloc(1, sizeof(LineInfo));
    l->number = 0;
    int max = 1000;
    l->lines = (LineInfoItem*)malloc(1000 * sizeof(LineInfoItem));

    uint8_t* data = elfReadSection(h);
    if (data == NULL)
        return;
    data += unit->lineInfo;
    uint32_t totalLen = elfRead4Bytes(data);
    data += 4;
    uint8_t* end = data + totalLen;
    //  uint16_t version = elfRead2Bytes(data);
    data += 2;
    //  uint32_t offset = elfRead4Bytes(data);
    data += 4;
    int minInstrSize = *data++;
    int defaultIsStmt = *data++;
    int lineBase = (int8_t)*data++;
    int lineRange = *data++;
    int opcodeBase = *data++;
    uint8_t* stdOpLen = (uint8_t*)malloc(opcodeBase * sizeof(uint8_t));
    stdOpLen[0] = 1;
    int i;
    for (i = 1; i < opcodeBase; i++)
        stdOpLen[i] = *data++;

    free(stdOpLen); // todo
    int bytes = 0;

    char* s;
    while ((s = elfReadString(data, &bytes)) != NULL) {
        data += bytes;
        //    fprintf(stderr, "Directory is %s\n", s);
    }
    data += bytes;
    int count = 4;
    int index = 0;
    l->files = (char**)malloc(sizeof(char*) * count);

    while ((s = elfReadString(data, &bytes)) != NULL) {
        l->files[index++] = s;

        data += bytes;
        // directory
        elfReadLEB128(data, &bytes);
        data += bytes;
        // time
        elfReadLEB128(data, &bytes);
        data += bytes;
        // size
        elfReadLEB128(data, &bytes);
        data += bytes;
        //    fprintf(stderr, "File is %s\n", s);
        if (index == count) {
            count += 4;
            l->files = (char**)realloc(l->files, sizeof(char*) * count);
        }
    }
    l->fileCount = index;
    data += bytes;

    while (data < end) {
        uint32_t address = 0;
        int file = 1;
        int line = 1;
        int isStmt = defaultIsStmt;
        int endSeq = 0;

        while (!endSeq) {
            int op = *data++;
            switch (op) {
            case DW_LNS_extended_op: {
                data++;
                op = *data++;
                switch (op) {
                case DW_LNE_end_sequence:
                    endSeq = 1;
                    break;
                case DW_LNE_set_address:
                    address = elfRead4Bytes(data);
                    data += 4;
                    break;
                default:
                    fprintf(stderr, "Warning: unknown extended LINE opcode %02x — line info skipped\n", op);
                    return;
                }
            } break;
            case DW_LNS_copy:
                //      fprintf(stderr, "Address %08x line %d (%d)\n", address, line, file);
                elfAddLine(l, address, file, line, &max);
                break;
            case DW_LNS_advance_pc:
                address += minInstrSize * elfReadLEB128(data, &bytes);
                data += bytes;
                break;
            case DW_LNS_advance_line:
                line += elfReadSignedLEB128(data, &bytes);
                data += bytes;
                break;
            case DW_LNS_set_file:
                file = elfReadLEB128(data, &bytes);
                data += bytes;
                break;
            case DW_LNS_set_column:
                elfReadLEB128(data, &bytes);
                data += bytes;
                break;
            case DW_LNS_negate_stmt:
                isStmt = !isStmt;
                break;
            case DW_LNS_set_basic_block:
                break;
            case DW_LNS_const_add_pc:
                address += (minInstrSize * ((255 - opcodeBase) / lineRange));
                break;
            case DW_LNS_fixed_advance_pc:
                address += elfRead2Bytes(data);
                data += 2;
                break;
            default:
                op = op - opcodeBase;
                address += (op / lineRange) * minInstrSize;
                line += lineBase + (op % lineRange);
                elfAddLine(l, address, file, line, &max);
                //        fprintf(stderr, "Address %08x line %d (%d)\n", address, line,file);
                break;
            }
        }
    }
    l->lines = (LineInfoItem*)realloc(l->lines, l->number * sizeof(LineInfoItem));
}

uint8_t* elfSkipData(uint8_t* data, ELFAbbrev* abbrev, ELFAbbrev** abbrevs)
{
    int i;
    int bytes;

    for (i = 0; i < abbrev->numAttrs; i++) {
        data = elfReadAttribute(data, &abbrev->attrs[i]);
        if (abbrev->attrs[i].form == DW_FORM_block1)
            free(abbrev->attrs[i].block);
    }

    if (abbrev->hasChildren) {
        int nesting = 1;
        while (nesting) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                continue;
            }

            abbrev = elfGetAbbrev(abbrevs, abbrevNum);

            for (i = 0; i < abbrev->numAttrs; i++) {
                data = elfReadAttribute(data, &abbrev->attrs[i]);
                if (abbrev->attrs[i].form == DW_FORM_block1)
                    free(abbrev->attrs[i].block);
            }

            if (abbrev->hasChildren) {
                nesting++;
            }
        }
    }
    return data;
}

Type* elfParseType(CompileUnit* unit, uint32_t);
uint8_t* elfParseObject(uint8_t* data, ELFAbbrev* abbrev, CompileUnit* unit,
    Object** object);
uint8_t* elfParseFunction(uint8_t* data, ELFAbbrev* abbrev, CompileUnit* unit,
    Function** function);
void elfCleanUp(Function*);

void elfAddType(Type* type, CompileUnit* unit, uint32_t offset)
{
    if (type->next == NULL) {
        if (unit->types != type && type->offset == 0) {
            type->offset = offset;
            type->next = unit->types;
            unit->types = type;
        }
    }
}

void elfParseType(uint8_t* data, uint32_t offset, ELFAbbrev* abbrev, CompileUnit* unit,
    Type** type)
{
    switch (abbrev->tag) {
    case DW_TAG_typedef: {
        uint32_t typeref = 0;
        char* name = NULL;
        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_name:
                name = attr->string;
                break;
            case DW_AT_type:
                typeref = attr->value;
                break;
            case DW_AT_decl_file:
            case DW_AT_decl_line:
                break;
            default:
                fprintf(stderr, "Unknown attribute for typedef %02x\n", attr->name);
                break;
            }
        }
        if (abbrev->hasChildren)
            fprintf(stderr, "Unexpected children for typedef\n");
        *type = elfParseType(unit, typeref);
        if (name)
            (*type)->name = name;
        return;
    } break;
    case DW_TAG_union_type:
    case DW_TAG_structure_type: {
        Type* t = (Type*)calloc(1, sizeof(Type));
        if (abbrev->tag == DW_TAG_structure_type)
            t->type = TYPE_struct;
        else
            t->type = TYPE_union;

        Struct* s = (Struct*)calloc(1, sizeof(Struct));
        t->structure = s;
        elfAddType(t, unit, offset);

        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_name:
                t->name = attr->string;
                break;
            case DW_AT_byte_size:
                t->size = attr->value;
                break;
            case DW_AT_decl_file:
            case DW_AT_decl_line:
            case DW_AT_sibling:
            case DW_AT_containing_type: // todo?
            case DW_AT_declaration:
            case DW_AT_specification: // TODO:
                break;
            default:
                fprintf(stderr, "Unknown attribute for struct %02x\n", attr->name);
                break;
            }
        }
        if (abbrev->hasChildren) {
            int bytes;
            uint32_t num = elfReadLEB128(data, &bytes);
            data += bytes;
            int index = 0;
            while (num) {
                ELFAbbrev* abbr = elfGetAbbrev(unit->abbrevs, num);

                switch (abbr->tag) {
                case DW_TAG_member: {
                    if ((index % 4) == 0)
                        s->members = (Member*)realloc(s->members,
                            sizeof(Member) * (index + 4));
                    Member* m = &s->members[index];
                    m->location = NULL;
                    m->bitOffset = 0;
                    m->bitSize = 0;
                    m->byteSize = 0;
                    for (int i = 0; i < abbr->numAttrs; i++) {
                        ELFAttr* attr = &abbr->attrs[i];
                        data = elfReadAttribute(data, attr);
                        switch (attr->name) {
                        case DW_AT_name:
                            m->name = attr->string;
                            break;
                        case DW_AT_type:
                            m->type = elfParseType(unit, attr->value);
                            break;
                        case DW_AT_data_member_location:
                            m->location = attr->block;
                            break;
                        case DW_AT_byte_size:
                            m->byteSize = attr->value;
                            break;
                        case DW_AT_bit_offset:
                            m->bitOffset = attr->value;
                            break;
                        case DW_AT_bit_size:
                            m->bitSize = attr->value;
                            break;
                        case DW_AT_decl_file:
                        case DW_AT_decl_line:
                        case DW_AT_accessibility:
                        case DW_AT_artificial: // todo?
                            break;
                        default:
                            fprintf(stderr, "Unknown member attribute %02x\n",
                                attr->name);
                        }
                    }
                    index++;
                } break;
                case DW_TAG_subprogram: {
                    Function* fnc = NULL;
                    data = elfParseFunction(data, abbr, unit, &fnc);
                    if (fnc != NULL) {
                        if (unit->lastFunction)
                            unit->lastFunction->next = fnc;
                        else
                            unit->functions = fnc;
                        unit->lastFunction = fnc;
                    }
                } break;
                case DW_TAG_inheritance:
                    // TODO: add support
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                CASE_TYPE_TAG:
                    // skip types... parsed only when used
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                case DW_TAG_variable:
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                default:
                    fprintf(stderr, "Unknown struct tag %02x %s\n", abbr->tag, t->name);
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                }
                num = elfReadLEB128(data, &bytes);
                data += bytes;
            }
            s->memberCount = index;
        }
        *type = t;
        return;
    } break;
    case DW_TAG_base_type: {
        Type* t = (Type*)calloc(1, sizeof(Type));

        t->type = TYPE_base;
        elfAddType(t, unit, offset);
        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_name:
                t->name = attr->string;
                break;
            case DW_AT_encoding:
                t->encoding = attr->value;
                break;
            case DW_AT_byte_size:
                t->size = attr->value;
                break;
            case DW_AT_bit_size:
                t->bitSize = attr->value;
                break;
            default:
                fprintf(stderr, "Unknown attribute for base type %02x\n",
                    attr->name);
                break;
            }
        }
        if (abbrev->hasChildren)
            fprintf(stderr, "Unexpected children for base type\n");
        *type = t;
        return;
    } break;
    case DW_TAG_pointer_type: {
        Type* t = (Type*)calloc(1, sizeof(Type));

        t->type = TYPE_pointer;

        elfAddType(t, unit, offset);

        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_type:
                t->pointer = elfParseType(unit, attr->value);
                break;
            case DW_AT_byte_size:
                t->size = attr->value;
                break;
            default:
                fprintf(stderr, "Unknown pointer type attribute %02x\n", attr->name);
                break;
            }
        }
        if (abbrev->hasChildren)
            fprintf(stderr, "Unexpected children for pointer type\n");
        *type = t;
        return;
    } break;
    case DW_TAG_reference_type: {
        Type* t = (Type*)calloc(1, sizeof(Type));

        t->type = TYPE_reference;

        elfAddType(t, unit, offset);

        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_type:
                t->pointer = elfParseType(unit, attr->value);
                break;
            case DW_AT_byte_size:
                t->size = attr->value;
                break;
            default:
                fprintf(stderr, "Unknown ref type attribute %02x\n", attr->name);
                break;
            }
        }
        if (abbrev->hasChildren)
            fprintf(stderr, "Unexpected children for ref type\n");
        *type = t;
        return;
    } break;
    case DW_TAG_volatile_type: {
        uint32_t typeref = 0;

        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_type:
                typeref = attr->value;
                break;
            default:
                fprintf(stderr, "Unknown volatile attribute for type %02x\n",
                    attr->name);
                break;
            }
        }
        if (abbrev->hasChildren)
            fprintf(stderr, "Unexpected children for volatile type\n");
        *type = elfParseType(unit, typeref);
        return;
    } break;
    case DW_TAG_const_type: {
        uint32_t typeref = 0;

        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_type:
                typeref = attr->value;
                break;
            default:
                fprintf(stderr, "Unknown const attribute for type %02x\n",
                    attr->name);
                break;
            }
        }
        if (abbrev->hasChildren)
            fprintf(stderr, "Unexpected children for const type\n");
        *type = elfParseType(unit, typeref);
        return;
    } break;
    case DW_TAG_enumeration_type: {
        Type* t = (Type*)calloc(1, sizeof(Type));
        t->type = TYPE_enum;
        Enum* e = (Enum*)calloc(1, sizeof(Enum));
        t->enumeration = e;
        elfAddType(t, unit, offset);
        int count = 0;
        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_name:
                t->name = attr->string;
                break;
            case DW_AT_byte_size:
                t->size = attr->value;
                break;
            case DW_AT_sibling:
            case DW_AT_decl_file:
            case DW_AT_decl_line:
                break;
            default:
                fprintf(stderr, "Unknown enum attribute %02x\n", attr->name);
            }
        }
        if (abbrev->hasChildren) {
            int bytes;
            uint32_t num = elfReadLEB128(data, &bytes);
            data += bytes;
            while (num) {
                ELFAbbrev* abbr = elfGetAbbrev(unit->abbrevs, num);

                switch (abbr->tag) {
                case DW_TAG_enumerator: {
                    count++;
                    e->members = (EnumMember*)realloc(e->members,
                        count * sizeof(EnumMember));
                    EnumMember* m = &e->members[count - 1];
                    for (int i = 0; i < abbr->numAttrs; i++) {
                        ELFAttr* attr = &abbr->attrs[i];
                        data = elfReadAttribute(data, attr);
                        switch (attr->name) {
                        case DW_AT_name:
                            m->name = attr->string;
                            break;
                        case DW_AT_const_value:
                            m->value = attr->value;
                            break;
                        default:
                            fprintf(stderr, "Unknown sub param attribute %02x\n",
                                attr->name);
                        }
                    }
                } break;
                default:
                    fprintf(stderr, "Unknown enum tag %02x\n", abbr->tag);
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                }
                num = elfReadLEB128(data, &bytes);
                data += bytes;
            }
        }
        e->count = count;
        *type = t;
        return;
    } break;
    case DW_TAG_subroutine_type: {
        Type* t = (Type*)calloc(1, sizeof(Type));
        t->type = TYPE_function;
        FunctionType* f = (FunctionType*)calloc(1, sizeof(FunctionType));
        t->function = f;
        elfAddType(t, unit, offset);
        for (int i = 0; i < abbrev->numAttrs; i++) {
            ELFAttr* attr = &abbrev->attrs[i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_prototyped:
            case DW_AT_sibling:
                break;
            case DW_AT_type:
                f->returnType = elfParseType(unit, attr->value);
                break;
            default:
                fprintf(stderr, "Unknown subroutine attribute %02x\n", attr->name);
            }
        }
        if (abbrev->hasChildren) {
            int bytes;
            uint32_t num = elfReadLEB128(data, &bytes);
            data += bytes;
            Object* lastVar = NULL;
            while (num) {
                ELFAbbrev* abbr = elfGetAbbrev(unit->abbrevs, num);

                switch (abbr->tag) {
                case DW_TAG_formal_parameter: {
                    Object* o;
                    data = elfParseObject(data, abbr, unit, &o);
                    if (f->args)
                        lastVar->next = o;
                    else
                        f->args = o;
                    lastVar = o;
                } break;
                case DW_TAG_unspecified_parameters:
                    // no use in the debugger yet
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                CASE_TYPE_TAG:
                    // skip types... parsed only when used
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                default:
                    fprintf(stderr, "Unknown subroutine tag %02x\n", abbr->tag);
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                }
                num = elfReadLEB128(data, &bytes);
                data += bytes;
            }
        }
        *type = t;
        return;
    } break;
    case DW_TAG_array_type: {
        uint32_t typeref = 0;
        int _i;
        Array* array = (Array*)calloc(1, sizeof(Array));
        Type* t = (Type*)calloc(1, sizeof(Type));
        t->type = TYPE_array;
        elfAddType(t, unit, offset);

        for (_i = 0; _i < abbrev->numAttrs; _i++) {
            ELFAttr* attr = &abbrev->attrs[_i];
            data = elfReadAttribute(data, attr);
            switch (attr->name) {
            case DW_AT_sibling:
                break;
            case DW_AT_type:
                typeref = attr->value;
                array->type = elfParseType(unit, typeref);
                break;
            default:
                fprintf(stderr, "Unknown array attribute %02x\n", attr->name);
            }
        }
        if (abbrev->hasChildren) {
            int bytes;
            uint32_t num = elfReadLEB128(data, &bytes);
            data += bytes;
            int index = 0;
            int maxBounds = 0;
            while (num) {
                ELFAbbrev* abbr = elfGetAbbrev(unit->abbrevs, num);

                switch (abbr->tag) {
                case DW_TAG_subrange_type: {
                    if (maxBounds == index) {
                        maxBounds += 4;
                        array->bounds = (int*)realloc(array->bounds,
                            sizeof(int) * maxBounds);
                    }
                    for (int i = 0; i < abbr->numAttrs; i++) {
                        ELFAttr* attr = &abbr->attrs[i];
                        data = elfReadAttribute(data, attr);
                        switch (attr->name) {
                        case DW_AT_upper_bound:
                            array->bounds[index] = attr->value + 1;
                            break;
                        case DW_AT_type: // ignore
                            break;
                        default:
                            fprintf(stderr, "Unknown subrange attribute %02x\n",
                                attr->name);
                        }
                    }
                    index++;
                } break;
                default:
                    fprintf(stderr, "Unknown array tag %02x\n", abbr->tag);
                    data = elfSkipData(data, abbr, unit->abbrevs);
                    break;
                }
                num = elfReadLEB128(data, &bytes);
                data += bytes;
            }
            array->maxBounds = index;
        }
        t->size = array->type->size;
        for (_i = 0; _i < array->maxBounds; _i++)
            t->size *= array->bounds[_i];
        t->array = array;
        *type = t;
        return;
    } break;
    default:
        fprintf(stderr, "Warning: unknown type TAG %02x — skipped\n", abbrev->tag);
        *type = NULL;
        return;
    }
}

Type* elfParseType(CompileUnit* unit, uint32_t offset)
{
    Type* _t = unit->types;

    while (_t) {
        if (_t->offset == offset)
            return _t;
        _t = _t->next;
    }
    if (offset == 0) {
        Type* t = (Type*)calloc(1, sizeof(Type));
        t->type = TYPE_void;
        t->offset = 0;
        elfAddType(t, unit, 0);
        return t;
    }
    uint8_t* data = unit->top + offset;
    int bytes;
    int abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;
    Type* type = NULL;

    ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);

    elfParseType(data, offset, abbrev, unit, &type);
    return type;
}

void elfGetObjectAttributes(CompileUnit* unit, uint32_t offset, Object* o)
{
    uint8_t* data = unit->top + offset;
    int bytes;
    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;

    if (!abbrevNum) {
        return;
    }

    ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);

    for (int i = 0; i < abbrev->numAttrs; i++) {
        ELFAttr* attr = &abbrev->attrs[i];
        data = elfReadAttribute(data, attr);
        switch (attr->name) {
        case DW_AT_location:
            o->location = attr->block;
            break;
        case DW_AT_name:
            if (o->name == NULL)
                o->name = attr->string;
            break;
        case DW_AT_MIPS_linkage_name:
            o->name = attr->string;
            break;
        case DW_AT_decl_file:
            o->file = attr->value;
            break;
        case DW_AT_decl_line:
            o->line = attr->value;
            break;
        case DW_AT_type:
            o->type = elfParseType(unit, attr->value);
            break;
        case DW_AT_external:
            o->external = attr->flag;
            break;
        case DW_AT_const_value:
        case DW_AT_abstract_origin:
        case DW_AT_declaration:
        case DW_AT_artificial:
            // todo
            break;
        case DW_AT_specification:
            // TODO:
            break;
        default:
            fprintf(stderr, "Unknown object attribute %02x\n", attr->name);
            break;
        }
    }
}

uint8_t* elfParseObject(uint8_t* data, ELFAbbrev* abbrev, CompileUnit* unit,
    Object** object)
{
    Object* o = (Object*)calloc(1, sizeof(Object));

    o->next = NULL;

    for (int i = 0; i < abbrev->numAttrs; i++) {
        ELFAttr* attr = &abbrev->attrs[i];
        data = elfReadAttribute(data, attr);
        switch (attr->name) {
        case DW_AT_location:
            o->location = attr->block;
            break;
        case DW_AT_name:
            if (o->name == NULL)
                o->name = attr->string;
            break;
        case DW_AT_MIPS_linkage_name:
            o->name = attr->string;
            break;
        case DW_AT_decl_file:
            o->file = attr->value;
            break;
        case DW_AT_decl_line:
            o->line = attr->value;
            break;
        case DW_AT_type:
            o->type = elfParseType(unit, attr->value);
            break;
        case DW_AT_external:
            o->external = attr->flag;
            break;
        case DW_AT_abstract_origin:
            elfGetObjectAttributes(unit, attr->value, o);
            break;
        case DW_AT_const_value:
        case DW_AT_declaration:
        case DW_AT_artificial:
            break;
        case DW_AT_specification:
            // TODO:
            break;
        default:
            fprintf(stderr, "Unknown object attribute %02x\n", attr->name);
            break;
        }
    }
    *object = o;
    return data;
}

uint8_t* elfParseBlock(uint8_t* data, ELFAbbrev* abbrev, CompileUnit* unit,
    Function* func, Object** lastVar)
{
    int bytes;
    uint32_t start = func->lowPC;

    for (int i = 0; i < abbrev->numAttrs; i++) {
        ELFAttr* attr = &abbrev->attrs[i];
        data = elfReadAttribute(data, attr);
        switch (attr->name) {
        case DW_AT_sibling:
            break;
        case DW_AT_low_pc:
            start = attr->value;
            break;
        case DW_AT_high_pc:
            break;
        case DW_AT_ranges: // ignore for now
            break;
        default:
            fprintf(stderr, "Unknown block attribute %02x\n", attr->name);
            break;
        }
    }

    if (abbrev->hasChildren) {
        int nesting = 1;

        while (nesting) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                continue;
            }

            abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);

            switch (abbrev->tag) {
            CASE_TYPE_TAG: // types only parsed when used
            case DW_TAG_label: // not needed
                data = elfSkipData(data, abbrev, unit->abbrevs);
                break;
            case DW_TAG_lexical_block:
                data = elfParseBlock(data, abbrev, unit, func, lastVar);
                break;
            case DW_TAG_subprogram: {
                Function* f = NULL;
                data = elfParseFunction(data, abbrev, unit, &f);
                if (f != NULL) {
                    if (unit->lastFunction)
                        unit->lastFunction->next = f;
                    else
                        unit->functions = f;
                    unit->lastFunction = f;
                }
            } break;
            case DW_TAG_variable: {
                Object* o;
                data = elfParseObject(data, abbrev, unit, &o);
                if (o->startScope == 0)
                    o->startScope = start;
                if (o->endScope == 0)
                    o->endScope = 0;
                if (func->variables)
                    (*lastVar)->next = o;
                else
                    func->variables = o;
                *lastVar = o;
            } break;
            case DW_TAG_inlined_subroutine:
                // TODO:
                data = elfSkipData(data, abbrev, unit->abbrevs);
                break;
            default: {
                fprintf(stderr, "Unknown block TAG %02x\n", abbrev->tag);
                data = elfSkipData(data, abbrev, unit->abbrevs);
            } break;
            }
        }
    }
    return data;
}

void elfGetFunctionAttributes(CompileUnit* unit, uint32_t offset, Function* func)
{
    uint8_t* data = unit->top + offset;
    int bytes;
    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;

    if (!abbrevNum) {
        return;
    }

    ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);

    for (int i = 0; i < abbrev->numAttrs; i++) {
        ELFAttr* attr = &abbrev->attrs[i];
        data = elfReadAttribute(data, attr);

        switch (attr->name) {
        case DW_AT_sibling:
            break;
        case DW_AT_name:
            if (func->name == NULL)
                func->name = attr->string;
            break;
        case DW_AT_MIPS_linkage_name:
            func->name = attr->string;
            break;
        case DW_AT_low_pc:
            func->lowPC = attr->value;
            break;
        case DW_AT_high_pc:
            func->highPC = attr->value;
            break;
        case DW_AT_decl_file:
            func->file = attr->value;
            break;
        case DW_AT_decl_line:
            func->line = attr->value;
            break;
        case DW_AT_external:
            func->external = attr->flag;
            break;
        case DW_AT_frame_base:
            func->frameBase = attr->block;
            break;
        case DW_AT_type:
            func->returnType = elfParseType(unit, attr->value);
            break;
        case DW_AT_inline:
        case DW_AT_specification:
        case DW_AT_declaration:
        case DW_AT_artificial:
        case DW_AT_prototyped:
        case DW_AT_proc_body:
        case DW_AT_save_offset:
        case DW_AT_user_2002:
        case DW_AT_virtuality:
        case DW_AT_containing_type:
        case DW_AT_accessibility:
            // todo;
            break;
        case DW_AT_vtable_elem_location:
            free(attr->block);
            break;
        default:
            fprintf(stderr, "Unknown function attribute %02x\n", attr->name);
            break;
        }
    }

    return;
}

uint8_t* elfParseFunction(uint8_t* data, ELFAbbrev* abbrev, CompileUnit* unit,
    Function** f)
{
    Function* func = (Function*)calloc(1, sizeof(Function));
    *f = func;

    int bytes;
    bool declaration = false;
    for (int i = 0; i < abbrev->numAttrs; i++) {
        ELFAttr* attr = &abbrev->attrs[i];
        data = elfReadAttribute(data, attr);
        switch (attr->name) {
        case DW_AT_sibling:
            break;
        case DW_AT_name:
            if (func->name == NULL)
                func->name = attr->string;
            break;
        case DW_AT_MIPS_linkage_name:
            func->name = attr->string;
            break;
        case DW_AT_low_pc:
            func->lowPC = attr->value;
            break;
        case DW_AT_high_pc:
            func->highPC = attr->value;
            break;
        case DW_AT_prototyped:
            break;
        case DW_AT_decl_file:
            func->file = attr->value;
            break;
        case DW_AT_decl_line:
            func->line = attr->value;
            break;
        case DW_AT_external:
            func->external = attr->flag;
            break;
        case DW_AT_frame_base:
            func->frameBase = attr->block;
            break;
        case DW_AT_type:
            func->returnType = elfParseType(unit, attr->value);
            break;
        case DW_AT_abstract_origin:
            elfGetFunctionAttributes(unit, attr->value, func);
            break;
        case DW_AT_declaration:
            declaration = attr->flag;
            break;
        case DW_AT_inline:
        case DW_AT_specification:
        case DW_AT_artificial:
        case DW_AT_proc_body:
        case DW_AT_save_offset:
        case DW_AT_user_2002:
        case DW_AT_virtuality:
        case DW_AT_containing_type:
        case DW_AT_accessibility:
            // todo;
            break;
        case DW_AT_vtable_elem_location:
            free(attr->block);
            break;
        default:
            fprintf(stderr, "Unknown function attribute %02x\n", attr->name);
            break;
        }
    }

    if (declaration) {
        elfCleanUp(func);
        free(func);
        *f = NULL;

        while (1) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                return data;
            }

            abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);

            data = elfSkipData(data, abbrev, unit->abbrevs);
        }
    }

    if (abbrev->hasChildren) {
        int nesting = 1;
        Object* lastParam = NULL;
        Object* lastVar = NULL;

        while (nesting) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                continue;
            }

            abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);

            switch (abbrev->tag) {
            CASE_TYPE_TAG: // no need to parse types. only parsed when used
            case DW_TAG_label: // not needed
                data = elfSkipData(data, abbrev, unit->abbrevs);
                break;
            case DW_TAG_subprogram: {
                Function* fnc = NULL;
                data = elfParseFunction(data, abbrev, unit, &fnc);
                if (fnc != NULL) {
                    if (unit->lastFunction == NULL)
                        unit->functions = fnc;
                    else
                        unit->lastFunction->next = fnc;
                    unit->lastFunction = fnc;
                }
            } break;
            case DW_TAG_lexical_block: {
                data = elfParseBlock(data, abbrev, unit, func, &lastVar);
            } break;
            case DW_TAG_formal_parameter: {
                Object* o;
                data = elfParseObject(data, abbrev, unit, &o);
                if (func->parameters)
                    lastParam->next = o;
                else
                    func->parameters = o;
                lastParam = o;
            } break;
            case DW_TAG_variable: {
                Object* o;
                data = elfParseObject(data, abbrev, unit, &o);
                if (func->variables)
                    lastVar->next = o;
                else
                    func->variables = o;
                lastVar = o;
            } break;
            case DW_TAG_unspecified_parameters:
            case DW_TAG_inlined_subroutine: {
                // todo
                for (int i = 0; i < abbrev->numAttrs; i++) {
                    data = elfReadAttribute(data, &abbrev->attrs[i]);
                    if (abbrev->attrs[i].form == DW_FORM_block1)
                        free(abbrev->attrs[i].block);
                }

                if (abbrev->hasChildren)
                    nesting++;
            } break;
            default: {
                fprintf(stderr, "Unknown function TAG %02x\n", abbrev->tag);
                data = elfSkipData(data, abbrev, unit->abbrevs);
            } break;
            }
        }
    }
    return data;
}

uint8_t* elfParseUnknownData(uint8_t* data, ELFAbbrev* abbrev, ELFAbbrev** abbrevs)
{
    int i;
    int bytes;
    //  switch(abbrev->tag) {
    //  default:
    fprintf(stderr, "Unknown TAG %02x\n", abbrev->tag);

    for (i = 0; i < abbrev->numAttrs; i++) {
        data = elfReadAttribute(data, &abbrev->attrs[i]);
        if (abbrev->attrs[i].form == DW_FORM_block1)
            free(abbrev->attrs[i].block);
    }

    if (abbrev->hasChildren) {
        int nesting = 1;
        while (nesting) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                continue;
            }

            abbrev = elfGetAbbrev(abbrevs, abbrevNum);

            fprintf(stderr, "Unknown TAG %02x\n", abbrev->tag);

            for (i = 0; i < abbrev->numAttrs; i++) {
                data = elfReadAttribute(data, &abbrev->attrs[i]);
                if (abbrev->attrs[i].form == DW_FORM_block1)
                    free(abbrev->attrs[i].block);
            }

            if (abbrev->hasChildren) {
                nesting++;
            }
        }
    }
    //  }
    return data;
}

uint8_t* elfParseCompileUnitChildren(uint8_t* data, CompileUnit* unit)
{
    int bytes;
    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;
    Object* lastObj = NULL;
    while (abbrevNum) {
        ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);
        switch (abbrev->tag) {
        case DW_TAG_subprogram: {
            Function* func = NULL;
            data = elfParseFunction(data, abbrev, unit, &func);
            if (func != NULL) {
                if (unit->lastFunction)
                    unit->lastFunction->next = func;
                else
                    unit->functions = func;
                unit->lastFunction = func;
            }
        } break;
        CASE_TYPE_TAG:
            data = elfSkipData(data, abbrev, unit->abbrevs);
            break;
        case DW_TAG_variable: {
            Object* var = NULL;
            data = elfParseObject(data, abbrev, unit, &var);
            if (lastObj)
                lastObj->next = var;
            else
                unit->variables = var;
            lastObj = var;
        } break;
        default:
            data = elfParseUnknownData(data, abbrev, unit->abbrevs);
            break;
        }

        abbrevNum = elfReadLEB128(data, &bytes);
        data += bytes;
    }
    return data;
}

CompileUnit* elfParseCompUnit(uint8_t* data, uint8_t* abbrevData)
{
    int bytes;
    uint8_t* top = data;

    uint32_t length = elfRead4Bytes(data);
    data += 4;

    uint16_t version = elfRead2Bytes(data);
    data += 2;

    uint32_t offset = elfRead4Bytes(data);
    data += 4;

    uint8_t addrSize = *data++;

    if (version != 2) {
        fprintf(stderr, "Unsupported debugging information version %d\n", version);
        return NULL;
    }

    if (addrSize != 4) {
        fprintf(stderr, "Unsupported address size %d\n", addrSize);
        return NULL;
    }

    ELFAbbrev** abbrevs = elfReadAbbrevs(abbrevData, offset);

    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;

    ELFAbbrev* abbrev = elfGetAbbrev(abbrevs, abbrevNum);

    CompileUnit* unit = (CompileUnit*)calloc(1, sizeof(CompileUnit));
    unit->top = top;
    unit->length = length;
    unit->abbrevs = abbrevs;
    unit->next = NULL;

    elfCurrentUnit = unit;

    int i;

    for (i = 0; i < abbrev->numAttrs; i++) {
        ELFAttr* attr = &abbrev->attrs[i];
        data = elfReadAttribute(data, attr);

        switch (attr->name) {
        case DW_AT_name:
            unit->name = attr->string;
            break;
        case DW_AT_stmt_list:
            unit->hasLineInfo = true;
            unit->lineInfo = attr->value;
            break;
        case DW_AT_low_pc:
            unit->lowPC = attr->value;
            break;
        case DW_AT_high_pc:
            unit->highPC = attr->value;
            break;
        case DW_AT_compdir:
            unit->compdir = attr->string;
            break;
        // ignore
        case DW_AT_language:
        case DW_AT_producer:
        case DW_AT_macro_info:
        case DW_AT_entry_pc:
            break;
        default:
            fprintf(stderr, "Unknown attribute %02x\n", attr->name);
            break;
        }
    }

    if (abbrev->hasChildren)
        elfParseCompileUnitChildren(data, unit);

    return unit;
}

void elfParseAranges()
{
    ElfSection* sh = elfGetSectionByName(".debug_aranges");
    if (sh == NULL) {
        fprintf(stderr, "No aranges found\n");
        return;
    }

    uint8_t* data = elfReadSection(sh);
    if (data == NULL)
        return;
    uint8_t* end = data + sh->size;

    int max = 4;
    ARanges* ranges = (ARanges*)calloc(max, sizeof(ARanges));
    if (ranges == NULL)
        return;

    int index = 0;

    while (data < end) {
        if ((size_t)(end - data) < 4) {
            fprintf(stderr, "Invalid aranges record length\n");
            break;
        }

        uint32_t _len = elfRead4Bytes(data);
        data += 4;

        if (_len < 20 || _len > (uint32_t)(end - data)) {
            fprintf(stderr, "Invalid aranges record length\n");
            break;
        }

        uint8_t* recordEnd = data + _len;
        data += 2; // version
        uint32_t offset = elfRead4Bytes(data);
        data += 4;
        uint8_t addrSize = *data++;
        uint8_t segSize = *data++;

        // The legacy parser only supports 32-bit, non-segmented addresses.
        if (addrSize != 4 || segSize != 0 || (size_t)(recordEnd - data) < 12) {
            fprintf(stderr, "Unsupported aranges address format\n");
            break;
        }

        // Align the first tuple to the eight-byte address/length pair.
        data += 4;
        size_t tupleCount = (size_t)(recordEnd - data) / 8;
        if (tupleCount == 0 || (size_t)(recordEnd - data) % 8 != 0) {
            fprintf(stderr, "Invalid aranges record length\n");
            break;
        }

        if (index == max) {
            int newMax = max + 4;
            ARanges* newRanges = (ARanges*)realloc(ranges,
                newMax * sizeof(ARanges));
            if (newRanges == NULL)
                break;
            memset(newRanges + max, 0, (newMax - max) * sizeof(ARanges));
            ranges = newRanges;
            max = newMax;
        }

        ranges[index].count = (int)(tupleCount - 1);
        ranges[index].offset = offset;
        ranges[index].ranges = (ARange*)calloc(tupleCount - 1, sizeof(ARange));
        if (tupleCount > 1 && ranges[index].ranges == NULL)
            break;

        bool terminated = false;
        size_t i;
        for (i = 0; i < tupleCount; i++) {
            uint32_t addr = elfRead4Bytes(data);
            data += 4;
            uint32_t len = elfRead4Bytes(data);
            data += 4;

            if (addr == 0 && len == 0) {
                terminated = true;
                break;
            }

            if (i == tupleCount - 1)
                break;

            ranges[index].ranges[i].lowPC = addr;
            ranges[index].ranges[i].highPC = addr + len;
        }

        if (!terminated) {
            fprintf(stderr, "Unterminated aranges record\n");
            break;
        }

        ranges[index].count = (int)i;
        index++;
        data = recordEnd;
    }
    elfDebugInfo->numRanges = index;
    elfDebugInfo->ranges = ranges;
}

void elfReadSymtab()
{
    ELFIO::section* symsec = nullptr;
    for (const auto& s : g_elfReader.sections) {
        if (s->get_type() == ELFIO::SHT_SYMTAB) {
            symsec = s.get();
            break;
        }
    }
    if (symsec == nullptr)
        return;

    ELFIO::symbol_section_accessor syms(g_elfReader, symsec);
    const ELFIO::Elf_Xword total = syms.get_symbols_num();

    elfSymbolsCount = 0;
    elfSymbols = (Symbol*)malloc((size_t)(sizeof(Symbol) * (total > 0 ? total : 1)));

    // Stash the linked string table so lifetime matches elfio's storage; we
    // stage copies into a persistent per-symbol name buffer below.
    ELFIO::section* strsec = nullptr;
    const unsigned link = symsec->get_link();
    if (link < g_elfReader.sections.size())
        strsec = g_elfReader.sections[link];
    elfSymbolsStrTab = strsec ? const_cast<char*>(strsec->get_data()) : NULL;

    // Two passes: first globals/weak (bind != 0), then locals (bind == 0).
    // This preserves ordering semantics the old parser relied on.
    for (int pass = 0; pass < 2; ++pass) {
        for (ELFIO::Elf_Xword i = 0; i < total; ++i) {
            std::string   name;
            ELFIO::Elf64_Addr value = 0;
            ELFIO::Elf_Xword  ssize = 0;
            unsigned char bind = 0, type = 0, other = 0;
            ELFIO::Elf_Half sect_idx = 0;

            if (!syms.get_symbol(i, name, value, ssize, bind, type, sect_idx, other))
                continue;

            const bool wantBound = (pass == 0);
            if ((bind != 0) != wantBound)
                continue;

            Symbol* sym = &elfSymbols[elfSymbolsCount++];
            // Names are stored in the elfio-owned string table; resolve back to
            // the pointer inside that buffer so existing consumers can read
            // through elfSymbolsStrTab.
            if (elfSymbolsStrTab != NULL) {
                // Walk the strtab to find the matching offset. Fallback: dup.
                sym->name = elfSymbolsStrTab;
                // Quick scan: elfio already validated the strtab bounds.
                const char* p = elfSymbolsStrTab;
                const char* stbEnd = p + strsec->get_size();
                while (p < stbEnd) {
                    if (name == p) {
                        sym->name = p;
                        break;
                    }
                    p += strlen(p) + 1;
                }
            } else {
                sym->name = "";
            }
            sym->binding = bind;
            sym->type    = type;
            sym->value   = static_cast<uint32_t>(value);
            sym->size    = static_cast<uint32_t>(ssize);
        }
    }
}

// Canonical Nintendo GBA logo (156 bytes at ROM offset 0x04). ELF images
// emitted by devkitPro-style toolchains leave this zeroed until `gbafix` is
// run during .gba conversion. When loading the ELF directly we patch it in
// ourselves so the real BIOS accepts the cartridge.
static const uint8_t kGbaNintendoLogo[156] = {
    0x24, 0xff, 0xae, 0x51, 0x69, 0x9a, 0xa2, 0x21, 0x3d, 0x84, 0x82, 0x0a,
    0x84, 0xe4, 0x09, 0xad, 0x11, 0x24, 0x8b, 0x98, 0xc0, 0x81, 0x7f, 0x21,
    0xa3, 0x52, 0xbe, 0x19, 0x93, 0x09, 0xce, 0x20, 0x10, 0x46, 0x4a, 0x4a,
    0xf8, 0x27, 0x31, 0xec, 0x58, 0xc7, 0xe8, 0x33, 0x82, 0xe3, 0xce, 0xbf,
    0x85, 0xf4, 0xdf, 0x94, 0xce, 0x4b, 0x09, 0xc1, 0x94, 0x56, 0x8a, 0xc0,
    0x13, 0x72, 0xa7, 0xfc, 0x9f, 0x84, 0x4d, 0x73, 0xa3, 0xca, 0x9a, 0x61,
    0x58, 0x97, 0xa3, 0x27, 0xfc, 0x03, 0x98, 0x76, 0x23, 0x1d, 0xc7, 0x61,
    0x03, 0x04, 0xae, 0x56, 0xbf, 0x38, 0x84, 0x00, 0x40, 0xa7, 0x0e, 0xfd,
    0xff, 0x52, 0xfe, 0x03, 0x6f, 0x95, 0x30, 0xf1, 0x97, 0xfb, 0xc0, 0x85,
    0x60, 0xd6, 0x80, 0x25, 0xa9, 0x63, 0xbe, 0x03, 0x01, 0x4e, 0x38, 0xe2,
    0xf9, 0xa2, 0x34, 0xff, 0xbb, 0x3e, 0x03, 0x44, 0x78, 0x00, 0x90, 0xcb,
    0x88, 0x11, 0x3a, 0x94, 0x65, 0xc0, 0x7c, 0x63, 0x87, 0xf0, 0x3c, 0xaf,
    0xd6, 0x25, 0xe4, 0x8b, 0x38, 0x0a, 0xac, 0x72, 0x21, 0xd4, 0xf8, 0x07
};

// Patch the GBA cartridge header so a real BIOS's Nintendo-logo check passes.
// Mirrors what `gbafix` does when producing a .gba from an .elf: fills in the
// logo, marks the fixed 0x96 byte at 0xB2, and recomputes the 0xBD checksum.
// Only touches bytes the ELF left at zero — preserves any game title / game
// code the linker did place in the image.
static void elfPatchGbaCartridgeHeader()
{
    if (g_rom == NULL)
        return;

    memcpy(&g_rom[0x04], kGbaNintendoLogo, sizeof(kGbaNintendoLogo));

    // Fixed byte marking this as a GBA cartridge; BIOS reads it.
    g_rom[0xB2] = 0x96;

    // Header checksum: chk = (-0x19 - sum(rom[0xA0..0xBC])) & 0xFF, written
    // at offset 0xBD. See GBATEK "Cartridge Header".
    int sum = 0;
    for (unsigned i = 0xA0; i <= 0xBC; ++i)
        sum += g_rom[i];
    g_rom[0xBD] = static_cast<uint8_t>((-(sum + 0x19)) & 0xFF);
}

// Populates g_elfSections from the already-loaded g_elfReader. This is the
// single point at which elfio's validated data is copied into the lightweight
// ElfSection records consumed by the DWARF code.
static void elfBuildSectionTable()
{
    g_elfSections.clear();
    g_elfSections.reserve(g_elfReader.sections.size());
    for (const auto& sec : g_elfReader.sections) {
        ElfSection info;
        info.name  = sec->get_name();
        info.type  = sec->get_type();
        info.flags = static_cast<uint32_t>(sec->get_flags());
        info.addr  = static_cast<uint32_t>(sec->get_address());
        info.size  = static_cast<uint32_t>(sec->get_size());
        info.link  = sec->get_link();
        info.info  = sec->get_info();
        info.bytes = reinterpret_cast<const uint8_t*>(sec->get_data());
        g_elfSections.push_back(std::move(info));
    }
}

bool elfReadProgram(int& size, bool parseDebug)
{
    size = 0;

    if (g_elfReader.get_entry() == 0x2000000)
        coreOptions.cpuIsMultiBoot = true;

    // Load PT_LOAD segments into WRAM (multiboot) or ROM.
    for (const auto& seg : g_elfReader.segments) {
        if (seg->get_type() != ELFIO::PT_LOAD)
            continue;

        const unsigned address      = static_cast<unsigned>(seg->get_physical_address());
        const unsigned section_size = static_cast<unsigned>(seg->get_file_size());
        const char*    source       = seg->get_data();

        if (section_size == 0 || source == nullptr)
            continue;

        if (coreOptions.cpuIsMultiBoot) {
            const unsigned effective_address = address - 0x2000000;
            if (effective_address < SIZE_WRAM
                && effective_address + section_size < SIZE_WRAM) {
                memcpy(&g_workRAM[effective_address], source, section_size);
                size += section_size;
            }
        } else {
            const unsigned effective_address = address - 0x8000000;
            if (effective_address < SIZE_ROM
                && effective_address + section_size < SIZE_ROM) {
                memcpy(&g_rom[effective_address], source, section_size);
                size += section_size;
            }
        }
    }

    // Mirror loadable sections (SHF_ALLOC) into WRAM/ROM — preserves the
    // original behavior where sections overlay segments.
    for (const auto& sec : g_elfReader.sections) {
        if ((sec->get_flags() & ELFIO::SHF_ALLOC) == 0)
            continue;
        if (sec->get_type() == ELFIO::SHT_NOBITS)
            continue;

        const unsigned address      = static_cast<unsigned>(sec->get_address());
        const unsigned section_size = static_cast<unsigned>(sec->get_size());
        const char*    source       = sec->get_data();

        if (section_size == 0 || source == nullptr)
            continue;

        if (coreOptions.cpuIsMultiBoot) {
            if (address >= 0x2000000 && address <= 0x203ffff) {
                memcpy(&g_workRAM[address & 0x3ffff], source, section_size);
                size += section_size;
            }
        } else {
            if (address >= 0x8000000 && address <= 0x9ffffff) {
                memcpy(&g_rom[address & 0x1ffffff], source, section_size);
                size += section_size;
            }
        }
    }

    // After all LOAD segments/sections are copied, patch the GBA cartridge
    // header so a real BIOS accepts the cart. Skip for multiboot images,
    // which aren't subject to the BIOS logo check.
    if (!coreOptions.cpuIsMultiBoot)
        elfPatchGbaCartridgeHeader();

    if (!parseDebug)
        return true;

    fprintf(stderr, "Parsing debug info\n");

    ElfSection* dbgHeader = elfGetSectionByName(".debug_info");
    if (dbgHeader == NULL || dbgHeader->bytes == NULL) {
        fprintf(stderr, "Cannot find debug information\n");
        return true;
    }

    ElfSection* abbrevSec = elfGetSectionByName(".debug_abbrev");
    if (abbrevSec == NULL || abbrevSec->bytes == NULL) {
        fprintf(stderr, "Cannot find abbreviation table\n");
        return true;
    }

    elfDebugInfo = (DebugInfo*)calloc(1, sizeof(DebugInfo));
    uint8_t* abbrevdata = const_cast<uint8_t*>(abbrevSec->bytes);

    ElfSection* strSec = elfGetSectionByName(".debug_str");
    elfDebugStrings = strSec ? reinterpret_cast<char*>(const_cast<uint8_t*>(strSec->bytes)) : NULL;

    uint8_t* debugdata = const_cast<uint8_t*>(dbgHeader->bytes);

    elfDebugInfo->debugdata = debugdata;
    elfDebugInfo->infodata  = debugdata;

    const uint32_t total = dbgHeader->size;
    uint8_t* end_ptr = debugdata + total;
    uint8_t* ddata   = debugdata;

    CompileUnit* last = NULL;
    CompileUnit* unit = NULL;

    while (ddata < end_ptr) {
        unit = elfParseCompUnit(ddata, abbrevdata);
        if (unit == NULL)
            break;
        unit->offset = (uint32_t)(ddata - debugdata);
        elfParseLineInfo(unit);
        if (last == NULL)
            elfCompileUnits = unit;
        else
            last->next = unit;
        last = unit;
        if (unit->length == 0)
            break;
        ddata += 4 + unit->length;
    }
    elfParseAranges();
    CompileUnit* comp = elfCompileUnits;
    while (comp) {
        ARanges* r = elfDebugInfo->ranges;
        for (int i = 0; i < elfDebugInfo->numRanges; i++)
            if (r[i].offset == comp->offset) {
                comp->ranges = &r[i];
                break;
            }
        comp = comp->next;
    }
    elfParseCFA();
    elfReadSymtab();

    return true;
}

bool elfRead(const char* name, int& siz, FILE* f)
{
    // The caller opens the FILE* to pre-check with CPUIsELF; elfio does its
    // own I/O via load(), so close the handle and let it take over.
    if (f) fclose(f);

    if (!g_elfReader.load(name)) {
        systemMessage(0, N_("Not a valid ELF file %s"), name);
        return false;
    }

    // GBA targets are 32-bit little-endian ARM. Enforce those constraints.
    if (g_elfReader.get_class() != ELFIO::ELFCLASS32
        || g_elfReader.get_encoding() != ELFIO::ELFDATA2LSB
        || g_elfReader.get_machine() != ELFIO::EM_ARM) {
        systemMessage(0, N_("Not a valid ELF file %s"), name);
        return false;
    }

    elfBuildSectionTable();

    if (!elfReadProgram(siz, coreOptions.parseDebug))
        return false;

    return true;
}

void elfCleanUp(Object* o)
{
    free(o->location);
}

void elfCleanUp(Function* func)
{
    Object* o = func->parameters;
    while (o) {
        elfCleanUp(o);
        Object* next = o->next;
        free(o);
        o = next;
    }

    o = func->variables;
    while (o) {
        elfCleanUp(o);
        Object* next = o->next;
        free(o);
        o = next;
    }
    free(func->frameBase);
}

void elfCleanUp(ELFAbbrev** abbrevs)
{
    for (int i = 0; i < 121; i++) {
        ELFAbbrev* abbrev = abbrevs[i];

        while (abbrev) {
            free(abbrev->attrs);
            ELFAbbrev* next = abbrev->next;
            free(abbrev);

            abbrev = next;
        }
    }
}

void elfCleanUp(Type* t)
{
    switch (t->type) {
    case TYPE_function:
        if (t->function) {
            Object* o = t->function->args;
            while (o) {
                elfCleanUp(o);
                Object* next = o->next;
                free(o);
                o = next;
            }
            free(t->function);
        }
        break;
    case TYPE_array:
        if (t->array) {
            free(t->array->bounds);
            free(t->array);
        }
        break;
    case TYPE_struct:
    case TYPE_union:
        if (t->structure) {
            for (int i = 0; i < t->structure->memberCount; i++) {
                free(t->structure->members[i].location);
            }
            free(t->structure->members);
            free(t->structure);
        }
        break;
    case TYPE_enum:
        if (t->enumeration) {
            free(t->enumeration->members);
            free(t->enumeration);
        }
        break;
    case TYPE_base:
    case TYPE_pointer:
    case TYPE_void:
    case TYPE_reference:
        break; // nothing to do
    }
}

void elfCleanUp(CompileUnit* comp)
{
    elfCleanUp(comp->abbrevs);
    free(comp->abbrevs);
    Function* func = comp->functions;
    while (func) {
        elfCleanUp(func);
        Function* next = func->next;
        free(func);
        func = next;
    }
    Type* t = comp->types;
    while (t) {
        elfCleanUp(t);
        Type* next = t->next;
        free(t);
        t = next;
    }
    Object* o = comp->variables;
    while (o) {
        elfCleanUp(o);
        Object* next = o->next;
        free(o);
        o = next;
    }
    if (comp->lineInfoTable) {
        free(comp->lineInfoTable->lines);
        free(comp->lineInfoTable->files);
        free(comp->lineInfoTable);
    }
}

void elfCleanUp()
{
    CompileUnit* comp = elfCompileUnits;

    while (comp) {
        elfCleanUp(comp);
        CompileUnit* next = comp->next;
        free(comp);
        comp = next;
    }
    elfCompileUnits = NULL;
    free(elfSymbols);
    elfSymbols = NULL;
    //  free(elfSymbolsStrTab);
    elfSymbolsStrTab = NULL;

    elfDebugStrings = NULL;
    if (elfDebugInfo) {
        int num = elfDebugInfo->numRanges;
        int i;
        for (i = 0; i < num; i++) {
            free(elfDebugInfo->ranges[i].ranges);
        }
        free(elfDebugInfo->ranges);
        free(elfDebugInfo);
        elfDebugInfo = NULL;
    }

    if (elfFdes) {
        if (elfFdeCount) {
            for (int i = 0; i < elfFdeCount; i++)
                free(elfFdes[i]);
        }
        free(elfFdes);

        elfFdes = NULL;
        elfFdeCount = 0;
    }

    ELFcie* cie = elfCies;
    while (cie) {
        ELFcie* next = cie->next;
        free(cie);
        cie = next;
    }
    elfCies = NULL;

    // Drop elfio-owned state; reader's default-constructed instance will be
    // repopulated on the next elfRead().
    g_elfSections.clear();
    g_elfReader = ELFIO::elfio();
}
