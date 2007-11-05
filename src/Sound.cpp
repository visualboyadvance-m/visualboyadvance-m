// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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

#include <memory.h>

#include "GBA.h"
#include "Globals.h"
#include "Sound.h"
#include "Util.h"

#include "snd_interp.h"

#include "Gb_Apu/Multi_Buffer.h"
#include "Gb_Apu/Gb_Apu.h"

#ifdef _WIN32
#include <float.h>
#endif

#define USE_TICKS_AS  380
#define SOUND_MAGIC   0x60000000
#define SOUND_MAGIC_2 0x30000000
#define NOISE_MAGIC 5

extern bool stopState;
extern bool cpuDmaHack2;

u8 soundWavePattern[4][32] = {
  {0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff},
  {0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff},
  {0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff},
  {0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff}
};

int soundFreqRatio[8] = {
  1048576, // 0
  524288,  // 1
  262144,  // 2
  174763,  // 3
  131072,  // 4
  104858,  // 5
  87381,   // 6
  74898    // 7
};

int soundShiftClock[16]= {
      2, // 0
      4, // 1
      8, // 2
     16, // 3
     32, // 4
     64, // 5
    128, // 6
    256, // 7
    512, // 8
   1024, // 9
   2048, // 10
   4096, // 11
   8192, // 12
  16384, // 13
  1,     // 14
  1      // 15
};

int soundVolume = 0;

u8 soundBuffer[6][735];
u16 directBuffer[2][735];
u16 soundFinalWave[1470];

int soundBufferLen = 1470;
int soundBufferTotalLen = 14700;
int soundQuality = 2;
int soundInterpolation = 0;
bool soundPaused = true;
int soundPlay = 0;
int soundTicks = soundQuality * USE_TICKS_AS;
int SOUND_CLOCK_TICKS = soundQuality * USE_TICKS_AS;
u32 soundNextPosition = 0;

Multi_Buffer * apu_out   = NULL;
Gb_Apu       * apu       = NULL;
bool           apu_saved = false;

int soundLevel1 = 0;
int soundLevel2 = 0;
int soundBalance = 0;
int soundMasterOn = 0;
int soundIndex = 0;
int soundBufferIndex = 0;
int soundDebug = 0;
bool soundOffFlag = false;

int sound1On = 0;
int sound1ATL = 0;
int sound1Skip = 0;
int sound1Index = 0;
int sound1Continue = 0;
int sound1EnvelopeVolume =  0;
int sound1EnvelopeATL = 0;
int sound1EnvelopeUpDown = 0;
int sound1EnvelopeATLReload = 0;
int sound1SweepATL = 0;
int sound1SweepATLReload = 0;
int sound1SweepSteps = 0;
int sound1SweepUpDown = 0;
int sound1SweepStep = 0;
u8 *sound1Wave = soundWavePattern[2];

int sound2On = 0;
int sound2ATL = 0;
int sound2Skip = 0;
int sound2Index = 0;
int sound2Continue = 0;
int sound2EnvelopeVolume =  0;
int sound2EnvelopeATL = 0;
int sound2EnvelopeUpDown = 0;
int sound2EnvelopeATLReload = 0;
u8 *sound2Wave = soundWavePattern[2];

int sound3On = 0;
int sound3ATL = 0;
int sound3Skip = 0;
int sound3Index = 0;
int sound3Continue = 0;
int sound3OutputLevel = 0;
int sound3Last = 0;
u8 sound3WaveRam[0x20];
int sound3Bank = 0;
int sound3DataSize = 0;
int sound3ForcedOutput = 0;

int sound4On = 0;
int sound4Clock = 0;
int sound4ATL = 0;
int sound4Skip = 0;
int sound4Index = 0;
int sound4ShiftRight = 0x7f;
int sound4ShiftSkip = 0;
int sound4ShiftIndex = 0;
int sound4NSteps = 0;
int sound4CountDown = 0;
int sound4Continue = 0;
int sound4EnvelopeVolume =  0;
int sound4EnvelopeATL = 0;
int sound4EnvelopeUpDown = 0;
int sound4EnvelopeATLReload = 0;

int soundControl = 0;

int soundDSFifoAIndex = 0;
int soundDSFifoACount = 0;
int soundDSFifoAWriteIndex = 0;
bool soundDSAEnabled = false;
int soundDSATimer = 0;
u8  soundDSFifoA[32];
u8 soundDSAValue = 0;

int soundDSFifoBIndex = 0;
int soundDSFifoBCount = 0;
int soundDSFifoBWriteIndex = 0;
bool soundDSBEnabled = false;
int soundDSBTimer = 0;
u8  soundDSFifoB[32];
u8 soundDSBValue = 0;

int soundEnableFlag = 0x3ff;

s16 soundFilter[4000];
s16 soundRight[5] = { 0, 0, 0, 0, 0 };
s16 soundLeft[5] = { 0, 0, 0, 0, 0 };
int soundEchoIndex = 0;
bool soundEcho = false;
bool soundLowPass = false;
bool soundReverse = false;

