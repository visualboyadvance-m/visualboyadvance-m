// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// The following define updates oldreg when debugger_last is activated

#define UPDATE_OLD_REG \
		   if (debugger_last) { \
			   sprintf(oldbuffer,"%08x", armState ? reg[15].I - 4 : reg[15].I - 4); \
			   for (xxx=0; xxx<18; xxx++){ \
				   oldreg[xxx]=reg[xxx].I; \
			   } \
		   } 


#ifdef C_CORE
#define NEG(i) ((i) >> 31)
#define POS(i) ((~(i)) >> 31)
#define ADDCARRY(a, b, c) \
  C_FLAG = ((NEG(a) & NEG(b)) |\
            (NEG(a) & POS(c)) |\
            (NEG(b) & POS(c))) ? true : false;
#define ADDOVERFLOW(a, b, c) \
  V_FLAG = ((NEG(a) & NEG(b) & POS(c)) |\
            (POS(a) & POS(b) & NEG(c))) ? true : false;
#define SUBCARRY(a, b, c) \
  C_FLAG = ((NEG(a) & POS(b)) |\
            (NEG(a) & POS(c)) |\
            (POS(b) & POS(c))) ? true : false;
#define SUBOVERFLOW(a, b, c)\
  V_FLAG = ((NEG(a) & POS(b) & POS(c)) |\
            (POS(a) & NEG(b) & NEG(c))) ? true : false;
