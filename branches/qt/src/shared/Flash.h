#ifndef FLASH_H
#define FLASH_H

void flashSaveGame( gzFile _gzFile );
void flashReadGame( gzFile _gzFile, int version );
void flashReadGameSkip( gzFile _gzFile, int version );
u8 flashRead( u32 address );
void flashWrite( u32 address, u8 byte );
void flashDelayedWrite( u32 address, u8 byte );
void flashSaveDecide( u32 address, u8 byte );
void flashReset();
void flashSetSize( int size );
void flashInit();

extern u8 flashSaveMemory[0x20000];
extern int flashSize;

#endif
