// malloc() for classic Mac OS (Retro68) that does not make a Memory Manager
// block of every allocation.
//
// Retro68's own malloc() is NewPtr() and its free() DisposePtr(). The Memory
// Manager walks the heap's blocks to place each nonrelocatable one, so the
// cost of an allocation grows with the number of blocks there are, and a C++
// program makes a great many small ones: every wxString, XML node, map entry
// and std::vector. Starting up -- parsing the XRC resources, building the
// menus, decoding the application icon -- took minutes on a 68040 that way,
// long enough to look like a hang.
//
// Here the small blocks come out of 64 KB chunks the Memory Manager gives us
// once, with a free list per size class, and only large blocks (and the
// chunks) are Memory Manager blocks. The small classes never give their
// memory back to the Memory Manager, which only matters for a program that
// keeps many more small blocks alive at once than it starts with; the large
// blocks that hold ROMs, save states and pictures are returned when freed.
//
// There are no threads, and nothing calls malloc() at interrupt time (the
// Memory Manager could not be called there either), so there is no locking.
// Defining the whole family keeps libretrocrt's malloc.c out of the link.

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <MacMemory.h>

// Blocks are 8-byte aligned: the PowerPC wants doubles there. Each small
// block has a 4-byte header just before the pointer handed out, holding the
// block's size (header included, a multiple of 8). A large block's header has
// kLargeFlag set; the word before that holds the Memory Manager pointer that
// DisposePtr() takes back.
enum {
    kAlign = 8,
    kHeader = 4,
    kLargeHeader = 8,
    kLargeFlag = 1,
    kMaxSmall = 2048,  // block size, header included
    kNumClasses = kMaxSmall / kAlign + 1,
    kChunkSize = 64 * 1024,
};

typedef struct FreeBlock {
    struct FreeBlock* next;
} FreeBlock;

static FreeBlock* free_lists[kNumClasses];

// The chunk being carved up: [chunk_next, chunk_end).
static char* chunk_next;
static char* chunk_end;

static uint32_t* header_of(void* p) {
    return (uint32_t*)((char*)p - kHeader);
}

static void push_free(char* block_start, uint32_t block_size) {
    // block_start is where the header goes; the list links through the
    // payload, which is at least 4 bytes in every class.
    char* p = block_start + kHeader;
    *header_of(p) = block_size;
    FreeBlock* fb = (FreeBlock*)p;
    fb->next = free_lists[block_size / kAlign];
    free_lists[block_size / kAlign] = fb;
}

static int new_chunk(void) {
    // The remainder of the old chunk becomes a free block of its own.
    if (chunk_end - chunk_next >= 2 * kAlign) {
        uint32_t rest = (uint32_t)(chunk_end - chunk_next) & ~(uint32_t)(kAlign - 1);
        push_free(chunk_next, rest);
    }

    char* c = (char*)NewPtr(kChunkSize);
    if (!c)
        return 0;

    // The first header sits 4 bytes below an 8-byte boundary, so every
    // payload after it is aligned; NewPtr() returns at least 4-byte aligned
    // memory.
    char* start = c;
    while (((uintptr_t)(start + kHeader) & (kAlign - 1)) != 0)
        start += 2;
    chunk_next = start;
    chunk_end = c + kChunkSize;
    return 1;
}

static void* alloc_large(size_t size, size_t align) {
    if (size > 0x7FFFFFF0u - kLargeHeader - align)
        return NULL;

    char* base = (char*)NewPtr((Size)(size + kLargeHeader + align));
    if (!base)
        return NULL;

    uintptr_t p = (uintptr_t)base + kLargeHeader;
    p = (p + align - 1) & ~(uintptr_t)(align - 1);

    ((char**)p)[-2] = base;
    *header_of((void*)p) = (uint32_t)size | kLargeFlag;
    return (void*)p;
}

