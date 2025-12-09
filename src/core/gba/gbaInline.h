#ifndef VBAM_CORE_GBA_GBAINLINE_H_
#define VBAM_CORE_GBA_GBAINLINE_H_

#include <cstdint>
#include <type_traits>

#include "core/base/port.h"
#include "core/base/system.h"
#include "core/gba/gbaCpu.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaPrint.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"

#if defined(VBAM_ENABLE_DEBUGGER)
#include "core/gba/gbaRemote.h"
#endif  // defined(VBAM_ENABLE_DEBUGGER)

extern const uint32_t objTilesAddress[3];

extern bool stopState;
extern bool holdState;
extern int holdType;
extern int cpuNextEvent;
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;
extern bool cpuDmaRunning;
extern uint32_t cpuDmaPC;
extern bool timer0On;
extern int timer0Ticks;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Ticks;
extern int timer1ClockReload;
extern bool timer2On;
extern int timer2Ticks;
extern int timer2ClockReload;
extern bool timer3On;
extern int timer3Ticks;
extern int timer3ClockReload;
extern int cpuTotalTicks;

extern uint32_t cpuDmaBusValue;

#define CPUReadByteQuick(addr) map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]

#define CPUReadHalfWordQuick(addr) \
    READ16LE(((uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

#define CPUReadMemoryQuick(addr) \
    READ32LE(((uint32_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

static inline uint16_t DowncastU16(uint32_t value) {
    return static_cast<uint16_t>(value);
}

static inline int16_t Downcast16(int32_t value) {
    return static_cast<int16_t>(value);
}

template<typename T>
static inline uint8_t DowncastU8(T value) {
    static_assert(std::is_integral<T>::value, "Integral type required.");
    static_assert(sizeof(T) ==2 || sizeof(T) == 4, "16 or 32 bits int required");
    return static_cast<uint8_t>(value);
}

template<typename T>
static inline int8_t Downcast8(T value) {
    static_assert(std::is_integral<T>::value, "Integral type required.");
    static_assert(sizeof(T) ==2 || sizeof(T) == 4, "16 or 32 bits int required");
    return static_cast<int8_t>(value);
}

static inline uint32_t CPUReadOpenBus()
{
    /* DMA shadowing overrides everything */
    if (cpuDmaRunning || ((reg[15].I - cpuDmaPC) == (armState ? 4u : 2u)))
        return cpuDmaBusValue;

    /* THUMB: compose 32-bit from last fetched halfwords according to region/alignment */
    if (!armState) {
        uint32_t reg15 = reg[15].I;
        auto region = reg15 >> 24;

        switch (region)
        {
        /* Sequential regions: both halves from the newer fetch */
        case REGION_EWRAM: /* EWRAM */
        case REGION_PRAM: /* PALRAM */
        case REGION_VRAM: /* VRAM */
        case REGION_ROM0:
        case REGION_ROM0EX:
        case REGION_ROM1:
        case REGION_ROM1EX:
        case REGION_ROM2:
        case REGION_ROM2EX: /* CART regions */
            return cpuPrefetch[1] | (cpuPrefetch[1] << 16);

        /* BIOS / OAM: alignment affects low/high halves */
        case REGION_BIOS: /* BIOS */
        case REGION_OAM: /* OAM */
            if ((reg15 & 2) == 0)
                return cpuPrefetch[1] | (cpuPrefetch[1] << 16);
            return cpuPrefetch[0] | (cpuPrefetch[1] << 16);

        /* IWRAM: opposite order for aligned case */
        case REGION_IWRAM: /* IWRAM */
            if ((reg15 & 2) == 0)
                return cpuPrefetch[1] | (cpuPrefetch[0] << 16);
            return cpuPrefetch[0] | (cpuPrefetch[1] << 16);
        }
    }

    /* ARM: open bus is the last fetched 32-bit word */
    return cpuPrefetch[1];
}

static inline uint16_t ROMReadOOB(uint32_t address) {
    return (address >> 1) & 0xFFFF;
}

static inline uint32_t CPUReadMemory(uint32_t address)
{
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 2)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif
    uint32_t value = 0xFFFFFFFF;

    switch (address >> 24) {
    case REGION_BIOS:
        if (reg[15].I >> 24) {
            if (address < 0x4000) {
#ifdef GBA_LOGGING
                if (systemVerbose & VERBOSE_ILLEGAL_READ) {
                    log("Illegal word read from bios: %08x at %08x\n",
                        address,
                        armMode ? armNextPC - 4 : armNextPC - 2);
                }
#endif

                value = READ32LE(((uint32_t*)&biosProtected));
            } else
                goto unreadable;
        } else
            value = READ32LE(((uint32_t*)&g_bios[address & 0x3FFC]));
        break;
    case REGION_EWRAM:
        value = READ32LE(((uint32_t*)&g_workRAM[address & 0x3FFFC]));
        break;
    case REGION_IWRAM:
        value = READ32LE(((uint32_t*)&g_internalRAM[address & 0x7ffC]));
        break;
    case REGION_IO:
        if ((address < 0x4000400) && ioReadable[address & 0x3fc]) {
            if (ioReadable[(address & 0x3fc) + 2]) {
                value = READ32LE(((uint32_t*)&g_ioMem[address & 0x3fC]));
                if ((address & 0x3fc) == COMM_JOY_RECV_L)
                    UPDATE_REG(COMM_JOYSTAT,
                        READ16LE(&g_ioMem[COMM_JOYSTAT]) & ~JOYSTAT_RECV);
            } else {
                value = READ16LE(((uint16_t*)&g_ioMem[address & 0x3fc]));
            }
        } else
            goto unreadable;
        break;
    case REGION_PRAM:
        value = READ32LE(((uint32_t*)&g_paletteRAM[address & 0x3fC]));
        break;
    case REGION_VRAM: {
        unsigned addr = (address & 0x1fffc);
        if (((DISPCNT & 7) > 2) && ((addr & 0x1C000) == 0x18000)) {
            value = 0;
            break;
        }
        if ((addr & 0x18000) == 0x18000)
            addr &= 0x17fff;
        value = READ32LE(((uint32_t*)&g_vram[addr]));
        break;
    }
    case REGION_OAM:
        value = READ32LE(((uint32_t*)&g_oam[address & 0x3FC]));
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if ((address & 0x01FFFFFC) <= (gbaGetRomSize() - 4))
            value = READ32LE(((uint32_t *)&g_rom[address & 0x01FFFFFC]));
        else if (cpuEEPROMEnabled && ((address & eepromMask) == eepromMask))
            return 0; // ignore reads from eeprom region outside 0x0D page reads
        else {
            value = (uint16_t)ROMReadOOB(address & 0x01FFFFFC);
            value |= (uint16_t)ROMReadOOB((address & 0x01FFFFFC) + 2) << 16;
        }
        break;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled)
            // no need to swap this
            return eepromRead(address);
        goto unreadable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (cpuFlashEnabled | cpuSramEnabled) { // no need to swap this
            value = flashRead(address) * 0x01010101;
            break;
        }
	/* fallthrough */
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal word read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        value = CPUReadOpenBus();
        break;
    }

    if (address & 3) {
#ifdef C_CORE
        int shift = (address & 3) << 3;
        value = (value >> shift) | (value << (32 - shift));
#else
#ifdef __GNUC__
        asm("and $3, %%ecx;"
            "shl $3 ,%%ecx;"
            "ror %%cl, %0"
            : "=r"(value)
            : "r"(value), "c"(address));
#else
        __asm {
      mov ecx, address;
      and ecx, 3;
      shl ecx, 3;
      ror [dword ptr value], cl;
        }
#endif
#endif
    }

#ifdef GBA_LOGGING
    if (address & 3) {
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned word read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
    }
#endif
    return value;
}

static inline uint32_t CPUReadHalfWord(uint32_t address)
{
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    uint32_t value = 0xFFFFFFFF;

    switch (address >> 24) {
    case REGION_BIOS:
        if (reg[15].I >> 24) {
            if (address < 0x4000) {
#ifdef GBA_LOGGING
                if (systemVerbose & VERBOSE_ILLEGAL_READ) {
                    log("Illegal halfword read from bios: %08x at %08x\n",
                        address,
                        armMode ? armNextPC - 4 : armNextPC - 2);
                }
#endif
                value = READ16LE(((uint16_t*)&biosProtected[address & 2]));
            } else
                goto unreadable;
        } else
            value = READ16LE(((uint16_t*)&g_bios[address & 0x3FFE]));
        break;
    case REGION_EWRAM:
        value = READ16LE(((uint16_t*)&g_workRAM[address & 0x3FFFE]));
        break;
    case REGION_IWRAM:
        value = READ16LE(((uint16_t*)&g_internalRAM[address & 0x7ffe]));
        break;
    case REGION_IO:
        if ((address < 0x4000400) && ioReadable[address & 0x3fe]) {
            value = READ16LE(((uint16_t*)&g_ioMem[address & 0x3fe]));
            switch(address & 0x3FE) {
            case IO_REG_SOUND1CNT_X: value &= 0x4000; break;
            case IO_REG_SOUND2CNT_L: value &= 0xFFC0; break;
            case IO_REG_SOUND2CNT_H: value &= 0x4000; break;
            case IO_REG_SOUND3CNT_L: value &= 0x00E0; break;
            case IO_REG_SOUND3CNT_H: value &= 0xE000; break;
            case IO_REG_SOUND3CNT_X: value &= 0x4000; break;
            case IO_REG_SOUND4CNT_L: value &= 0xFF00; break;
            case IO_REG_SOUND4CNT_H: value &= 0x40FF; break;
            }
            if (((address & 0x3fe) > 0xFF) && ((address & 0x3fe) < 0x10E)) {
                if (((address & 0x3fe) == IO_REG_TM0CNT_L) && timer0On)
                    value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload);
                else if (((address & 0x3fe) == IO_REG_TM1CNT_L) && timer1On && !(TM1CNT & 4))
                    value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
                else if (((address & 0x3fe) == IO_REG_TM2CNT_L) && timer2On && !(TM2CNT & 4))
                    value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
                else if (((address & 0x3fe) == IO_REG_TM3CNT_L) && timer3On && !(TM3CNT & 4))
                    value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer3ClockReload);
            }
        } else if ((address < 0x4000400) && ioReadable[address & 0x3fc]) {
            value = 0;
        } else
            goto unreadable;
        break;
    case REGION_PRAM:
        value = READ16LE(((uint16_t*)&g_paletteRAM[address & 0x3fe]));
        break;
    case REGION_VRAM: {
        unsigned addr = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((addr & 0x1C000) == 0x18000)) {
            value = 0;
            break;
        }
        if ((addr & 0x18000) == 0x18000)
            addr &= 0x17fff;
        value = READ16LE(((uint16_t*)&g_vram[addr]));
        break;
    }
    case REGION_OAM:
        value = READ16LE(((uint16_t*)&g_oam[address & 0x3fe]));
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
            value = rtcRead(address);
        else if ((address & 0x01FFFFFE) <= (gbaGetRomSize() - 2))
            value = READ16LE(((uint16_t*)&g_rom[address & 0x01FFFFFE]));
        else if (cpuEEPROMEnabled && ((address & eepromMask) == eepromMask))
            return 0; // ignore reads from eeprom region outside 0x0D page reads
        else
            value = (uint16_t)ROMReadOOB(address & 0x01FFFFFE);
        break;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled)
            // no need to swap this
            return eepromRead(address);
        goto unreadable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (cpuFlashEnabled | cpuSramEnabled) {
            // no need to swap this
            value = flashRead(address) * 0x0101;
            break;
        }
	/* fallthrough */
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal halfword read: %08x at %08x (%08x)\n",
                address,
                reg[15].I,
                value);
        }
