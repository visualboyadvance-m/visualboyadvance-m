#ifndef __SND_INTERP_H__
#define __SND_INTERP_H__

class foo_interpolate
{
public:
	foo_interpolate() {}
	virtual ~foo_interpolate() {};

	virtual void reset() = 0;

	long lrate;

	virtual void rate(double rate)
	{
		lrate = (int)(32768. * rate);
	};

	virtual void push(int sample) = 0;
	virtual int pop() = 0;
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
