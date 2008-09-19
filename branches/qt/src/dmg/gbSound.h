#ifndef GBSOUND_H
#define GBSOUND_H

#define NR10 0xff10
#define NR11 0xff11
#define NR12 0xff12
#define NR13 0xff13
#define NR14 0xff14
#define NR21 0xff16
#define NR22 0xff17
#define NR23 0xff18
#define NR24 0xff19
#define NR30 0xff1a
#define NR31 0xff1b
#define NR32 0xff1c
#define NR33 0xff1d
#define NR34 0xff1e
#define NR41 0xff20
#define NR42 0xff21
#define NR43 0xff22
#define NR44 0xff23
#define NR50 0xff24
#define NR51 0xff25
#define NR52 0xff26

#define SOUND_EVENT(address,value) \
    gbSoundEvent(address,value)

void gbSoundTick();
void gbSoundPause();
void gbSoundResume();
void gbSoundEnable( int );
void gbSoundDisable( int );
int gbSoundGetEnable();
void gbSoundReset();
void gbSoundSaveGame( gzFile );
void gbSoundReadGame( int, gzFile );
void gbSoundEvent( register u16, register int );
void gbSoundSetQuality( int );

u8 gbSoundRead( u16 address );

extern int soundTicks;
extern int soundQuality;
extern int SOUND_CLOCK_TICKS;

struct gb_effects_config_t {
    bool enabled;   // false = disable all effects

    float echo;     // 0.0 = none, 1.0 = lots
    float stereo;   // 0.0 = channels in center, 1.0 = channels on left/right
    bool surround;  // true = put some channels in back
};

// Changes effects configuration
void gbSoundConfigEffects( gb_effects_config_t const& );
extern gb_effects_config_t gb_effects_config; // current configuration

#endif
