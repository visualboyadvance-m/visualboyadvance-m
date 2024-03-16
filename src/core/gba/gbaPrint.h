#ifndef VBAM_CORE_GBA_GBAPRINT_H_
#define VBAM_CORE_GBA_GBAPRINT_H_

#include <cstdint>

void agbPrintEnable(bool enable);
bool agbPrintIsEnabled();
void agbPrintReset();
bool agbPrintWrite(uint32_t address, uint16_t value);
void agbPrintFlush();

#endif  // VBAM_CORE_GBA_GBAPRINT_H_
