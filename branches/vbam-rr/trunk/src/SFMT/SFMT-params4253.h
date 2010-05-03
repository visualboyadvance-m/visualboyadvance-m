#ifndef SFMT_PARAMS4253_H
#define SFMT_PARAMS4253_H

#define POS1	17
#define SL1	20
#define SL2	1
#define SR1	7
#define SR2	1
#define MSK1	0x9f7bffffU
#define MSK2	0x9fffff5fU
#define MSK3	0x3efffffbU
#define MSK4	0xfffff7bbU
#define PARITY1	0xa8000001U
#define PARITY2	0xaf5390a3U
#define PARITY3	0xb740b3f8U
#define PARITY4	0x6c11486dU


/* PARAMETERS FOR ALTIVEC */
#if defined(__APPLE__)	/* For OSX */
    #define ALTI_SL1	(vector unsigned int)(SL1, SL1, SL1, SL1)
    #define ALTI_SR1	(vector unsigned int)(SR1, SR1, SR1, SR1)
    #define ALTI_MSK	(vector unsigned int)(MSK1, MSK2, MSK3, MSK4)
    #define ALTI_MSK64 \
	(vector unsigned int)(MSK2, MSK1, MSK4, MSK3)
    #define ALTI_SL2_PERM \
	(vector unsigned char)(1,2,3,23,5,6,7,0,9,10,11,4,13,14,15,8)
    #define ALTI_SL2_PERM64 \
	(vector unsigned char)(1,2,3,4,5,6,7,31,9,10,11,12,13,14,15,0)
    #define ALTI_SR2_PERM \
	(vector unsigned char)(7,0,1,2,11,4,5,6,15,8,9,10,17,12,13,14)
    #define ALTI_SR2_PERM64 \
	(vector unsigned char)(15,0,1,2,3,4,5,6,17,8,9,10,11,12,13,14)
#else	/* For OTHER OSs(Linux?) */
    #define ALTI_SL1	{SL1, SL1, SL1, SL1}
    #define ALTI_SR1	{SR1, SR1, SR1, SR1}
    #define ALTI_MSK	{MSK1, MSK2, MSK3, MSK4}
    #define ALTI_MSK64	{MSK2, MSK1, MSK4, MSK3}
    #define ALTI_SL2_PERM	{1,2,3,23,5,6,7,0,9,10,11,4,13,14,15,8}
    #define ALTI_SL2_PERM64	{1,2,3,4,5,6,7,31,9,10,11,12,13,14,15,0}
    #define ALTI_SR2_PERM	{7,0,1,2,11,4,5,6,15,8,9,10,17,12,13,14}
    #define ALTI_SR2_PERM64	{15,0,1,2,3,4,5,6,17,8,9,10,11,12,13,14}
#endif	/* For OSX */
#define IDSTR	"SFMT-4253:17-20-1-7-1:9f7bffff-9fffff5f-3efffffb-fffff7bb"

#endif /* SFMT_PARAMS4253_H */
