
// Private oscillators used by Gb_Apu

// Gb_Snd_Emu 0.1.4

#ifndef GB_OSCS_H
#define GB_OSCS_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

struct Gb_Osc
{
	enum { trigger = 0x80 };
	enum { len_enabled_mask = 0x40 };
	
	Blip_Buffer* outputs [4]; // NULL, right, left, center
	Blip_Buffer* output;
	int output_select;
	BOOST::uint8_t* regs; // osc's 5 registers
	
	int delay;
	int last_amp;
	int volume;
	int length;
	bool enabled;
	
	void reset();
	void clock_length();
	int frequency() const { return (regs [4] & 7) * 0x100 + regs [3]; }
};

struct Gb_Env : Gb_Osc
{
	int env_delay;
	
	void reset();
	void clock_envelope();
	bool write_register( int, int );
};

struct Gb_Square : Gb_Env
{
	enum { period_mask = 0x70 };
	enum { shift_mask  = 0x07 };
	
	typedef Blip_Synth<blip_good_quality,1> Synth;
	Synth const* synth;
	int sweep_delay;
	int sweep_freq;
	int phase;
	
	void reset();
	void clock_sweep();
	void run( gb_time_t, gb_time_t, int playing );
};

struct Gb_Noise : Gb_Env
{
	typedef Blip_Synth<blip_med_quality,1> Synth;
	Synth const* synth;
	unsigned bits;
	
	void run( gb_time_t, gb_time_t, int playing );
};

struct Gb_Wave : Gb_Osc
{
	typedef Blip_Synth<blip_med_quality,1> Synth;
	Synth const* synth;
	int volume_forced;
	int wave_pos;
	unsigned wave_mode;
	unsigned wave_size;
	unsigned wave_bank;
	BOOST::uint8_t wave [32 * 2];
	
	void reset(bool gba = false);
	void write_register( int, int );
	void run( gb_time_t, gb_time_t, int playing );
};

inline void Gb_Env::reset()
{
	env_delay = 0;
	Gb_Osc::reset();
}

#endif

