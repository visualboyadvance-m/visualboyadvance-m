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

#include "../System.h"
#include "../Util.h"
#include "gbGlobals.h"
#include "gbSound.h"

#include "../Gb_Apu/Multi_Buffer.h"
#include "../Gb_Apu/Gb_Apu.h"

extern u8 soundBuffer[6][735];
extern u16 soundFinalWave[1470];
extern int soundVolume;

#define SOUND_MAGIC   0x60000000
#define SOUND_MAGIC_2 0x30000000
#define NOISE_MAGIC 5

extern int speed;

extern void soundResume();

extern u8 soundWavePattern[4][32];

extern int soundBufferLen;
extern int soundBufferTotalLen;
extern int soundQuality;
extern bool soundPaused;
extern int soundPlay;
extern int soundTicks;
extern int SOUND_CLOCK_TICKS;
extern u32 soundNextPosition;

extern int soundLevel1;
extern int soundLevel2;
extern int soundBalance;
extern int soundMasterOn;
extern int soundIndex;
extern int soundBufferIndex;
int soundVIN = 0;
extern int soundDebug;

extern Multi_Buffer  * apu_out;
extern Gb_Apu        * apu;

extern int sound1On;
extern int sound1ATL;
extern int sound1Skip;
extern int sound1Index;
extern int sound1Continue;
extern int sound1EnvelopeVolume;
extern int sound1EnvelopeATL;
extern int sound1EnvelopeUpDown;
extern int sound1EnvelopeATLReload;
extern int sound1SweepATL;
extern int sound1SweepATLReload;
extern int sound1SweepSteps;
extern int sound1SweepUpDown;
extern int sound1SweepStep;
extern u8 *sound1Wave;

extern int sound2On;
extern int sound2ATL;
extern int sound2Skip;
extern int sound2Index;
extern int sound2Continue;
extern int sound2EnvelopeVolume;
extern int sound2EnvelopeATL;
extern int sound2EnvelopeUpDown;
extern int sound2EnvelopeATLReload;
extern u8 *sound2Wave;

extern int sound3On;
extern int sound3ATL;
extern int sound3Skip;
extern int sound3Index;
extern int sound3Continue;
extern int sound3OutputLevel;
extern int sound3Last;

extern int sound4On;
extern int sound4Clock;
extern int sound4ATL;
extern int sound4Skip;
extern int sound4Index;
extern int sound4ShiftRight;
extern int sound4ShiftSkip;
extern int sound4ShiftIndex;
extern int sound4NSteps;
extern int sound4CountDown;
extern int sound4Continue;
extern int sound4EnvelopeVolume;
extern int sound4EnvelopeATL;
extern int sound4EnvelopeUpDown;
extern int sound4EnvelopeATLReload;

extern int soundEnableFlag;

extern int soundFreqRatio[8];
extern int soundShiftClock[16];

extern s16 soundFilter[4000];
extern s16 soundLeft[5];
extern s16 soundRight[5];
extern int soundEchoIndex;
extern bool soundEcho;
extern bool soundLowPass;
extern bool soundReverse;
extern bool soundOffFlag;

u8 gbSoundRead(u16 address)
{
  if (address < NR10 || address > 0xFF3F || !apu) return gbMemory[address];
  if (address == NR51) return soundBalance;

  int clock = (SOUND_CLOCK_TICKS - soundTicks) * 95 / (24 * (gbSpeed ? 2 : 1));

  int ret = apu->read_register(clock, address);

  switch ( address )
  {
  case NR10:
    ret |= 0x80; break;

  case NR11:
  case NR21:
    ret |= 0x3F; break;

  case NR13:
  case NR23:
  case NR31:
  case NR33:
    ret = 0xFF; break;

  case NR14:
  case NR24:
  case NR34:
  case NR44:
    ret |= 0xBF; break;

  case NR30:
    ret |= 0x7F; break;

  case NR32:
    ret |= 0x9F; break;
  }

  return ret;
}

void gbSoundEvent(register u16 address, register int data)
{
  int freq = 0;
  
  gbMemory[address] = data;

#ifndef FINAL_VERSION
  if(soundDebug) {
    // don't translate. debug only
    log("Sound event: %08lx %02x\n", address, data);
  }
#endif
    if (apu && address >= NR10 && address <= 0xFF3F)
   {
     int clock = (SOUND_CLOCK_TICKS - soundTicks) * 95 / (24 * (gbSpeed ? 2 : 1));
     if (address == NR50)
     {
       apu->write_register(clock, address, data);
     }
     else if (address == NR51)
     {
       soundBalance = data;
       apu->write_register(clock, address, data & soundEnableFlag);
     }
     else
       apu->write_register(clock, address, data);
	}
}

 void gbSoundChannel1()
{
}

void gbSoundChannel2()
{
}  

void gbSoundChannel3()
{
}

void gbSoundChannel4()
{
}

