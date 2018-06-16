#include "../NLS.h"
#include "../System.h"
#include "../Util.h"
#include "../common/Port.h"
#include "GBA.h"
#include "Globals.h"

#include <memory.h>
#include <string.h>
#include <time.h>

enum RTCSTATE {
    IDLE = 0,
    COMMAND,
    DATA,
    READDATA
};

typedef struct
{
    uint8_t byte0;
    uint8_t select;
    uint8_t enable;
    uint8_t command;
    int dataLen;
    int bits;
    RTCSTATE state;
    uint8_t data[12];
    // reserved variables for future
    uint8_t reserved[12];
    bool reserved2;
    uint32_t reserved3;
} RTCCLOCKDATA;

struct tm gba_time;
static RTCCLOCKDATA rtcClockData;
static bool rtcClockEnabled = true;
static bool rtcRumbleEnabled = false;

uint32_t countTicks = 0;

void rtcEnable(bool e)
{
    rtcClockEnabled = e;
}

bool rtcIsEnabled()
{
    return rtcClockEnabled;
}

void rtcEnableRumble(bool e)
{
    rtcRumbleEnabled = e;
}

uint16_t rtcRead(uint32_t address)
{
    int res = 0;

    switch (address) {
    case 0x80000c8:
        return rtcClockData.enable;
        break;

    case 0x80000c6:
        return rtcClockData.select;
        break;

    case 0x80000c4:
        if (!(rtcClockData.enable & 1)) {
            return 0;
        }

        // Boktai Solar Sensor
        if (rtcClockData.select == 0x07) {
            if (rtcClockData.reserved[11] >= systemGetSensorDarkness()) {
                res |= 8;
            }
        }

        // WarioWare Twisted Tilt Sensor
        if (rtcClockData.select == 0x0b) {
            uint16_t v = systemGetSensorZ();
            v = 0x6C0 + v;
            res |= ((v >> rtcClockData.reserved[11]) & 1) << 2;
        }

        // Real Time Clock
        if (rtcClockEnabled && (rtcClockData.select & 0x04)) {
            res |= rtcClockData.byte0;
        }

        return res;
        break;
    }

    return READ16LE((&rom[address & 0x1FFFFFE]));
}

static uint8_t toBCD(uint8_t value)
{
    value = value % 100;
    int l = value % 10;
    int h = value / 10;
    return h * 16 + l;
}

void SetGBATime()
{
    time_t long_time;
    time(&long_time); /* Get time as long integer. */
    gba_time = *localtime(&long_time); /* Convert to local time. */
}

void rtcUpdateTime(int ticks)
{
    countTicks += ticks;

    if (countTicks > TICKS_PER_SECOND) {
        countTicks -= TICKS_PER_SECOND;
        gba_time.tm_sec++;
        mktime(&gba_time);
    }
}

