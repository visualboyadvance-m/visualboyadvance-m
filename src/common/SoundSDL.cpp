#include <SDL.h>
#include "../System.h"
#include "../Globals.h"

#include "SoundSDL.h"

extern int emulating;

u8          SoundSDL::sdlSoundBuffer[sdlSoundTotalLen];
int         SoundSDL::sdlSoundRPos;
int         SoundSDL::sdlSoundWPos;
SDL_cond  * SoundSDL::sdlSoundCond;
SDL_mutex * SoundSDL::sdlSoundMutex;

inline int SoundSDL::soundBufferFree()
{
  int ret = sdlSoundRPos - sdlSoundWPos - sdlSoundAlign;
  if (ret < 0)
    ret += sdlSoundTotalLen;
  return ret;
}

inline int SoundSDL::soundBufferUsed()
{
  int ret = sdlSoundWPos - sdlSoundRPos;
  if (ret < 0)
    ret += sdlSoundTotalLen;
  return ret;
}

void SoundSDL::soundCallback(void *,u8 *stream,int len)
{
  if (len <= 0 || !emulating)
    return;

  SDL_mutexP(sdlSoundMutex);
  const int nAvail = soundBufferUsed();
  if (len > nAvail)
    len = nAvail;
  const int nAvail2 = sdlSoundTotalLen - sdlSoundRPos;
  if (len >= nAvail2) {
    memcpy(stream, &sdlSoundBuffer[sdlSoundRPos], nAvail2);
    sdlSoundRPos = 0;
    stream += nAvail2;
    len    -= nAvail2;
  }
  if (len > 0) {
    memcpy(stream, &sdlSoundBuffer[sdlSoundRPos], len);
    sdlSoundRPos = (sdlSoundRPos + len) % sdlSoundTotalLen;
    stream += len;
  }
  SDL_CondSignal(sdlSoundCond);
  SDL_mutexV(sdlSoundMutex);
}

void SoundSDL::write(const u16 * finalWave, int length)
{
  if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING)
  {
    SDL_PauseAudio(0);
  }

  int remain = length;
  const u8 *wave = reinterpret_cast<const u8 *>(finalWave);

  SDL_mutexP(sdlSoundMutex);

  int n;
  while (remain >= (n = soundBufferFree())) {
  const int nAvail = (sdlSoundTotalLen - sdlSoundWPos) < n ? (sdlSoundTotalLen - sdlSoundWPos) : n;
   memcpy(&sdlSoundBuffer[sdlSoundWPos], wave, nAvail);
   sdlSoundWPos = (sdlSoundWPos + nAvail) % sdlSoundTotalLen;
    wave        += nAvail;
    remain      -= nAvail;

	if (!emulating || speedup || systemThrottle) {
       SDL_mutexV(sdlSoundMutex);
      return;
    }
    SDL_CondWait(sdlSoundCond, sdlSoundMutex);
  }

  const int nAvail = sdlSoundTotalLen - sdlSoundWPos;
  if (remain >= nAvail) {
    memcpy(&sdlSoundBuffer[sdlSoundWPos], wave, nAvail);
    sdlSoundWPos = 0;
    wave   += nAvail;
    remain -= nAvail;
  }
  if (remain > 0) {
    memcpy(&sdlSoundBuffer[sdlSoundWPos], wave, remain);
    sdlSoundWPos = (sdlSoundWPos + remain) % sdlSoundTotalLen;
  }
  SDL_mutexV(sdlSoundMutex);
}

bool SoundSDL::init(int quality)
{
  SDL_AudioSpec audio;

  switch(quality) {
  case 1:
    audio.freq = 44100;
    _bufferLen = 1470*2;
    break;
  case 2:
    audio.freq = 22050;
    _bufferLen = 736*2;
    break;
  case 4:
    audio.freq = 11025;
    _bufferLen = 368*2;
    break;
  }

  audio.format = AUDIO_S16SYS;
  audio.channels = 2;
  audio.samples = 1024;
  audio.callback = soundCallback;
  audio.userdata = NULL;
  if(SDL_OpenAudio(&audio, NULL)) {
    fprintf(stderr,"Failed to open audio: %s\n", SDL_GetError());
    return false;
  }

  sdlSoundCond  = SDL_CreateCond();
  sdlSoundMutex = SDL_CreateMutex();

  sdlSoundRPos = sdlSoundWPos = 0;
  return true;

}

SoundSDL::~SoundSDL()
{
  SDL_mutexP(sdlSoundMutex);
  int iSave = emulating;
  emulating = 0;
  SDL_CondSignal(sdlSoundCond);
  SDL_mutexV(sdlSoundMutex);

  SDL_DestroyCond(sdlSoundCond);
  sdlSoundCond = NULL;

  SDL_DestroyMutex(sdlSoundMutex);
  sdlSoundMutex = NULL;

  SDL_CloseAudio();

  emulating = iSave;
}

void SoundSDL::pause()
{
  SDL_PauseAudio(1);
}

void SoundSDL::resume()
{
  SDL_PauseAudio(0);
}

void SoundSDL::reset()
{
}

int SoundSDL::getBufferLength()
{
	return _bufferLen;
}
