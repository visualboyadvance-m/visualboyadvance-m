#include <math.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../libresample-0.1.3/include/libresample.h"

//#define LIBSAMPLERATE // buggy
#ifdef LIBSAMPLERATE
#include "../../libsamplerate-0.1.2/src/samplerate.h"
#endif

#include "snd_interp.h"

// this was once borrowed from libmodplug, and was also used to generate the FIR coefficient
// tables that ZSNES uses for its "FIR" interpolation mode

/* 
  ------------------------------------------------------------------------------------------------
   fir interpolation doc,
	(derived from "an engineer's guide to fir digital filters", n.j. loy)

	calculate coefficients for ideal lowpass filter (with cutoff = fc in 0..1 (mapped to 0..nyquist))
	  c[-N..N] = (i==0) ? fc : sin(fc*pi*i)/(pi*i)

	then apply selected window to coefficients
	  c[-N..N] *= w(0..N)
	with n in 2*N and w(n) being a window function (see loy)

	then calculate gain and scale filter coefs to have unity gain.
  ------------------------------------------------------------------------------------------------
*/
// quantizer scale of window coefs
#define WFIR_QUANTBITS		14
#define WFIR_QUANTSCALE		(1L<<WFIR_QUANTBITS)
#define WFIR_8SHIFT			(WFIR_QUANTBITS-8)
#define WFIR_16BITSHIFT		(WFIR_QUANTBITS)
// log2(number)-1 of precalculated taps range is [4..12]
#define WFIR_FRACBITS		12
#define WFIR_LUTLEN			((1L<<(WFIR_FRACBITS+1))+1)
// number of samples in window
#define WFIR_LOG2WIDTH		3
#define WFIR_WIDTH			(1L<<WFIR_LOG2WIDTH)
#define WFIR_SMPSPERWING	((WFIR_WIDTH-1)>>1)
// cutoff (1.0 == pi/2)
#define WFIR_CUTOFF			0.95f
// wfir type
#define WFIR_HANN			0
#define WFIR_HAMMING		1
#define WFIR_BLACKMANEXACT	2
#define WFIR_BLACKMAN3T61	3
#define WFIR_BLACKMAN3T67	4
#define WFIR_BLACKMAN4T92	5
#define WFIR_BLACKMAN4T74	6
#define WFIR_KAISER4T		7
#define WFIR_TYPE			WFIR_KAISER4T
// wfir help
#ifndef M_zPI
#define M_zPI			3.1415926535897932384626433832795
#endif
#define M_zEPS			1e-8
#define M_zBESSELEPS	1e-21

class CzWINDOWEDFIR
{	public:
		CzWINDOWEDFIR( );
		~CzWINDOWEDFIR( );
		float coef( int _PCnr, float _POfs, float _PCut, int _PWidth, int _PType ) //float _PPos, float _PFc, int _PLen )
		{	double	_LWidthM1		= _PWidth-1;
			double	_LWidthM1Half	= 0.5*_LWidthM1;
			double	_LPosU			= ((double)_PCnr - _POfs);
			double	_LPos			= _LPosU-_LWidthM1Half;
			double	_LPIdl			= 2.0*M_zPI/_LWidthM1;
			double	_LWc,_LSi;
			if( fabs(_LPos)<M_zEPS )
			{	_LWc	= 1.0;
				_LSi	= _PCut;
			}
			else
			{	switch( _PType )
				{	case WFIR_HANN:
						_LWc = 0.50 - 0.50 * cos(_LPIdl*_LPosU);
						break;
					case WFIR_HAMMING:
						_LWc = 0.54 - 0.46 * cos(_LPIdl*_LPosU);
						break;
					case WFIR_BLACKMANEXACT:
						_LWc = 0.42 - 0.50 * cos(_LPIdl*_LPosU) + 0.08 * cos(2.0*_LPIdl*_LPosU);
						break;
					case WFIR_BLACKMAN3T61:
						_LWc = 0.44959 - 0.49364 * cos(_LPIdl*_LPosU) + 0.05677 * cos(2.0*_LPIdl*_LPosU);
						break;
					case WFIR_BLACKMAN3T67:
						_LWc = 0.42323 - 0.49755 * cos(_LPIdl*_LPosU) + 0.07922 * cos(2.0*_LPIdl*_LPosU);
						break;
					case WFIR_BLACKMAN4T92:
						_LWc = 0.35875 - 0.48829 * cos(_LPIdl*_LPosU) + 0.14128 * cos(2.0*_LPIdl*_LPosU) - 0.01168 * cos(3.0*_LPIdl*_LPosU);
						break;
					case WFIR_BLACKMAN4T74:
						_LWc = 0.40217 - 0.49703 * cos(_LPIdl*_LPosU) + 0.09392 * cos(2.0*_LPIdl*_LPosU) - 0.00183 * cos(3.0*_LPIdl*_LPosU);
						break;
					case WFIR_KAISER4T:
						_LWc = 0.40243 - 0.49804 * cos(_LPIdl*_LPosU) + 0.09831 * cos(2.0*_LPIdl*_LPosU) - 0.00122 * cos(3.0*_LPIdl*_LPosU);
						break;
					default:
						_LWc = 1.0;
						break;
				}
				_LPos	 *= M_zPI;
				_LSi	 = sin(_PCut*_LPos)/_LPos;
			}
			return (float)(_LWc*_LSi);
		}
		static signed short lut[WFIR_LUTLEN*WFIR_WIDTH];
};

