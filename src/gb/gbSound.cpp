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

#include <memory.h>

#include "../System.h"
#include "../Util.h"
#include "gbGlobals.h"
#include "gbSound.h"

#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Effects_Buffer.h"

static Gb_Apu* gb_apu;
static Simple_Effects_Buffer* stereo_buffer;

extern u8 soundBuffer[6][735];
extern u16 soundFinalWave[1470];
extern int soundVolume;

#define SOUND_MAGIC   0x60000000
#define SOUND_MAGIC_2 0x30000000
#define NOISE_MAGIC 5

extern int speed;
extern int gbHardware;

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

extern int sound1On;
extern int sound1ATL;
int sound1ATLreload;
int freq1low;
int freq1high;
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
int sound2ATLreload;
int freq2low;
int freq2high;
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
int sound3ATLreload;
int freq3low;
int freq3high;
extern int sound3Skip;
extern int sound3Index;
extern int sound3Continue;
extern int sound3OutputLevel;
extern int sound3Last;

extern int sound4On;
extern int sound4Clock;
extern int sound4ATL;
int sound4ATLreload;
int freq4;
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

bool gbDigitalSound = false;

static inline blip_time_t ticks_to_time( int ticks )
{
	return ticks * 2;
}

static inline blip_time_t blip_time()
{
	return ticks_to_time( SOUND_CLOCK_TICKS - soundTicks );
}

u8 gbSoundRead( u16 address )
{
	if ( gb_apu && address >= NR10 && address <= 0xFF3F )
		return gb_apu->read_register( blip_time(), address );

	return gbMemory[address];
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

	if ( gb_apu && address >= NR10 && address <= 0xFF3F )
		gb_apu->write_register( blip_time(), address, data );
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

static void end_frame( blip_time_t time )
{
	gb_apu->end_frame( time );
	stereo_buffer->end_frame( time );
}

static void flush_samples()
{
	int const out_buf_size = soundBufferLen / sizeof *soundFinalWave;
	
	while ( stereo_buffer->samples_avail() >= out_buf_size )
	{
		stereo_buffer->read_samples( (blip_sample_t*) soundFinalWave, out_buf_size );
		if(systemSoundOn)
		{
			if(soundPaused)
				soundResume();

			systemWriteDataToSoundBuffer();
		}
		soundIndex = 0;
		soundBufferIndex = 0;
	}
}

int const chan_count = 4;

gb_effects_config_t gb_effects_config;
static gb_effects_config_t gb_effects_config_current;

static void apply_effects()
{
	gb_effects_config_current = gb_effects_config;
		
	stereo_buffer->config().enabled  = gb_effects_config_current.enabled;
	stereo_buffer->config().echo     = gb_effects_config_current.echo;
	stereo_buffer->config().stereo   = gb_effects_config_current.stereo;
	stereo_buffer->config().surround = gb_effects_config_current.surround;
	stereo_buffer->apply_config();
	
	for ( int i = 0; i < chan_count; i++ )
	{
		Multi_Buffer::channel_t ch = stereo_buffer->channel( i );
		gb_apu->set_output( ch.center, ch.left, ch.right, i );
	}
}

void gbSoundTick()
{
	if(systemSoundOn && gb_apu && stereo_buffer)
	{
		end_frame( ticks_to_time( SOUND_CLOCK_TICKS ) );
		flush_samples();
		
		// Update effects config if it was changed
		if ( memcmp( &gb_effects_config_current, &gb_effects_config,
				sizeof gb_effects_config ) )
			apply_effects();
	}
}

static void remake_stereo_buffer()
{
	if ( !gb_apu )
		gb_apu = new Gb_Apu;
	
	delete stereo_buffer;
	stereo_buffer = 0;
	
	stereo_buffer = new Simple_Effects_Buffer;
	stereo_buffer->set_sample_rate( 44100 / soundQuality );
	stereo_buffer->clock_rate( gb_apu->clock_rate );
	
	static int const chan_types [chan_count] = {
	Multi_Buffer::wave_type+1, Multi_Buffer::wave_type+2,
	Multi_Buffer::wave_type+3, Multi_Buffer::mixed_type+1
	};
	stereo_buffer->set_channel_count( chan_count, chan_types );
	
	apply_effects();
	
	gb_apu->reset( gb_apu->mode_cgb );
}

void gbSoundReset()
{  
  remake_stereo_buffer();
	
  soundPaused = 1;
  soundPlay = 0;
  SOUND_CLOCK_TICKS = 20000;
  soundTicks = SOUND_CLOCK_TICKS;
  soundNextPosition = 0;
  soundMasterOn = 1;
  soundIndex = 0;
  soundBufferIndex = 0;
  soundLevel1 = 7;
  soundLevel2 = 7;
  soundVIN = 0;

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

  // don't translate
#ifndef FINAL_VERSION
  if(soundDebug) {
    log("*** Sound Init ***\n");
  }
#endif
  gbSoundEvent(0xff10, 0x80);
  gbSoundEvent(0xff11, 0xbf);
  gbSoundEvent(0xff12, 0xf3);
  gbSoundEvent(0xff14, 0xbf);
  gbSoundEvent(0xff16, 0x3f);
  gbSoundEvent(0xff17, 0x00);
  gbSoundEvent(0xff19, 0xbf);

  gbSoundEvent(0xff1a, 0x7f);
  gbSoundEvent(0xff1b, 0xff);
  gbSoundEvent(0xff1c, 0xbf);
  gbSoundEvent(0xff1e, 0xbf);

  gbSoundEvent(0xff20, 0xff);
  gbSoundEvent(0xff21, 0x00);
  gbSoundEvent(0xff22, 0x00);
  gbSoundEvent(0xff23, 0xbf);
  gbSoundEvent(0xff24, 0x77);
  gbSoundEvent(0xff25, 0xf3);

  if (gbHardware & 0x4)
    gbSoundEvent(0xff26, 0xf0);
  else
    gbSoundEvent(0xff26, 0xf1);

  // don't translate
#ifndef FINAL_VERSION
  if(soundDebug) {
    log("*** Sound Init Complete ***\n");
  }
#endif
  sound1On = 0;
  sound2On = 0;
  sound3On = 0;
  sound4On = 0;

  int addr = 0xff30;

  while(addr < 0xff40) {
    gbMemory[addr++] = 0x00;
    gbMemory[addr++] = 0xff;
  }

  memset(soundFinalWave, 0x00, soundBufferLen);


  memset(soundFilter, 0, sizeof(soundFilter));
  soundEchoIndex = 0;
}

extern bool soundInit();
extern void soundShutdown();

void gbSoundSetQuality(int quality)
{
  if(soundQuality != quality && systemCanChangeSoundQuality()) {
    if(!soundOffFlag)
      soundShutdown();
    soundQuality = quality;
    soundNextPosition = 0;
    if(!soundOffFlag)
      soundInit();
    //SOUND_CLOCK_TICKS = (gbSpeed ? 2 : 1) * 24 * soundQuality;
    soundIndex = 0;
    soundBufferIndex = 0;
  } else {
    soundNextPosition = 0;
    //SOUND_CLOCK_TICKS = (gbSpeed ? 2 : 1) * 24 * soundQuality;
    soundIndex = 0;
    soundBufferIndex = 0;
  }
  remake_stereo_buffer();
}

void gbSoundSaveGame(gzFile gzFile)
{
}

void gbSoundReadGame(int version,gzFile gzFile)
{
}
