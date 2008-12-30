#include <SDL.h>
#include "../System.h"
#include "../Globals.h"

#include "SoundSDL.h"

extern int emulating;

u8          SoundSDL::_buffer[_bufferTotalLen];
int         SoundSDL::_readPosition;
int         SoundSDL::_writePosition;
SDL_cond  * SoundSDL::_cond;
SDL_mutex * SoundSDL::_mutex;

inline int SoundSDL::getBufferFree()
{
  int ret = _readPosition - _writePosition - _bufferAlign;
  if (ret < 0)
    ret += _bufferTotalLen;
  return ret;
}

inline int SoundSDL::getBufferUsed()
{
  int ret = _writePosition - _readPosition;
  if (ret < 0)
    ret += _bufferTotalLen;
  return ret;
}

void SoundSDL::soundCallback(void *,u8 *stream,int len)
{
  if (len <= 0 || !emulating)
    return;

  SDL_mutexP(_mutex);
  const int nAvail = getBufferUsed();
  if (len > nAvail)
    len = nAvail;
  const int nAvail2 = _bufferTotalLen - _readPosition;
  if (len >= nAvail2) {
    memcpy(stream, &_buffer[_readPosition], nAvail2);
    _readPosition = 0;
    stream += nAvail2;
    len    -= nAvail2;
  }
  if (len > 0) {
    memcpy(stream, &_buffer[_readPosition], len);
    _readPosition = (_readPosition + len) % _bufferTotalLen;
    stream += len;
  }
  SDL_CondSignal(_cond);
  SDL_mutexV(_mutex);
}

void SoundSDL::write(const u16 * finalWave, int length)
{
  if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING)
  {
    SDL_PauseAudio(0);
  }

  int remain = length;
  const u8 *wave = reinterpret_cast<const u8 *>(finalWave);

  SDL_mutexP(_mutex);

  int n;
  while (remain >= (n = getBufferFree())) {
  const int nAvail = (_bufferTotalLen - _writePosition) < n ? (_bufferTotalLen - _writePosition) : n;
   memcpy(&_buffer[_writePosition], wave, nAvail);
   _writePosition = (_writePosition + nAvail) % _bufferTotalLen;
    wave        += nAvail;
    remain      -= nAvail;

	if (!emulating || speedup || systemThrottle) {
       SDL_mutexV(_mutex);
      return;
    }
    SDL_CondWait(_cond, _mutex);
  }

  const int nAvail = _bufferTotalLen - _writePosition;
  if (remain >= nAvail) {
    memcpy(&_buffer[_writePosition], wave, nAvail);
    _writePosition = 0;
    wave   += nAvail;
    remain -= nAvail;
  }
  if (remain > 0) {
    memcpy(&_buffer[_writePosition], wave, remain);
    _writePosition = (_writePosition + remain) % _bufferTotalLen;
  }
  SDL_mutexV(_mutex);
}

bool SoundSDL::init(long sampleRate)
{
  SDL_AudioSpec audio;

  _bufferLen = sampleRate * 4 / 60;

  audio.freq = sampleRate;
  audio.format = AUDIO_S16SYS;
  audio.channels = 2;
  audio.samples = 1024;
  audio.callback = soundCallback;
  audio.userdata = NULL;
  if(SDL_OpenAudio(&audio, NULL)) {
    fprintf(stderr,"Failed to open audio: %s\n", SDL_GetError());
    return false;
  }

  _cond  = SDL_CreateCond();
  _mutex = SDL_CreateMutex();

  _readPosition = _writePosition = 0;
  return true;

}

SoundSDL::~SoundSDL()
{
  SDL_mutexP(_mutex);
  int iSave = emulating;
  emulating = 0;
  SDL_CondSignal(_cond);
  SDL_mutexV(_mutex);

  SDL_DestroyCond(_cond);
  _cond = NULL;

  SDL_DestroyMutex(_mutex);
  _mutex = NULL;

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
