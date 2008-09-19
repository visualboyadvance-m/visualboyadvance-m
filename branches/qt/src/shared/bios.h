#ifndef BIOS_H
#define BIOS_H

void BIOS_ArcTan();
void BIOS_ArcTan2();
void BIOS_BitUnPack();
void BIOS_GetBiosChecksum();
void BIOS_BgAffineSet();
void BIOS_CpuSet();
void BIOS_CpuFastSet();
void BIOS_Diff8bitUnFilterWram();
void BIOS_Diff8bitUnFilterVram();
void BIOS_Diff16bitUnFilter();
void BIOS_Div();
void BIOS_DivARM();
void BIOS_HuffUnComp();
void BIOS_LZ77UnCompVram();
void BIOS_LZ77UnCompWram();
void BIOS_ObjAffineSet();
void BIOS_RegisterRamReset();
void BIOS_RegisterRamReset(u32);
void BIOS_RLUnCompVram();
void BIOS_RLUnCompWram();
void BIOS_SoftReset();
void BIOS_Sqrt();
void BIOS_MidiKey2Freq();
void BIOS_SndDriverJmpTableCopy();

#endif
