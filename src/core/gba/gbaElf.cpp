#include "core/gba/gbaElf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/base/message.h"
#include "core/base/port.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"

#define elfReadMemory(addr) \
    READ32LE((&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

// Global bounds tracking for ELF file data validation
static uint8_t* g_elfFileStart = NULL;
static uint8_t* g_elfFileEnd = NULL;

// Helper function to check if a pointer is within valid ELF file bounds
static inline bool elfIsValidPtr(uint8_t* ptr, size_t size) {
    if (g_elfFileStart == NULL || g_elfFileEnd == NULL)
        return true; // Bounds checking not initialized yet
    if (ptr+0 < g_elfFileStart || ptr+size >= g_elfFileEnd)
        return false;
    return true;
}

// Forward declarations for read functions
uint32_t elfRead4Bytes(uint8_t*);
uint16_t elfRead2Bytes(uint8_t*);

// Safe read wrappers with automatic bounds checking
static inline bool elfSafeRead4Bytes(uint8_t* data, uint32_t* out) {
    if (!elfIsValidPtr(data, 4)) {
        fprintf(stderr, "Error: 4-byte read out of bounds\n");
        return false;
    }
    *out = elfRead4Bytes(data);
    return true;
}

static inline bool elfSafeRead2Bytes(uint8_t* data, uint16_t* out) {
    if (!elfIsValidPtr(data, 2)) {
        fprintf(stderr, "Error: 2-byte read out of bounds\n");
        return false;
    }
    *out = elfRead2Bytes(data);
    return true;
}

static inline bool elfSafeRead1Byte(uint8_t* data, uint8_t* out) {
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: 1-byte read out of bounds\n");
        return false;
    }
    *out = *data;
    return true;
}

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

ELFSectionHeader** elfSectionHeaders = NULL;
char* elfSectionHeadersStringTable = NULL;
int elfSectionHeadersCount = 0;
uint8_t* elfFileData = NULL;

CompileUnit* elfCompileUnits = NULL;
DebugInfo* elfDebugInfo = NULL;
char* elfDebugStrings = NULL;

ELFcie* elfCies = NULL;
ELFfde** elfFdes = NULL;
int elfFdeCount = 0;

CompileUnit* elfCurrentUnit = NULL;

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
    if (elfFdes == NULL || elfFdeCount == 0) {
        return NULL;
    }
    
    // Validate address is reasonable (within GBA address space)
    if (address < 0x08000000 && address >= 0x10000000) {
        fprintf(stderr, "Warning: FDE lookup for suspicious address: 0x%08x\n", address);
    }
    
    if (elfFdes) {
        int i;
        for (i = 0; i < elfFdeCount; i++) {
            if (elfFdes[i] == NULL) {
                fprintf(stderr, "Warning: NULL FDE at index %d\n", i);
                continue;
            }
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
    if (state == NULL) {
        fprintf(stderr, "Error: NULL state in CFA execution\n");
        return;
    }
    
    if (data == NULL || len == 0) {
        fprintf(stderr, "Error: Invalid CFA instruction data\n");
        return;
    }
    
    if (!elfIsValidPtr(data, len)) {
        fprintf(stderr, "Error: CFA instruction data out of bounds\n");
        return;
    }
    
    uint8_t* end = data + len;
    int _bytes;
    int _reg;
    ELFFrameStateRegisters* fs;
    
    static const int MAX_INSTRUCTIONS = 10000; // Prevent infinite loops
    int instruction_count = 0;

    while (data < end && state->pc < pc && instruction_count < MAX_INSTRUCTIONS) {
        if (!elfIsValidPtr(data, 1)) {
            fprintf(stderr, "Error: CFA instruction pointer out of bounds\n");
            break;
        }
        
        uint8_t op = *data++;
        instruction_count++;

        switch (op >> 6) {
        case DW_CFA_advance_loc:
            state->pc += (op & 0x3f) * state->codeAlign;
            break;
        case DW_CFA_offset:
            _reg = op & 0x3f;
            if (_reg >= 16) {
                fprintf(stderr, "Error: CFA register index %d out of range\n", _reg);
                return;
            }
            state->registers.regs[_reg].mode = REG_OFFSET;
            state->registers.regs[_reg].offset = state->dataAlign * (int32_t)elfReadLEB128(data, &_bytes);
            data += _bytes;
            break;
        case DW_CFA_restore:
            _reg = op & 0x3f;
            if (_reg >= 16) {
                fprintf(stderr, "Error: CFA register index %d out of range\n", _reg);
                return;
            }
            // we don't care much about the other possible settings,
            // so just setting to unset is enough for now
            state->registers.regs[_reg].mode = REG_NOT_SET;
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
                if (_reg >= 16) {
                    fprintf(stderr, "Error: CFA register index %d out of range\n", _reg);
                    return;
                }
                state->registers.regs[_reg].mode = REG_OFFSET;
                state->registers.regs[_reg].offset = state->dataAlign * (int32_t)elfReadLEB128(data, &_bytes);
                data += _bytes;
                break;
            case DW_CFA_restore_extended:
            case DW_CFA_undefined:
            case DW_CFA_same_value:
                _reg = elfReadLEB128(data, &_bytes);
                data += _bytes;
                if (_reg >= 16) {
                    fprintf(stderr, "Error: CFA register index %d out of range\n", _reg);
                    return;
                }
                state->registers.regs[_reg].mode = REG_NOT_SET;
                break;
            case DW_CFA_register:
                _reg = elfReadLEB128(data, &_bytes);
                data += _bytes;
                if (_reg >= 16) {
                    fprintf(stderr, "Error: CFA register index %d out of range\n", _reg);
                    return;
                }
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
    
    if (instruction_count >= MAX_INSTRUCTIONS) {
        fprintf(stderr, "Warning: CFA instruction limit reached, possible infinite loop\n");
    }
}

ELFFrameState* elfGetFrameState(ELFfde* fde, uint32_t address)
{
    if (fde == NULL) {
        fprintf(stderr, "Error: NULL FDE in elfGetFrameState\n");
        return NULL;
    }
    
    if (fde->cie == NULL) {
        fprintf(stderr, "Error: NULL CIE in FDE\n");
        return NULL;
    }
    
    // Validate address is within FDE range
    if (address < fde->address || address >= fde->end) {
        fprintf(stderr, "Warning: Address 0x%08x outside FDE range [0x%08x, 0x%08x)\n",
                       address, fde->address, fde->end);
    }
    
    ELFFrameState* state = (ELFFrameState*)calloc(1, sizeof(ELFFrameState));
    if (state == NULL) {
        fprintf(stderr, "Error: Failed to allocate frame state\n");
        return NULL;
    }
    
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
    
    // Validate initial address
    if (address == 0) {
        printf("Error: NULL address in call chain\n");
        return;
    }

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
        
        // Validate frame base block
        if (b->data == NULL || b->length == 0) {
            fprintf(stderr, "Error: Invalid frame base block\n");
            *type = LOCATION_memory;
            return 0;
        }
        
        if (!elfIsValidPtr(b->data, b->length)) {
            fprintf(stderr, "Error: Frame base data out of bounds\n");
            *type = LOCATION_memory;
            return 0;
        }
        
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
        // Validate location block
        if (loc->data == NULL) {
            fprintf(stderr, "Error: NULL location data\n");
            *type = LOCATION_memory;
            return 0;
        }
        
        if (!elfIsValidPtr(loc->data, loc->length)) {
            fprintf(stderr, "Error: Location data out of bounds\n");
            *type = LOCATION_memory;
            return 0;
        }
        
        switch (*loc->data) {
        case DW_OP_addr:
            if (loc->length < 5) {
                fprintf(stderr, "Error: DW_OP_addr block too small\n");
                *type = LOCATION_memory;
                return 0;
            }
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
            if (loc->length < 2) {
                fprintf(stderr, "Error: DW_OP_fbreg block too small\n");
                *type = LOCATION_memory;
                return 0;
            }
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
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: String read out of bounds\n");
        *bytesRead = 0;
        return NULL;
    }
    if (*data == 0) {
        *bytesRead = 1;
        return NULL;
    }
    // Scan for null terminator within bounds (max 4KB for safety)
    uint8_t* ptr = data;
    int maxLen = 4096;
    while (maxLen-- > 0 && elfIsValidPtr(ptr, 1) && *ptr != 0) {
        ptr++;
    }
    if (!elfIsValidPtr(ptr, 1) || *ptr != 0) {
        fprintf(stderr, "Error: Unterminated or too long string in ELF data\n");
        *bytesRead = 0;
        return NULL;
    }
    *bytesRead = (int)(ptr - data) + 1;
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
        // Prevent infinite loop on malformed data - LEB128 should not exceed 5 bytes for 32-bit values
        if (count > 5) {
            fprintf(stderr, "Error: Invalid LEB128 encoding (too many bytes)\n");
            *bytesRead = count;
            return 0;
        }
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
        // Prevent infinite loop on malformed data - LEB128 should not exceed 5 bytes for 32-bit values
        if (count > 5) {
            fprintf(stderr, "Error: Invalid LEB128 encoding (too many bytes)\n");
            *bytesRead = count;
            return 0;
        }
    } while (byte & 0x80);
    *bytesRead = count;
    return result;
}

uint8_t* elfReadSection(uint8_t* data, ELFSectionHeader* sh)
{
    uint32_t offset = READ32LE(&sh->offset);
    uint32_t size = READ32LE(&sh->size);
    
    // Validate section is within file bounds
    if (!elfIsValidPtr(data + offset, size)) {
        fprintf(stderr, "Error: Section at offset %u with size %u is out of bounds\n", offset, size);
        return NULL;
    }
    
    return data + offset;
}

ELFSectionHeader* elfGetSectionByName(const char* name)
{
    if (elfSectionHeadersStringTable == NULL)
        return NULL;
    
    if (name == NULL || strlen(name) == 0)
        return NULL;

    for (int i = 0; i < elfSectionHeadersCount; i++) {
        uint32_t nameOffset = READ32LE(&elfSectionHeaders[i]->name);
        
        // Validate name offset is reasonable (max 64KB string table)
        if (nameOffset > 65536) {
            fprintf(stderr, "Warning: Section name offset too large: %u\n", nameOffset);
            continue;
        }
        
        const char* sectionName = &elfSectionHeadersStringTable[nameOffset];
        
        // Validate section name pointer is within bounds
        if (!elfIsValidPtr((uint8_t*)sectionName, 1)) {
            fprintf(stderr, "Warning: Section name pointer out of bounds\n");
            continue;
        }
        
        if (strcmp(name, sectionName) == 0) {
            return elfSectionHeaders[i];
        }
    }
    return NULL;
}

ELFSectionHeader* elfGetSectionByNumber(int number)
{
    if (number < 0) {
        fprintf(stderr, "Error: Negative section number: %d\n", number);
        return NULL;
    }
    
    if (number < elfSectionHeadersCount) {
        return elfSectionHeaders[number];
    }
    
    fprintf(stderr, "Error: Section number %d exceeds count %d\n", number, elfSectionHeadersCount);
    return NULL;
}

CompileUnit* elfGetCompileUnitForData(uint8_t* data)
{
    // Validate data pointer is within file bounds
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: Data pointer out of bounds in compile unit lookup\n");
        return NULL;
    }
    
    // Check if elfDebugInfo is initialized
    if (elfDebugInfo == NULL || elfDebugInfo->infodata == NULL) {
        fprintf(stderr, "Error: Debug info not initialized\n");
        return NULL;
    }
    
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

    fprintf(stderr, "Error: cannot find reference to compile unit at offset %08x\n",
        (int)(data - elfDebugInfo->infodata));
    return NULL; // Return NULL instead of exiting
}

uint8_t* elfReadAttribute(uint8_t* data, ELFAttr* attr)
{
    int bytes;
    int form = attr->form;
    int indirectCount = 0;
start:
    // Prevent infinite indirect loops on malformed data
    if (indirectCount++ > 10) {
        fprintf(stderr, "Error: Too many indirect form redirections\n");
        return data;
    }
    
    // Basic bounds check using global file bounds
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: Attribute read out of bounds\n");
        return data;
    }
    
    switch (form) {
    case DW_FORM_addr:
        if (!elfIsValidPtr(data, 4)) {
            fprintf(stderr, "Error: DW_FORM_addr read out of bounds\n");
            return data;
        }
        attr->value = elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_data2:
        if (!elfIsValidPtr(data, 2)) {
            fprintf(stderr, "Error: DW_FORM_data2 read out of bounds\n");
            return data;
        }
        attr->value = elfRead2Bytes(data);
        data += 2;
        break;
    case DW_FORM_data4:
        if (!elfIsValidPtr(data, 4)) {
            fprintf(stderr, "Error: DW_FORM_data4 read out of bounds\n");
            return data;
        }
        attr->value = elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_string:
        attr->string = (char*)data;
        // Find null terminator within bounds
        while (elfIsValidPtr(data, 1) && *data != 0) {
            data++;
        }
        if (!elfIsValidPtr(data, 1)) {
            fprintf(stderr, "Error: Unterminated string in DW_FORM_string\n");
            return data;
        }
        data++; // Skip null terminator
        break;
    case DW_FORM_strp:
        if (!elfIsValidPtr(data, 4)) {
            fprintf(stderr, "Error: DW_FORM_strp read out of bounds\n");
            return data;
        }
        attr->string = elfDebugStrings + elfRead4Bytes(data);
        data += 4;
        break;
    case DW_FORM_block:
        attr->block = (ELFBlock*)malloc(sizeof(ELFBlock));
        if (attr->block == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for block\n");
            return data;
        }
        attr->block->length = elfReadLEB128(data, &bytes);
        data += bytes;
        if (!elfIsValidPtr(data, attr->block->length)) {
            fprintf(stderr, "Error: DW_FORM_block data exceeds bounds\n");
            free(attr->block);
            attr->block = NULL;
            return data;
        }
        attr->block->data = data;
        data += attr->block->length;
        break;
    case DW_FORM_block1:
        attr->block = (ELFBlock*)malloc(sizeof(ELFBlock));
        if (attr->block == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for block1\n");
            return data;
        }
        attr->block->length = *data++;
        if (!elfIsValidPtr(data, attr->block->length)) {
            fprintf(stderr, "Error: DW_FORM_block1 data exceeds bounds\n");
            free(attr->block);
            attr->block = NULL;
            return data;
        }
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
        if (!elfIsValidPtr(data, 4)) {
            fprintf(stderr, "Error: DW_FORM_ref_addr read out of bounds\n");
            return data;
        }
        attr->value = (uint32_t)((elfDebugInfo->infodata + elfRead4Bytes(data)) - elfGetCompileUnitForData(data)->top);
        data += 4;
        break;
    case DW_FORM_ref4:
        if (!elfIsValidPtr(data, 4)) {
            fprintf(stderr, "Error: DW_FORM_ref4 read out of bounds\n");
            return data;
        }
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
        fprintf(stderr, "Unsupported FORM %02x\n", form);
        exit(-1);
    }
    return data;
}

ELFAbbrev* elfGetAbbrev(ELFAbbrev** table, uint32_t number)
{
    // Validate abbreviation number is reasonable
    if (number == 0 || number > 1000000) {
        if (number != 0) // 0 is used as terminator, so don't warn
            fprintf(stderr, "Warning: Suspicious abbreviation number: %u\n", number);
        return NULL;
    }
    
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
            // Limit attributes per abbreviation to 1000 to prevent excessive memory use
            if (abbrev->numAttrs >= 1000) {
                fprintf(stderr, "Too many attributes in abbreviation (max 1000)\n");
                break;
            }
            
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

        if (elfGetAbbrev(abbrevs, number) != NULL) {
            // Duplicate abbreviation number detected - this could indicate corruption
            fprintf(stderr, "Warning: Duplicate abbreviation number %u detected\n", number);
            break;
        }
    }

    return abbrevs;
}