#endif
        value = (uint16_t)(CPUReadOpenBus() >> (8 * address & 2));
        break;
    }

    if (address & 1) {
        value = (value >> 8) | (value << 24);
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned halfword read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
#endif
    }

    return value;
}

static inline int32_t CPUReadHalfWordSigned(uint32_t address)
{
    uint16_t value = static_cast<uint16_t>(CPUReadHalfWord(address));
    if ((address & 1)) {
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned signed halfword read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
#endif
        return (int32_t)(int8_t)value;
    }
    return (int32_t)(int16_t)value;
}

static inline uint8_t CPUReadByte(uint32_t address)
{
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 0)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_BIOS:
        if (reg[15].I >> 24) {
            if (address < 0x4000) {
#ifdef GBA_LOGGING
                if (systemVerbose & VERBOSE_ILLEGAL_READ) {
                    log("Illegal byte read from bios: %08x at %08x\n",
                        address,
                        armMode ? armNextPC - 4 : armNextPC - 2);
                }
#endif
                return biosProtected[address & 3];
            } else
                goto unreadable;
        }
        return g_bios[address & 0x3FFF];
    case REGION_EWRAM:
        return g_workRAM[address & 0x3FFFF];
    case REGION_IWRAM:
        return g_internalRAM[address & 0x7fff];
    case REGION_IO:
        if ((address < 0x4000400) && ioReadable[address & 0x3ff])
            return g_ioMem[address & 0x3ff];
        else
            goto unreadable;
    case REGION_PRAM:
        return g_paletteRAM[address & 0x3ff];
    case REGION_VRAM:
        address = (address & 0x1ffff);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return 0;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;
        return g_vram[address];
    case REGION_OAM:
        return g_oam[address & 0x3ff];
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if ((address & 0x01FFFFFF) <= gbaGetRomSize())
            return g_rom[address & 0x01FFFFFF];
        else if (cpuEEPROMEnabled && ((address & eepromMask) == eepromMask))
            return 0; // ignore reads from eeprom region outside 0x0D page reads
        return (uint8_t)ROMReadOOB(address & 0x01FFFFFE);
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled)
            return DowncastU8(eepromRead(address));
        goto unreadable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if (cpuSramEnabled | cpuFlashEnabled)
            return flashRead(address);

        switch (address & 0x00008f00) {
        case 0x8200:
            return DowncastU8(systemGetSensorX());
        case 0x8300:
            return DowncastU8((systemGetSensorX() >> 8) | 0x80);
        case 0x8400:
            return DowncastU8(systemGetSensorY());
        case 0x8500:
            return DowncastU8(systemGetSensorY() >> 8);
        }
        return 0xFF;
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal byte read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        return (uint8_t)(CPUReadOpenBus() >> (8 * (address & 3)));
    }
}