signed short CzWINDOWEDFIR::lut[WFIR_LUTLEN*WFIR_WIDTH];

CzWINDOWEDFIR::CzWINDOWEDFIR()
{	int _LPcl;
	float _LPcllen	= (float)(1L<<WFIR_FRACBITS);	// number of precalculated lines for 0..1 (-1..0)
	float _LNorm	= 1.0f / (float)(2.0f * _LPcllen);
	float _LCut		= WFIR_CUTOFF;
	float _LScale	= (float)WFIR_QUANTSCALE;
	float _LGain,_LCoefs[WFIR_WIDTH];
	for( _LPcl=0;_LPcl<WFIR_LUTLEN;_LPcl++ )
	{
		float _LOfs		= ((float)_LPcl-_LPcllen)*_LNorm;
		int _LCc,_LIdx	= _LPcl<<WFIR_LOG2WIDTH;
		for( _LCc=0,_LGain=0.0f;_LCc<WFIR_WIDTH;_LCc++ )
		{	_LGain	+= (_LCoefs[_LCc] = coef( _LCc, _LOfs, _LCut, WFIR_WIDTH, WFIR_TYPE ));
		}
		_LGain = 1.0f/_LGain;
		for( _LCc=0;_LCc<WFIR_WIDTH;_LCc++ )
		{	float _LCoef = (float)floor( 0.5 + _LScale*_LCoefs[_LCc]*_LGain );
			lut[_LIdx+_LCc] = (signed short)( (_LCoef<-_LScale)?-_LScale:((_LCoef>_LScale)?_LScale:_LCoef) );
		}
	}
}

CzWINDOWEDFIR::~CzWINDOWEDFIR()
{	// nothing todo
}

CzWINDOWEDFIR sfir;

template <class T, int buffer_size>
class sample_buffer
{
	int ptr, filled;
	T * buffer;

public:
	sample_buffer() : ptr(0), filled(0), buffer(0) {}
	~sample_buffer()
	{
		if (buffer) delete [] buffer;
	}

	void clear()
	{
		if (buffer)
		{
			delete [] buffer;
			buffer = 0;
		}
		ptr = filled = 0;
	}

	inline int size() const
	{
		return filled;
	}

	void push_back(T sample)
	{
		if (!buffer) buffer = new T[buffer_size];
		buffer[ptr] = sample;
		if (++ptr >= buffer_size) ptr = 0;
		if (filled < buffer_size) filled++;
	}

	void erase(int count)
	{
		if (count > filled) filled = 0;
		else filled -= count;
	}

	T operator[] (int index) const
	{
		index += ptr - filled;
		if (index < 0) index += buffer_size;
		else if (index > buffer_size) index -= buffer_size;
		return buffer[index];
	}

	// omghax!
	void lock( T * & out1, unsigned & count1, T * & out2, unsigned & count2 )
	{
		if (!buffer) buffer = new T[buffer_size];
		unsigned free = buffer_size - filled;
		out1 = & buffer[ ptr ];
		if ( ptr )
		{
			count1 = buffer_size - ptr;
			out2 = &buffer[ 0 ];
			count2 = ptr;
			if ( count1 > free )
			{
				count1 = free;
				out2 = 0;
				count2 = 0;
			}
			else if ( count1 + count2 > free )
			{
				count2 = free - count1;
				if ( ! count2 ) out2 = 0;
			}
		}
		else
		{
			count1 = free;
			out2 = 0;
			count2 = 0;
		}
	}

	void push_count( unsigned count )
	{
		if ( count + filled > buffer_size )
		{
			count = buffer_size - filled;
		}

		ptr = ( ptr + count ) % buffer_size;
		filled += count;
	}
};

class foo_null : public foo_interpolate
{
	int sample;

public:
	foo_null() : sample(0) {}
	~foo_null() {}

	void reset() {}

	void push( double rate, int psample )
	{
		sample = psample;
	}

	int pop(double rate)
	{
		return sample;
	}
};

class foo_linear : public foo_interpolate
{
	sample_buffer<int,4> samples;