variable_desc soundSaveStruct[] = {
  { &soundPaused, sizeof(int) },
  { &soundPlay, sizeof(int) },
  { &soundTicks, sizeof(int) },
  { &SOUND_CLOCK_TICKS, sizeof(int) },
  { &soundLevel1, sizeof(int) },
  { &soundLevel2, sizeof(int) },
  { &soundBalance, sizeof(int) },
  { &soundMasterOn, sizeof(int) },
  { &soundIndex, sizeof(int) },
  { &sound1On, sizeof(int) },
  { &sound1ATL, sizeof(int) },
  { &sound1Skip, sizeof(int) },
  { &sound1Index, sizeof(int) },
  { &sound1Continue, sizeof(int) },
  { &sound1EnvelopeVolume, sizeof(int) },
  { &sound1EnvelopeATL, sizeof(int) },
  { &sound1EnvelopeATLReload, sizeof(int) },
  { &sound1EnvelopeUpDown, sizeof(int) },
  { &sound1SweepATL, sizeof(int) },
  { &sound1SweepATLReload, sizeof(int) },
  { &sound1SweepSteps, sizeof(int) },
  { &sound1SweepUpDown, sizeof(int) },
  { &sound1SweepStep, sizeof(int) },
  { &sound2On, sizeof(int) },
  { &sound2ATL, sizeof(int) },
  { &sound2Skip, sizeof(int) },
  { &sound2Index, sizeof(int) },
  { &sound2Continue, sizeof(int) },
  { &sound2EnvelopeVolume, sizeof(int) },
  { &sound2EnvelopeATL, sizeof(int) },
  { &sound2EnvelopeATLReload, sizeof(int) },
  { &sound2EnvelopeUpDown, sizeof(int) },
  { &sound3On, sizeof(int) },
  { &sound3ATL, sizeof(int) },
  { &sound3Skip, sizeof(int) },
  { &sound3Index, sizeof(int) },
  { &sound3Continue, sizeof(int) },
  { &sound3OutputLevel, sizeof(int) },
  { &sound4On, sizeof(int) },
  { &sound4ATL, sizeof(int) },
  { &sound4Skip, sizeof(int) },
  { &sound4Index, sizeof(int) },
  { &sound4Clock, sizeof(int) },
  { &sound4ShiftRight, sizeof(int) },
  { &sound4ShiftSkip, sizeof(int) },
  { &sound4ShiftIndex, sizeof(int) },
  { &sound4NSteps, sizeof(int) },
  { &sound4CountDown, sizeof(int) },
  { &sound4Continue, sizeof(int) },
  { &sound4EnvelopeVolume, sizeof(int) },
  { &sound4EnvelopeATL, sizeof(int) },
  { &sound4EnvelopeATLReload, sizeof(int) },
  { &sound4EnvelopeUpDown, sizeof(int) },
  { &soundEnableFlag, sizeof(int) },
  { &soundControl, sizeof(int) },
  { &soundDSFifoAIndex, sizeof(int) },
  { &soundDSFifoACount, sizeof(int) },
  { &soundDSFifoAWriteIndex, sizeof(int) },
  { &soundDSAEnabled, sizeof(bool) },
  { &soundDSATimer, sizeof(int) },
  { &soundDSFifoA[0], 32 },
  { &soundDSAValue, sizeof(u8) },
  { &soundDSFifoBIndex, sizeof(int) },
  { &soundDSFifoBCount, sizeof(int) },
  { &soundDSFifoBWriteIndex, sizeof(int) },
  { &soundDSBEnabled, sizeof(int) },
  { &soundDSBTimer, sizeof(int) },
  { &soundDSFifoB[0], 32 },
  { &soundDSBValue, sizeof(int) },
  { &soundBuffer[0][0], 6*735 },
  { &soundFinalWave[0], 2*735 },
  { NULL, 0 }
};

variable_desc soundSaveStructV2[] = {
  { &sound3WaveRam[0], 0x20 },
  { &sound3Bank, sizeof(int) },
  { &sound3DataSize, sizeof(int) },
  { &sound3ForcedOutput, sizeof(int) },
  { NULL, 0 }
};


// *** Begin snd_interp code 


// and here is the implementation specific code, in a messier state than the stuff above

extern bool timer0On;
extern int timer0Reload;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Reload;
extern int timer1ClockReload;

extern int SOUND_CLOCK_TICKS;
extern int soundInterpolation;

inline double calc_rate(int timer)
{
	if (timer ? timer1On : timer0On)
	{
		return double(SOUND_CLOCK_TICKS) /
			double((0x10000 - (timer ? timer1Reload : timer0Reload)) * 
			(timer ? timer1ClockReload : timer0ClockReload));
	}
	else
	{
		return 1.;
	}
}

