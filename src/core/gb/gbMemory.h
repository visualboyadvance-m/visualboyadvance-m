#ifndef VBAM_CORE_GB_GBMEMORY_H_
#define VBAM_CORE_GB_GBMEMORY_H_

#include <cstdint>
#include <ctime>

struct mapperMBC1 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperMemoryModel;
    int mapperROMHighAddress;
    int mapperRAMAddress;
    int mapperRomBank0Remapping;
};

struct mapperMBC2 {
    int mapperRAMEnable;
    int mapperROMBank;
};

struct mapperMBC3 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperRAMAddress;
    int mapperClockLatch;
    int mapperClockRegister;
    int mapperSeconds;
    int mapperMinutes;
    int mapperHours;
    int mapperDays;
    int mapperControl;
    int mapperLSeconds;
    int mapperLMinutes;
    int mapperLHours;
    int mapperLDays;
    int mapperLControl;
    union {
        time_t mapperLastTime;
        uint64_t _time_pad; /* so that 32bit and 64bit saves are compatible */
    };
};

struct mapperMBC5 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperROMHighAddress;
    int mapperRAMAddress;
    int isRumbleCartridge;
};

struct mapperMBC7 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperRAMAddress;
    int cs;
    int sk;
    int state;
    int buffer;
    int idle;
    int count;
    int code;
    int address;
    int writeEnable;
    int value;
};

struct mapperHuC1 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperMemoryModel;
    int mapperROMHighAddress;
    int mapperRAMAddress;
};

struct mapperHuC3 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperRAMAddress;
    int mapperAddress;
    int mapperRAMFlag;
    int mapperRAMValue;
    int mapperRegister1;
    int mapperRegister2;
    int mapperRegister3;
    int mapperRegister4;
    int mapperRegister5;
    int mapperRegister6;
    int mapperRegister7;
    int mapperRegister8;
};

struct mapperHuC3RTC {
    union {
        time_t mapperLastTime;
        uint64_t _time_pad; /* so that 32bit and 64bit saves are compatible */
    };
    unsigned mapperDateTime;
    unsigned mapperWritingTime;
    unsigned memoryTimerRead;
    unsigned mapperClockShift;
    unsigned reserved[4];
};

struct mapperTAMA5 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperRAMAddress;
    int mapperRamByteSelect;
    int mapperCommandNumber;
    int mapperLastCommandNumber;
    int mapperCommands[0x10];
    int mapperRegister;
    int mapperClockLatch;
    int mapperClockRegister;
    int mapperSeconds;
    int mapperMinutes;
    int mapperHours;
    int mapperDays;
    int mapperMonths;
    int mapperYears;
    int mapperControl;
    int mapperLSeconds;
    int mapperLMinutes;
    int mapperLHours;
    int mapperLDays;
    int mapperLMonths;
    int mapperLYears;
    int mapperLControl;
    union {
        time_t mapperLastTime;
        uint64_t _time_pad; /* so that 32bit and 64bit saves are compatible */
    };
};

struct mapperMMM01 {
    int mapperRAMEnable;
    int mapperROMBank;
    int mapperRAMBank;
    int mapperMemoryModel;
    int mapperROMHighAddress;
    int mapperRAMAddress;
    int mapperRomBank0Remapping;
};

struct mapperGS3 {
    int mapperROMBank;
};

extern mapperMBC1 gbDataMBC1;
extern mapperMBC2 gbDataMBC2;
extern mapperMBC3 gbDataMBC3;
extern mapperMBC5 gbDataMBC5;
extern mapperMBC7 gbDataMBC7;
extern mapperHuC1 gbDataHuC1;
extern mapperHuC3 gbDataHuC3;
extern mapperHuC3RTC gbRTCHuC3;
extern mapperTAMA5 gbDataTAMA5;
extern mapperMMM01 gbDataMMM01;
extern mapperGS3 gbDataGS3;

void mapperMBC1ROM(uint16_t, uint8_t);
void mapperMBC1RAM(uint16_t, uint8_t);
uint8_t mapperMBC1ReadRAM(uint16_t);
void mapperMBC2ROM(uint16_t, uint8_t);
void mapperMBC2RAM(uint16_t, uint8_t);
void mapperMBC3ROM(uint16_t, uint8_t);
void mapperMBC3RAM(uint16_t, uint8_t);
uint8_t mapperMBC3ReadRAM(uint16_t);
void mapperMBC5ROM(uint16_t, uint8_t);
void mapperMBC5RAM(uint16_t, uint8_t);
uint8_t mapperMBC5ReadRAM(uint16_t);
void mapperMBC7ROM(uint16_t, uint8_t);
void mapperMBC7RAM(uint16_t, uint8_t);
uint8_t mapperMBC7ReadRAM(uint16_t);
void mapperHuC1ROM(uint16_t, uint8_t);
void mapperHuC1RAM(uint16_t, uint8_t);
void mapperHuC3ROM(uint16_t, uint8_t);
void mapperHuC3RAM(uint16_t, uint8_t);
uint8_t mapperHuC3ReadRAM(uint16_t);
void mapperTAMA5RAM(uint16_t, uint8_t);
uint8_t mapperTAMA5ReadRAM(uint16_t);
void memoryUpdateTAMA5Clock();
void mapperMMM01ROM(uint16_t, uint8_t);
void mapperMMM01RAM(uint16_t, uint8_t);
void mapperGGROM(uint16_t, uint8_t);
void mapperGS3ROM(uint16_t, uint8_t);

extern void memoryUpdateMapMBC1();
extern void memoryUpdateMapMBC2();
extern void memoryUpdateMapMBC3();
extern void memoryUpdateMapMBC5();
extern void memoryUpdateMapMBC7();
extern void memoryUpdateMapHuC1();
extern void memoryUpdateMapHuC3();
extern void memoryUpdateMapTAMA5();
extern void memoryUpdateMapMMM01();
extern void memoryUpdateMapGS3();

#define MBC3_RTC_DATA_SIZE  sizeof(int) * 10 + sizeof(uint64_t)

#define TAMA5_RTC_DATA_SIZE sizeof(int) * 14 + sizeof(uint64_t)

#define HUC3_RTC_DATA_SIZE sizeof(int) * 4 + sizeof(uint64_t)

#endif // VBAM_CORE_GB_GBMEMORY_H_