static void* do_malloc(size_t size) {
    if (size > kMaxSmall - kHeader)
        return alloc_large((size + 1) & ~(size_t)1, kAlign);

    uint32_t block_size = (uint32_t)((size + kHeader + kAlign - 1) & ~(size_t)(kAlign - 1));
    if (block_size < 2 * kAlign)
        block_size = 2 * kAlign;

    FreeBlock* fb = free_lists[block_size / kAlign];
    if (fb) {
        free_lists[block_size / kAlign] = fb->next;
        return fb;
    }

    if ((size_t)(chunk_end - chunk_next) < block_size && !new_chunk())
        return NULL;

    char* p = chunk_next + kHeader;
    chunk_next += block_size;
    *header_of(p) = block_size;
    return p;
}

static size_t usable_size(void* p) {
    uint32_t h = *header_of(p);
    if (h & kLargeFlag)
        return h & ~(uint32_t)kLargeFlag;
    return h - kHeader;
}

static void do_free(void* p) {
    if (!p)
        return;

    uint32_t h = *header_of(p);
    if (h & kLargeFlag) {
        DisposePtr((Ptr)((char**)p)[-2]);
        return;
    }

    FreeBlock* fb = (FreeBlock*)p;
    fb->next = free_lists[h / kAlign];
    free_lists[h / kAlign] = fb;
}

static void* do_realloc(void* p, size_t size) {
    if (!p)
        return do_malloc(size);
    if (size == 0) {
        do_free(p);
        return NULL;
    }

    size_t old = usable_size(p);
    uint32_t h = *header_of(p);

    if (!(h & kLargeFlag)) {
        // Keep the block unless it is more than twice what is wanted now.
        if (size <= old && (old <= 2 * kAlign || size > old / 2))
            return p;
    } else if (size > kMaxSmall - kHeader) {
        char* base = ((char**)p)[-2];
        size_t even = (size + 1) & ~(size_t)1;
        size_t offset = (size_t)((char*)p - base);
        SetPtrSize((Ptr)base, (Size)(even + offset));
        if (MemError() == noErr) {
            *header_of(p) = (uint32_t)even | kLargeFlag;
            return p;
        }
    }

    void* n = do_malloc(size);
    if (!n)
        return NULL;
    memcpy(n, p, old < size ? old : size);
    do_free(p);
    return n;
}

static void* do_memalign(size_t align, size_t size) {
    if (align <= kAlign)
        return do_malloc(size);
    if (align & (align - 1))
        return NULL;
    return alloc_large((size + 1) & ~(size_t)1, align);
}

static void* do_calloc(size_t n, size_t size) {
    if (size && n > (size_t)-1 / size)
        return NULL;
    void* p = do_malloc(n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

void* malloc(size_t size) {
    void* p = do_malloc(size);
    if (!p)
        errno = ENOMEM;
    return p;
}

void free(void* p) {
    do_free(p);
}

void* realloc(void* p, size_t size) {
    void* n = do_realloc(p, size);
    if (!n && size)
        errno = ENOMEM;
    return n;
}

void* calloc(size_t n, size_t size) {
    void* p = do_calloc(n, size);
    if (!p)
        errno = ENOMEM;
    return p;
}

void* memalign(size_t align, size_t size) {
    void* p = do_memalign(align, size);
    if (!p)
        errno = ENOMEM;
    return p;
}

void* aligned_alloc(size_t align, size_t size) {
    return memalign(align, size);
}

int posix_memalign(void** out, size_t align, size_t size) {
    if (align < sizeof(void*) || (align & (align - 1)))
        return EINVAL;
    void* p = do_memalign(align, size);
    if (!p)
        return ENOMEM;
    *out = p;
    return 0;
}

size_t malloc_usable_size(void* p) {
    return p ? usable_size(p) : 0;
}

// newlib's reentrant entry points, which the rest of libc calls.
struct _reent;

void* _malloc_r(struct _reent* r, size_t size) {
    (void)r;
    return malloc(size);
}

void _free_r(struct _reent* r, void* p) {
    (void)r;
    do_free(p);
}

void* _realloc_r(struct _reent* r, void* p, size_t size) {
    (void)r;
    return realloc(p, size);
}

void* _calloc_r(struct _reent* r, size_t n, size_t size) {
    (void)r;
    return calloc(n, size);
}

void* _memalign_r(struct _reent* r, size_t align, size_t size) {
    (void)r;
    return memalign(align, size);
}

size_t _malloc_usable_size_r(struct _reent* r, void* p) {
    (void)r;
    return malloc_usable_size(p);
}