static inline void CPUWriteMemory(uint32_t address, uint32_t value)
{
#ifdef GBA_LOGGING
    if (address & 3) {
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned word write: %08x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
    }
#endif

#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, value, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_EWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeWorkRAM[address & 0x3FFFC]))
            cheatsWriteMemory(address & 0x203FFFC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_workRAM[address & 0x3FFFC]), value);
        break;
    case REGION_IWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeInternalRAM[address & 0x7ffc]))
            cheatsWriteMemory(address & 0x3007FFC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_internalRAM[address & 0x7ffC]), value);
        break;
    case REGION_IO:
        if (address < 0x4000400) {
            CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
            CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
        } else
            goto unwritable;
        break;
    case REGION_PRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezePRAM[address & 0x3fc]))
            cheatsWriteMemory(address & 0x70003FC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_paletteRAM[address & 0x3FC]), value);
        break;
    case REGION_VRAM:
        address = (address & 0x1fffc);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;

#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeVRAM[address]))
            cheatsWriteMemory(address + 0x06000000, value);
        else
#endif

            WRITE32LE(((uint32_t*)&g_vram[address]), value);
        break;
    case REGION_OAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint32_t*)&freezeOAM[address & 0x3fc]))
            cheatsWriteMemory(address & 0x70003FC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_oam[address & 0x3fc]), value);
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (GBAMatrix.size && (address & 0x01FFFF00) == 0x00800100)
        {
            GBAMatrixWrite(&GBAMatrix, address & 0x3C, value);
            break;
        }
        goto unwritable;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, DowncastU8(value));
            break;
        }
        goto unwritable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
            (*cpuSaveGameFunc)(address, (uint8_t)(value >> (8 * (address & 3))));
            break;
        }
        goto unwritable;
    // fallthrough
    default:
    unwritable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_WRITE) {
            log("Illegal word write: %08x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        break;
    }
}

