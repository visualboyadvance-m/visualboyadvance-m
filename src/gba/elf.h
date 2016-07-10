#ifndef ELF_H
#define ELF_H

enum LocationType { LOCATION_register,
    LOCATION_memory,
    LOCATION_value };

#define DW_ATE_boolean 0x02
#define DW_ATE_signed 0x05
#define DW_ATE_unsigned 0x07
#define DW_ATE_unsigned_char 0x08

struct ELFHeader {
    uint32_t magic;
    uint8_t clazz;
    uint8_t data;
    uint8_t version;
    uint8_t pad[9];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct ELFProgramHeader {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
};

struct ELFSectionHeader {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
};

struct ELFSymbol {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
};

struct ELFBlock {
    int length;
    uint8_t* data;
};

struct ELFAttr {
    uint32_t name;
    uint32_t form;
    union {
        uint32_t value;
        char* string;
        uint8_t* data;
        bool flag;
        ELFBlock* block;
    };
};

struct ELFAbbrev {
    uint32_t number;
    uint32_t tag;
    bool hasChildren;
    int numAttrs;
    ELFAttr* attrs;
    ELFAbbrev* next;
};

enum TypeEnum {
    TYPE_base,
    TYPE_pointer,
    TYPE_function,
    TYPE_void,
    TYPE_array,
    TYPE_struct,
    TYPE_reference,
    TYPE_enum,
    TYPE_union
};

struct Type;
struct Object;

struct FunctionType {
    Type* returnType;
    Object* args;
};

struct Member {
    char* name;
    Type* type;
    int bitSize;
    int bitOffset;
    int byteSize;
    ELFBlock* location;
};

struct Struct {
    int memberCount;
    Member* members;
};

struct Array {
    Type* type;
    int maxBounds;
    int* bounds;
};

struct EnumMember {
    char* name;
    uint32_t value;
};

struct Enum {
    int count;
    EnumMember* members;
};

struct Type {
    uint32_t offset;
    TypeEnum type;
    const char* name;
    int encoding;
    int size;
    int bitSize;
    union {
        Type* pointer;
        FunctionType* function;
        Array* array;
        Struct* structure;
        Enum* enumeration;
    };
    Type* next;
};

struct Object {
    char* name;
    int file;
    int line;
    bool external;
    Type* type;
    ELFBlock* location;
    uint32_t startScope;
    uint32_t endScope;
    Object* next;
};

struct Function {
    char* name;
    uint32_t lowPC;
    uint32_t highPC;
    int file;
    int line;
    bool external;
    Type* returnType;
    Object* parameters;
    Object* variables;
    ELFBlock* frameBase;
    Function* next;
};

struct LineInfoItem {
    uint32_t address;
    char* file;
    int line;
};

struct LineInfo {
    int fileCount;
    char** files;
    int number;
    LineInfoItem* lines;
};

struct ARange {
    uint32_t lowPC;
    uint32_t highPC;
};

struct ARanges {
    uint32_t offset;
    int count;
    ARange* ranges;
};

struct CompileUnit {
    uint32_t length;
    uint8_t* top;
    uint32_t offset;
    ELFAbbrev** abbrevs;
    ARanges* ranges;
    char* name;
    char* compdir;
    uint32_t lowPC;
    uint32_t highPC;
    bool hasLineInfo;
    uint32_t lineInfo;
    LineInfo* lineInfoTable;
    Function* functions;
    Function* lastFunction;
    Object* variables;
    Type* types;
    CompileUnit* next;
};

struct DebugInfo {
    uint8_t* debugfile;
    uint8_t* abbrevdata;
    uint8_t* debugdata;
    uint8_t* infodata;
    int numRanges;
    ARanges* ranges;
};

struct Symbol {
    const char* name;
    int type;
    int binding;
    uint32_t address;
    uint32_t value;
    uint32_t size;
};

extern uint32_t elfReadLEB128(uint8_t*, int*);
extern int32_t elfReadSignedLEB128(uint8_t*, int*);
extern bool elfRead(const char*, int&, FILE* f);
extern bool elfGetSymbolAddress(const char*, uint32_t*, uint32_t*, int*);
extern const char* elfGetAddressSymbol(uint32_t);
extern const char* elfGetSymbol(int, uint32_t*, uint32_t*, int*);
extern void elfCleanUp();
extern bool elfGetCurrentFunction(uint32_t, Function**, CompileUnit** c);
extern bool elfGetObject(const char*, Function*, CompileUnit*, Object**);
extern bool elfFindLineInUnit(uint32_t*, CompileUnit*, int);
extern bool elfFindLineInModule(uint32_t*, const char*, int);
uint32_t elfDecodeLocation(Function*, ELFBlock*, LocationType*);
uint32_t elfDecodeLocation(Function*, ELFBlock*, LocationType*, uint32_t);
int elfFindLine(CompileUnit* unit, Function* func, uint32_t addr, const char**);

#endif // ELF_H