	int position;

	inline int smp(int index)
	{
		return samples[index];
	}

public:
	foo_linear()
	{
		position = 0;
	}

	~foo_linear() {}

	void reset()
	{
		position = 0;
		samples.clear();
	}

	void push(double rate, int sample)
	{
		samples.push_back(sample);
	}

	int pop(double rate)
	{
		int ret, lrate;

		if (position > 0x7fff)
		{
			int howmany = position >> 15;
			position &= 0x7fff;
			samples.erase(howmany);
		}

		if (samples.size() < 2) return 0;

		ret  = smp(0) * (0x8000 - position);
		ret += smp(1) * position;
		ret >>= 15;

		// wahoo, takes care of drifting
		if (samples.size() > 2)
		{
			rate += (.5 / 32768.);
		}

		lrate = (int)(32768. * rate);
		position += lrate;

		return ret;
	}
};

// and this integer cubic interpolation implementation was kind of borrowed from either TiMidity
// or the P.E.Op.S. SPU project, or is in use in both, or something...

class foo_cubic : public foo_interpolate
{
	sample_buffer<int,12> samples;

	int position;

	inline int smp(int index)
	{
		return samples[index];
	}

public:
	foo_cubic()
	{
		position = 0;
	}

	~foo_cubic() {}

	void reset()
	{
		position = 0;
		samples.clear();
	}

	void push(double rate, int sample)
	{
		samples.push_back(sample);
	}

	int pop(double rate)
	{
		int ret, lrate;

		if (position > 0x7fff)
		{
			int howmany = position >> 15;
			position &= 0x7fff;
			samples.erase(howmany);
		}

		if (samples.size() < 4) return 0;

		ret  = smp(3) - 3 * smp(2) + 3 * smp(1) - smp(0);
		ret *= (position - (2 << 15)) / 6;
		ret >>= 15;
		ret += smp(2) - 2 * smp(1) + smp(0);
		ret *= (position - (1 << 15)) >> 1;
		ret >>= 15;
		ret += smp(1) - smp(0);
		ret *= position;
		ret >>= 15;
		ret += smp(0);

		if (ret > 32767) ret = 32767;
		else if (ret < -32768) ret = -32768;

		// wahoo, takes care of drifting
		if (samples.size() > 8)
		{
			rate += (.5 / 32768.);
		}

		lrate = (int)(32768. * rate);
		position += lrate;

		return ret;
	}
};

class foo_fir : public foo_interpolate
{
	sample_buffer<int,24> samples;

	int position;

	inline int smp(int index)
	{
		return samples[index];
	}

public:
	foo_fir()
	{
		position = 0;
	}

	~foo_fir() {}

	void reset()
	{
		position = 0;
		samples.clear();
	}

	void push(double rate, int sample)
	{
		samples.push_back(sample);
	}

	int pop(double rate)
	{
		int ret, lrate;

		if (position > 0x7fff)
		{
			int howmany = position >> 15;
			position &= 0x7fff;
			samples.erase(howmany);
		}

		if (samples.size() < 8) return 0;

		ret  = smp(0) * CzWINDOWEDFIR::lut[(position & ~7)    ];
		ret += smp(1) * CzWINDOWEDFIR::lut[(position & ~7) + 1];
		ret += smp(2) * CzWINDOWEDFIR::lut[(position & ~7) + 2];
		ret += smp(3) * CzWINDOWEDFIR::lut[(position & ~7) + 3];
		ret += smp(4) * CzWINDOWEDFIR::lut[(position & ~7) + 4];
		ret += smp(5) * CzWINDOWEDFIR::lut[(position & ~7) + 5];
		ret += smp(6) * CzWINDOWEDFIR::lut[(position & ~7) + 6];
		ret += smp(7) * CzWINDOWEDFIR::lut[(position & ~7) + 7];
		ret >>= WFIR_QUANTBITS;

		if (ret > 32767) ret = 32767;
		else if (ret < -32768) ret = -32768;

		// wahoo, takes care of drifting
		if (samples.size() > 16)
		{
			rate += (.5 / 32768.);
		}

		lrate = (int)(32768. * rate);
		position += lrate;

		return ret;
	}
};

class foo_libresample : public foo_interpolate
{
	sample_buffer<float,32> samples;

	void * resampler;

public:
	foo_libresample()
	{
		resampler = 0;
	}

	~foo_libresample()
	{
		reset();
	}

	void reset()
	{
		samples.clear();
		if (resampler)
		{
			resample_close( resampler );
			resampler = 0;
		}
	}