bool rtcWrite(uint32_t address, uint16_t value)
{
    if (address == 0x80000c8) {
        rtcClockData.enable = (uint8_t)value; // bit 0 = enable reading from 0x80000c4 c6 and c8
    } else if (address == 0x80000c6) {
        rtcClockData.select = (uint8_t)value; // 0=read/1=write (for each of 4 low bits)

        // rumble is off when not writing to that pin
        if (rtcRumbleEnabled && !(value & 8))
            systemCartridgeRumble(false);
    } else if (address == 0x80000c4) // 4 bits of I/O Port Data (upper bits not used)
    {
        // WarioWare Twisted rumble
        if (rtcRumbleEnabled && (rtcClockData.select & 0x08)) {
            systemCartridgeRumble(value & 8);
        }

        // Boktai solar sensor
        if (rtcClockData.select == 0x07) {
            if (value & 2) {
                // reset counter to 0
                rtcClockData.reserved[11] = 0;
            }

            if ((value & 1) && !(rtcClockData.reserved[10] & 1)) {
                // increase counter, ready to do another read
                if (rtcClockData.reserved[11] < 255) {
                    rtcClockData.reserved[11]++;
                } else {
                    rtcClockData.reserved[11] = 0;
                }
            }

            rtcClockData.reserved[10] = value & rtcClockData.select;
        }

        // WarioWare Twisted rotation sensor
        if (rtcClockData.select == 0x0b) {
            if (value & 2) {
                // clock goes high in preperation for reading a bit
                rtcClockData.reserved[11]--;
            }

            if (value & 1) {
                // start ADC conversion
                rtcClockData.reserved[11] = 15;
            }

            rtcClockData.byte0 = value & rtcClockData.select;
        }

        // Real Time Clock
        if (rtcClockData.select & 4) {
            if (rtcClockData.state == IDLE && rtcClockData.byte0 == 1 && value == 5) {
                rtcClockData.state = COMMAND;
                rtcClockData.bits = 0;
                rtcClockData.command = 0;
            } else if (!(rtcClockData.byte0 & 1) && (value & 1)) // bit transfer
            {
                rtcClockData.byte0 = (uint8_t)value;

                switch (rtcClockData.state) {
                case COMMAND:
                    rtcClockData.command |= ((value & 2) >> 1) << (7 - rtcClockData.bits);
                    rtcClockData.bits++;

                    if (rtcClockData.bits == 8) {
                        rtcClockData.bits = 0;

                        switch (rtcClockData.command) {
                        case 0x60:
                            // not sure what this command does but it doesn't take parameters
                            // maybe it is a reset or stop
                            rtcClockData.state = IDLE;
                            rtcClockData.bits = 0;
                            break;

                        case 0x62:
                            // this sets the control state but not sure what those values are
                            rtcClockData.state = READDATA;
                            rtcClockData.dataLen = 1;
                            break;

                        case 0x63:
                            rtcClockData.dataLen = 1;
                            rtcClockData.data[0] = 0x40;
                            rtcClockData.state = DATA;
                            break;

                        case 0x64:
                            break;

                        case 0x65: {
                            if (rtcEnabled)
                                SetGBATime();

                            rtcClockData.dataLen = 7;
                            rtcClockData.data[0] = toBCD(gba_time.tm_year);
                            rtcClockData.data[1] = toBCD(gba_time.tm_mon + 1);
                            rtcClockData.data[2] = toBCD(gba_time.tm_mday);
                            rtcClockData.data[3] = toBCD(gba_time.tm_wday);
                            rtcClockData.data[4] = toBCD(gba_time.tm_hour);
                            rtcClockData.data[5] = toBCD(gba_time.tm_min);
                            rtcClockData.data[6] = toBCD(gba_time.tm_sec);
                            rtcClockData.state = DATA;
                        } break;

                        case 0x67: {
                            if (rtcEnabled)
                                SetGBATime();

                            rtcClockData.dataLen = 3;
                            rtcClockData.data[0] = toBCD(gba_time.tm_hour);
                            rtcClockData.data[1] = toBCD(gba_time.tm_min);
                            rtcClockData.data[2] = toBCD(gba_time.tm_sec);
                            rtcClockData.state = DATA;
                        } break;

                        default:
#ifdef GBA_LOGGING
                            log(N_("Unknown RTC command %02x"), rtcClockData.command);
#endif
                            rtcClockData.state = IDLE;
                            break;
                        }
                    }

                    break;

                case DATA:
                    if (rtcClockData.select & 2) {
                    } else if (rtcClockData.select & 4) {
                        rtcClockData.byte0 = (rtcClockData.byte0 & ~2) | ((rtcClockData.data[rtcClockData.bits >> 3] >> (rtcClockData.bits & 7)) & 1) * 2;
                        rtcClockData.bits++;

                        if (rtcClockData.bits == 8 * rtcClockData.dataLen) {
                            rtcClockData.bits = 0;
                            rtcClockData.state = IDLE;
                        }
                    }

                    break;

                case READDATA:
                    if (!(rtcClockData.select & 2)) {
                    } else {
                        rtcClockData.data[rtcClockData.bits >> 3] = (rtcClockData.data[rtcClockData.bits >> 3] >> 1) | ((value << 6) & 128);
                        rtcClockData.bits++;

                        if (rtcClockData.bits == 8 * rtcClockData.dataLen) {
                            rtcClockData.bits = 0;
                            rtcClockData.state = IDLE;
                        }
                    }

                    break;

                default:
                    break;
                }
            } else
                rtcClockData.byte0 = (uint8_t)value;
        }
    }

    return true;
}

void rtcReset()
{
    memset(&rtcClockData, 0, sizeof(rtcClockData));
    rtcClockData.byte0 = 0;
    rtcClockData.select = 0;
    rtcClockData.enable = 0;
    rtcClockData.command = 0;
    rtcClockData.dataLen = 0;
    rtcClockData.bits = 0;
    rtcClockData.state = IDLE;
    rtcClockData.reserved[11] = 0;
    SetGBATime();
}

#ifdef __LIBRETRO__
void rtcSaveGame(uint8_t*& data)
{
    utilWriteMem(data, &rtcClockData, sizeof(rtcClockData));
}

void rtcReadGame(const uint8_t*& data)
{
    utilReadMem(&rtcClockData, data, sizeof(rtcClockData));
}
#else
void rtcSaveGame(gzFile gzFile)
{
    utilGzWrite(gzFile, &rtcClockData, sizeof(rtcClockData));
}

void rtcReadGame(gzFile gzFile)
{
    utilGzRead(gzFile, &rtcClockData, sizeof(rtcClockData));
}
#endif