static foo_interpolate * interp[2];

class foo_interpolate_setup
{
public:
	foo_interpolate_setup()
	{
		for (int i = 0; i < 2; i++)
		{
			interp[i] = get_filter(0);
		}
	}

	~foo_interpolate_setup()
	{
		for (int i = 0; i < 2; i++)
		{
			delete interp[i];
		}
	}
};

static foo_interpolate_setup blah;

static int interpolation = 0;

void interp_switch(int which)
{
	for (int i = 0; i < 2; i++)
	{
		delete interp[i];
		interp[i] = get_filter(which);
	}
	interp_rate();
	interpolation = which;
}


void interp_rate()
{
	interp[0]->rate(calc_rate(soundDSATimer));
	interp[1]->rate(calc_rate(soundDSBTimer));
}

void interp_reset(int ch)
{
	setSoundFn();
	interp[ch]->reset();
	interp_rate();
}


inline void interp_push(int ch, int sample)
{
	interp[ch]->push(sample);
}

#ifdef ENHANCED_RATE
inline int interp_pop(int ch, double rate)
{
	return interp[ch]->pop(rate);
#else
inline int interp_pop(int ch)
{
	return interp[ch]->pop();
#endif
}


// *** End snd_interp code

static void soundEventGB(u32 address, u8 data)
  {
  if ( apu )
  {
    int divisor = 4 * soundQuality;
    int clock = (SOUND_CLOCK_TICKS - soundTicks + (divisor >> 1)) / divisor;
    apu->write_register(clock, address, data);
  }
  else
    ioMem[address] = data;
}

u8 soundRead(u32 address)
{
  if (address < NR10 || address > 0x9F || !apu) return ioMem[address];
  if (address == NR51) return soundBalance;

  int divisor = 4 * soundQuality;
  int clock = (SOUND_CLOCK_TICKS - soundTicks + (divisor >> 1)) / divisor;
  switch (address)
  {
  case NR10:
    return apu->read_register(clock, 0xFF10);
  case NR11:
    return apu->read_register(clock, 0xFF11);
  case NR12:
    return apu->read_register(clock, 0xFF12);
  case NR13:
    return apu->read_register(clock, 0xFF13);
  case NR14:
    return apu->read_register(clock, 0xFF14);
  case NR21:
    return apu->read_register(clock, 0xFF16);
  case NR22:
    return apu->read_register(clock, 0xFF17);
  case NR23:
    return apu->read_register(clock, 0xFF18);
  case NR24:
    return apu->read_register(clock, 0xFF19);
  case NR30:
    return apu->read_register(clock, 0xFF1A);
  case NR31:
    return apu->read_register(clock, 0xFF1B);
  case NR32:
    return apu->read_register(clock, 0xFF1C);
  case NR33:
    return apu->read_register(clock, 0xFF1D);
  case NR34:
    return apu->read_register(clock, 0xFF1E);
  case NR41:
    return apu->read_register(clock, 0xFF20);
  case NR42:
    return apu->read_register(clock, 0xFF21);
  case NR43:
    return apu->read_register(clock, 0xFF22);
  case NR44:
    return apu->read_register(clock, 0xFF23);
  case NR50:
    return apu->read_register(clock, 0xFF24);
  case NR51:
    return apu->read_register(clock, 0xFF25);
  case NR52:
    return apu->read_register(clock, 0xFF26);
  case 0x90:
  case 0x91:
  case 0x92:
  case 0x93:
  case 0x94:
  case 0x95:
  case 0x96:
  case 0x97:
  case 0x98:
  case 0x99:
  case 0x9a:
  case 0x9b:
  case 0x9c:
  case 0x9d:
  case 0x9e:
  case 0x9f:
    return apu->read_register(clock, 0xFF30 - 0x90 + address);
  }
 
  return ioMem[address];
 }
 
 u16 soundRead16(u32 address)
 {
  address &= ~1;
  u16 temp = soundRead(address);
  temp |= soundRead(address + 1) << 8;
  return temp;
 }
 
 u32 soundRead32(u32 address)
 {
  address &= ~3;
  u32 temp = soundRead16(address);
  temp |= soundRead16(address + 2) << 16;
  return temp;
 }
  
 
 void soundEvent(u32 address, u8 data)
 {
    switch(address) {
    case NR10:
    soundEventGB(0xFF10, data);
      break;
    case NR11:
    soundEventGB(0xFF11, data);
      break;
    case NR12:
    soundEventGB(0xFF12, data);
      break;
    case NR13:
    soundEventGB(0xFF13, data);
      break;
    case NR14:
    soundEventGB(0xFF14, data);
      break;
    case NR21:
    soundEventGB(0xFF16, data);
      break;
    case NR22:
    soundEventGB(0xFF17, data);
      break;
    case NR23:
    soundEventGB(0xFF18, data);
      break;
    case NR24:
    soundEventGB(0xFF19, data);
    break;
    case NR30:
    soundEventGB(0xFF1A, data);
      break;
    case NR31:
    soundEventGB(0xFF1B, data);
      break;
    case NR32:
    soundEventGB(0xFF1C, data);
      break;
    case NR33:
    soundEventGB(0xFF1D, data);
      break;
    case NR34:
    soundEventGB(0xFF1E, data);
      break;
    case NR41:
    soundEventGB(0xFF20, data);
      break;
    case NR42:
    soundEventGB(0xFF21, data);
      break;
    case NR43:
    soundEventGB(0xFF22, data);
      break;
    case NR44:
    soundEventGB(0xFF23, data);
      break;
    case NR50:
    soundEventGB(0xFF24, data);
      break;
    case NR51:
    soundBalance = data;
    soundEventGB(0xFF25, data & soundEnableFlag);
      break;
    case NR52:
    soundEventGB(0xFF26, data);
      break;

  case 0x90:
  case 0x92:
  case 0x94:
  case 0x96:
  case 0x98:
  case 0x9a:
  case 0x9c:
  case 0x9e:
    soundEventGB(0xFF30 + (address & 14), data);
    soundEventGB(0xFF31 + (address & 14), 0); // data >> 8);
    break;    
  }
}

void soundEvent(u32 address, u16 data)
{
  switch(address) {
  case SGCNT0_H:
    data &= 0xFF0F;
    soundControl = data & 0x770F;;
    if(data & 0x0800) {
      soundDSFifoAWriteIndex = 0;
      soundDSFifoAIndex = 0;
      soundDSFifoACount = 0;
      soundDSAValue = 0;
      memset(soundDSFifoA, 0, 32);
    }
    soundDSAEnabled = (data & 0x0300) ? true : false;
    soundDSATimer = (data & 0x0400) ? 1 : 0;    
    if(data & 0x8000) {
      soundDSFifoBWriteIndex = 0;
      soundDSFifoBIndex = 0;
      soundDSFifoBCount = 0;
      soundDSBValue = 0;
      memset(soundDSFifoB, 0, 32);
    }
    soundDSBEnabled = (data & 0x3000) ? true : false;
    soundDSBTimer = (data & 0x4000) ? 1 : 0;
	if (data & 0x8000) 
	{
		interp_reset(0);
		interp_reset(1);
	}
	*((u16 *)&ioMem[address]) = data;    
    break;
  case FIFOA_L:
  case FIFOA_H:
    soundDSFifoA[soundDSFifoAWriteIndex++] = data & 0xFF;
    soundDSFifoA[soundDSFifoAWriteIndex++] = data >> 8;
    soundDSFifoACount += 2;
    soundDSFifoAWriteIndex &= 31;
    *((u16 *)&ioMem[address]) = data;    
    break;
  case FIFOB_L:
  case FIFOB_H:
    soundDSFifoB[soundDSFifoBWriteIndex++] = data & 0xFF;
    soundDSFifoB[soundDSFifoBWriteIndex++] = data >> 8;
    soundDSFifoBCount += 2;
    soundDSFifoBWriteIndex &= 31;
    *((u16 *)&ioMem[address]) = data;    
    break;
  case 0x88:
    data &= 0xC3FF;
    *((u16 *)&ioMem[address]) = data;
    break;
  case 0x90:
  case 0x92:
  case 0x94:
  case 0x96:
  case 0x98:
  case 0x9a:
  case 0x9c:
  case 0x9e:
    soundEventGB(0xFF30 + (address & 14), data & 0xff);
    soundEventGB(0xFF31 + (address & 14), data >> 8);  
    break;    
  }
}

void soundChannel1()
{
}

void soundChannel2()
{
}  

void soundChannel3()
{
}

void soundChannel4()
{
}

#include <stdio.h>


inline void soundDirectSoundA()
{
#ifdef ENHANCED_RATE
	double cr = calc_rate(soundDSATimer);
	static int cnt = 0;
	static double lastcr = 0;
	static FILE *fp = NULL;
	
	if (fp==NULL)
		fp=fopen("C:\\cr.txt", "at");
	if (cr!=lastcr)
	{
		fprintf(fp, "%f %d\n", lastcr, cnt);
		cnt=0;
		lastcr=cr;
	}
	else
		cnt++;

	directBuffer[0][soundIndex] = interp_pop(0, calc_rate(soundDSATimer)); //soundDSAValue;
#else
	directBuffer[0][soundIndex] = interp_pop(0); //soundDSAValue;
#endif

}

void soundDirectSoundATimer()
{
  if(soundDSAEnabled) {
    if(soundDSFifoACount <= 16) {
      cpuDmaHack2 = CPUCheckDMA(3, 2);
      if(soundDSFifoACount <= 16) {
        soundEvent(FIFOA_L, (u16)0);
        soundEvent(FIFOA_H, (u16)0);
        soundEvent(FIFOA_L, (u16)0);
        soundEvent(FIFOA_H, (u16)0);
        soundEvent(FIFOA_L, (u16)0);
        soundEvent(FIFOA_H, (u16)0);
        soundEvent(FIFOA_L, (u16)0);
        soundEvent(FIFOA_H, (u16)0);
      }
    }
    soundDSAValue = (soundDSFifoA[soundDSFifoAIndex]);
	interp_push(0, (s8)soundDSAValue << 8);
	soundDSFifoAIndex = (++soundDSFifoAIndex) & 31;
    soundDSFifoACount--;
  } else
    soundDSAValue = 0;
}

inline void soundDirectSoundB()
{
#ifdef ENHANCED_RATE
	directBuffer[1][soundIndex] = interp_pop(1, calc_rate(soundDSBTimer)); //soundDSBValue;
#else
	directBuffer[1][soundIndex] = interp_pop(1); //soundDSBValue;
#endif
}

void soundDirectSoundBTimer()
{
  if(soundDSBEnabled) {
    if(soundDSFifoBCount <= 16) {
      cpuDmaHack2 = CPUCheckDMA(3, 4);
      if(soundDSFifoBCount <= 16) {
        soundEvent(FIFOB_L, (u16)0);
        soundEvent(FIFOB_H, (u16)0);
        soundEvent(FIFOB_L, (u16)0);
        soundEvent(FIFOB_H, (u16)0);
        soundEvent(FIFOB_L, (u16)0);
        soundEvent(FIFOB_H, (u16)0);
        soundEvent(FIFOB_L, (u16)0);
        soundEvent(FIFOB_H, (u16)0);
      }
    }
    
    soundDSBValue = (soundDSFifoB[soundDSFifoBIndex]);
	interp_push(1, (s8)soundDSBValue << 8);
	soundDSFifoBIndex = (++soundDSFifoBIndex) & 31;
    soundDSFifoBCount--;
  } else {
    soundDSBValue = 0;
  }
}

void soundTimerOverflow(int timer)
{
  if(soundDSAEnabled && (soundDSATimer == timer)) {
    soundDirectSoundATimer();
  }
  if(soundDSBEnabled && (soundDSBTimer == timer)) {
    soundDirectSoundBTimer();
  }
}

#ifndef max
#define max(a,b) (a)<(b)?(b):(a)
#endif

void soundMix()
{
  int res = 0;
  int cgbRes = 0;
  int ratio = ioMem[0x82] & 3;
  int dsaRatio = ioMem[0x82] & 4;
  int dsbRatio = ioMem[0x82] & 8;

  blip_sample_t out[2] = {0, 0};

  if ( ! apu_out ) return;

  while (!apu_out->read_samples(&out[0], 2))
  {
    int ticks = SOUND_CLOCK_TICKS / (4 * soundQuality);
    bool was_stereo = apu->end_frame( ticks );
    apu_out->end_frame( ticks, was_stereo );
  }

  cgbRes = out[0];

  if((soundControl & 0x0200) && (soundEnableFlag & 0x100)){
    if(!dsaRatio)
      res = ((s16)directBuffer[0][soundIndex])>>1;
    else
      res = ((s16)directBuffer[0][soundIndex]);
  }
  
  if((soundControl & 0x2000) && (soundEnableFlag & 0x200)){
    if(!dsbRatio)
      res += ((s16)directBuffer[1][soundIndex])>>1;
    else
      res += ((s16)directBuffer[1][soundIndex]);
  }
  
  res = (res * 170) >> 8;
  cgbRes = (cgbRes * 52 * 7) >> 8;

  switch(ratio) {
  case 0:
  case 3: // prohibited, but 25%    
    cgbRes >>= 2;
    break;
  case 1:
    cgbRes >>= 1;
    break;
  case 2:
    break;
  }

  res += cgbRes;

  if(soundEcho) {
    res *= 2;
    res += soundFilter[soundEchoIndex];
    res /= 2;
    soundFilter[soundEchoIndex++] = res;
  }

  if(soundLowPass) {
    soundLeft[4] = soundLeft[3];
    soundLeft[3] = soundLeft[2];
    soundLeft[2] = soundLeft[1];
    soundLeft[1] = soundLeft[0];
    soundLeft[0] = res;
    res = (soundLeft[4] + 2*soundLeft[3] + 8*soundLeft[2] + 2*soundLeft[1] +
           soundLeft[0])/14;
  }

  switch(soundVolume) {
  case 0:
  case 1:
  case 2:
  case 3:
    res *= (soundVolume+1);
    break;
  case 4:
    res >>= 2;
    break;
  case 5:
    res >>= 1;
    break;
  }
  
  if(res > 32767)
    res = 32767;
  if(res < -32768)
    res = -32768;

  if(soundReverse)
    soundFinalWave[++soundBufferIndex] = res;
  else
    soundFinalWave[soundBufferIndex++] = res;
  
  res = 0;
  cgbRes = out[1];
  
  if((soundControl & 0x0100) && (soundEnableFlag & 0x100)){
    if(!dsaRatio)
      res = ((s16)directBuffer[0][soundIndex])>>1;
    else
      res = ((s16)directBuffer[0][soundIndex]);
  }
  
  if((soundControl & 0x1000) && (soundEnableFlag & 0x200)){
    if(!dsbRatio)
      res += ((s16)directBuffer[1][soundIndex])>>1;
    else
      res += ((s16)directBuffer[1][soundIndex]);
  }

  res = (res * 170) >> 8;
  cgbRes = (cgbRes * 52 * 7) >> 8;
  
  switch(ratio) {
  case 0:
  case 3: // prohibited, but 25%
    cgbRes >>= 2;
    break;
  case 1:
    cgbRes >>= 1;
    break;
  case 2:
    break;
  }

  res += cgbRes;
  
  if(soundEcho) {
    res *= 2;
    res += soundFilter[soundEchoIndex];
    res /= 2;
    soundFilter[soundEchoIndex++] = res;

    if(soundEchoIndex >= 4000)
      soundEchoIndex = 0;
  }

  if(soundLowPass) {
    soundRight[4] = soundRight[3];
    soundRight[3] = soundRight[2];
    soundRight[2] = soundRight[1];
    soundRight[1] = soundRight[0];
    soundRight[0] = res;
    res = (soundRight[4] + 2*soundRight[3] + 8*soundRight[2] + 2*soundRight[1] +
           soundRight[0])/14;
  }

  switch(soundVolume) {
  case 0:
  case 1:
  case 2:
  case 3:
    res *= (soundVolume+1);
    break;
  case 4:
    res >>= 2;
    break;
  case 5:
    res >>= 1;
    break;
  }
  
  if(res > 32767)
    res = 32767;
  if(res < -32768)
    res = -32768;
  
  if(soundReverse)
    soundFinalWave[-1+soundBufferIndex++] = res;
  else
    soundFinalWave[soundBufferIndex++] = res;
}

// soundTick gets called a lot
// if we are operating normally
// call normalSoundTick to avoid
// all the comparison checks.

void normalsoundTick()
{
	soundDirectSoundA();
	soundDirectSoundB();
    soundMix();
    soundIndex++;
    
    if(2*soundBufferIndex >= soundBufferLen)
	{
		systemWriteDataToSoundBuffer();
	    soundIndex = 0;
		soundBufferIndex = 0;
    }
}


void soundTick()
{
  if(systemSoundOn) {
    if(soundMasterOn && !stopState) {
      soundDirectSoundA();
      soundDirectSoundB();
      soundMix();
    } else {
      soundFinalWave[soundBufferIndex++] = 0;
      soundFinalWave[soundBufferIndex++] = 0;
    }
  
    soundIndex++;
    
    if(2*soundBufferIndex >= soundBufferLen) {
      if(systemSoundOn) {
        if(soundPaused) {
          soundResume();
		  setSoundFn();
        }      
        
        systemWriteDataToSoundBuffer();
      }
      soundIndex = 0;
      soundBufferIndex = 0;
    }
  }
}

void (*psoundTickfn)()=soundTick;


void setSoundFn()
{
	if (systemSoundOn && !soundPaused && soundMasterOn)
		psoundTickfn = normalsoundTick;
	else
		psoundTickfn = soundTick;

	if (soundInterpolation != interpolation)
		interp_switch(soundInterpolation);

}

void setsystemSoundOn(bool value)
{
	systemSoundOn = value;
	setSoundFn();
}

void setsoundPaused(bool value)
{
	soundPaused = value;
	setSoundFn();
}

void setsoundMasterOn(bool value)
{
	soundMasterOn = value;
	setSoundFn();
}



void soundShutdown()
{
  systemSoundShutdown();

  int i;
  u8 j;

  if (apu)
  {
    j = soundRead(NR30);
    if (!(j & 0x40)) soundEvent(NR30, u8(j | 0x40));
    for (i = 0; i < 16; i++) sound3WaveRam[i] = soundRead(i + 0x90);
    soundEvent(NR30, u8(j & ~0x40));
    for (i = 16; i < 32; i++) sound3WaveRam[i] = soundRead(i - 16 + 0x90);
    if (j & 0x40) soundEvent(NR30, j);
    apu_saved = true;

    delete apu; apu = NULL;
  }
  if (apu_out) { delete apu_out; apu_out = NULL; }
}

void soundPause()
{
  systemSoundPause();
  setsoundPaused(true);	
}

void soundResume()
{
  systemSoundResume();
  setsoundPaused(false);
}

void soundEnable(int channels)
{
  int c = channels & 0x0f;
  
  soundEnableFlag |= ((channels & 0x30f) |c | (c << 4));
  if(apu)
    soundEventGB(0xFF25, soundBalance & soundEnableFlag);
}

void soundDisable(int channels)
{
  int c = channels & 0x0f;
  
  soundEnableFlag &= (~((channels & 0x30f)|c|(c<<4)));
  if(apu)
    soundEventGB(0xFF25, soundBalance & soundEnableFlag);
}

int soundGetEnable()
{
  return (soundEnableFlag & 0x30f);
}

const BOOST::uint8_t sound_data [Gb_Apu::register_count] = {
  0x80, 0xbf, 0x00, 0x00, 0xbf, // square 1
  0x00, 0x3f, 0x00, 0x00, 0xbf, // square 2
  0x40, 0xff, 0x9f, 0x00, 0xbf, // wave
  0x00, 0xff, 0x00, 0x00, 0xbf, // noise

  0x77, 0xf3, 0xf1, // vin/volume, status, power mode

  0, 0, 0, 0, 0, 0, 0, 0, 0, // unused

  0xac, 0xdd, 0xda, 0x48, 0x36, 0x02, 0xcf, 0x16, // waveform data
  0x2c, 0x04, 0xe5, 0x2c, 0xac, 0xdd, 0xda, 0x48
};

void soundReset()
{
  systemSoundReset();

  setsoundPaused(true);
  soundPlay = 0;
  SOUND_CLOCK_TICKS = soundQuality * USE_TICKS_AS;  
  soundTicks = SOUND_CLOCK_TICKS;
  soundNextPosition = 0;
  setsoundMasterOn(true);
  soundIndex = 0;
  soundBufferIndex = 0;
  soundLevel1 = 7;
  soundLevel2 = 7;

  /*
  sound1On = 0;
  sound1ATL = 0;
  sound1Skip = 0;
  sound1Index = 0;
  sound1Continue = 0;
  sound1EnvelopeVolume =  0;
  sound1EnvelopeATL = 0;
  sound1EnvelopeUpDown = 0;
  sound1EnvelopeATLReload = 0;
  sound1SweepATL = 0;
  sound1SweepATLReload = 0;
  sound1SweepSteps = 0;
  sound1SweepUpDown = 0;
  sound1SweepStep = 0;
  sound1Wave = soundWavePattern[2];
  
  sound2On = 0;
  sound2ATL = 0;
  sound2Skip = 0;
  sound2Index = 0;
  sound2Continue = 0;
  sound2EnvelopeVolume =  0;
  sound2EnvelopeATL = 0;
  sound2EnvelopeUpDown = 0;
  sound2EnvelopeATLReload = 0;
  sound2Wave = soundWavePattern[2];
  
  sound3On = 0;
  sound3ATL = 0;
  sound3Skip = 0;
  sound3Index = 0;
  sound3Continue = 0;
  sound3OutputLevel = 0;
  sound3Last = 0;
  sound3Bank = 0;
  sound3DataSize = 0;
  sound3ForcedOutput = 0;
  
  sound4On = 0;
  sound4Clock = 0;
  sound4ATL = 0;
  sound4Skip = 0;
  sound4Index = 0;
  sound4ShiftRight = 0x7f;
  sound4NSteps = 0;
  sound4CountDown = 0;
  sound4Continue = 0;
  sound4EnvelopeVolume =  0;
  sound4EnvelopeATL = 0;
  sound4EnvelopeUpDown = 0;
  sound4EnvelopeATLReload = 0;

  sound1On = 0;
  sound2On = 0;
  sound3On = 0;
  sound4On = 0;
  */

  apu_out->clear();
  apu->reset(true);

  int addr = 0;
  
  while (addr < 0x30) {
    switch (addr)
    {
    case 4:
    case 9:
    case 14:
    case 19:
      apu->write_register( 0, 0xFF10 + addr, 0 );
      break;
    
    default:
      apu->write_register( 0, 0xFF10 + addr, sound_data [ addr ] );
      break;
    }
    addr++;
  }
  apu->write_register( 0, 0xFF1A, 0 );

  addr = 0x20;
  while (addr < 0x30) {
    apu->write_register( 0, 0xFF10 + addr, sound_data [ addr ] );
    addr++;
  }
  
  /*addr = 0x90;

  while(addr < 0xA0) {
    ioMem[addr++] = 0x00;
    ioMem[addr++] = 0xff;
  }

  addr = 0;
  while(addr < 0x20) {
    sound3WaveRam[addr++] = 0x00;
    sound3WaveRam[addr++] = 0xff;
  }
  */

  memset(soundFinalWave, 0, soundBufferLen);

  memset(soundFilter, 0, sizeof(soundFilter));
  soundEchoIndex = 0;
}

bool soundInit(bool gba)
{
  if(systemSoundInit()) {
    memset(soundBuffer[0], 0, 735);
    memset(soundBuffer[1], 0, 735);
    memset(soundBuffer[2], 0, 735);
    memset(soundBuffer[3], 0, 735);
    
    memset(soundFinalWave, 0, soundBufferLen);

#if defined(_WIN32) && 0
    _fpreset(); // FUCKO, Direct3D display code does something dirty to FPU
#endif
    
    Stereo_Buffer * buf = new Stereo_Buffer;
    apu = new Gb_Apu;

    buf->clock_rate( 4194304 );
    buf->set_sample_rate( 44100 / soundQuality , 1000 / 20 );
    buf->bass_freq( 120 );
    buf->set_channel_count(Gb_Apu::osc_count);
    buf->clear();

    apu_out = buf;

    apu->treble_eq( blip_eq_t( -1.0, 0, 44100 / soundQuality ) );
    apu->reset( gba );
    apu->volume( 1.0 );

    for ( int i = apu->osc_count; i--; )
    {
      Multi_Buffer::channel_t ch = apu_out->channel( i );
      apu->osc_output( i, ch.center, ch.left, ch.right );
    }

    int addr = 0;

    while (addr < 0x30) {
      apu->write_register( 0, 0xFF10 + addr, sound_data [ addr ] );
      addr++;
    }

    if (apu_saved)
    {
      int i;
      apu->write_register( 0, 0xFF1A, 0x40);
      for (i = 0; i < 16; i++) apu->write_register( 0, 0xFF30 + i, sound3WaveRam[i]);
      apu->write_register( 0, 0xFF1A, 0);
      for (i = 16; i < 32; i++) apu->write_register( 0, 0xFF30 - 16 + i, sound3WaveRam[i]);
      apu->write_register( 0, 0xFF1A, 0x40);
    }
    else
    {
      apu->write_register( 0, 0xFF1A, 0 );

      addr = 0x20;
      while (addr < 0x30) {
        apu->write_register( 0, 0xFF10 + addr, sound_data [ addr ] );
        addr++;
      }
    }

    setsoundPaused(true);
    return true;
  }
  return false;
}  

void soundSetQuality(int quality)
{
  if(soundQuality != quality && systemCanChangeSoundQuality()) {
    if(!soundOffFlag)
      soundShutdown();
    soundQuality = quality;
    soundNextPosition = 0;
    if(!soundOffFlag)
      soundInit();
    SOUND_CLOCK_TICKS = USE_TICKS_AS * soundQuality;
    soundIndex = 0;
    soundBufferIndex = 0;
  } else if(soundQuality != quality) {
    soundNextPosition = 0;
    SOUND_CLOCK_TICKS = USE_TICKS_AS * soundQuality;
    soundIndex = 0;
    soundBufferIndex = 0;
  }
}

void soundSaveGame(gzFile gzFile)
{
  int i;
  u8 j;

  j = soundRead(NR30);
  if (!(j & 0x40)) soundEvent(NR30, u8(j | 0x40));
  for (i = 0; i < 16; i++) sound3WaveRam[i] = soundRead(i + 0x90);
  soundEvent(NR30, u8(j & ~0x40));
  for (i = 16; i < 32; i++) sound3WaveRam[i] = soundRead(i - 16 + 0x90);
  if (j & 0x40) soundEvent(NR30, j);

  utilWriteData(gzFile, soundSaveStruct);
  utilWriteData(gzFile, soundSaveStructV2);
  
  utilGzWrite(gzFile, &soundQuality, sizeof(int));
}

void soundReadGame(gzFile gzFile, int version)
{
  utilReadData(gzFile, soundSaveStruct);
  if(version >= SAVE_GAME_VERSION_3) {
    utilReadData(gzFile, soundSaveStructV2);
  } else {
/*    sound3Bank = (ioMem[NR30] >> 6) & 1;
    sound3DataSize = (ioMem[NR30] >> 5) & 1;
    sound3ForcedOutput = (ioMem[NR32] >> 7) & 1;*/
    // nothing better to do here...
    memcpy(&sound3WaveRam[0x00], &ioMem[0x90], 0x10);    
    memcpy(&sound3WaveRam[0x10], &ioMem[0x90], 0x10);
  }
  soundBufferIndex = soundIndex * 2;
  
  int quality = 1;
  utilGzRead(gzFile, &quality, sizeof(int));
  soundSetQuality(quality);
  apu->reset(true);
 
   int i;
   u8 j = ioMem[NR30];
   for (i = NR10; i <= NR52; i++) soundEvent(i, ioMem[i]);
   if (!(j & 0x40)) soundEventGB(0xFF1A, j | 0x40);
   for (i = 0; i < 16; i++) soundEventGB(0xFF30 + i, sound3WaveRam[i]);
   soundEventGB(0xFF1A, j & ~0x40);
   for (i = 16; i < 32; i++) soundEventGB(0xFF30 - 16 + i, sound3WaveRam[i]);
   if (j & 0x40) soundEventGB(0xFF1A, j);
  
  //sound1Wave = soundWavePattern[ioMem[NR11] >> 6];
  //sound2Wave = soundWavePattern[ioMem[NR21] >> 6];
}