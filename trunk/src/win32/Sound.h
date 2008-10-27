#pragma once

enum AUDIO_API {
	DIRECTSOUND = 0
#ifndef NO_OAL
	, OPENAL_SOUND = 1
#endif
#ifndef NO_XAUDIO2
	, XAUDIO2 = 2
#endif
};

class ISound
{
 public:
  virtual ~ISound() {};

  virtual bool init() = 0;
  virtual void pause() = 0;
  virtual void reset() = 0;
  virtual void resume() = 0;
  virtual void write() = 0;

  virtual void setThrottle( unsigned short throttle ) {};
};
