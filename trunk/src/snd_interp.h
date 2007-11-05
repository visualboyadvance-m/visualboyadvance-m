#ifndef __SND_INTERP_H__
#define __SND_INTERP_H__

// simple interface that could easily be recycled
#define ENHANCED_RATE

class foo_interpolate
{
public:
	foo_interpolate() {}
	virtual ~foo_interpolate() {};

#ifdef ENHANCED_RATE
	virtual void reset() = 0;
#else
	long lrate;

	virtual void reset(double rate)
	{
		lrate = (int)(32768. * rate);
	};
#endif

	virtual void push(int sample) = 0;
#ifdef ENHANCED_RATE
	virtual int pop(double rate) = 0;
#else
	virtual int pop() = 0;
#endif 

};

extern foo_interpolate * get_filter(int which);

/*

// complicated, synced interface, specific to this implementation

double calc_rate(int timer);
void interp_switch(int which);
void interp_reset(int ch);
inline void interp_push(int ch, int sample);
inline int interp_pop(int ch, double rate); */

#endif