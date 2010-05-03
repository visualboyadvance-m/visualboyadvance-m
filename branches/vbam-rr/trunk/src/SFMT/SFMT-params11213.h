#ifndef SFMT_PARAMS11213_H
#define SFMT_PARAMS11213_H

#define POS1	68
#define SL1	14
#define SL2	3
#define SR1	7
#define SR2	3
#define MSK1	0xeffff7fbU
#define MSK2	0xffffffefU
#define MSK3	0xdfdfbfffU
#define MSK4	0x7fffdbfdU
#define PARITY1	0x00000001U
#define PARITY2	0x00000000U
#define PARITY3	0xe8148000U
#define PARITY4	0xd0c7afa3U


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
	(vector unsigned char)(5,6,7,0,9,10,11,4,13,14,15,8,19,19,19,12)
    #define ALTI_SR2_PERM64 \
	(vector unsigned char)(13,14,15,0,1,2,3,4,19,19,19,8,9,10,11,12)
#else	/* For OTHER OSs(Linux?) */
    #define ALTI_SL1	{SL1, SL1, SL1, SL1}
    #define ALTI_SR1	{SR1, SR1, SR1, SR1}
    #define ALTI_MSK	{MSK1, MSK2, MSK3, MSK4}
    #define ALTI_MSK64	{MSK2, MSK1, MSK4, MSK3}
    #define ALTI_SL2_PERM	{3,21,21,21,7,0,1,2,11,4,5,6,15,8,9,10}
    #define ALTI_SL2_PERM64	{3,4,5,6,7,29,29,29,11,12,13,14,15,0,1,2}
    #define ALTI_SR2_PERM	{5,6,7,0,9,10,11,4,13,14,15,8,19,19,19,12}
    #define ALTI_SR2_PERM64	{13,14,15,0,1,2,3,4,19,19,19,8,9,10,11,12}
#endif	/* For OSX */
#define IDSTR	"SFMT-11213:68-14-3-7-3:effff7fb-ffffffef-dfdfbfff-7fffdbfd"

#endif /* SFMT_PARAMS11213_H */