void gbSoundMix()
{
  int res = 0;
 
   blip_sample_t out[2] = {0, 0};
 
   if ( ! apu_out ) return;
 
   while (!apu_out->read_samples(&out[0], 2))
   {
     int ticks = SOUND_CLOCK_TICKS * 95 / (24 * (gbSpeed ? 2 : 1));
     bool was_stereo = apu->end_frame( ticks );
     apu_out->end_frame( ticks, was_stereo );
    }
  
   res = out[0];
 
   //res = (res * 7 * 60) >> 8;

  
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
  
  res = out[1];
  
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

void gbSoundTick()
{
  if(systemSoundOn) {
    if(soundMasterOn) {
      /*gbSoundChannel1();
      gbSoundChannel2();
      gbSoundChannel3();
      gbSoundChannel4();*/
      
      gbSoundMix();
    } else {
      soundFinalWave[soundBufferIndex++] = 0;
      soundFinalWave[soundBufferIndex++] = 0;
    }
  
    soundIndex++;
    
    if(2*soundBufferIndex >= soundBufferLen) {
      if(systemSoundOn) {
        if(soundPaused) {
          soundResume();
        }      
        
        systemWriteDataToSoundBuffer();
      }
      soundIndex = 0;
      soundBufferIndex = 0;
    }
  }
}

void gbSoundReset()
{
  soundPaused = 1;
  soundPlay = 0;
  SOUND_CLOCK_TICKS = soundQuality * 24;  
  soundTicks = SOUND_CLOCK_TICKS;
  soundNextPosition = 0;
  soundMasterOn = 1;
  soundIndex = 0;
  soundBufferIndex = 0;
  soundLevel1 = 7;
  soundLevel2 = 7;
  soundVIN = 0;
  

  // don't translate
  if(soundDebug) {
    log("*** Sound Init ***\n");
  }
  
  
  if (apu_out)
  {
    apu_out->clear();
    apu->reset(false);
 
     extern const BOOST::uint8_t sound_data[Gb_Apu::end_addr - Gb_Apu::start_addr + 1];
 
     int addr = 0;
 
     while (addr < 0x30) {
       apu->write_register( 0, 0xFF10 + addr, sound_data [ addr ] );
       addr++;
     }
   }
  // don't translate
  if(soundDebug) {
    log("*** Sound Init Complete ***\n");
  }
  
  if (apu)
   {
     int addr = 0xff30;
 
     while(addr < 0xff40) {
       /*gbMemory[addr++] = 0x00;
       gbMemory[addr++] = 0xff;*/
       gbSoundEvent(addr++, 0x00);
       gbSoundEvent(addr++, 0xFF);
     }
    }

  memset(soundFinalWave, 0x00, soundBufferLen);


  memset(soundFilter, 0, sizeof(soundFilter));
  soundEchoIndex = 0;
}

extern bool soundInit(bool gba = true);
extern void soundShutdown();

void gbSoundSetQuality(int quality)
{
  if(soundQuality != quality && systemCanChangeSoundQuality()) {
    if(!soundOffFlag)
      soundShutdown();
    soundQuality = quality;
    soundNextPosition = 0;
    if(!soundOffFlag)
      soundInit(false);
    SOUND_CLOCK_TICKS = (gbSpeed ? 2 : 1) * 24 * soundQuality;
    soundIndex = 0;
    soundBufferIndex = 0;
  } else {
    soundNextPosition = 0;
    SOUND_CLOCK_TICKS = (gbSpeed ? 2 : 1) * 24 * soundQuality;
    soundIndex = 0;
    soundBufferIndex = 0;
  }
}

variable_desc gbSoundSaveStruct[] = {
  { &soundPaused, sizeof(int) },
  { &soundPlay, sizeof(int) },
  { &soundTicks, sizeof(int) },
  { &SOUND_CLOCK_TICKS, sizeof(int) },
  { &soundLevel1, sizeof(int) },
  { &soundLevel2, sizeof(int) },
  { &soundBalance, sizeof(int) },
  { &soundMasterOn, sizeof(int) },
  { &soundIndex, sizeof(int) },
  { &soundVIN, sizeof(int) },
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
  { NULL, 0 }
};

void gbSoundSaveGame(gzFile gzFile)
{
  utilWriteData(gzFile, gbSoundSaveStruct);

  utilGzWrite(gzFile, soundBuffer, 4*735);
  utilGzWrite(gzFile, soundFinalWave, 2*735);
  utilGzWrite(gzFile, &soundQuality, sizeof(int));
}

void gbSoundReadGame(int version,gzFile gzFile)
{
  utilReadData(gzFile, gbSoundSaveStruct);

  soundBufferIndex = soundIndex * 2;
  
  utilGzRead(gzFile, soundBuffer, 4*735);
  utilGzRead(gzFile, soundFinalWave, 2*735);

  if(version >=7) {
    int quality = 1;
    utilGzRead(gzFile, &quality, sizeof(int));
    gbSoundSetQuality(quality);
  } else {
    soundQuality = -1;
    gbSoundSetQuality(1);
  }
  
  sound1Wave = soundWavePattern[gbMemory[NR11] >> 6];
  sound2Wave = soundWavePattern[gbMemory[NR21] >> 6];
}
