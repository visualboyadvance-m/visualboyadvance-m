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

extern u16 soundFinalWave[1470];
extern int soundVolume;

extern int gbHardware;

extern void soundResume();

extern int soundBufferLen;
extern int soundQuality;
extern bool soundPaused;
extern int soundTicks;
extern int SOUND_CLOCK_TICKS;
extern u32 soundNextPosition;

extern int soundDebug;

extern bool soundEcho;
extern bool soundLowPass;
extern bool soundReverse;
extern bool soundOffFlag;

int const ticks_to_time = 2 * GB_APU_OVERCLOCK;

static inline blip_time_t blip_time()
{
	return (SOUND_CLOCK_TICKS - soundTicks) * ticks_to_time;
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

static void end_frame( blip_time_t time )
{
	gb_apu       ->end_frame( time );
	stereo_buffer->end_frame( time );
}

static void flush_samples()
{
	// number of samples in output buffer
	int const out_buf_size = soundBufferLen / sizeof *soundFinalWave;
	
	// Keep filling and writing soundFinalWave until it can't be fully filled
	while ( stereo_buffer->samples_avail() >= out_buf_size )
	{
		stereo_buffer->read_samples( (blip_sample_t*) soundFinalWave, out_buf_size );
		if(systemSoundOn)
		{
			if(soundPaused)
				soundResume();

			systemWriteDataToSoundBuffer();
		}
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
 	if ( systemSoundOn && gb_apu && stereo_buffer )
	{
		// Run sound hardware to present
		end_frame( SOUND_CLOCK_TICKS * ticks_to_time );
		
		flush_samples();
		
		// Update effects config if it was changed
		if ( memcmp( &gb_effects_config_current, &gb_effects_config,
				sizeof gb_effects_config ) )
			apply_effects();
	}
}

static void remake_stereo_buffer()
{
	// Stereo_Buffer
	delete stereo_buffer;
	stereo_buffer = 0;
	
	stereo_buffer = new Simple_Effects_Buffer; // TODO: handle out of memory
	stereo_buffer->set_sample_rate( 44100 / soundQuality ); // TODO: handle out of memory
	stereo_buffer->clock_rate( gb_apu->clock_rate );
	
	// APU
	static int const chan_types [chan_count] = {
		Multi_Buffer::wave_type+1, Multi_Buffer::wave_type+2,
		Multi_Buffer::wave_type+3, Multi_Buffer::mixed_type+1
	};
	stereo_buffer->set_channel_count( chan_count, chan_types );
	
	if ( !gb_apu )
		gb_apu = new Gb_Apu;
	
	apply_effects();
	
	// Use DMG or CGB sound differences based on type of game
	gb_apu->reset( gbHardware & 1 ? gb_apu->mode_dmg : gb_apu->mode_cgb );
}

void gbSoundReset()
{
	remake_stereo_buffer();
	
	soundPaused       = 1;
	SOUND_CLOCK_TICKS = 20000;
	soundTicks        = SOUND_CLOCK_TICKS;
	soundNextPosition = 0;

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
	int addr = 0xff30;

	while(addr < 0xff40) {
		gbMemory[addr++] = 0x00;
		gbMemory[addr++] = 0xff;
	}
}

extern bool soundInit();
extern void soundShutdown();

void gbSoundSetQuality(int quality)
{
	if ( soundQuality != quality )
	{
		if ( systemCanChangeSoundQuality() )
		{
			if ( !soundOffFlag )
				soundShutdown();
				
			soundQuality      = quality;
			soundNextPosition = 0;
			
			if ( !soundOffFlag )
				soundInit();
		}
		else
		{
			soundQuality      = quality;
			soundNextPosition = 0;
		}
		
		remake_stereo_buffer();
	}
}

void gbSoundSaveGame(gzFile gzFile)
{
	// TODO: implement
}

void gbSoundReadGame(int version,gzFile gzFile)
{
	// TODO: implement
}
