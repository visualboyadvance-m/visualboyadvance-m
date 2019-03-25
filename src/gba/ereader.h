extern unsigned char* DotCodeData;
extern char filebuffer[];

int OpenDotCodeFile(void);
int CheckEReaderRegion(void);
int LoadDotCodeData(int size, uint32_t* DCdata, unsigned long MEM1, unsigned long MEM2);
void EReaderWriteMemory(uint32_t address, uint32_t value);

void BIOS_EReader_ScanCard(int swi_num);