	void push( double rate, int sample )
	{
		if ( ! resampler )
		{
			resampler = resample_open( 0, .25, 44100. / 4000. );
		}

		{
			float in = float( sample );
			float * samples1, * samples2;
			unsigned count1, count2;

			samples.lock( samples1, count1, samples2, count2 );

			int used;
			int processed = resample_process( resampler, 1. / rate, & in, 1, 0, & used, samples1, count1 );

			samples.push_count( processed );

			if ( ! used && count2 )
			{
				processed = resample_process( resampler, 1. / rate, & in, 1, 0, & used, samples2, count2 );

				samples.push_count( processed );
			}
		}
	}

	int pop( double rate )
	{
		int ret;

		if ( samples.size() )
		{
			ret = int( samples[ 0 ] );
			samples.erase( 1 );
		}
		else ret = 0;

		if ( ret > 32767 ) ret = 32767;
		else if ( ret < -32768 ) ret = -32768;

		return ret;
	}
};

#ifdef LIBSAMPLERATE
class foo_src : public foo_interpolate
{
	sample_buffer<float,32> samples;

	SRC_STATE * resampler;

	SRC_DATA resampler_data;

public:
	foo_src()
	{
		resampler = 0;
	}

	~foo_src()
	{
		reset();
	}

	void reset()
	{
		samples.clear();
		if (resampler)
		{
			resampler = src_delete( resampler );
		}
	}

	void push( double rate, int sample )
	{
		if ( ! resampler )
		{
			int err;
			resampler = src_new( SRC_LINEAR, 1, & err );
			if ( err )
			{
				if ( resampler ) resampler = src_delete( resampler );
				return;
			}
		}

		{
			float in = float( sample );
			float * samples1, * samples2;
			unsigned count1, count2;

			samples.lock( samples1, count1, samples2, count2 );

			resampler_data.data_in = & in;
			resampler_data.input_frames = 1;
			resampler_data.data_out = samples1;
			resampler_data.output_frames = count1;
			resampler_data.src_ratio = 1. / rate;

			if ( src_process( resampler, & resampler_data ) )
				return;

			samples.push_count( resampler_data.output_frames_gen );

			if ( ! resampler_data.input_frames_used && count2 )
			{
				resampler_data.data_out = samples2;
				resampler_data.output_frames = count2;

				if ( src_process( resampler, & resampler_data ) )
					return;

				samples.push_count( resampler_data.output_frames_gen );
			}
		}
	}

	int pop(double rate)
	{
		int ret;

		if ( samples.size() )
		{
			ret = int( samples[ 0 ] );
			samples.erase( 1 );
		}
		else ret = 0;

		if ( ret > 32767 ) ret = 32767;
		else if ( ret < -32768 ) ret = -32768;

		return ret;
	}
};
#endif

foo_interpolate * get_filter(int which)
{
	switch (which)
	{
	default:
		return new foo_null;
	case 1:
		return new foo_linear;
	case 2:
		return new foo_cubic;
	case 3:
		return new foo_fir;
	case 4:
		return new foo_libresample;
	}
}

// and here is the implementation specific code, in a messier state than the stuff above

extern bool timer0On;
extern int timer0Reload;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Reload;
extern int timer1ClockReload;

extern int SOUND_CLOCK_TICKS;
extern int soundInterpolation;

double calc_rate(int timer)
{
	if (timer ? timer1On : timer0On)
	{
		return double(SOUND_CLOCK_TICKS) /
			double((0x10000 - (timer ? timer1Reload : timer0Reload)) << 
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

class critical_section
{
	CRITICAL_SECTION cs;

public:
	critical_section() { InitializeCriticalSection(&cs); }
	~critical_section() { DeleteCriticalSection(&cs); }

	void enter() { EnterCriticalSection(&cs); }
	void leave() { LeaveCriticalSection(&cs); }
};

static critical_section interp_sync;
static int interpolation = 0;

class scopelock
{
	critical_section * cs;

public:
	scopelock(critical_section & pcs) { cs = &pcs; cs->enter(); }
	~scopelock() { cs->leave(); }
};

void interp_switch(int which)
{
	scopelock sl(interp_sync);

	for (int i = 0; i < 2; i++)
	{
		delete interp[i];
		interp[i] = get_filter(which);
	}

	interpolation = which;
}

void interp_reset(int ch)
{
	scopelock sl(interp_sync);
	if (soundInterpolation != interpolation) interp_switch(soundInterpolation);

	interp[ch]->reset();
}

void interp_push(int ch, double rate, int sample)
{
	scopelock sl(interp_sync);
	if (soundInterpolation != interpolation) interp_switch(soundInterpolation);

	interp[ch]->push(rate, sample);
}

int interp_pop(int ch, double rate)
{
	scopelock sl(interp_sync);
	if (soundInterpolation != interpolation) interp_switch(soundInterpolation);

	return interp[ch]->pop(rate);
}
