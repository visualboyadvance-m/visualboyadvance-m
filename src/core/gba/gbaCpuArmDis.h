/************************************************************************/
/* Arm/Thumb command set disassembler                                   */
/************************************************************************/

#ifndef VBAM_CORE_GBA_GBACPUARMDIS_H_
#define VBAM_CORE_GBA_GBACPUARMDIS_H_

#include <cstdint>

#define DIS_VIEW_ADDRESS 1
#define DIS_VIEW_CODE 2

int disThumb(uint32_t offset, char* dest, unsigned dest_sz, int flags);
int disArm(uint32_t offset, char* dest, unsigned dest_sz, int flags);

#endif  // VBAM_CORE_GBA_GBACPUARMDIS_H_
