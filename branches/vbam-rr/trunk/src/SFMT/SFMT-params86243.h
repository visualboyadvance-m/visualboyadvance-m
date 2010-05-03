#ifndef SFMT_PARAMS86243_H
#define SFMT_PARAMS86243_H

#define POS1	366
#define SL1	6
#define SL2	7
#define SR1	19
#define SR2	1
#define MSK1	0xfdbffbffU
#define MSK2	0xbff7ff3fU
#define MSK3	0xfd77efffU
#define MSK4	0xbf9ff3ffU
#define PARITY1	0x00000001U
#define PARITY2	0x00000000U
#define PARITY3	0x00000000U
#define PARITY4	0xe9528d85U


/* PARAMETERS FOR ALTIVEC */
#if defined(__APPLE__)	/* For OSX */
    #define ALTI_SL1	(vector unsigned int)(SL1, SL1, SL1, SL1)
    #define ALTI_SR1	(vector unsigned int)(SR1, SR1, SR1, SR1)
    #define ALTI_MSK	(vector unsigned int)(MSK1, MSK2, MSK3, MSK4)
    #define ALTI_MSK64 \
	(vector unsigned int)(MSK2, MSK1, MSK4, MSK3)
    #define ALTI_SL2_PERM \
	(vector unsigned char)(25,25,25,25,3,25,25,25,7,0,1,2,11,4,5,6)
    #define ALTI_SL2_PERM64 \
	(vector unsigned char)(7,25,25,25,25,25,25,25,15,0,1,2,3,4,5,6)
    #define ALTI_SR2_PERM \
	(vector unsigned char)(7,0,1,2,11,4,5,6,15,8,9,10,17,12,13,14)
    #define ALTI_SR2_PERM64 \
	(vector unsigned char)(15,0,1,2,3,4,5,6,17,8,9,10,11,12,13,14)
#else	/* For OTHER OSs(Linux?) */
    #define ALTI_SL1	{SL1, SL1, SL1, SL1}
    #define ALTI_SR1	{SR1, SR1, SR1, SR1}
    #define ALTI_MSK	{MSK1, MSK2, MSK3, MSK4}
    #define ALTI_MSK64	{MSK2, MSK1, MSK4, MSK3}
    #define ALTI_SL2_PERM	{25,25,25,25,3,25,25,25,7,0,1,2,11,4,5,6}
    #define ALTI_SL2_PERM64	{7,25,25,25,25,25,25,25,15,0,1,2,3,4,5,6}
    #define ALTI_SR2_PERM	{7,0,1,2,11,4,5,6,15,8,9,10,17,12,13,14}
    #define ALTI_SR2_PERM64	{15,0,1,2,3,4,5,6,17,8,9,10,11,12,13,14}
#endif	/* For OSX */
#define IDSTR	"SFMT-86243:366-6-7-19-1:fdbffbff-bff7ff3f-fd77efff-bf9ff3ff"

#endif /* SFMT_PARAMS86243_H */
