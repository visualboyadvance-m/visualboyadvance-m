#ifndef AGBPRINT_H
#define AGBPRINT_H

void agbPrintEnable( bool );
bool agbPrintIsEnabled();
void agbPrintReset();
bool agbPrintWrite( u32, u16 );
void agbPrintFlush();

#endif
