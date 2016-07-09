#ifndef AGBPRINT_H
#define AGBPRINT_H

void agbPrintEnable(bool enable);
bool agbPrintIsEnabled();
void agbPrintReset();
bool agbPrintWrite(uint32_t address, uint16_t value);
void agbPrintFlush();

#endif // AGBPRINT_H