static inline void CPUWriteHalfWord(uint32_t address, uint16_t value)
{
#ifdef GBA_LOGGING
    if (address & 1) {
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned halfword write: %04x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
    }
#endif

#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, value, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_EWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeWorkRAM[address & 0x3FFFE]))
            cheatsWriteHalfWord(address & 0x203FFFE, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_workRAM[address & 0x3FFFE]), value);
        break;
    case REGION_IWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeInternalRAM[address & 0x7ffe]))
            cheatsWriteHalfWord(address & 0x3007ffe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_internalRAM[address & 0x7ffe]), value);
        break;
    case REGION_IO:
        if (address < 0x4000400)
            CPUUpdateRegister(address & 0x3fe, value);
        else
            goto unwritable;
        break;
    case REGION_PRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezePRAM[address & 0x03fe]))
            cheatsWriteHalfWord(address & 0x70003fe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_paletteRAM[address & 0x3fe]), value);
        break;
    case REGION_VRAM:
        address = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeVRAM[address]))
            cheatsWriteHalfWord(address + 0x06000000, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_vram[address]), value);
        break;
    case REGION_OAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (*((uint16_t*)&freezeOAM[address & 0x03fe]))
            cheatsWriteHalfWord(address & 0x70003fe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_oam[address & 0x3fe]), value);
        break;
    case REGION_ROM0:
    case REGION_ROM0EX:
        if (GBAMatrix.size && (address & 0x01FFFF00) == 0x00800100)
        {
            GBAMatrixWrite16(&GBAMatrix, address & 0x3C, value);
            break;
        }
        if (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
            if (!rtcWrite(address, value))
                goto unwritable;
        } else if (!agbPrintWrite(address, value))
            goto unwritable;
        break;
    case REGION_ROM1:
    case REGION_ROM1EX:
    case REGION_ROM2:
        if (GBAMatrix.size && (address & 0x01FFFF00) == 0x00800100)
        {
            GBAMatrixWrite16(&GBAMatrix, address & 0x3C, value);
            break;
        }
        goto unwritable;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, (uint8_t)value);
            break;
        }
        goto unwritable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
            (*cpuSaveGameFunc)(address, (uint8_t)(value >> (8 * (address & 1))));
            break;
        }
        /* fallthrough */
    default:
    unwritable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_WRITE) {
            log("Illegal halfword write: %04x to %08x from %08x\n",
                value,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        break;
    }
}

