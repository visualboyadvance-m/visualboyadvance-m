#ifndef GBAINLINE_H
#define GBAINLINE_H

#include <type_traits>

#include "../System.h"
#include "../common/Port.h"
#include "GBALink.h"
#include "GBAcpu.h"
#include "RTC.h"
#include "Sound.h"
#include "agbprint.h"
#include "remote.h"
#include "stdint.h"

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
extern uint32_t cpuDmaLast;
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

extern uint32_t myROM[];

static inline uint32_t CPUReadMemory(uint32_t address)
{
#ifdef BKPT_SUPPORT
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 2)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif
    uint32_t value = 0;

    switch (address >> 24) {
    case 0:
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
    case 2:
        value = READ32LE(((uint32_t*)&g_workRAM[address & 0x3FFFC]));
        break;
    case 3:
        value = READ32LE(((uint32_t*)&g_internalRAM[address & 0x7ffC]));
        break;
    case 4:
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
    case 5:
        value = READ32LE(((uint32_t*)&g_paletteRAM[address & 0x3fC]));
        break;
    case 6: {
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
    case 7:
        value = READ32LE(((uint32_t*)&g_oam[address & 0x3FC]));
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        value = READ32LE(((uint32_t*)&g_rom[address & 0x1FFFFFC]));
        break;
    case 13:
        if (cpuEEPROMEnabled)
            // no need to swap this
            return eepromRead(address);
        goto unreadable;
    case 14:
    case 15:
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
        if (cpuDmaRunning || ((reg[15].I - cpuDmaPC) == (armState ? 4u : 2u))) {
            value = cpuDmaLast;
        } else {
            if (armState) {
                value = CPUReadMemoryQuick(reg[15].I);
            } else {
                value = CPUReadHalfWordQuick(reg[15].I) | CPUReadHalfWordQuick(reg[15].I) << 16;
            }
        }
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
#ifdef BKPT_SUPPORT
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    uint32_t value = 0;

    switch (address >> 24) {
    case 0:
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
    case 2:
        value = READ16LE(((uint16_t*)&g_workRAM[address & 0x3FFFE]));
        break;
    case 3:
        value = READ16LE(((uint16_t*)&g_internalRAM[address & 0x7ffe]));
        break;
    case 4:
        if ((address < 0x4000400) && ioReadable[address & 0x3fe]) {
            value = READ16LE(((uint16_t*)&g_ioMem[address & 0x3fe]));
            if (((address & 0x3fe) > 0xFF) && ((address & 0x3fe) < 0x10E)) {
                if (((address & 0x3fe) == 0x100) && timer0On)
                    value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload);
                else if (((address & 0x3fe) == 0x104) && timer1On && !(TM1CNT & 4))
                    value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
                else if (((address & 0x3fe) == 0x108) && timer2On && !(TM2CNT & 4))
                    value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
                else if (((address & 0x3fe) == 0x10C) && timer3On && !(TM3CNT & 4))
                    value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer3ClockReload);
            }
        } else if ((address < 0x4000400) && ioReadable[address & 0x3fc]) {
            value = 0;
        } else
            goto unreadable;
        break;
    case 5:
        value = READ16LE(((uint16_t*)&g_paletteRAM[address & 0x3fe]));
        break;
    case 6: {
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
    case 7:
        value = READ16LE(((uint16_t*)&g_oam[address & 0x3fe]));
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        if (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
            value = rtcRead(address);
        else
            value = READ16LE(((uint16_t*)&g_rom[address & 0x1FFFFFE]));
        break;
    case 13:
        if (cpuEEPROMEnabled)
            // no need to swap this
            return eepromRead(address);
        goto unreadable;
    case 14:
    case 15:
        if (cpuFlashEnabled | cpuSramEnabled) {
            // no need to swap this
            value = flashRead(address) * 0x0101;
            break;
        }
	/* fallthrough */
    default:
    unreadable:
        if (cpuDmaRunning|| ((reg[15].I - cpuDmaPC) == (armState ? 4u : 2u))) {
            value = cpuDmaLast & 0xFFFF;
        } else {
            int param = reg[15].I;
            if (armState)
                param += (address & 2);
            value = CPUReadHalfWordQuick(param);
        }
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal halfword read: %08x at %08x (%08x)\n",
                address,
                reg[15].I,
                value);
        }
#endif
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

static inline int16_t CPUReadHalfWordSigned(uint32_t address)
{
    int32_t value = (int32_t)CPUReadHalfWord(address);
    if ((address & 1)) {
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
            log("Unaligned signed halfword read from: %08x at %08x (%08x)\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2,
                value);
        }
#endif
        return (int8_t)value;
    }
    return (int16_t)value;
}

