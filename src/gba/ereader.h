extern unsigned char *DotCodeData;
extern char filebuffer[];

int OpenDotCodeFile(void);
int CheckEReaderRegion(void);
int LoadDotCodeData(int size, u32* DCdata, unsigned long MEM1, unsigned long MEM2);
void EReaderWriteMemory(u32 address, u32 value);

void BIOS_EReader_ScanCard(int swi_num);