#define ADD_RD_RS_RN \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = value;\
     u32 res = lhs + rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define ADD_RD_RS_O3 \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = value;\
     u32 res = lhs + rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define ADD_RN_O8(d) \
   {\
     u32 lhs = reg[(d)].I;\
     u32 rhs = (opcode & 255);\
     u32 res = lhs + rhs;\
     reg[(d)].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define CMN_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs + rhs;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define ADC_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs + rhs + (u32)C_FLAG;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define SUB_RD_RS_RN \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = value;\
     u32 res = lhs - rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define SUB_RD_RS_O3 \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = value;\
     u32 res = lhs - rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define SUB_RN_O8(d) \
   {\
     u32 lhs = reg[(d)].I;\
     u32 rhs = (opcode & 255);\
     u32 res = lhs - rhs;\
     reg[(d)].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define CMP_RN_O8(d) \
   {\
     u32 lhs = reg[(d)].I;\
     u32 rhs = (opcode & 255);\
     u32 res = lhs - rhs;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define SBC_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs - rhs - !((u32)C_FLAG);\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define LSL_RD_RM_I5 \
   {\
     C_FLAG = (reg[source].I >> (32 - shift)) & 1 ? true : false;\
     value = reg[source].I << shift;\
   }
#define LSL_RD_RS \
   {\
     C_FLAG = (reg[dest].I >> (32 - value)) & 1 ? true : false;\
     value = reg[dest].I << value;\
   }
#define LSR_RD_RM_I5 \
   {\
     C_FLAG = (reg[source].I >> (shift - 1)) & 1 ? true : false;\
     value = reg[source].I >> shift;\
   }
#define LSR_RD_RS \
   {\
     C_FLAG = (reg[dest].I >> (value - 1)) & 1 ? true : false;\
     value = reg[dest].I >> value;\
   }
#define ASR_RD_RM_I5 \
   {\
     C_FLAG = ((s32)reg[source].I >> (int)(shift - 1)) & 1 ? true : false;\
     value = (s32)reg[source].I >> (int)shift;\
   }
#define ASR_RD_RS \
   {\
     C_FLAG = ((s32)reg[dest].I >> (int)(value - 1)) & 1 ? true : false;\
     value = (s32)reg[dest].I >> (int)value;\
   }
#define ROR_RD_RS \
   {\
     C_FLAG = (reg[dest].I >> (value - 1)) & 1 ? true : false;\
     value = ((reg[dest].I << (32 - value)) |\
              (reg[dest].I >> value));\
   }
#define NEG_RD_RS \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = 0;\
     u32 res = rhs - lhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(rhs, lhs, res);\
     SUBOVERFLOW(rhs, lhs, res);\
   }
#define CMP_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs - rhs;\
     Z_FLAG = (res == 0) ? true : false;\
     N_FLAG = NEG(res) ? true : false;\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#else
#ifdef __GNUC__
#ifdef __POWERPC__
            #define ADD_RD_RS_RN \
            {										\
                register int Flags;					\
                register int Result;				\
                asm volatile("addco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[source].I), 	\
                              "r" (value)			\
                            );						\
                reg[dest].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define ADD_RD_RS_O3 ADD_RD_RS_RN
            #define ADD_RN_O8(d) \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("addco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[(d)].I), 	\
                              "r" (opcode & 255)	\
                            );						\
                reg[(d)].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define CMN_RD_RS \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("addco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[dest].I), 	\
                              "r" (value)			\
                            );						\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define ADC_RD_RS \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("mtspr xer, %4\n"		\
                             "addeo. %0, %2, %3\n"	\
                             "mcrxr cr1\n"			\
                             "mfcr	%1\n"			\
                             : "=r" (Result),		\
                               "=r" (Flags)			\
                             : "r" (reg[dest].I),	\
                               "r" (value),			\
                               "r" (C_FLAG << 29)	\
                             );						\
                reg[dest].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define SUB_RD_RS_RN \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("subco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[source].I), 	\
                              "r" (value)			\
                            );						\
                reg[dest].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define SUB_RD_RS_O3 SUB_RD_RS_RN
            #define SUB_RN_O8(d) \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("subco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[(d)].I), 	\
                              "r" (opcode & 255)	\
                            );						\
                reg[(d)].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define CMP_RN_O8(d) \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("subco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[(d)].I), 	\
                              "r" (opcode & 255)	\
                            );						\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define SBC_RD_RS \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("mtspr xer, %4\n"		\
                             "subfeo. %0, %3, %2\n"	\
                             "mcrxr cr1\n"			\
                             "mfcr	%1\n"			\
                             : "=r" (Result),		\
                               "=r" (Flags)			\
                             : "r" (reg[dest].I),	\
                               "r" (value),			\
                               "r" (C_FLAG << 29) 	\
                             );						\
                reg[dest].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define LSL_RD_RM_I5 \
            {\
                C_FLAG = (reg[source].I >> (32 - shift)) & 1 ? true : false;\
                value = reg[source].I << shift;\
            }
            #define LSL_RD_RS \
            {\
                C_FLAG = (reg[dest].I >> (32 - value)) & 1 ? true : false;\
                value = reg[dest].I << value;\
            }
            #define LSR_RD_RM_I5 \
            {\
                C_FLAG = (reg[source].I >> (shift - 1)) & 1 ? true : false;\
                value = reg[source].I >> shift;\
            }
            #define LSR_RD_RS \
            {\
                C_FLAG = (reg[dest].I >> (value - 1)) & 1 ? true : false;\
                value = reg[dest].I >> value;\
            }
            #define ASR_RD_RM_I5 \
            {\
                C_FLAG = ((s32)reg[source].I >> (int)(shift - 1)) & 1 ? true : false;\
                value = (s32)reg[source].I >> (int)shift;\
            }
            #define ASR_RD_RS \
            {\
                C_FLAG = ((s32)reg[dest].I >> (int)(value - 1)) & 1 ? true : false;\
                value = (s32)reg[dest].I >> (int)value;\
            }
            #define ROR_RD_RS \
            {\
                C_FLAG = (reg[dest].I >> (value - 1)) & 1 ? true : false;\
                value = ((reg[dest].I << (32 - value)) |\
                        (reg[dest].I >> value));\
            }
            #define NEG_RD_RS \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("subfco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[source].I), 	\
                              "r" (0)				\
                            );						\
                reg[dest].I = Result;				\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
            #define CMP_RD_RS \
            {\
                register int Flags;					\
                register int Result;				\
                asm volatile("subco. %0, %2, %3\n"	\
                            "mcrxr cr1\n"			\
                            "mfcr %1\n"				\
                            : "=r" (Result), 		\
                              "=r" (Flags)			\
                            : "r" (reg[dest].I), 	\
                              "r" (value)			\
                            );						\
                Z_FLAG = (Flags >> 29) & 1;			\
                N_FLAG = (Flags >> 31) & 1;			\
                C_FLAG = (Flags >> 25) & 1;			\
                V_FLAG = (Flags >> 26) & 1;			\
            }
#else
#define ADD_RD_RS_RN \
     asm ("add %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setcb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[dest].I)\
          : "r" (value), "b" (reg[source].I));
#define ADD_RD_RS_O3 \
     asm ("add %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setcb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[dest].I)\
          : "r" (value), "b" (reg[source].I));
#define ADD_RN_O8(d) \
     asm ("add %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setcb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[(d)].I)\
          : "r" (opcode & 255), "b" (reg[(d)].I));
#define CMN_RD_RS \
     asm ("add %0, %1;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setcb C_FLAG;"\
          "setob V_FLAG;"\
          : \
          : "r" (value), "r" (reg[dest].I):"1");
#define ADC_RD_RS \
     asm ("bt $0, C_FLAG;"\
          "adc %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setcb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[dest].I)\
          : "r" (value), "b" (reg[dest].I));
#define SUB_RD_RS_RN \
     asm ("sub %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setncb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[dest].I)\
          : "r" (value), "b" (reg[source].I));
#define SUB_RD_RS_O3 \
     asm ("sub %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setncb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[dest].I)\
          : "r" (value), "b" (reg[source].I));
#define SUB_RN_O8(d) \
     asm ("sub %1, %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setncb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[(d)].I)\
          : "r" (opcode & 255), "b" (reg[(d)].I));
#define CMP_RN_O8(d) \
     asm ("sub %0, %1;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setncb C_FLAG;"\
          "setob V_FLAG;"\
          : \
          : "r" (opcode & 255), "r" (reg[(d)].I) : "1");
#define SBC_RD_RS \
     asm volatile ("bt $0, C_FLAG;"\
                   "cmc;"\
                   "sbb %1, %%ebx;"\
                   "setsb N_FLAG;"\
                   "setzb Z_FLAG;"\
                   "setncb C_FLAG;"\
                   "setob V_FLAG;"\
                   : "=b" (reg[dest].I)\
                   : "r" (value), "b" (reg[dest].I) : "cc", "memory");
#define LSL_RD_RM_I5 \
       asm ("shl %%cl, %%eax;"\
            "setcb C_FLAG;"\
            : "=a" (value)\
            : "a" (reg[source].I), "c" (shift));
#define LSL_RD_RS \
         asm ("shl %%cl, %%eax;"\
              "setcb C_FLAG;"\
              : "=a" (value)\
              : "a" (reg[dest].I), "c" (value));
#define LSR_RD_RM_I5 \
       asm ("shr %%cl, %%eax;"\
            "setcb C_FLAG;"\
            : "=a" (value)\
            : "a" (reg[source].I), "c" (shift));
#define LSR_RD_RS \
         asm ("shr %%cl, %%eax;"\
              "setcb C_FLAG;"\
              : "=a" (value)\
              : "a" (reg[dest].I), "c" (value));
#define ASR_RD_RM_I5 \
     asm ("sar %%cl, %%eax;"\
          "setcb C_FLAG;"\
          : "=a" (value)\
          : "a" (reg[source].I), "c" (shift));
#define ASR_RD_RS \
         asm ("sar %%cl, %%eax;"\
              "setcb C_FLAG;"\
              : "=a" (value)\
              : "a" (reg[dest].I), "c" (value));
#define ROR_RD_RS \
         asm ("ror %%cl, %%eax;"\
              "setcb C_FLAG;"\
              : "=a" (value)\
              : "a" (reg[dest].I), "c" (value));
#define NEG_RD_RS \
     asm ("neg %%ebx;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setncb C_FLAG;"\
          "setob V_FLAG;"\
          : "=b" (reg[dest].I)\
          : "b" (reg[source].I));
#define CMP_RD_RS \
     asm ("sub %0, %1;"\
          "setsb N_FLAG;"\
          "setzb Z_FLAG;"\
          "setncb C_FLAG;"\
          "setob V_FLAG;"\
          : \
          : "r" (value), "r" (reg[dest].I):"1");
#endif
#else
#define ADD_RD_RS_RN \
   {\
     __asm mov eax, source\
     __asm mov ebx, dword ptr [OFFSET reg+4*eax]\
     __asm add ebx, value\
     __asm mov eax, dest\
     __asm mov dword ptr [OFFSET reg+4*eax], ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define ADD_RD_RS_O3 \
   {\
     __asm mov eax, source\
     __asm mov ebx, dword ptr [OFFSET reg+4*eax]\
     __asm add ebx, value\
     __asm mov eax, dest\
     __asm mov dword ptr [OFFSET reg+4*eax], ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define ADD_RN_O8(d) \
   {\
     __asm mov ebx, opcode\
     __asm and ebx, 255\
     __asm add dword ptr [OFFSET reg+4*(d)], ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define CMN_RD_RS \
     {\
       __asm mov eax, dest\
       __asm mov ebx, dword ptr [OFFSET reg+4*eax]\
       __asm add ebx, value\
       __asm sets byte ptr N_FLAG\
       __asm setz byte ptr Z_FLAG\
       __asm setc byte ptr C_FLAG\
       __asm seto byte ptr V_FLAG\
     }
#define ADC_RD_RS \
     {\
       __asm mov ebx, dest\
       __asm mov ebx, dword ptr [OFFSET reg+4*ebx]\
       __asm bt word ptr C_FLAG, 0\
       __asm adc ebx, value\
       __asm mov eax, dest\
       __asm mov dword ptr [OFFSET reg+4*eax], ebx\
       __asm sets byte ptr N_FLAG\
       __asm setz byte ptr Z_FLAG\
       __asm setc byte ptr C_FLAG\
       __asm seto byte ptr V_FLAG\
     }
#define SUB_RD_RS_RN \
   {\
     __asm mov eax, source\
     __asm mov ebx, dword ptr [OFFSET reg+4*eax]\
     __asm sub ebx, value\
     __asm mov eax, dest\
     __asm mov dword ptr [OFFSET reg+4*eax], ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setnc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define SUB_RD_RS_O3 \
   {\
     __asm mov eax, source\
     __asm mov ebx, dword ptr [OFFSET reg+4*eax]\
     __asm sub ebx, value\
     __asm mov eax, dest\
     __asm mov dword ptr [OFFSET reg+4*eax], ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setnc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define SUB_RN_O8(d) \
   {\
     __asm mov ebx, opcode\
     __asm and ebx, 255\
     __asm sub dword ptr [OFFSET reg + 4*(d)], ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setnc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define CMP_RN_O8(d) \
   {\
     __asm mov eax, dword ptr [OFFSET reg+4*(d)]\
     __asm mov ebx, opcode\
     __asm and ebx, 255\
     __asm sub eax, ebx\
     __asm sets byte ptr N_FLAG\
     __asm setz byte ptr Z_FLAG\
     __asm setnc byte ptr C_FLAG\
     __asm seto byte ptr V_FLAG\
   }
#define SBC_RD_RS \
     {\
       __asm mov ebx, dest\
       __asm mov ebx, dword ptr [OFFSET reg + 4*ebx]\
       __asm mov eax, value\
       __asm bt word ptr C_FLAG, 0\
       __asm cmc\
       __asm sbb ebx, eax\
       __asm mov eax, dest\
       __asm mov dword ptr [OFFSET reg + 4*eax], ebx\
       __asm sets byte ptr N_FLAG\
       __asm setz byte ptr Z_FLAG\
       __asm setnc byte ptr C_FLAG\
       __asm seto byte ptr V_FLAG\
     }
#define LSL_RD_RM_I5 \
     {\
       __asm mov eax, source\
       __asm mov eax, dword ptr [OFFSET reg + 4 * eax]\
       __asm mov cl, byte ptr shift\
       __asm shl eax, cl\
       __asm mov value, eax\
       __asm setc byte ptr C_FLAG\
     }
#define LSL_RD_RS \
         {\
           __asm mov eax, dest\
           __asm mov eax, dword ptr [OFFSET reg + 4 * eax]\
           __asm mov cl, byte ptr value\
           __asm shl eax, cl\
           __asm mov value, eax\
           __asm setc byte ptr C_FLAG\
         }
#define LSR_RD_RM_I5 \
     {\
       __asm mov eax, source\
       __asm mov eax, dword ptr [OFFSET reg + 4 * eax]\
       __asm mov cl, byte ptr shift\
       __asm shr eax, cl\
       __asm mov value, eax\
       __asm setc byte ptr C_FLAG\
     }
#define LSR_RD_RS \
         {\
           __asm mov eax, dest\
           __asm mov eax, dword ptr [OFFSET reg + 4 * eax]\
           __asm mov cl, byte ptr value\
           __asm shr eax, cl\
           __asm mov value, eax\
           __asm setc byte ptr C_FLAG\
         }
#define ASR_RD_RM_I5 \
     {\
       __asm mov eax, source\
       __asm mov eax, dword ptr [OFFSET reg + 4*eax]\
       __asm mov cl, byte ptr shift\
       __asm sar eax, cl\
       __asm mov value, eax\
       __asm setc byte ptr C_FLAG\
     }
#define ASR_RD_RS \
         {\
           __asm mov eax, dest\
           __asm mov eax, dword ptr [OFFSET reg + 4*eax]\
           __asm mov cl, byte ptr value\
           __asm sar eax, cl\
           __asm mov value, eax\
           __asm setc byte ptr C_FLAG\
         }
#define ROR_RD_RS \
         {\
           __asm mov eax, dest\
           __asm mov eax, dword ptr [OFFSET reg + 4*eax]\
           __asm mov cl, byte ptr value\
           __asm ror eax, cl\
           __asm mov value, eax\
           __asm setc byte ptr C_FLAG\
         }
#define NEG_RD_RS \
     {\
       __asm mov ebx, source\
       __asm mov ebx, dword ptr [OFFSET reg+4*ebx]\
       __asm neg ebx\
       __asm mov eax, dest\
       __asm mov dword ptr [OFFSET reg+4*eax],ebx\
       __asm sets byte ptr N_FLAG\
       __asm setz byte ptr Z_FLAG\
       __asm setnc byte ptr C_FLAG\
       __asm seto byte ptr V_FLAG\
     }
#define CMP_RD_RS \
     {\
       __asm mov eax, dest\
       __asm mov ebx, dword ptr [OFFSET reg+4*eax]\
       __asm sub ebx, value\
       __asm sets byte ptr N_FLAG\
       __asm setz byte ptr Z_FLAG\
       __asm setnc byte ptr C_FLAG\
       __asm seto byte ptr V_FLAG\
     }
#endif
#endif


#ifdef BKPT_SUPPORT
u8 xxx;
#endif

u32 opcode = cpuPrefetch[0];
cpuPrefetch[0] = cpuPrefetch[1];

busPrefetch = false;
  if (busPrefetchCount & 0xFFFFFF00)
    busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF);

clockTicks = 0;
u32 oldArmNextPC = armNextPC;
#ifndef FINAL_VERSION
if(armNextPC == stop) {
  armNextPC++;
}
#endif

armNextPC = reg[15].I;
reg[15].I += 2;
THUMB_PREFETCH_NEXT;

switch(opcode >> 8) {
 case 0x00:
 case 0x01:
 case 0x02:
 case 0x03:
 case 0x04:
 case 0x05:
 case 0x06:
 case 0x07:
   {
     // LSL Rd, Rm, #Imm 5
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     int shift = (opcode >> 6) & 0x1f;
     u32 value;
     
     if(shift) {
       LSL_RD_RM_I5;
     } else {
       value = reg[source].I;
     }
     reg[dest].I = value;
     // C_FLAG set above
     N_FLAG = (value & 0x80000000 ? true : false);
     Z_FLAG = (value ? false : true);
   }
   break;
 case 0x08:
 case 0x09:
 case 0x0a:
 case 0x0b:
 case 0x0c:
 case 0x0d:
 case 0x0e:
 case 0x0f:
   {
     // LSR Rd, Rm, #Imm 5
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     int shift = (opcode >> 6) & 0x1f;
     u32 value;
     
     if(shift) {
       LSR_RD_RM_I5;
     } else {
       C_FLAG = reg[source].I & 0x80000000 ? true : false;
       value = 0;
     }
     reg[dest].I = value;
     // C_FLAG set above
     N_FLAG = (value & 0x80000000 ? true : false);
     Z_FLAG = (value ? false : true);
   }
   break;
 case 0x10:
 case 0x11:
 case 0x12:
 case 0x13:
 case 0x14:
 case 0x15:
 case 0x16:
 case 0x17:
   {     
     // ASR Rd, Rm, #Imm 5
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     int shift = (opcode >> 6) & 0x1f;
     u32 value;
     
     if(shift) {
       ASR_RD_RM_I5;
     } else {
       if(reg[source].I & 0x80000000) {
         value = 0xFFFFFFFF;
         C_FLAG = true;
       } else {
         value = 0;
         C_FLAG = false;
       }
     }
     reg[dest].I = value;
     // C_FLAG set above
     N_FLAG = (value & 0x80000000 ? true : false);
     Z_FLAG = (value ? false :true);
   }
   break;
 case 0x18:
 case 0x19:
   {
     // ADD Rd, Rs, Rn
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     u32 value = reg[(opcode>>6)& 0x07].I;
     ADD_RD_RS_RN;
   }
   break;
 case 0x1a:
 case 0x1b:
   {
     // SUB Rd, Rs, Rn
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     u32 value = reg[(opcode>>6)& 0x07].I;
     SUB_RD_RS_RN;
   }
   break;
 case 0x1c:
 case 0x1d:
   {
     // ADD Rd, Rs, #Offset3
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     u32 value = (opcode >> 6) & 7;
     ADD_RD_RS_O3;
   }
   break;
 case 0x1e:
 case 0x1f:
   {
     // SUB Rd, Rs, #Offset3
     int dest = opcode & 0x07;
     int source = (opcode >> 3) & 0x07;
     u32 value = (opcode >> 6) & 7;
     SUB_RD_RS_O3;
   }
   break;
 case 0x20:
 case 0x21:
 case 0x22:
 case 0x23:
 case 0x24:
 case 0x25:
 case 0x26:
 case 0x27:
     {
   u8 regist = (opcode >> 8) & 7;
   // MOV R0~R7, #Offset8
   reg[regist].I = opcode & 255;
   N_FLAG = false;
   Z_FLAG = (reg[regist].I ? false : true);
     }
   break;
case 0x28:
   // CMP R0, #Offset8
   CMP_RN_O8(0);
   break;
 case 0x29:
   // CMP R1, #Offset8
   CMP_RN_O8(1);
   break;
 case 0x2a:
   // CMP R2, #Offset8
   CMP_RN_O8(2);
   break;
 case 0x2b:
   // CMP R3, #Offset8
   CMP_RN_O8(3);
   break;
 case 0x2c:
   // CMP R4, #Offset8
   CMP_RN_O8(4);
   break;
 case 0x2d:
   // CMP R5, #Offset8
   CMP_RN_O8(5);
   break;
 case 0x2e:
   // CMP R6, #Offset8
   CMP_RN_O8(6);
   break;
 case 0x2f:
   // CMP R7, #Offset8
   CMP_RN_O8(7);
   break;
 case 0x30:
   // ADD R0,#Offset8
   ADD_RN_O8(0);
   break;   
 case 0x31:
   // ADD R1,#Offset8
   ADD_RN_O8(1);
   break;   
 case 0x32:
   // ADD R2,#Offset8
   ADD_RN_O8(2);
   break;   
 case 0x33:
   // ADD R3,#Offset8
   ADD_RN_O8(3);
   break;   
 case 0x34:
   // ADD R4,#Offset8
   ADD_RN_O8(4);
   break;   
 case 0x35:
   // ADD R5,#Offset8
   ADD_RN_O8(5);
   break;   
 case 0x36:
   // ADD R6,#Offset8
   ADD_RN_O8(6);
   break;   
 case 0x37:
   // ADD R7,#Offset8
   ADD_RN_O8(7);
   break;
 case 0x38:
   // SUB R0,#Offset8
   SUB_RN_O8(0);
   break;
 case 0x39:
   // SUB R1,#Offset8
   SUB_RN_O8(1);
   break;
 case 0x3a:
   // SUB R2,#Offset8
   SUB_RN_O8(2);
   break;
 case 0x3b:
   // SUB R3,#Offset8
   SUB_RN_O8(3);
   break;
 case 0x3c:
   // SUB R4,#Offset8
   SUB_RN_O8(4);
   break;
 case 0x3d:
   // SUB R5,#Offset8
   SUB_RN_O8(5);
   break;
 case 0x3e:
   // SUB R6,#Offset8
   SUB_RN_O8(6);
   break;
 case 0x3f:
   // SUB R7,#Offset8
   SUB_RN_O8(7);
   break;

 case 0x40:
   switch((opcode >> 6) & 3) {
   case 0x00:
     {
       // AND Rd, Rs
       int dest = opcode & 7;
       reg[dest].I &= reg[(opcode >> 3)&7].I;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
       Z_FLAG = reg[dest].I ? false : true;
#ifdef BKPT_SUPPORT     
#define THUMB_CONSOLE_OUTPUT(a,b) \
  if((opcode == 0x4000) && (reg[0].I == 0xC0DED00D)) {\
    extern void (*dbgOutput)(char *, u32);\
    dbgOutput((a), (b));\
  }
#else
#define THUMB_CONSOLE_OUTPUT(a,b)
#endif
       THUMB_CONSOLE_OUTPUT(NULL, reg[2].I);
     }
     break;
   case 0x01:
     // EOR Rd, Rs
     {
       int dest = opcode & 7;
       reg[dest].I ^= reg[(opcode >> 3)&7].I;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
       Z_FLAG = reg[dest].I ? false : true;
     }
     break;
   case 0x02:
     // LSL Rd, Rs
     {
       int dest = opcode & 7;
       u32 value = reg[(opcode >> 3)&7].B.B0;
       if(value) {
         if(value == 32) {
           value = 0;
           C_FLAG = (reg[dest].I & 1 ? true : false);
         } else if(value < 32) {
           LSL_RD_RS;
         } else {
           value = 0;
           C_FLAG = false;
         }
         reg[dest].I = value;        
       }
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
       Z_FLAG = reg[dest].I ? false : true;
       clockTicks = codeTicksAccess16(armNextPC)+2;
     }
     break;
   case 0x03:
     {
       // LSR Rd, Rs
       int dest = opcode & 7;
       u32 value = reg[(opcode >> 3)&7].B.B0;
       if(value) {
         if(value == 32) {
           value = 0;
           C_FLAG = (reg[dest].I & 0x80000000 ? true : false);
         } else if(value < 32) {
           LSR_RD_RS;
         } else {
           value = 0;
           C_FLAG = false;
         }
         reg[dest].I = value;        
       }
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
       Z_FLAG = reg[dest].I ? false : true;
       clockTicks = codeTicksAccess16(armNextPC)+2;
     }
     break;
   }
   break;
 case 0x41:
   switch((opcode >> 6) & 3) {
   case 0x00:
     {
       // ASR Rd, Rs
       int dest = opcode & 7;
       u32 value = reg[(opcode >> 3)&7].B.B0;
       // ASR
       if(value) {
         if(value < 32) {
           ASR_RD_RS;
           reg[dest].I = value;        
         } else {
           if(reg[dest].I & 0x80000000){
             reg[dest].I = 0xFFFFFFFF;
             C_FLAG = true;
           } else {
             reg[dest].I = 0x00000000;
             C_FLAG = false;
           }
         }
       }
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
       Z_FLAG = reg[dest].I ? false : true;
       clockTicks = codeTicksAccess16(armNextPC)+2;
     }
     break;
   case 0x01:
     {
       // ADC Rd, Rs
       int dest = opcode & 0x07;
       u32 value = reg[(opcode >> 3)&7].I;
       // ADC
       ADC_RD_RS;
     }
     break;
   case 0x02:
     {
       // SBC Rd, Rs
       int dest = opcode & 0x07;
       u32 value = reg[(opcode >> 3)&7].I;
       
       // SBC
       SBC_RD_RS;
     }
     break;
   case 0x03:
     // ROR Rd, Rs
     {
       int dest = opcode & 7;
       u32 value = reg[(opcode >> 3)&7].B.B0;
       
       if(value) {
         value = value & 0x1f;
         if(value == 0) {
           C_FLAG = (reg[dest].I & 0x80000000 ? true : false);
         } else {
           ROR_RD_RS;
           reg[dest].I = value;
         }
       }
       clockTicks = codeTicksAccess16(armNextPC)+2;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
       Z_FLAG = reg[dest].I ? false : true;
     }
     break;
   }
   break;
 case 0x42:
   switch((opcode >> 6) & 3) {
   case 0x00:
     {
       // TST Rd, Rs
       u32 value = reg[opcode & 7].I & reg[(opcode >> 3) & 7].I;
       N_FLAG = value & 0x80000000 ? true : false;
       Z_FLAG = value ? false : true;
     }
     break;
   case 0x01:
     {
       // NEG Rd, Rs
       int dest = opcode & 7;
       int source = (opcode >> 3) & 7;
       NEG_RD_RS;
     }
     break;
   case 0x02:
     {
       // CMP Rd, Rs
       int dest = opcode & 7;
       u32 value = reg[(opcode >> 3)&7].I;
       CMP_RD_RS;
     }
     break;
   case 0x03:
     {
       // CMN Rd, Rs
       int dest = opcode & 7;
       u32 value = reg[(opcode >> 3)&7].I;
       // CMN
       CMN_RD_RS;
     }
     break;
   }
   break;
 case 0x43:
   switch((opcode >> 6) & 3) {
   case 0x00:
     {
       // ORR Rd, Rs       
       int dest = opcode & 7;
       reg[dest].I |= reg[(opcode >> 3) & 7].I;
       Z_FLAG = reg[dest].I ? false : true;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
     }
     break;
   case 0x01:
     {
       // MUL Rd, Rs
       clockTicks = 1;
       int dest = opcode & 7;
       u32 rm = reg[dest].I;
       reg[dest].I = reg[(opcode >> 3) & 7].I * rm;
       if (((s32)rm) < 0)
         rm = ~rm;
       if ((rm & 0xFFFFFF00) == 0)
         clockTicks += 0;
       else if ((rm & 0xFFFF0000) == 0)
         clockTicks += 1;
       else if ((rm & 0xFF000000) == 0)
         clockTicks += 2;
       else
         clockTicks += 3;
      busPrefetchCount = (busPrefetchCount<<clockTicks) | (0xFF>>(8-clockTicks));
      clockTicks += codeTicksAccess16(armNextPC) + 1;
       Z_FLAG = reg[dest].I ? false : true;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
     }
     break;
   case 0x02:
     {
       // BIC Rd, Rs
       int dest = opcode & 7;
       reg[dest].I &= (~reg[(opcode >> 3) & 7].I);
       Z_FLAG = reg[dest].I ? false : true;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
     }
     break;
   case 0x03:
     {
       // MVN Rd, Rs
       int dest = opcode & 7;
       reg[dest].I = ~reg[(opcode >> 3) & 7].I;
       Z_FLAG = reg[dest].I ? false : true;
       N_FLAG = reg[dest].I & 0x80000000 ? true : false;
     }
     break;
   }
   break;
 case 0x44:
   {
     int dest = opcode & 7;
     int base = (opcode >> 3) & 7;
     switch((opcode >> 6)& 3) {
     default:
       goto unknown_thumb;
     case 1:
       // ADD Rd, Hs
       reg[dest].I += reg[base+8].I;
       break;
     case 2:
       // ADD Hd, Rs
       reg[dest+8].I += reg[base].I;
       if(dest == 7) {
         reg[15].I &= 0xFFFFFFFE;
         armNextPC = reg[15].I;
         reg[15].I += 2;
         THUMB_PREFETCH;
         clockTicks = codeTicksAccessSeq16(armNextPC)+1;
         clockTicks += clockTicks+codeTicksAccess16(armNextPC)+1;
       }       
       break;
     case 3:
       // ADD Hd, Hs
       reg[dest+8].I += reg[base+8].I;
       if(dest == 7) {
         reg[15].I &= 0xFFFFFFFE;
         armNextPC = reg[15].I;
         reg[15].I += 2;
         THUMB_PREFETCH;
         clockTicks = codeTicksAccessSeq16(armNextPC)+1;
         clockTicks += clockTicks+codeTicksAccess16(armNextPC)+1;     
       }
       break;
     }
   }
   break;
 case 0x45:
   {
     int dest = opcode & 7;
     int base = (opcode >> 3) & 7;
     u32 value;
     switch((opcode >> 6) & 3) {
     case 0:
       // CMP Rd, Hs
       value = reg[base].I;
       CMP_RD_RS;
       break;
     case 1:
       // CMP Rd, Hs
       value = reg[base+8].I;
       CMP_RD_RS;
       break;
     case 2:
       // CMP Hd, Rs
       value = reg[base].I;
       dest += 8;
       CMP_RD_RS;
       break;
     case 3:
       // CMP Hd, Hs
       value = reg[base+8].I;
       dest += 8;
       CMP_RD_RS;
       break;
     }
   }
   break;
 case 0x46:
   {
     int dest = opcode & 7;
     int base = (opcode >> 3) & 7;
     switch((opcode >> 6) & 3) {
     case 0:
       // this form should not be used...
       // MOV Rd, Rs
       reg[dest].I = reg[base].I;
       break;
     case 1:
       // MOV Rd, Hs
       reg[dest].I = reg[base+8].I;
       break;
     case 2:
       // MOV Hd, Rs
       reg[dest+8].I = reg[base].I;
       if(dest == 7) {
#ifdef BKPT_SUPPORT
	     UPDATE_OLD_REG
#endif

         reg[15].I &= 0xFFFFFFFE;
         armNextPC = reg[15].I;
         reg[15].I += 2;
         THUMB_PREFETCH;
         clockTicks = codeTicksAccessSeq16(armNextPC)+1;
         clockTicks += clockTicks+codeTicksAccess16(armNextPC)+1; 
       }
       break;
     case 3:
       // MOV Hd, Hs
       reg[dest+8].I = reg[base+8].I;
       if(dest == 7) {

#ifdef BKPT_SUPPORT
	     UPDATE_OLD_REG
#endif

         reg[15].I &= 0xFFFFFFFE;
         armNextPC = reg[15].I;
         reg[15].I += 2;
         THUMB_PREFETCH;
         clockTicks = codeTicksAccessSeq16(armNextPC)+1;
         clockTicks += clockTicks+codeTicksAccess16(armNextPC)+1; 
       }   
       break;
     }
   }
   break;
 case 0x47:
   {
     int base = (opcode >> 3) & 7;
     busPrefetchCount=0;
     switch((opcode >>6) & 3) {
     case 0:
       // BX Rs
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
       reg[15].I = (reg[base].I) & 0xFFFFFFFE;
       if(reg[base].I & 1) {
         armState = false;
         armNextPC = reg[15].I;
         reg[15].I += 2;
         THUMB_PREFETCH;
         clockTicks = codeTicksAccessSeq16(armNextPC) +
             codeTicksAccessSeq16(armNextPC) + codeTicksAccess16(armNextPC) + 3;
       } else {
         armState = true;
         reg[15].I &= 0xFFFFFFFC;
         armNextPC = reg[15].I;
         reg[15].I += 4;
         ARM_PREFETCH;
         clockTicks = codeTicksAccessSeq32(armNextPC) +
             codeTicksAccessSeq32(armNextPC) + codeTicksAccess32(armNextPC) + 3;
       }
       break;
     case 1:
       // BX Hs

#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif

       reg[15].I = (reg[8+base].I) & 0xFFFFFFFE;
       if(reg[8+base].I & 1) {
         armState = false;
         armNextPC = reg[15].I;
         reg[15].I += 2;
         THUMB_PREFETCH;
         clockTicks = dataTicksAccess16(armNextPC) + dataTicksAccess16(armNextPC) +
             codeTicksAccess16(armNextPC) + 3;
       } else {
         armState = true;
         reg[15].I &= 0xFFFFFFFC;       
         armNextPC = reg[15].I;
         reg[15].I += 4;
         ARM_PREFETCH;
         clockTicks = dataTicksAccess32(armNextPC) + dataTicksAccess32(armNextPC) +
             codeTicksAccess32(armNextPC) + 3;
       }
       break;
     default:
       goto unknown_thumb;
     }
   }
   break;
 case 0x48:
 case 0x49:
 case 0x4a:
 case 0x4b:
 case 0x4c:
 case 0x4d:
 case 0x4e:
 case 0x4f:
   // LDR R0~R7,[PC, #Imm]
   {
   u8 regist = (opcode >> 8) & 7;
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = (reg[15].I & 0xFFFFFFFC) + ((opcode & 0xFF) << 2);
     reg[regist].I = CPUReadMemoryQuick(address);
     busPrefetchCount=0;
     clockTicks = 3 + dataTicksAccess32(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x50:
 case 0x51:
   // STR Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     CPUWriteMemory(address,
                    reg[opcode & 7].I);
     clockTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;
 case 0x52:
 case 0x53:
   // STRH Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     CPUWriteHalfWord(address,
                      reg[opcode&7].W.W0);
     clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;
 case 0x54:
 case 0x55:
   // STRB Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode >>6)&7].I;
     CPUWriteByte(address,
                  reg[opcode & 7].B.B0);
     clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;
 case 0x56:
 case 0x57:
   // LDSB Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     reg[opcode&7].I = (s8)CPUReadByte(address);
     clockTicks = 3 + dataTicksAccess16(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x58:
 case 0x59:
   // LDR Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     reg[opcode&7].I = CPUReadMemory(address);
     clockTicks = 3 + dataTicksAccess32(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x5a:
 case 0x5b:
   // LDRH Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     reg[opcode&7].I = CPUReadHalfWord(address);
     clockTicks = 3 + dataTicksAccess32(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x5c:
 case 0x5d:
   // LDRB Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     reg[opcode&7].I = CPUReadByte(address);
     clockTicks = 3 + dataTicksAccess16(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x5e:
 case 0x5f:
   // LDSH Rd, [Rs, Rn]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
     reg[opcode&7].I = (s16)CPUReadHalfWordSigned(address);
     clockTicks = 3 + dataTicksAccess16(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x60:
 case 0x61:
 case 0x62:
 case 0x63:
 case 0x64:
 case 0x65:
 case 0x66:
 case 0x67:
   // STR Rd, [Rs, #Imm]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<2);
     CPUWriteMemory(address,
                    reg[opcode&7].I);
     clockTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;
 case 0x68:
 case 0x69:
 case 0x6a:
 case 0x6b:
 case 0x6c:
 case 0x6d:
 case 0x6e:
 case 0x6f:
   // LDR Rd, [Rs, #Imm]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<2);
     reg[opcode&7].I = CPUReadMemory(address);
     clockTicks = 3 + dataTicksAccess32(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x70:
 case 0x71:
 case 0x72:
 case 0x73:
 case 0x74:
 case 0x75:
 case 0x76:
 case 0x77:
   // STRB Rd, [Rs, #Imm]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31));
     CPUWriteByte(address,
                  reg[opcode&7].B.B0);
     clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;
 case 0x78:
 case 0x79:
 case 0x7a:
 case 0x7b:
 case 0x7c:
 case 0x7d:
 case 0x7e:
 case 0x7f:
   // LDRB Rd, [Rs, #Imm]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31));
     reg[opcode&7].I = CPUReadByte(address);
     clockTicks = 3 + dataTicksAccess16(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x80:
 case 0x81:
 case 0x82:
 case 0x83:
 case 0x84:
 case 0x85:
 case 0x86:
 case 0x87:
   // STRH Rd, [Rs, #Imm]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<1);
     CPUWriteHalfWord(address,
                      reg[opcode&7].W.W0);
     clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;   
 case 0x88:
 case 0x89:
 case 0x8a:
 case 0x8b:
 case 0x8c:
 case 0x8d:
 case 0x8e:
 case 0x8f:
   // LDRH Rd, [Rs, #Imm]
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<1);
     reg[opcode&7].I = CPUReadHalfWord(address);
     clockTicks = 3 + dataTicksAccess16(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0x90:
 case 0x91:
 case 0x92:
 case 0x93:
 case 0x94:
 case 0x95:
 case 0x96:
 case 0x97:
   // STR R0~R7, [SP, #Imm]
   {
   u8 regist = (opcode >> 8) & 7;
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[13].I + ((opcode&255)<<2);
     CPUWriteMemory(address, reg[regist].I);
     clockTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
   }
   break;      
 case 0x98:
 case 0x99:
 case 0x9a:
 case 0x9b:
 case 0x9c:
 case 0x9d:
 case 0x9e:
 case 0x9f:
   // LDR R0~R7, [SP, #Imm]
   {
   u8 regist = (opcode >> 8) & 7;
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[13].I + ((opcode&255)<<2);   
     reg[regist].I = CPUReadMemoryQuick(address);
     clockTicks = 3 + dataTicksAccess32(address) +
         codeTicksAccess16(armNextPC);
   }
   break;
 case 0xa0:
 case 0xa1:
 case 0xa2:
 case 0xa3:
 case 0xa4:
 case 0xa5:
 case 0xa6:
 case 0xa7:
     {
   // ADD R0~R7, PC, Imm
   u8 regist = (opcode >> 8) & 7;
   reg[regist].I = (reg[15].I & 0xFFFFFFFC) + ((opcode&255)<<2);
     }
   break;   
 case 0xa8:
 case 0xa9:
 case 0xaa:
 case 0xab:
 case 0xac:
 case 0xad:
 case 0xae:
 case 0xaf:
     {
   // ADD R0~R7, SP, Imm
   u8 regist = (opcode >> 8) & 7;
   reg[regist].I = reg[13].I + ((opcode&255)<<2);
     }
   break;     
 case 0xb0:
   {
     // ADD SP, Imm
     int offset = (opcode & 127) << 2;
     if(opcode & 0x80)
       offset = -offset;
     reg[13].I += offset;
   }
   break;
#define PUSH_REG(val, r) \
  if(opcode & (val)) {\
    CPUWriteMemory(address, reg[(r)].I);\
    if(offset)\
      clockTicks += 1 + dataTicksAccessSeq32(address);\
    else\
      clockTicks += 1 + dataTicksAccess32(address);\
    offset = 1;\
    address += 4;\
  }
 case 0xb4:
   // PUSH {Rlist}
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     int offset = 0;
     u32 temp = reg[13].I - 4 * cpuBitsSet[opcode & 0xff];
     u32 address = temp & 0xFFFFFFFC;
     PUSH_REG(1, 0);
     PUSH_REG(2, 1);
     PUSH_REG(4, 2);
     PUSH_REG(8, 3);
     PUSH_REG(16, 4);
     PUSH_REG(32, 5);
     PUSH_REG(64, 6);
     PUSH_REG(128, 7);
     clockTicks += codeTicksAccess16(armNextPC)+1;
     reg[13].I = temp;
   }
   break;
 case 0xb5:
   // PUSH {Rlist, LR}
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     int offset = 0;
     u32 temp = reg[13].I - 4 - 4 * cpuBitsSet[opcode & 0xff];
     u32 address = temp & 0xFFFFFFFC;
     PUSH_REG(1, 0);
     PUSH_REG(2, 1);
     PUSH_REG(4, 2);
     PUSH_REG(8, 3);
     PUSH_REG(16, 4);
     PUSH_REG(32, 5);
     PUSH_REG(64, 6);
     PUSH_REG(128, 7);
     PUSH_REG(256, 14);
     clockTicks += codeTicksAccess16(armNextPC)+1;
     reg[13].I = temp;
   }
   break;
#define POP_REG(val, r) \
  if(opcode & (val)) {\
    reg[(r)].I = CPUReadMemory(address);\
    if(offset)\
      clockTicks += 1 + dataTicksAccessSeq32(address);\
    else\
      clockTicks += 1 + dataTicksAccess32(address);\
    offset = 1;\
    address += 4;\
  }
 case 0xbc:
   // POP {Rlist}
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     int offset = 0;
     u32 address = reg[13].I & 0xFFFFFFFC;
     u32 temp = reg[13].I + 4*cpuBitsSet[opcode & 0xFF];
     clockTicks = 0;
     POP_REG(1, 0);
     POP_REG(2, 1);
     POP_REG(4, 2);
     POP_REG(8, 3);
     POP_REG(16, 4);
     POP_REG(32, 5);
     POP_REG(64, 6);
     POP_REG(128, 7);
     reg[13].I = temp;
     clockTicks += codeTicksAccess16(armNextPC)+2;
   }
   break;   
 case 0xbd:
   // POP {Rlist, PC}
   {
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     int offset = 0;
     u32 address = reg[13].I & 0xFFFFFFFC;
     u32 temp = reg[13].I + 4 + 4*cpuBitsSet[opcode & 0xFF];
     clockTicks = 0;
     POP_REG(1, 0);
     POP_REG(2, 1);
     POP_REG(4, 2);
     POP_REG(8, 3);
     POP_REG(16, 4);
     POP_REG(32, 5);
     POP_REG(64, 6);
     POP_REG(128, 7);
     reg[15].I = (CPUReadMemory(address) & 0xFFFFFFFE);
     if(offset)
       clockTicks += 1 + dataTicksAccessSeq32(address);
     else
       clockTicks += 1 + dataTicksAccess32(address);
     armNextPC = reg[15].I;
     reg[15].I += 2;
     reg[13].I = temp;
     THUMB_PREFETCH;
     busPrefetchCount=0;
     clockTicks += codeTicksAccess16(armNextPC) + codeTicksAccess16(armNextPC) + 3;
   }
   break;      
#define THUMB_STM_REG(val,r,b) \
  if(opcode & (val)) {\
    CPUWriteMemory(address, reg[(r)].I);\
    if(!offset) {\
      reg[(b)].I = temp;\
      clockTicks += 1 + dataTicksAccess32(address);\
    } else \
      clockTicks += 1 + dataTicksAccessSeq32(address);\
    offset = 1;\
    address += 4;\
  }
 case 0xc0:
 case 0xc1:
 case 0xc2:
 case 0xc3:
 case 0xc4:
 case 0xc5:
 case 0xc6:
 case 0xc7:
   {
     // STM R0~7!, {Rlist}
     u8 regist = (opcode >> 8) & 7;
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[regist].I & 0xFFFFFFFC;
     u32 temp = reg[regist].I + 4*cpuBitsSet[opcode & 0xff];
     int offset = 0;
     // store
     THUMB_STM_REG(1, 0, regist);
     THUMB_STM_REG(2, 1, regist);
     THUMB_STM_REG(4, 2, regist);
     THUMB_STM_REG(8, 3, regist);
     THUMB_STM_REG(16, 4, regist);
     THUMB_STM_REG(32, 5, regist);
     THUMB_STM_REG(64, 6, regist);
     THUMB_STM_REG(128, 7, regist);
     clockTicks = codeTicksAccess16(armNextPC)+1;
   }
   break;   
#define THUMB_LDM_REG(val,r) \
  if(opcode & (val)) {\
    reg[(r)].I = CPUReadMemory(address);\
    if(offset)\
      clockTicks += 1 + dataTicksAccessSeq32(address);\
    else \
      clockTicks += 1 + dataTicksAccess32(address);\
    offset = 1;\
    address += 4;\
  }
 case 0xc8:
 case 0xc9:
 case 0xca:
 case 0xcb:
 case 0xcc:
 case 0xcd:
 case 0xce:
 case 0xcf:
   {
     // LDM R0~R7!, {Rlist}
     u8 regist = (opcode >> 8) & 7;
     if (!busPrefetchCount)
       busPrefetch = busPrefetchEnable;
     u32 address = reg[regist].I & 0xFFFFFFFC;
     u32 temp = reg[regist].I + 4*cpuBitsSet[opcode & 0xFF];
     int offset = 0;
     clockTicks = 0;
     // load
     THUMB_LDM_REG(1, 0);
     THUMB_LDM_REG(2, 1);
     THUMB_LDM_REG(4, 2);
     THUMB_LDM_REG(8, 3);
     THUMB_LDM_REG(16, 4);
     THUMB_LDM_REG(32, 5);
     THUMB_LDM_REG(64, 6);
     THUMB_LDM_REG(128, 7);
     clockTicks += codeTicksAccess16(armNextPC)+2;
     if(!(opcode & (1<<regist)))
       reg[regist].I = temp;
   }
   break;
 case 0xd0:
   // BEQ offset
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
   if(Z_FLAG) {
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;      
 case 0xd1:
   // BNE offset
   if(!Z_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd2:
   // BCS offset
   if(C_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd3:
   // BCC offset
   if(!C_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd4:
   // BMI offset
   if(N_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd5:
   // BPL offset
   if(!N_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd6:
   // BVS offset
   if(V_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd7:
   // BVC offset
   if(!V_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd8:
   // BHI offset
   if(C_FLAG && !Z_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xd9:
   // BLS offset
   if(!C_FLAG || Z_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xda:
   // BGE offset
   if(N_FLAG == V_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xdb:
   // BLT offset
   if(N_FLAG != V_FLAG) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xdc:
   // BGT offset
   if(!Z_FLAG && (N_FLAG == V_FLAG)) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xdd:
   // BLE offset
   if(Z_FLAG || (N_FLAG != V_FLAG)) {
#ifdef BKPT_SUPPORT
		 UPDATE_OLD_REG
#endif
     reg[15].I += ((s8)(opcode & 0xFF)) << 1;       
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC)+3;
     busPrefetchCount=0;
   }
   break;   
 case 0xdf:
     {
   // SWI #comment
   u32 address = 0;
   clockTicks = codeTicksAccessSeq16(address) + codeTicksAccessSeq16(address) +
       codeTicksAccess16(address)+3;
   busPrefetchCount=0;
   CPUSoftwareInterrupt(opcode & 0xFF);
   break;
     }
 case 0xe0:
 case 0xe1:
 case 0xe2:
 case 0xe3:
 case 0xe4:
 case 0xe5:
 case 0xe6:
 case 0xe7:
   {
     // B offset
     int offset = (opcode & 0x3FF) << 1;
     if(opcode & 0x0400)
       offset |= 0xFFFFF800;
     reg[15].I += offset;
     armNextPC = reg[15].I;
     reg[15].I += 2;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) + codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC) + 3;
     busPrefetchCount=0;
   }
   break;
 case 0xf0:
 case 0xf1:
 case 0xf2:
 case 0xf3:
   {
     // BLL #offset
     int offset = (opcode & 0x7FF);
     reg[14].I = reg[15].I + (offset << 12);
     clockTicks = codeTicksAccessSeq16(armNextPC) + 1;
   }
   break;      
 case 0xf4:
 case 0xf5:
 case 0xf6:
 case 0xf7:
   {
     // BLL #offset
     int offset = (opcode & 0x7FF);
     reg[14].I = reg[15].I + ((offset << 12) | 0xFF800000);
     clockTicks = codeTicksAccessSeq16(armNextPC) + 1;
   }
   break;   
 case 0xf8:
 case 0xf9:
 case 0xfa:
 case 0xfb:
 case 0xfc:
 case 0xfd:
 case 0xfe:
 case 0xff:
   {
     // BLH #offset
     int offset = (opcode & 0x7FF);
     u32 temp = reg[15].I-2;
     reg[15].I = (reg[14].I + (offset<<1))&0xFFFFFFFE;
     armNextPC = reg[15].I;
     reg[15].I += 2;
     reg[14].I = temp|1;
     THUMB_PREFETCH;
     clockTicks = codeTicksAccessSeq16(armNextPC) +
         codeTicksAccess16(armNextPC) + codeTicksAccessSeq16(armNextPC) + 3;
     busPrefetchCount = 0;
   }
   break;
#ifdef BKPT_SUPPORT
 case 0xbe:
   // BKPT #comment
   extern void (*dbgSignal)(int,int);
   reg[15].I -= 2;
   armNextPC -= 2;   
   dbgSignal(5, opcode & 255);
   return;
#endif
 case 0xb1:
 case 0xb2:
 case 0xb3:
 case 0xb6:
 case 0xb7:
 case 0xb8:
 case 0xb9:
 case 0xba:
 case 0xbb:
#ifndef BKPT_SUPPORT
 case 0xbe:
#endif
 case 0xbf:
 case 0xde:
 default:
 unknown_thumb:
#ifdef DEV_VERSION
   if(systemVerbose & VERBOSE_UNDEFINED)
     log("Undefined THUMB instruction %04x at %08x\n", opcode, armNextPC-2);
#endif
   CPUUndefinedException();
   break;
}

if (clockTicks==0)
clockTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;
