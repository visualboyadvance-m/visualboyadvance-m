#ifndef RTC_H
#define RTC_H

uint16_t rtcRead(uint32_t address);
void rtcUpdateTime(int ticks);
bool rtcWrite(uint32_t address, uint16_t value);
void rtcEnable(bool);
void rtcEnableRumble(bool e);
bool rtcIsEnabled();
void rtcReset();

#ifdef __LIBRETRO__
void rtcReadGame(const uint8_t*& data);
void rtcSaveGame(uint8_t*& data);
#else
void rtcReadGame(gzFile gzFile);
void rtcSaveGame(gzFile gzFile);
#endif

#endif // RTC_H
