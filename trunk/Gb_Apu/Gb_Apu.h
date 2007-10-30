
// Nintendo Game Boy PAPU sound chip emulator

// Gb_Snd_Emu 0.1.4

#ifndef GB_APU_H
#define GB_APU_H

typedef long     gb_time_t; // clock cycle count
typedef unsigned gb_addr_t; // 16-bit address

#include "Gb_Oscs.h"

class Gb_Apu {
public:
	
	// Set overall volume of all oscillators, where 1.0 is full volume
	void volume( double );
	
	// Set treble equalization
	void treble_eq( const blip_eq_t& );
	
	// Outputs can be assigned to a single buffer for mono output, or to three
	// buffers for stereo output (using Stereo_Buffer to do the mixing).
	
	// Assign all oscillator outputs to specified buffer(s). If buffer
	// is NULL, silences all oscillators.
	void output( Blip_Buffer* mono );
	void output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right );
	
	// Assign single oscillator output to buffer(s). Valid indicies are 0 to 3,
	// which refer to Square 1, Square 2, Wave, and Noise. If buffer is NULL,
	// silences oscillator.
	enum { osc_count = 4 };
	void osc_output( int index, Blip_Buffer* mono );
	void osc_output( int index, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right );
	
	// Reset oscillators and internal state
	void reset(bool gba = false);
	
	// Reads and writes at addr must satisfy start_addr <= addr <= end_addr
	enum { start_addr = 0xFF10 };
	enum { end_addr   = 0xFF3f };
	enum { register_count = end_addr - start_addr + 1 };
	
	// Write 'data' to address at specified time
	void write_register( gb_time_t, gb_addr_t, int data );
	
	// Read from address at specified time
	int read_register( gb_time_t, gb_addr_t );
	
	// Run all oscillators up to specified time, end current time frame, then
	// start a new frame at time 0. Returns true if any oscillators added
	// sound to one of the left/right buffers, false if they only added
	// to the center buffer.
	bool end_frame( gb_time_t );
	
public:
	Gb_Apu();
	~Gb_Apu();
private:
	// noncopyable
	Gb_Apu( const Gb_Apu& );
	Gb_Apu& operator = ( const Gb_Apu& );
	
	Gb_Osc*     oscs [osc_count];
	gb_time_t   next_frame_time;
	gb_time_t   last_time;
	double      volume_unit;
	int         frame_count;
	bool        stereo_found;
	
	Gb_Square   square1;
	Gb_Square   square2;
	Gb_Wave     wave;
	Gb_Noise    noise;
	BOOST::uint8_t regs [register_count];
	Gb_Square::Synth square_synth; // used by squares
	Gb_Wave::Synth   other_synth;  // used by wave and noise
	
	bool gba; // enable GBA extensions to wave channel
	
	void update_volume();
	void run_until( gb_time_t );
	void write_osc( int index, int reg, int data );
};

inline void Gb_Apu::output( Blip_Buffer* b ) { output( b, b, b ); }
	
inline void Gb_Apu::osc_output( int i, Blip_Buffer* b ) { osc_output( i, b, b, b ); }

inline void Gb_Apu::volume( double vol )
{
	volume_unit = 0.60 / osc_count / 15 /*steps*/ / 2 /*?*/ / 8 /*master vol range*/ * vol;
	update_volume();
}

#endif

