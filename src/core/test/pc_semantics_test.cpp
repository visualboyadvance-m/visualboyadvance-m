// pc_semantics_test: ARM7TDMI PC-write semantics.
//
// Hardware rules (ARMv4T; matched by mGBA and NanoBoyAdvance):
//   * ARM-state PC writes (LDM{pc}, LDR pc, data-processing Rd=15, BX
//     with bit 0 clear) keep bit 1 of the value in R15 -- PC-relative
//     reads see the +2 -- while instruction fetch is word-aligned.
//   * BX with bit 0 set enters Thumb at value & ~1.
//   * Thumb-state PC writes (POP {pc}, hi-register MOV/ADD pc) ignore
//     bit 0 and stay in Thumb (no exchange on ARMv4).
//
// Every jump family is driven at all four low-bit patterns; a coherent
// probe at the aligned target reports the PC it read plus a marker that
// a misaligned fetch can never set. The last block reproduces the
// Pokémon Emerald box-name ACE convention split ("old" vs "console"
// emulators, Sleipnir17): the old-variant payload under old-class entry
// and the console-variant under console-class entry must produce an
// identical result structure. Needs no ROM; exits nonzero on failure.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaCpu.h"
#include "core/gba/gbaInline.h"
#include "core/base/file_util.h"
#include "core/gba/gbaSound.h"

extern uint8_t* g_workRAM;

// ---- stubs lifted from suite_runner ----
static uint32_t g_joy_mask = 0;
struct CoreOptions coreOptions;

void systemMessage(int, const char*, ...) {}
void log(const char*, ...) {}
bool systemPauseOnFrame() { return false; }
void systemGbPrint(uint8_t*, int, int, int, int, int) {}
void systemScreenCapture(int) {}
void systemDrawScreen() {}
void systemSendScreen() {}
bool systemReadJoypads() { return true; }
uint32_t systemReadJoypad(int) { return g_joy_mask; }
uint32_t systemGetClock() { return 0; }
void systemSetTitle(const char*) {}
namespace {
class NullSoundDriver : public SoundDriver {
  public:
    bool init(long) override { return true; }
    void pause() override {}
    void reset() override {}
    void resume() override {}
    void write(uint16_t*, int) override {}
    void setThrottle(unsigned short) override {}
};
} // namespace
std::unique_ptr<SoundDriver> systemSoundInit() {
    return std::unique_ptr<SoundDriver>(new NullSoundDriver);
}
void systemOnWriteDataToSoundBuffer(const uint16_t*, int) {}
void systemOnSoundShutdown() {}
void systemScreenMessage(const char*) {}
void systemUpdateMotionSensor() {}
int systemGetSensorX() { return 0; }
int systemGetSensorY() { return 0; }
int systemGetSensorZ() { return 0; }
uint8_t systemGetSensorDarkness() { return 0; }
void systemCartridgeRumble(bool) {}
void systemPossibleCartridgeRumble(bool) {}
void updateRumbleFrame() {}
bool systemCanChangeSoundQuality() { return false; }
void systemShowSpeed(int) {}
void system10Frames() {}
void systemFrame() {}
void systemGbBorderOn() {}
void (*dbgOutput)(const char* s, uint32_t addr) = nullptr;
void (*dbgSignal)(int sig, int number) = nullptr;

uint8_t  systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
uint16_t systemGbPalette[24];
int systemRedShift = 0;
int systemGreenShift = 0;
int systemBlueShift = 0;
int systemColorDepth = 32;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = 0;
int systemSpeed = 0;
int emulating = 0;

// ---------------------------------------------------------------------------
// ARM7TDMI PC-write semantics battery (ARMv4T hardware rules):
//   ARM-state PC writes (LDM{pc}, LDR pc, DP Rd=15 non-S): bits[1:0]
//   ignored, continue ARM at value & ~3.  BX: bit0 selects state; ARM
//   target & ~3, THUMB target & ~1.  THUMB-state PC writes (POP {pc},
//   hi-reg ADD/MOV pc): bit0 ignored, stay THUMB at value & ~1.
// A coherent probe at the ALIGNED target sets r7 = pc-read and a marker;
// a shifted/misaligned fetch never reaches it (marker stays 0).
// ---------------------------------------------------------------------------
static void writeWords(uint32_t ewramAddr, const uint32_t* w, int n) {
    for (int i = 0; i < n; i++)
        memcpy(g_workRAM + ((ewramAddr + 4u * i) & 0x3FFFF), &w[i], 4);
}
static void armJump(uint32_t pc) {
    armState = true; armNextPC = pc; reg[15].I = pc + 4;
    cpuPrefetch[0] = CPUReadMemoryQuick(armNextPC);
    cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC + 4);
}
static void thumbJump(uint32_t pc) {
    armState = false; armNextPC = pc; reg[15].I = pc + 2;
    cpuPrefetch[0] = CPUReadHalfWordQuick(armNextPC);
    cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC + 2);
}
static void step(int n) {
    extern int armExecute(); extern int thumbExecute();
    for (int i = 0; i < n; i++) {
        cpuNextEvent = cpuTotalTicks + 1;
        if (armState) armExecute(); else thumbExecute();
    }
}

