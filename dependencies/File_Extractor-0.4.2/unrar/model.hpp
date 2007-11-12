// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef _RAR_PPMMODEL_
#define _RAR_PPMMODEL_

const int MAX_O=64;                   /* maximum allowed model order */

const int INT_BITS=7, PERIOD_BITS=7, TOT_BITS=INT_BITS+PERIOD_BITS,
					INTERVAL=1 << INT_BITS, BIN_SCALE=1 << TOT_BITS, MAX_FREQ=124;

#pragma pack(1)

struct _PACK_ATTR SEE2_CONTEXT
{ // SEE-contexts for PPM-contexts with masked symbols
	ushort Summ;
	byte Shift, Count;
	void init(int InitVal)
	{
		Summ=InitVal << (Shift=PERIOD_BITS-4);
		Count=4;
	}
	uint getMean()
	{
		uint RetVal=(Summ & 0xFFFF) >> Shift;
		Summ -= RetVal;
		return RetVal+(RetVal == 0);
	}
	void update()
	{
		if (Shift < PERIOD_BITS && --Count == 0)
		{
			Summ += Summ;
			Count=3 << Shift++;
		}
	}
};


class ModelPPM;
struct PPM_CONTEXT;

struct _PACK_ATTR STATE
{
	byte Symbol;
	byte Freq;
	PPM_CONTEXT* Successor;
};

struct _PACK_ATTR FreqData
{
	ushort SummFreq;
	STATE* Stats;
};

struct PPM_CONTEXT 
{
		ushort NumStats;
		union
		{
			FreqData U;
			STATE OneState;
		};

		PPM_CONTEXT* Suffix;
		inline void encodeBinSymbol(ModelPPM *Model,int symbol);  // MaxOrder:
		inline void encodeSymbol1(ModelPPM *Model,int symbol);    //  ABCD    context
		inline void encodeSymbol2(ModelPPM *Model,int symbol);    //   BCD    suffix
		inline void decodeBinSymbol(ModelPPM *Model);  //   BCDE   successor
		inline bool decodeSymbol1(ModelPPM *Model);    // other orders:
		inline bool decodeSymbol2(ModelPPM *Model);    //   BCD    context
		inline void update1(ModelPPM *Model,STATE* p); //    CD    suffix
		inline void update2(ModelPPM *Model,STATE* p); //   BCDE   successor
		void rescale(ModelPPM *Model);
		inline PPM_CONTEXT* createChild(ModelPPM *Model,STATE* pStats,STATE& FirstState);
		inline SEE2_CONTEXT* makeEscFreq2(ModelPPM *Model,int Diff);
};
#pragma pack()

#define Max( x, y ) ((x) < (y) ? (y) : (x))
const uint UNIT_SIZE=Max(sizeof(PPM_CONTEXT),sizeof(RAR_MEM_BLK));
const uint FIXED_UNIT_SIZE=12;

/*
inline PPM_CONTEXT::PPM_CONTEXT(STATE* pStats,PPM_CONTEXT* ShorterContext):
				NumStats(1), Suffix(ShorterContext) { pStats->Successor=this; }
inline PPM_CONTEXT::PPM_CONTEXT(): NumStats(0) {}
*/

inline void _PPMD_SWAP( STATE& t1,STATE& t2) { STATE tmp=t1; t1=t2; t2=tmp; }

// 'Carryless rangecoder' by Dmitry Subbotin
class RangeCoder {
public:
	void InitDecoder(Unpack *UnpackRead);
	inline int GetCurrentCount();
	inline uint GetCurrentShiftCount(uint SHIFT);
	inline void Decode();
	inline void PutChar(unsigned int c);
	inline unsigned int GetChar();

	uint low, code, range;
	struct SUBRANGE 
	{
		uint LowCount, HighCount, scale;
	} SubRange;

	Unpack *UnpackRead;
};

class ModelPPM
{
	private:
		friend struct PPM_CONTEXT;
		
		SEE2_CONTEXT SEE2Cont[25][16], DummySEE2Cont;
		
		struct PPM_CONTEXT *MinContext, *MedContext, *MaxContext;
		STATE* FoundState;      // found next state transition
		int NumMasked, InitEsc, OrderFall, MaxOrder, RunLength, InitRL;
		byte CharMask[256], NS2Indx[256], NS2BSIndx[256], HB2Flag[256];
		byte EscCount, PrevSuccess, HiBitsFlag;
		ushort BinSumm[128][64];               // binary SEE-contexts

		RangeCoder Coder;
		SubAllocator SubAlloc;

		void RestartModelRare();
		void StartModelRare(int MaxOrder);
		inline PPM_CONTEXT* CreateSuccessors(bool Skip,STATE* p1);

		inline void UpdateModel();
		inline void ClearMask();
	public:
		int MaxMB; // mega memory allocation size, for informational purposes only
		ModelPPM( Rar_Error_Handler* );
		bool DecodeInit(Unpack *UnpackRead,int &EscChar);
		int DecodeChar();
};

#endif