void elfParseCFA(uint8_t* top)
{
    ELFSectionHeader* h = elfGetSectionByName(".debug_frame");

    if (h == NULL) {
        return;
    }

    uint8_t* data = elfReadSection(top, h);
    if (data == NULL) {
        fprintf(stderr, "Failed to read debug frame section\n");
        return;
    }

    uint8_t* topOffset = data;
    
    uint32_t size = READ32LE(&h->size);
    // Sanity check: frame data should not exceed 10MB
    if (size > 10 * 1024 * 1024) {
        fprintf(stderr, "Debug frame section too large: %u bytes\n", size);
        return;
    }

    uint8_t* end = data + size;
    if (!elfIsValidPtr(end, 0)) {
        fprintf(stderr, "Debug frame section exceeds file bounds\n");
        return;
    }

    ELFcie* cies = NULL;
    int cieCount = 0;
    const int MAX_CIES = 10000;
    const int MAX_FDES = 100000;

    while (data < end) {
        if (!elfIsValidPtr(data, 8)) {
            fprintf(stderr, "CFA entry header out of bounds\n");
            break;
        }
        
        uint32_t offset = (uint32_t)(data - topOffset);
        uint32_t len = elfRead4Bytes(data);
        
        // Validate length is reasonable
        if (len > 1024 * 1024) {
            fprintf(stderr, "CFA entry too large: %u bytes\n", len);
            break;
        }
        
        // Ensure we won't read past end
        if (data + 4 + len > end) {
            fprintf(stderr, "CFA entry extends beyond section\n");
            break;
        }
        data += 4;

        uint8_t* dataEnd = data + len;

        uint32_t id = elfRead4Bytes(data);
        data += 4;

        if (id == 0xffffffff) {
            // Limit number of CIEs
            if (cieCount >= MAX_CIES) {
                fprintf(stderr, "Too many CIEs in debug frame (max %d)\n", MAX_CIES);
                break;
            }
            cieCount++;
            
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
                fprintf(stderr, "Error: augmentation not supported\n");
                exit(-1);
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
            // Limit number of FDEs
            if (elfFdeCount >= MAX_FDES) {
                fprintf(stderr, "Too many FDEs in debug frame (max %d)\n", MAX_FDES);
                break;
            }
            
            ELFfde* fde = (ELFfde*)calloc(1, sizeof(ELFfde));

            ELFcie* cie = cies;

            while (cie != NULL) {
                if (cie->offset == id)
                    break;
                cie = cie->next;
            }

            if (!cie) {
                fprintf(stderr, "Cannot find CIE %08x\n", id);
                exit(-1);
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
    // Validate file index
    if (file < 1 || file > l->fileCount) {
        fprintf(stderr, "Invalid file index %d (max %d)\n", file, l->fileCount);
        return;
    }
    
    // Prevent excessive line table growth (max 1 million lines)
    if (l->number >= 1000000) {
        fprintf(stderr, "Line table too large, ignoring additional entries\n");
        return;
    }
    
    if (l->number == *max) {
        int newMax = *max + 1000;
        // Cap maximum at 1 million lines
        if (newMax > 1000000) newMax = 1000000;
        
        // Check for multiplication overflow
        size_t itemSize = sizeof(LineInfoItem);
        if (newMax > 0 && SIZE_MAX / itemSize < (size_t)newMax) {
            fprintf(stderr, "Error: Line info allocation would overflow\n");
            return;
        }
        
        LineInfoItem* newLines = (LineInfoItem*)realloc(l->lines, newMax * itemSize);
        if (newLines == NULL) {
            fprintf(stderr, "Failed to allocate memory for line info\n");
            return;
        }
        l->lines = newLines;
        *max = newMax;
    }
    LineInfoItem* li = &l->lines[l->number];
    li->file = l->files[file - 1];
    li->address = a;
    li->line = line;
    l->number++;
}

void elfParseLineInfo(CompileUnit* unit, uint8_t* top)
{
    ELFSectionHeader* h = elfGetSectionByName(".debug_line");
    if (h == NULL) {
        fprintf(stderr, "No line information found\n");
        return;
    }
    LineInfo* l = unit->lineInfoTable = (LineInfo*)calloc(1, sizeof(LineInfo));
    l->number = 0;
    int max = 1000;
    l->lines = (LineInfoItem*)malloc(1000 * sizeof(LineInfoItem));

    uint8_t* data = elfReadSection(top, h);
    if (data == NULL) {
        fprintf(stderr, "Failed to read line info section\n");
        free(l->lines);
        free(l);
        unit->lineInfoTable = NULL;
        return;
    }
    
    // Validate unit->lineInfo offset
    if (!elfIsValidPtr(data + unit->lineInfo, 4)) {
        fprintf(stderr, "Line info offset out of bounds\n");
        free(l->lines);
        free(l);
        unit->lineInfoTable = NULL;
        return;
    }
    
    data += unit->lineInfo;
    uint32_t totalLen = elfRead4Bytes(data);
    
    // Sanity check: line info should not exceed 10MB
    if (totalLen > 10 * 1024 * 1024) {
        fprintf(stderr, "Line info section too large: %u bytes\n", totalLen);
        free(l->lines);
        free(l);
        unit->lineInfoTable = NULL;
        return;
    }
    
    data += 4;
    uint8_t* end = data + totalLen;
    
    // Validate end pointer
    if (!elfIsValidPtr(end, 0)) {
        fprintf(stderr, "Line info section exceeds file bounds\n");
        free(l->lines);
        free(l);
        unit->lineInfoTable = NULL;
        return;
    }
    //  uint16_t version = elfRead2Bytes(data);
    data += 2;
    //  uint32_t offset = elfRead4Bytes(data);
    data += 4;
    int minInstrSize = *data++;
    int defaultIsStmt = *data++;
    int lineBase = (int8_t)*data++;
    int lineRange = *data++;
    int opcodeBase = *data++;
    
    // Validate opcodeBase to prevent huge allocation
    if (opcodeBase < 0 || opcodeBase > 256) {
        fprintf(stderr, "Invalid opcodeBase: %d\n", opcodeBase);
        free(l->lines);
        free(l);
        unit->lineInfoTable = NULL;
        return;
    }
    
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

    // Maximum 10000 files to prevent excessive memory allocation
    const int MAX_FILES = 10000;
    
    while ((s = elfReadString(data, &bytes)) != NULL) {
        if (index >= MAX_FILES) {
            fprintf(stderr, "Too many files in line info (max %d)\n", MAX_FILES);
            break;
        }
        
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
            if (count > MAX_FILES) count = MAX_FILES;
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
                    fprintf(stderr, "Unknown extended LINE opcode %02x\n", op);
                    exit(-1);
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
        int depth = 0;
        const int MAX_NESTING = 100; // Prevent excessive stack depth
        
        while (nesting && depth < MAX_NESTING) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                depth--;
                continue;
            }

            abbrev = elfGetAbbrev(abbrevs, abbrevNum);
            if (abbrev == NULL) {
                fprintf(stderr, "Invalid abbreviation number in skip\n");
                break;
            }

            for (i = 0; i < abbrev->numAttrs; i++) {
                data = elfReadAttribute(data, &abbrev->attrs[i]);
                if (abbrev->attrs[i].form == DW_FORM_block1)
                    free(abbrev->attrs[i].block);
            }

            if (abbrev->hasChildren) {
                nesting++;
                depth++;
            }
        }
        
        if (depth >= MAX_NESTING) {
            fprintf(stderr, "Maximum nesting depth exceeded in skip\n");
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
                    // Limit maximum struct members to 10000 to prevent huge allocations
                    if (index >= 10000) {
                        fprintf(stderr, "Struct has too many members (max 10000)\n");
                        data = elfSkipData(data, abbr, unit->abbrevs);
                        break;
                    }
                    
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
            const int MAX_ENUM_MEMBERS = 100000;
            
            while (num) {
                ELFAbbrev* abbr = elfGetAbbrev(unit->abbrevs, num);
                if (abbr == NULL) {
                    fprintf(stderr, "Invalid abbreviation in enum\n");
                    break;
                }

                switch (abbr->tag) {
                case DW_TAG_enumerator: {
                    if (count >= MAX_ENUM_MEMBERS) {
                        fprintf(stderr, "Too many enum members (max %d)\n", MAX_ENUM_MEMBERS);
                        data = elfSkipData(data, abbr, unit->abbrevs);
                        break;
                    }
                    
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
            const int MAX_ARRAY_DIMENSIONS = 10; // Reasonable limit for array dimensions
            
            while (num) {
                ELFAbbrev* abbr = elfGetAbbrev(unit->abbrevs, num);
                if (abbr == NULL) {
                    fprintf(stderr, "Invalid abbreviation in array\n");
                    break;
                }

                switch (abbr->tag) {
                case DW_TAG_subrange_type: {
                    if (index >= MAX_ARRAY_DIMENSIONS) {
                        fprintf(stderr, "Too many array dimensions (max %d)\n", MAX_ARRAY_DIMENSIONS);
                        data = elfSkipData(data, abbr, unit->abbrevs);
                        break;
                    }
                    
                    if (maxBounds == index) {
                        maxBounds += 4;
                        if (maxBounds > MAX_ARRAY_DIMENSIONS) maxBounds = MAX_ARRAY_DIMENSIONS;
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
        fprintf(stderr, "Unknown type TAG %02x\n", abbrev->tag);
        exit(-1);
    }
}

Type* elfParseType(CompileUnit* unit, uint32_t offset)
{
    // Prevent parsing the same type recursively (circular reference)
    static int parseDepth = 0;
    const int MAX_TYPE_DEPTH = 50;
    
    if (parseDepth >= MAX_TYPE_DEPTH) {
        fprintf(stderr, "Error: Maximum type parsing depth exceeded (circular reference?)\n");
        return NULL;
    }
    
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
    
    // Validate offset is within compile unit bounds
    if (offset > unit->length) {
        fprintf(stderr, "Error: Type offset %u exceeds compile unit length %u\n", offset, unit->length);
        return NULL;
    }
    
    uint8_t* data = unit->top + offset;
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: Type data at offset %u is out of bounds\n", offset);
        return NULL;
    }
    
    int bytes;
    int abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;
    Type* type = NULL;

    ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);
    if (abbrev == NULL) {
        fprintf(stderr, "Error: Invalid abbreviation %d for type\n", abbrevNum);
        return NULL;
    }

    parseDepth++;
    elfParseType(data, offset, abbrev, unit, &type);
    parseDepth--;
    
    return type;
}

void elfGetObjectAttributes(CompileUnit* unit, uint32_t offset, Object* o)
{
    // Validate offset is within compile unit
    if (offset > unit->length) {
        fprintf(stderr, "Error: Object attribute offset %u exceeds unit length %u\n", offset, unit->length);
        return;
    }
    
    uint8_t* data = unit->top + offset;
    
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: Object attribute data out of bounds\n");
        return;
    }
    
    int bytes;
    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;

    if (!abbrevNum) {
        return;
    }

    ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);
    if (abbrev == NULL) {
        fprintf(stderr, "Error: Invalid abbreviation in object attributes\n");
        return;
    }

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
        int depth = 0;
        const int MAX_BLOCK_NESTING = 50;

        while (nesting && depth < MAX_BLOCK_NESTING) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                depth--;
                continue;
            }

            abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);
            if (abbrev == NULL) {
                fprintf(stderr, "Invalid abbreviation in block\n");
                break;
            }

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
            
            if (abbrev->hasChildren) {
                nesting++;
                depth++;
            }
        }
        
        if (depth >= MAX_BLOCK_NESTING) {
            fprintf(stderr, "Maximum block nesting depth exceeded\n");
        }
    }
    return data;
}

void elfGetFunctionAttributes(CompileUnit* unit, uint32_t offset, Function* func)
{
    // Validate offset is within compile unit
    if (offset > unit->length) {
        fprintf(stderr, "Error: Function attribute offset %u exceeds unit length %u\n", offset, unit->length);
        return;
    }
    
    uint8_t* data = unit->top + offset;
    
    if (!elfIsValidPtr(data, 1)) {
        fprintf(stderr, "Error: Function attribute data out of bounds\n");
        return;
    }
    
    int bytes;
    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;

    if (!abbrevNum) {
        return;
    }

    ELFAbbrev* abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);
    if (abbrev == NULL) {
        fprintf(stderr, "Error: Invalid abbreviation in function attributes\n");
        return;
    }

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
        int depth = 0;
        const int MAX_FUNCTION_NESTING = 50;
        Object* lastParam = NULL;
        Object* lastVar = NULL;

        while (nesting && depth < MAX_FUNCTION_NESTING) {
            uint32_t abbrevNum = elfReadLEB128(data, &bytes);
            data += bytes;

            if (!abbrevNum) {
                nesting--;
                depth--;
                continue;
            }

            abbrev = elfGetAbbrev(unit->abbrevs, abbrevNum);
            if (abbrev == NULL) {
                fprintf(stderr, "Invalid abbreviation in function\n");
                break;
            }

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

                if (abbrev->hasChildren) {
                    nesting++;
                    depth++;
                }
            } break;
            default: {
                fprintf(stderr, "Unknown function TAG %02x\n", abbrev->tag);
                data = elfSkipData(data, abbrev, unit->abbrevs);
            } break;
            }
        }
        
        if (depth >= MAX_FUNCTION_NESTING) {
            fprintf(stderr, "Maximum function nesting depth exceeded\n");
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
    
    // Validate we can read the header
    if (!elfIsValidPtr(data, 11)) {
        fprintf(stderr, "Compile unit header out of bounds\n");
        return NULL;
    }

    uint32_t length = elfRead4Bytes(data);
    
    // Sanity check: compile unit should not exceed 100MB
    if (length > 100 * 1024 * 1024) {
        fprintf(stderr, "Compile unit too large: %u bytes\n", length);
        return NULL;
    }
    
    // Validate the unit data is within bounds
    if (!elfIsValidPtr(data + 4, length)) {
        fprintf(stderr, "Compile unit extends beyond file bounds\n");
        return NULL;
    }
    
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
    if (abbrevs == NULL) {
        fprintf(stderr, "Failed to read abbreviations\n");
        return NULL;
    }

    uint32_t abbrevNum = elfReadLEB128(data, &bytes);
    data += bytes;

    ELFAbbrev* abbrev = elfGetAbbrev(abbrevs, abbrevNum);
    if (abbrev == NULL) {
        fprintf(stderr, "Invalid abbreviation number %u\n", abbrevNum);
        // Clean up abbreviations
        for (int i = 0; i < 121; i++) {
            ELFAbbrev* ab = abbrevs[i];
            while (ab) {
                free(ab->attrs);
                ELFAbbrev* next = ab->next;
                free(ab);
                ab = next;
            }
        }
        free(abbrevs);
        return NULL;
    }

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

void elfParseAranges(uint8_t* data)
{
    ELFSectionHeader* sh = elfGetSectionByName(".debug_aranges");
    if (sh == NULL) {
        fprintf(stderr, "No aranges found\n");
        return;
    }

    data = elfReadSection(data, sh);
    if (data == NULL) {
        fprintf(stderr, "Failed to read aranges section\n");
        return;
    }
    
    uint32_t size = READ32LE(&sh->size);
    // Sanity check: aranges section should not exceed 1MB
    if (size > 1024 * 1024) {
        fprintf(stderr, "Aranges section too large: %u bytes\n", size);
        return;
    }
    
    uint8_t* end = data + size;
    if (!elfIsValidPtr(end, 0)) {
        fprintf(stderr, "Aranges section exceeds file bounds\n");
        return;
    }

    int max = 4;
    ARanges* ranges = (ARanges*)calloc(4, sizeof(ARanges));

    int index = 0;
    const int MAX_ARANGES = 10000;

    while (data < end && index < MAX_ARANGES) {
        if (!elfIsValidPtr(data, 20)) {
            fprintf(stderr, "Arange entry extends beyond section\n");
            break;
        }
        
        uint32_t _len = elfRead4Bytes(data);
        
        // Validate length is reasonable (max 1MB per entry)
        if (_len > 1024 * 1024 || _len < 20) {
            fprintf(stderr, "Invalid arange entry length: %u\n", _len);
            break;
        }
        
        data += 4;
        //    uint16_t version = elfRead2Bytes(data);
        data += 2;
        uint32_t offset = elfRead4Bytes(data);
        data += 4;
        //    uint8_t addrSize = *data++;
        //    uint8_t segSize = *data++;
        data += 2; // remove if uncommenting above
        data += 4;
        
        uint32_t rangeCount = (_len - 20) / 8;
        // Sanity check: max 100000 ranges per entry
        if (rangeCount > 100000) {
            fprintf(stderr, "Too many ranges in arange entry: %u\n", rangeCount);
            break;
        }
        
        ranges[index].count = rangeCount;
        ranges[index].offset = offset;
        ranges[index].ranges = (ARange*)calloc(rangeCount, sizeof(ARange));
        int i = 0;
        while (i < (int)rangeCount) {
            if (!elfIsValidPtr(data, 8)) {
                fprintf(stderr, "Range data extends beyond bounds\n");
                break;
            }
            
            uint32_t addr = elfRead4Bytes(data);
            data += 4;
            uint32_t len = elfRead4Bytes(data);
            data += 4;
            if (addr == 0 && len == 0)
                break;
                
            // Validate address range doesn't overflow
            if (len > 0 && addr > UINT32_MAX - len) {
                fprintf(stderr, "Address range overflow detected\n");
                break;
            }
            
            ranges[index].ranges[i].lowPC = addr;
            ranges[index].ranges[i].highPC = addr + len;
            i++;
        }
        index++;
        if (index == max) {
            max += 4;
            ranges = (ARanges*)realloc(ranges, max * sizeof(ARanges));
        }
    }
    elfDebugInfo->numRanges = index;
    elfDebugInfo->ranges = ranges;
}

void elfReadSymtab(uint8_t* data)
{
    ELFSectionHeader* sh = elfGetSectionByName(".symtab");
    if (sh == NULL) {
        fprintf(stderr, "No symbol table found\n");
        return;
    }
    
    int table = READ32LE(&sh->link);
    
    // Validate link index
    if (table >= elfSectionHeadersCount) {
        fprintf(stderr, "Invalid string table index in symtab: %d\n", table);
        return;
    }

    char* strtable = (char*)elfReadSection(data, elfGetSectionByNumber(table));
    if (strtable == NULL) {
        fprintf(stderr, "Failed to read symbol string table\n");
        return;
    }

    ELFSymbol* symtab = (ELFSymbol*)elfReadSection(data, sh);
    if (symtab == NULL) {
        fprintf(stderr, "Failed to read symbol table\n");
        return;
    }

    uint32_t size = READ32LE(&sh->size);
    int count = size / sizeof(ELFSymbol);
    
    // Sanity check: max 1 million symbols
    if (count > 1000000) {
        fprintf(stderr, "Symbol table too large: %d symbols\n", count);
        return;
    }
    
    elfSymbolsCount = 0;

    elfSymbols = (Symbol*)malloc(sizeof(Symbol) * count);

    int i;

    for (i = 0; i < count; i++) {
        ELFSymbol* s = &symtab[i];
        int type = s->info & 15;
        int binding = s->info >> 4;

        if (binding) {
            Symbol* sym = &elfSymbols[elfSymbolsCount];
            sym->name = &strtable[READ32LE(&s->name)];
            sym->binding = binding;
            sym->type = type;
            sym->value = READ32LE(&s->value);
            sym->size = READ32LE(&s->size);
            elfSymbolsCount++;
        }
    }
    for (i = 0; i < count; i++) {
        ELFSymbol* s = &symtab[i];
        int bind = s->info >> 4;
        int type = s->info & 15;

        if (!bind) {
            Symbol* sym = &elfSymbols[elfSymbolsCount];
            sym->name = &strtable[READ32LE(&s->name)];
            sym->binding = (s->info >> 4);
            sym->type = type;
            sym->value = READ32LE(&s->value);
            sym->size = READ32LE(&s->size);
            elfSymbolsCount++;
        }
    }
    elfSymbolsStrTab = strtable;
    //  free(symtab);
}

bool elfReadProgram(ELFHeader* eh, uint8_t* data, unsigned long data_size, int& size, bool parseDebug)
{
    int count = READ16LE(&eh->e_phnum);
    int _i;

    if (READ32LE(&eh->e_entry) == 0x2000000)
        coreOptions.cpuIsMultiBoot = true;

    // Validate program header offset and count
    uint32_t phoff = READ32LE(&eh->e_phoff);
    uint16_t phentsize = READ16LE(&eh->e_phentsize);
    if (phoff > data_size || phentsize < sizeof(ELFProgramHeader)) {
        fprintf(stderr, "Error: Invalid program header offset or size\n");
        return false;
    }
    if (count > 0 && (phoff + (uint64_t)count * phentsize > data_size)) {
        fprintf(stderr, "Error: Program headers extend beyond file size\n");
        return false;
    }

    // read program headers... should probably move this code down
    uint8_t* p = data + phoff;
    size = 0;
    for (_i = 0; _i < count; _i++) {
        ELFProgramHeader* ph = (ELFProgramHeader*)p;
        p += sizeof(ELFProgramHeader);
        if (phentsize != sizeof(ELFProgramHeader)) {
            p += phentsize - sizeof(ELFProgramHeader);
        }

        //    printf("PH %d %08x %08x %08x %08x %08x %08x %08x %08x\n",
        //     i, ph->type, ph->offset, ph->vaddr, ph->paddr,
        //     ph->filesz, ph->memsz, ph->flags, ph->align);

        unsigned address      = READ32LE(&ph->paddr);
        unsigned offset       = READ32LE(&ph->offset);
        unsigned section_size = READ32LE(&ph->filesz);

        if (offset > data_size || section_size > data_size || offset + section_size > data_size)
            continue;

        uint8_t* source  = data + offset;

        if (coreOptions.cpuIsMultiBoot) {
            unsigned effective_address = address - 0x2000000;

            if (effective_address + section_size < SIZE_WRAM) {
                memcpy(&g_workRAM[effective_address], source, section_size);
                size += section_size;
            }
        } else {
            unsigned effective_address = address - 0x8000000;

            if (effective_address + section_size < SIZE_ROM) {
                memcpy(&g_rom[effective_address], source, section_size);
                size += section_size;
            }
        }
    }

    // these must be pre-declared or clang barfs on the goto statement
    ELFSectionHeader** sh = NULL;
    char* stringTable     = NULL;
    uint32_t shoff = 0;
    uint16_t shentsize = 0;
    uint16_t shstrndx = 0;

    // read section headers (if string table is good)
    if (READ16LE(&eh->e_shstrndx) == 0)
        goto end;

    shoff = READ32LE(&eh->e_shoff);
    shentsize = READ16LE(&eh->e_shentsize);
    count = READ16LE(&eh->e_shnum);
    
    // Validate section header offset and count
    if (shoff > data_size || shentsize < sizeof(ELFSectionHeader)) {
        fprintf(stderr, "Error: Invalid section header offset or size\n");
        goto end;
    }
    if (count > 0 && (shoff + (uint64_t)count * (uint64_t)shentsize > data_size)) {
        fprintf(stderr, "Error: Section headers extend beyond file size\n");
        goto end;
    }
    
    // Additional sanity check: reasonable section count (prevent huge allocations)
    if (count > 10000) {
        fprintf(stderr, "Error: Unreasonable section header count: %d\n", count);
        goto end;
    }

    p = data + shoff;

    sh = (ELFSectionHeader**)
        malloc(sizeof(ELFSectionHeader*) * count);

    for (_i = 0; _i < count; _i++) {
        sh[_i] = (ELFSectionHeader*)p;
        p += sizeof(ELFSectionHeader);
        if (shentsize != sizeof(ELFSectionHeader))
            p += shentsize - sizeof(ELFSectionHeader);
    }
    
    shstrndx = READ16LE(&eh->e_shstrndx);
    if (shstrndx >= count) {
        fprintf(stderr, "Error: Invalid string table section index: %d\n", shstrndx);
        free(sh);
        goto end;
    }
	
	stringTable = (char*)elfReadSection(data, sh[shstrndx]);
	if (stringTable == NULL) {
        fprintf(stderr, "Error: Failed to read section header string table\n");
        free(sh);
        goto end;
    }

    elfSectionHeaders            = sh;
    elfSectionHeadersStringTable = stringTable;
    elfSectionHeadersCount       = count;

    for (_i = 0; _i < count; _i++) {
        //    printf("SH %d %-20s %08x %08x %08x %08x %08x %08x %08x %08x\n",
        //   i, &stringTable[sh[_i]->name], sh[_i]->name, sh[_i]->type,
        //   sh[_i]->flags, sh[_i]->addr, sh[_i]->offset, sh[_i]->size,
        //   sh[_i]->link, sh[_i]->info);
        if (READ32LE(&sh[_i]->flags) & 2) { // load section
            uint32_t sect_addr = READ32LE(&sh[_i]->addr);
            uint32_t sect_offset = READ32LE(&sh[_i]->offset);
            uint32_t sect_size = READ32LE(&sh[_i]->size);
            
            // Validate section data is within file bounds
            if (sect_offset > data_size || sect_size > data_size || 
                sect_offset + sect_size > data_size) {
                fprintf(stderr, "Warning: Section %d data exceeds file bounds, skipping\n", _i);
                continue;
            }
            
            if (coreOptions.cpuIsMultiBoot) {
                if (sect_addr >= 0x2000000 && sect_addr <= 0x203ffff) {
                    uint32_t dest_offset = sect_addr & 0x3ffff;
                    // Validate destination bounds
                    if (dest_offset + sect_size > SIZE_WRAM) {
                        fprintf(stderr, "Warning: Section would overflow WRAM, truncating\n");
                        sect_size = SIZE_WRAM - dest_offset;
                    }
                    memcpy(&g_workRAM[dest_offset], data + sect_offset, sect_size);
                    size += sect_size;
                }
            } else {
                if (sect_addr >= 0x8000000 && sect_addr <= 0x9ffffff) {
                    uint32_t dest_offset = sect_addr & 0x1ffffff;
                    // Validate destination bounds
                    if (dest_offset + sect_size > SIZE_ROM) {
                        fprintf(stderr, "Warning: Section would overflow ROM, truncating\n");
                        sect_size = SIZE_ROM - dest_offset;
                    }
                    memcpy(&g_rom[dest_offset], data + sect_offset, sect_size);
                    size += sect_size;
                }
            }
        }
    }

    if (parseDebug) {
        fprintf(stderr, "Parsing debug info\n");

        ELFSectionHeader* dbgHeader = elfGetSectionByName(".debug_info");
        if (dbgHeader == NULL) {
            fprintf(stderr, "Cannot find debug information\n");
            goto end;
        }

        ELFSectionHeader* h = elfGetSectionByName(".debug_abbrev");
        if (h == NULL) {
            fprintf(stderr, "Cannot find abbreviation table\n");
            goto end;
        }

        elfDebugInfo = (DebugInfo*)calloc(1, sizeof(DebugInfo));
        uint8_t* abbrevdata = elfReadSection(data, h);
        if (abbrevdata == NULL) {
            fprintf(stderr, "Error: Failed to read abbreviation section\n");
            free(elfDebugInfo);
            elfDebugInfo = NULL;
            goto end;
        }

        h = elfGetSectionByName(".debug_str");

        if (h == NULL)
            elfDebugStrings = NULL;
        else {
            elfDebugStrings = (char*)elfReadSection(data, h);
            if (elfDebugStrings == NULL) {
                fprintf(stderr, "Warning: Failed to read debug strings section\n");
            }
        }

        uint8_t* debugdata = elfReadSection(data, dbgHeader);
        if (debugdata == NULL) {
            fprintf(stderr, "Error: Failed to read debug info section\n");
            free(elfDebugInfo);
            elfDebugInfo = NULL;
            goto end;
        }

        elfDebugInfo->debugdata = data;
        elfDebugInfo->infodata = debugdata;

        uint32_t total = READ32LE(&dbgHeader->size);
        uint8_t* end = debugdata + total;
        
        // Validate debug data end pointer
        if (!elfIsValidPtr(end, 0)) {
            fprintf(stderr, "Error: Debug data section exceeds file bounds\n");
            free(elfDebugInfo);
            elfDebugInfo = NULL;
            goto end;
        }
        
        uint8_t* ddata = debugdata;

        CompileUnit* last = NULL;
        CompileUnit* unit = NULL;

        while (ddata < end) {
            // Ensure we can read at least the length field (4 bytes)
            if (!elfIsValidPtr(ddata, 4)) {
                fprintf(stderr, "Error: Compile unit header out of bounds\n");
                break;
            }
            
            unit = elfParseCompUnit(ddata, abbrevdata);
            if (unit == NULL) {
                fprintf(stderr, "Warning: Failed to parse compile unit\n");
                break;
            }
            
            unit->offset = (uint32_t)(ddata - debugdata);
            elfParseLineInfo(unit, data);
            if (last == NULL)
                elfCompileUnits = unit;
            else
                last->next = unit;
            last = unit;
            
            // Validate we can advance by the unit length
            if (unit->length > (uint32_t)(end - ddata - 4)) {
                fprintf(stderr, "Error: Compile unit length exceeds remaining data\n");
                break;
            }
            
            ddata += 4 + unit->length;
        }
        elfParseAranges(data);
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
        elfParseCFA(data);
        elfReadSymtab(data);
    }
end:
    if (sh) {
        free(sh);
    }

    elfSectionHeaders = NULL;
    elfSectionHeadersStringTable = NULL;
    elfSectionHeadersCount = 0;

    return true;
}

bool elfRead(const char* name, int& siz, FILE* f)
{
    fseek(f, 0, SEEK_END);
    unsigned long size = ftell(f);
    
    // Sanity check: ELF file should not exceed 256MB
    if (size > 256 * 1024 * 1024) {
        fprintf(stderr, "ELF file too large: %lu bytes\n", size);
        return false;
    }
    
    // Minimum ELF header size
    if (size < sizeof(ELFHeader)) {
        fprintf(stderr, "File too small to be valid ELF\n");
        return false;
    }
    
    elfFileData = (uint8_t*)malloc(size);
    if (elfFileData == NULL) {
        fprintf(stderr, "Failed to allocate memory for ELF file\n");
        return false;
    }
    
    fseek(f, 0, SEEK_SET);
    int res = (int)fread(elfFileData, 1, size, f);
    fclose(f);

    if (res < 0 || (unsigned long)res != size) {
        fprintf(stderr, "Failed to read ELF file completely\n");
        free(elfFileData);
        elfFileData = NULL;
        g_elfFileStart = NULL;
        g_elfFileEnd = NULL;
        return false;
    }
    
    // Initialize global file bounds for validation
    g_elfFileStart = elfFileData;
    g_elfFileEnd = elfFileData + size;

    ELFHeader* header = (ELFHeader*)elfFileData;

    // Validate ELF magic number (0x7F 'E' 'L' 'F')
    uint32_t magic = READ32LE(&header->magic);
    if (magic != 0x464C457F) {
        systemMessage(0, N_("Not a valid ELF file %s (bad magic: 0x%08x)"), name, magic);
        free(elfFileData);
        elfFileData = NULL;
        g_elfFileStart = NULL;
        g_elfFileEnd = NULL;
        return false;
    }
    
    // Validate ELF class (32-bit)
    if (header->clazz != 1) {
        systemMessage(0, N_("Not a 32-bit ELF file %s (class: %d)"), name, header->clazz);
        free(elfFileData);
        elfFileData = NULL;
        g_elfFileStart = NULL;
        g_elfFileEnd = NULL;
        return false;
    }
    
    // Validate ELF machine type (ARM)
    uint16_t machine = READ16LE(&header->e_machine);
    if (machine != 40) {
        systemMessage(0, N_("Not an ARM ELF file %s (machine: %d)"), name, machine);
        free(elfFileData);
        elfFileData = NULL;
        g_elfFileStart = NULL;
        g_elfFileEnd = NULL;
        return false;
    }

    if (!elfReadProgram(header, elfFileData, size, siz, coreOptions.parseDebug)) {
        free(elfFileData);
        elfFileData = NULL;
        return false;
    }

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

    if (elfFileData) {
        free(elfFileData);
        elfFileData = NULL;
    }
    
    // Clear global file bounds
    g_elfFileStart = NULL;
    g_elfFileEnd = NULL;
}