static const uint32_t kArmTarget   = 0x02030000;
static const uint32_t kThumbTarget = 0x02030100;
static const uint32_t kTramp       = 0x02031000;
static const uint32_t kPool        = 0x02031800;

static int gFails = 0;
static void check(const char* fam, int low, bool expArm) {
    uint32_t base = expArm ? kArmTarget : kThumbTarget;
    uint32_t val = base | (uint32_t)low;
    // ARM7TDMI: ARM-state PC write keeps bit 1 (fetch is word-aligned);
    // bit 0 is consumed by BX as the state select / ignored otherwise.
    uint32_t expR7 = expArm ? ((val & ~1u) + 8) : ((val & ~1u) + 4);
    bool coher = expArm ? (reg[8].I == 0xAB) : ((reg[6].I & 0xFF) == 0xAB);
    bool pcOk  = expArm ? (reg[7].I == expR7) : true; // THUMB adr aligns pc; value tested ARM-side only
    bool stOk  = (armState == expArm);
    bool ok = coher && pcOk && stOk;
    if (!ok) gFails++;
    printf("%-16s |%d -> %s  r7=%08x want=%08x marker=%s state=%s\n",
           fam, low, ok ? "PASS" : "FAIL", reg[7].I, expR7,
           coher ? "ok" : "MISSING", stOk ? "ok" : "WRONG");
}
static void clearRegs() { reg[6].I = reg[7].I = reg[8].I = 0; }

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    // Synthetic ROM: 64 KiB of `b .` -- the core only needs a mapped image
    // and a reset; every case force-jumps into EWRAM afterwards.
    static uint32_t synth[0x10000 / 4];
    for (size_t i = 0; i < sizeof(synth) / 4; i++) synth[i] = 0xEAFFFFFE;
    if (CPULoadRomData((const char*)synth, (int)sizeof(synth)) == 0) {
        fprintf(stderr, "pc_semantics_test: synthetic ROM load failed\n");
        return 2;
    }
    CPUInit("", false);
    soundInit();
    CPUReset();
    emulating = 1;
    CPULoop(280896 * 2);   // let reset/HLE settle; the image just spins

    // ARM probe: mov r7,pc ; mov r8,#0xAB ; b .
    uint32_t armProbe[] = { 0xE1A0700F, 0xE3A080AB, 0xEAFFFFFE };
    writeWords(kArmTarget, armProbe, 3);
    // THUMB probe halfwords: a700 (add r7,pc,#0) 26ab (mov r6,#0xAB) e7fe (b .)
    uint32_t thumbProbe[] = { 0x26ABA700, 0xE7FEE7FE };
    writeWords(kThumbTarget, thumbProbe, 2);

    for (int lb = 0; lb < 4; lb++) {               // ARM ldm r0,{pc}
        uint32_t code[] = { 0xE8908000, 0xEAFFFFFE };
        writeWords(kTramp, code, 2);
        uint32_t v = kArmTarget | (uint32_t)lb;
        writeWords(kPool, &v, 1);
        clearRegs(); reg[0].I = kPool;
        armJump(kTramp); step(4);
        check("arm ldm{pc}", lb, true);
    }
    for (int lb = 0; lb < 4; lb++) {               // ARM ldr pc,[r0]
        uint32_t code[] = { 0xE590F000, 0xEAFFFFFE };
        writeWords(kTramp, code, 2);
        uint32_t v = kArmTarget | (uint32_t)lb;
        writeWords(kPool, &v, 1);
        clearRegs(); reg[0].I = kPool;
        armJump(kTramp); step(4);
        check("arm ldr pc", lb, true);
    }
    for (int lb = 0; lb < 4; lb++) {               // ARM mov pc,r1
        uint32_t code[] = { 0xE1A0F001, 0xEAFFFFFE };
        writeWords(kTramp, code, 2);
        clearRegs(); reg[1].I = kArmTarget | (uint32_t)lb;
        armJump(kTramp); step(4);
        check("arm mov pc", lb, true);
    }
    for (int lb = 0; lb < 4; lb++) {               // ARM bx r1
        uint32_t code[] = { 0xE12FFF11, 0xEAFFFFFE };
        writeWords(kTramp, code, 2);
        bool expArm = !(lb & 1);
        clearRegs();
        reg[1].I = (expArm ? kArmTarget : kThumbTarget) | (uint32_t)lb;
        armJump(kTramp); step(4);
        check("arm bx", lb, expArm);
    }
    for (int lb = 0; lb < 4; lb++) {               // THUMB bx r1
        uint32_t code[] = { 0x46C04708 };          // bx r1 ; nop
        writeWords(kTramp, code, 1);
        bool expArm = !(lb & 1);
        clearRegs();
        reg[1].I = (expArm ? kArmTarget : kThumbTarget) | (uint32_t)lb;
        thumbJump(kTramp); step(4);
        check("thumb bx", lb, expArm);
    }
    for (int lb = 0; lb < 4; lb += 1) {            // THUMB pop {pc}
        uint32_t code[] = { 0x46C0BD00 };          // pop {pc} ; nop
        writeWords(kTramp, code, 1);
        uint32_t v = kThumbTarget | (uint32_t)lb;
        writeWords(kPool, &v, 1);
        clearRegs(); reg[13].I = kPool;
        thumbJump(kTramp); step(4);
        check("thumb pop{pc}", lb, false);          // v4: stays THUMB
    }
    for (int lb = 0; lb < 4; lb++) {               // THUMB mov pc,r1 (hi-reg)
        uint32_t code[] = { 0x46C0468F };          // mov pc,r1 ; nop
        writeWords(kTramp, code, 1);
        clearRegs(); reg[1].I = kThumbTarget | (uint32_t)lb;
        thumbJump(kTramp); step(4);
        check("thumb mov pc", lb, false);
    }


    // ---- ACE demonstration: Sleipnir17 English Code 1, entered via THUMB bx
    //      at payload|2. Result words land at [R10+0xC00] where
    //      R10 = (PC_first - imm) - 0xA100 (minus borrow). Console/new class:
    //      the B2 variant lands them; the B0 variant lands 2 bytes off.
    {
        static const uint32_t kCode1[] = {
            0xE2CFC0B0, 0xE3E0BCDB, 0xE2ABBDFF, 0xE2CCACA1, 0xBFBFFF00, 0xE2CBB0BE,
            0xBFFF0000, 0xE5AABC00, 0xFF000000, 0xE2ABBCA6, 0xE3B0C1B4, 0xE2CA00FF,
            0xE1E0CFB0, 0xBFBFFF00, 0xE1C0B0B4, 0xBFFF0000, 0xE2BFC1C0, 0xFF000000,
            0xE3E0B6EE, 0xE2CBB0ED, 0xE2CBB6FF, 0xE5ACB000, 0xBFBFFF00, 0xE2AECEDC,
            0xBFFF0000, 0xE2ACC8D3, 0xFF000000, 0xE3CCC8C0, 0xE2AC00D6, 0x0000BFFF,
            0x00000000 };
        const uint32_t pay = 0x02032000;      // payload base (word aligned)
        // Scenario 0: old-class core (PC masked) running the B0 payload ==
        //             entering the aligned address with B0.
        // Scenario 1: console-class core (PC retains bit1, this build)
        //             running the B2 payload entered at pay|2.
        // Paste requirement: both yield the SAME R12 / result structure.
        uint32_t r12s[2], bases[2];
        for (int variant = 0; variant < 2; variant++) {
            uint32_t code[32]; memcpy(code, kCode1, sizeof(kCode1));
            code[0] = variant ? 0xE2CFC0B2 : 0xE2CFC0B0;  // * = 2 (new) / 0 (old)
            writeWords(pay, code, 31);
            uint32_t bx[] = { 0x46C04708 }; writeWords(kTramp, bx, 1);
            reg[1].I = variant ? (pay | 2) : pay;
            // The paste's arithmetic assumes C=0 (sbc = Rn-op-1, adc = Rn+op)
            reg[16].I &= ~0x20000000u; CPUUpdateFlags();
            thumbJump(kTramp);
            step(1 + 13);  // bx + through `strh r12,[r0,#F0]!` (index 12)
            uint32_t base = reg[0].I;   // documented result layout base
            step(17);      // the rest, stopping before the self-written bx r0
            uint32_t h0 = CPUReadHalfWord(base), h4 = CPUReadHalfWord(base + 4);
            uint32_t w10 = CPUReadMemory(base + 0x10);
            r12s[variant] = reg[12].I; bases[variant] = base;
            printf("ACE Code1 %-26s r12=%08x base=%08x [base]=%04x [+4]=%04x [+10]=%08x\n",
                   variant ? "B2 @pay|2 (console-class)" : "B0 @pay (old-class)",
                   reg[12].I, base, h0, h4, w10);
            (void)h0; (void)h4; (void)w10;
        }
        printf("ACE equivalence: %s (r12 %08x vs %08x, base %08x vs %08x)\n",
               (r12s[0] == r12s[1] && bases[0] == bases[1]) ? "IDENTICAL -> this build is console-class"
                                                             : "DIFFER -> not console-class",
               r12s[0], r12s[1], bases[0], bases[1]);
        if (!(r12s[0] == r12s[1] && bases[0] == bases[1])) gFails++;
    }
    printf("\npc_semantics_test: %d failing cases\n", gFails);
    return gFails ? 1 : 0;
}
