/************************************************************************/
/* Arm/Thumb command set disassembler                                   */
/************************************************************************/

#ifndef __ARMDIS_H__
#define __ARMDIS_H__

#define DIS_VIEW_ADDRESS 1
#define DIS_VIEW_CODE 2

int disThumb(uint32_t offset, char* dest, unsigned dest_sz, int flags);
int disArm(uint32_t offset, char* dest, unsigned dest_sz, int flags);

#endif // __ARMDIS_H__