static inline uint8_t CPUReadByte(uint32_t address)
{
#ifdef BKPT_SUPPORT
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakReadCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnRead(address, 0)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case 0:
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
    case 2:
        return g_workRAM[address & 0x3FFFF];
    case 3:
        return g_internalRAM[address & 0x7fff];
    case 4:
        if ((address < 0x4000400) && ioReadable[address & 0x3ff])
            return g_ioMem[address & 0x3ff];
        else
            goto unreadable;
    case 5:
        return g_paletteRAM[address & 0x3ff];
    case 6:
        address = (address & 0x1ffff);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return 0;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;
        return g_vram[address];
    case 7:
        return g_oam[address & 0x3ff];
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        return g_rom[address & 0x1FFFFFF];
    case 13:
        if (cpuEEPROMEnabled)
            return DowncastU8(eepromRead(address));
        goto unreadable;
    case 14:
    case 15:
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
	/* fallthrough */
    default:
    unreadable:
#ifdef GBA_LOGGING
        if (systemVerbose & VERBOSE_ILLEGAL_READ) {
            log("Illegal byte read: %08x at %08x\n",
                address,
                armMode ? armNextPC - 4 : armNextPC - 2);
        }
#endif
        if (cpuDmaRunning || ((reg[15].I - cpuDmaPC) == (armState ? 4u : 2u))) {
            return cpuDmaLast & 0xFF;
        } else {
            if (armState) {
                return CPUReadByteQuick(reg[15].I + (address & 3));
            } else {
                return CPUReadByteQuick(reg[15].I + (address & 1));
            }
        }
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

#ifdef BKPT_SUPPORT
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, value, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case 0x02:
#ifdef BKPT_SUPPORT
        if (*((uint32_t*)&freezeWorkRAM[address & 0x3FFFC]))
            cheatsWriteMemory(address & 0x203FFFC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_workRAM[address & 0x3FFFC]), value);
        break;
    case 0x03:
#ifdef BKPT_SUPPORT
        if (*((uint32_t*)&freezeInternalRAM[address & 0x7ffc]))
            cheatsWriteMemory(address & 0x3007FFC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_internalRAM[address & 0x7ffC]), value);
        break;
    case 0x04:
        if (address < 0x4000400) {
            CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
            CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
        } else
            goto unwritable;
        break;
    case 0x05:
#ifdef BKPT_SUPPORT
        if (*((uint32_t*)&freezePRAM[address & 0x3fc]))
            cheatsWriteMemory(address & 0x70003FC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_paletteRAM[address & 0x3FC]), value);
        break;
    case 0x06:
        address = (address & 0x1fffc);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;

#ifdef BKPT_SUPPORT
        if (*((uint32_t*)&freezeVRAM[address]))
            cheatsWriteMemory(address + 0x06000000, value);
        else
#endif

            WRITE32LE(((uint32_t*)&g_vram[address]), value);
        break;
    case 0x07:
#ifdef BKPT_SUPPORT
        if (*((uint32_t*)&freezeOAM[address & 0x3fc]))
            cheatsWriteMemory(address & 0x70003FC, value);
        else
#endif
            WRITE32LE(((uint32_t*)&g_oam[address & 0x3fc]), value);
        break;
    case 0x0D:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, DowncastU8(value));
            break;
        }
        goto unwritable;
    case 0x0E:
    case 0x0F:
        if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
            (*cpuSaveGameFunc)(address, (uint8_t)value);
            break;
        }
    // default
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

#ifdef BKPT_SUPPORT
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, value, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case 2:
#ifdef BKPT_SUPPORT
        if (*((uint16_t*)&freezeWorkRAM[address & 0x3FFFE]))
            cheatsWriteHalfWord(address & 0x203FFFE, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_workRAM[address & 0x3FFFE]), value);
        break;
    case 3:
#ifdef BKPT_SUPPORT
        if (*((uint16_t*)&freezeInternalRAM[address & 0x7ffe]))
            cheatsWriteHalfWord(address & 0x3007ffe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_internalRAM[address & 0x7ffe]), value);
        break;
    case 4:
        if (address < 0x4000400)
            CPUUpdateRegister(address & 0x3fe, value);
        else
            goto unwritable;
        break;
    case 5:
