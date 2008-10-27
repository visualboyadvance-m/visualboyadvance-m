#ifndef VBA_AGBPRINT_H
#define VBA_AGBPRINT_H
extern void agbPrintEnable(bool);
extern bool agbPrintIsEnabled();
extern void agbPrintReset();
extern bool agbPrintWrite(u32, u16);
extern void agbPrintFlush();
#endif
