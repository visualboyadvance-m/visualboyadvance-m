#include "ed25519.h"

#ifndef ED25519_NO_SEED

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#elif defined(macintosh) && !defined(__APPLE__)
/* Classic Mac OS (Retro68): there is no /dev/urandom nor any other OS
   entropy source, so the seed is derived from timer jitter and Toolbox
   state, hashed with SHA-512. */
#define ED25519_CLASSIC_MAC 1
#include <stdint.h>
#include <string.h>
#include <Multiverse.h>
#include "sha512.h"
#else
#include <stdio.h>
#endif

#ifdef ED25519_CLASSIC_MAC

#if defined(__m68k__)
/* _Microseconds (0xA193) returns the high half in A0, the low one in D0. */
static uint64_t classic_microseconds(void) {
    register uint32_t hi __asm__("a0");
    register uint32_t lo __asm__("d0");
    __asm__ __volatile__(".short 0xA193"
                         : "=r"(hi), "=r"(lo)
                         :
                         : "d1", "d2", "a1", "cc", "memory");
    return ((uint64_t)hi << 32) | lo;
}
#else
/* PowerPC: exported by InterfaceLib (CarbonLib for Carbon). */
typedef struct { uint32_t hi, lo; } ed25519_unsigned_wide;
extern void Microseconds(ed25519_unsigned_wide *microTickCount);

static uint64_t classic_microseconds(void) {
    ed25519_unsigned_wide w;
    Microseconds(&w);
    return ((uint64_t)w.hi << 32) | w.lo;
}
#endif

static void add_u64(sha512_context *ctx, uint64_t v) {
    unsigned char b[8];
    int i;

    for (i = 0; i < 8; i++) {
        b[i] = (unsigned char)(v >> (8 * i));
    }
    sha512_update(ctx, b, sizeof(b));
}

static int classic_create_seed(unsigned char *seed) {
    /* chained across calls, so consecutive seeds never repeat even if the
       collected entropy were identical */
    static unsigned char pool[64];
    static uint64_t counter;

    sha512_context ctx;
    unsigned char digest[64];
    uint64_t prev, now;
    volatile uint32_t spin = 0;
    int i, j;

    if (sha512_init(&ctx) != 0) {
        return 1;
    }

    sha512_update(&ctx, pool, sizeof(pool));
    add_u64(&ctx, ++counter);

    /* slowly changing state */
    add_u64(&ctx, (uint64_t)(uintptr_t)&ctx);       /* stack address */
    add_u64(&ctx, (uint64_t)(uintptr_t)seed);
    add_u64(&ctx, (uint64_t)TickCount());
    add_u64(&ctx, (uint64_t)FreeMem());
#if !defined(TARGET_API_MAC_CARBON) || !TARGET_API_MAC_CARBON
    /* low memory globals: not available under Carbon */
    add_u64(&ctx, (uint64_t)LMGetTime());           /* seconds since 1904 */
    {
        Point mouse = LMGetMouseLocation();
        add_u64(&ctx, ((uint64_t)(uint16_t)mouse.h << 16) | (uint16_t)mouse.v);
    }
#endif

    /* Timer jitter: the duration of a busy loop varies with interrupts
       (VBL, Time Manager tasks, network, disk, ...). Collect 1024 samples
       of microsecond deltas; only their low bits carry entropy, the hash
       distils it. */
    prev = classic_microseconds();
    add_u64(&ctx, prev);
    for (i = 0; i < 1024; i++) {
        for (j = 0; j < 64 + (int)(prev & 0x3F); j++) {
            spin += (uint32_t)j ^ (uint32_t)prev;
        }
        now = classic_microseconds();
        add_u64(&ctx, now - prev);
        prev = now;
    }
    add_u64(&ctx, spin);
    add_u64(&ctx, (uint64_t)TickCount());

    if (sha512_final(&ctx, digest) != 0) {
        return 1;
    }

    /* half of the digest is the seed, the other half is the next pool */
    memcpy(seed, digest, 32);
    memcpy(pool, digest + 32, 32);
    memset(pool + 32, 0, 32);
    memset(digest, 0, sizeof(digest));
    return 0;
}

#endif /* ED25519_CLASSIC_MAC */

int ed25519_create_seed(unsigned char *seed) {
#ifdef _WIN32
    HCRYPTPROV prov;

    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))  {
        return 1;
    }

    if (!CryptGenRandom(prov, 32, seed))  {
        CryptReleaseContext(prov, 0);
        return 1;
    }

    CryptReleaseContext(prov, 0);
#elif defined(ED25519_CLASSIC_MAC)
    return classic_create_seed(seed);
#else
    FILE *f = fopen("/dev/urandom", "rb");
    size_t got;

    if (f == NULL) {
        return 1;
    }

    got = fread(seed, 1, 32, f);
    fclose(f);

    if (got != 32) {
        return 1;
    }
#endif

    return 0;
}

#endif