#ifdef BKPT_SUPPORT
        if (*((uint16_t*)&freezePRAM[address & 0x03fe]))
            cheatsWriteHalfWord(address & 0x70003fe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_paletteRAM[address & 0x3fe]), value);
        break;
    case 6:
        address = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;
#ifdef BKPT_SUPPORT
        if (*((uint16_t*)&freezeVRAM[address]))
            cheatsWriteHalfWord(address + 0x06000000, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_vram[address]), value);
        break;
    case 7:
#ifdef BKPT_SUPPORT
        if (*((uint16_t*)&freezeOAM[address & 0x03fe]))
            cheatsWriteHalfWord(address & 0x70003fe, value);
        else
#endif
            WRITE16LE(((uint16_t*)&g_oam[address & 0x3fe]), value);
        break;
    case 8:
    case 9:
        if (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
            if (!rtcWrite(address, value))
                goto unwritable;
        } else if (!agbPrintWrite(address, value))
            goto unwritable;
        break;
    case 13:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, (uint8_t)value);
            break;
        }
        goto unwritable;
    case 14:
    case 15:
        if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled) {
            (*cpuSaveGameFunc)(address, (uint8_t)value);
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
#ifdef BKPT_SUPPORT
    memoryMap* m = &map[address >> 24];
    if (m->breakPoints && BreakWriteCheck(m->breakPoints, address & m->mask)) {
        if (debuggerBreakOnWrite(address, b, 1)) {
            // CPU_BREAK_LOOP_2;
        }
    }
#endif

    switch (address >> 24) {
    case 2:
#ifdef BKPT_SUPPORT
        if (freezeWorkRAM[address & 0x3FFFF])
            cheatsWriteByte(address & 0x203FFFF, b);
        else
#endif
            g_workRAM[address & 0x3FFFF] = b;
        break;
    case 3:
#ifdef BKPT_SUPPORT
        if (freezeInternalRAM[address & 0x7fff])
            cheatsWriteByte(address & 0x3007fff, b);
        else
#endif
            g_internalRAM[address & 0x7fff] = b;
        break;
    case 4:
        if (address < 0x4000400) {
            switch (address & 0x3FF) {
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x68:
            case 0x69:
            case 0x6c:
            case 0x6d:
            case 0x70:
            case 0x71:
            case 0x72:
            case 0x73:
            case 0x74:
            case 0x75:
            case 0x78:
            case 0x79:
            case 0x7c:
            case 0x7d:
            case 0x80:
            case 0x81:
            case 0x84:
            case 0x85:
            case 0x90:
            case 0x91:
            case 0x92:
            case 0x93:
            case 0x94:
            case 0x95:
            case 0x96:
            case 0x97:
            case 0x98:
            case 0x99:
            case 0x9a:
            case 0x9b:
            case 0x9c:
            case 0x9d:
            case 0x9e:
            case 0x9f:
                soundEvent8(address & 0xFF, b);
                break;
            case 0x301: // HALTCNT, undocumented
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
    case 5:
        // no need to switch
        *((uint16_t*)&g_paletteRAM[address & 0x3FE]) = (b << 8) | b;
        break;
    case 6:
        address = (address & 0x1fffe);
        if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
            return;
        if ((address & 0x18000) == 0x18000)
            address &= 0x17fff;

        // no need to switch
        // byte writes to OBJ VRAM are ignored
        if ((address) < objTilesAddress[((DISPCNT & 7) + 1) >> 2]) {
#ifdef BKPT_SUPPORT
            if (freezeVRAM[address])
                cheatsWriteByte(address + 0x06000000, b);
            else
#endif
                *((uint16_t*)&g_vram[address]) = (b << 8) | b;
        }
        break;
    case 7:
        // no need to switch
        // byte writes to OAM are ignored
        //    *((uint16_t *)&g_oam[address & 0x3FE]) = (b << 8) | b;
        break;
    case 13:
        if (cpuEEPROMEnabled) {
            eepromWrite(address, b);
            break;
        }
        goto unwritable;
    case 14:
    case 15:
        if ((coreOptions.saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)) {
            // if(!cpuEEPROMEnabled && (cpuSramEnabled | cpuFlashEnabled)) {

            (*cpuSaveGameFunc)(address, b);
            break;
        }
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

#endif // GBAINLINE_H
