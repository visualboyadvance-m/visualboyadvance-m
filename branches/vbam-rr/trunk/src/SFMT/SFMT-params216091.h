#ifndef SFMT_PARAMS216091_H
#define SFMT_PARAMS216091_H

#define POS1	627
#define SL1	11
#define SL2	3
#define SR1	10
#define SR2	1
#define MSK1	0xbff7bff7U
#define MSK2	0xbfffffffU
#define MSK3	0xbffffa7fU
#define MSK4	0xffddfbfbU
#define PARITY1	0xf8000001U
#define PARITY2	0x89e80709U
#define PARITY3	0x3bd2b64bU
#define PARITY4	0x0c64b1e4U


/* PARAMETERS FOR ALTIVEC */
#if defined(__APPLE__)	/* For OSX */
    #define ALTI_SL1	(vector unsigned int)(SL1, SL1, SL1, SL1)
    #define ALTI_SR1	(vector unsigned int)(SR1, SR1, SR1, SR1)
    #define ALTI_MSK	(vector unsigned int)(MSK1, MSK2, MSK3, MSK4)
    #define ALTI_MSK64 \
	(vector unsigned int)(MSK2, MSK1, MSK4, MSK3)
    #define ALTI_SL2_PERM \
	(vector unsigned char)(3,21,21,21,7,0,1,2,11,4,5,6,15,8,9,10)
    #define ALTI_SL2_PERM64 \
	(vector unsigned char)(3,4,5,6,7,29,29,29,11,12,13,14,15,0,1,2)
    #define ALTI_SR2_PERM \
	(vector unsigned char)(7,0,1,2,11,4,5,6,15,8,9,10,17,12,13,14)
    #define ALTI_SR2_PERM64 \
	(vector unsigned char)(15,0,1,2,3,4,5,6,17,8,9,10,11,12,13,14)
#else	/* For OTHER OSs(Linux?) */
    #define ALTI_SL1	{SL1, SL1, SL1, SL1}
    #define ALTI_SR1	{SR1, SR1, SR1, SR1}
    #define ALTI_MSK	{MSK1, MSK2, MSK3, MSK4}
    #define ALTI_MSK64	{MSK2, MSK1, MSK4, MSK3}
    #define ALTI_SL2_PERM	{3,21,21,21,7,0,1,2,11,4,5,6,15,8,9,10}
    #define ALTI_SL2_PERM64	{3,4,5,6,7,29,29,29,11,12,13,14,15,0,1,2}
    #define ALTI_SR2_PERM	{7,0,1,2,11,4,5,6,15,8,9,10,17,12,13,14}
    #define ALTI_SR2_PERM64	{15,0,1,2,3,4,5,6,17,8,9,10,11,12,13,14}
#endif	/* For OSX */
#define IDSTR	"SFMT-216091:627-11-3-10-1:bff7bff7-bfffffff-bffffa7f-ffddfbfb"

#endif /* SFMT_PARAMS216091_H */
