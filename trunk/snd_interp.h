#ifndef __SND_INTERP_H__
#define __SND_INTERP_H__

// simple interface that could easily be recycled

class foo_interpolate
{
public:
	foo_interpolate() {}
	virtual ~foo_interpolate() {};

	virtual void reset() = 0;

	virtual void push( double rate, int sample ) = 0;
	virtual int pop( double rate ) = 0;
};

foo_interpolate * get_filter(int which);


// complicated, synced interface, specific to this implementation

double calc_rate(int timer);

void interp_switch(int which);
void interp_reset(int ch);
void interp_push(int ch, double rate, int sample);
int interp_pop(int ch, double rate);

#endif