static inline void CPUWriteByte(uint32_t address, uint8_t b)
{
#ifdef VBAM_ENABLE_DEBUGGER
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, b, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case REGION_EWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (freezeWorkRAM[address & 0x3FFFF])
            cheatsWriteByte(address & 0x203FFFF, b);
        else
#endif
            g_workRAM[address & 0x3FFFF] = b;
        break;
    case REGION_IWRAM:
#ifdef VBAM_ENABLE_DEBUGGER
        if (freezeInternalRAM[address & 0x7fff])
            cheatsWriteByte(address & 0x3007fff, b);
        else
#endif
            g_internalRAM[address & 0x7fff] = b;
        break;
    case REGION_IO:
        if (address < 0x4000400) {
            switch (address & 0x3FF) {
            case IO_REG_SOUND1CNT_L:
            //case IO_REG_SOUND1CNT_L + 1:
            case IO_REG_SOUND1CNT_H:
            case IO_REG_SOUND1CNT_H + 1:
            case IO_REG_SOUND1CNT_X:
            case IO_REG_SOUND1CNT_X + 1:
            case IO_REG_SOUND2CNT_L:
            case IO_REG_SOUND2CNT_L + 1:
            case IO_REG_SOUND2CNT_H:
            case IO_REG_SOUND2CNT_H + 1:
            case IO_REG_SOUND3CNT_L:
            case IO_REG_SOUND3CNT_L + 1:
            case IO_REG_SOUND3CNT_H:
            case IO_REG_SOUND3CNT_H + 1:
            case IO_REG_SOUND3CNT_X:
            case IO_REG_SOUND3CNT_X + 1:
            case IO_REG_SOUND4CNT_L:
            case IO_REG_SOUND4CNT_L + 1:
            case IO_REG_SOUND4CNT_H:
            case IO_REG_SOUND4CNT_H + 1:
            case IO_REG_SOUNDCNT_L:
            case IO_REG_SOUNDCNT_L + 1:
            //case IO_REG_SOUNDCNT_H:
            //case IO_REG_SOUNDCNT_H + 1:
            case IO_REG_SOUNDCNT_X:
            //case IO_REG_SOUNDCNT_X + 1:
            //case IO_REG_SOUNDBIAS:
            //case IO_REG_SOUNDBIAS + 1:
            case IO_REG_WAVE_RAM0_L + 0:
            case IO_REG_WAVE_RAM0_L + 1:
            case IO_REG_WAVE_RAM0_L + 2:
            case IO_REG_WAVE_RAM0_L + 3:
            case IO_REG_WAVE_RAM1_L + 0:
            case IO_REG_WAVE_RAM1_L + 1:
            case IO_REG_WAVE_RAM1_L + 2:
            case IO_REG_WAVE_RAM1_L + 3:
            case IO_REG_WAVE_RAM2_L + 0:
            case IO_REG_WAVE_RAM2_L + 1:
            case IO_REG_WAVE_RAM2_L + 2:
            case IO_REG_WAVE_RAM2_L + 3:
            case IO_REG_WAVE_RAM3_L + 0:
            case IO_REG_WAVE_RAM3_L + 1:
            case IO_REG_WAVE_RAM3_L + 2:
            case IO_REG_WAVE_RAM3_L + 3:
                soundEvent8(address & 0xFF, b);
                break;
            case IO_REG_HALTCNT: // HALTCNT, undocumented
                if (b == 0x80)
                    stopState = true;
                holdState = 1;
                holdType = -1;
                cpuNextEvent = cpuTotalTicks;
                break;
            default: // every other register
                uint32_t lowerBits = address & 0x3fe;
                if (address & 1) {
                    CPUUpdateRegister(lowerBits,
                        (READ16LE(&g_ioMem[lowerBits]) & 0x00FF) | (b << 8));
                } else {
                    CPUUpdateRegister(lowerBits,
                        (READ16LE(&g_ioMem[lowerBits]) & 0xFF00) | b);
                }
            }
            break;
        } else
            goto unwritable;
        break;
    case REGION_PRAM:
        // no need to switch
        *((uint16_t*)&g_paletteRAM[address & 0x3FE]) = (b << 8) | b;
        break;
    case REGION_VRAM:
        address = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;

        // no need to switch
        // byte writes to OBJ VRAM are ignored
        if ((address) < objTilesAddress[((DISPCNT & 7) + 1) >> 2]) {
#ifdef VBAM_ENABLE_DEBUGGER
            if (freezeVRAM[address])
                cheatsWriteByte(address + 0x06000000, b);
            else
#endif
                *((uint16_t*)&g_vram[address]) = (b << 8) | b;
        }
        break;
    case REGION_OAM:
        // no need to switch
        // byte writes to OAM are ignored
        //    *((uint16_t *)&g_oam[address & 0x3FE]) = (b << 8) | b;
        break;
    case REGION_ROM2EX:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, b);
            break;
        }
        goto unwritable;
    case REGION_SRAM:
    case REGION_SRAMEX:
        if ((coreOptions.saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)) {
            (*cpuSaveGameFunc)(address, b);
            break;
        }
        goto unwritable;
    // default
    default:
    unwritable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_WRITE) {
            log("Illegal byte write: %02x to %08x from %08x\n",
                b,
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        break;
    }
}

#endif  // VBAM_CORE_GBA_GBAINLINE_H_
