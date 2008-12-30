#include <SDL.h>
#include "../System.h"
#include "../Globals.h"

#include "SoundSDL.h"

extern int emulating;

SoundSDL::SoundSDL():
	_rbuf(0)
{

}

void SoundSDL::soundCallback(void *data, u8 *stream, int len)
{
	reinterpret_cast<SoundSDL*>(data)->read(reinterpret_cast<u16 *>(stream), len);
}

void SoundSDL::read(u16 * stream, int length)
{
	if (length <= 0 || !emulating)
		return;

	SDL_mutexP(_mutex);

	_rbuf.read(stream, std::min(static_cast<std::size_t>(length) / 2, _rbuf.used()));

	SDL_CondSignal(_cond);
	SDL_mutexV(_mutex);
}

void SoundSDL::write(u16 * finalWave, int length)
{
	if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING)
		SDL_PauseAudio(0);

	SDL_mutexP(_mutex);

	unsigned int samples = length / 4;

	std::size_t avail;
	while ((avail = _rbuf.avail() / 2) < samples)
	{
		_rbuf.write(finalWave, avail * 2);

		finalWave += avail * 2;
		samples -= avail;

		// If emulating and not in speed up mode, synchronize to audio
		// by waiting till there is enough room in the buffer
		if (emulating && !speedup && !systemThrottle)
		{
			SDL_CondWait(_cond, _mutex);
		}
		else
		{
			// Drop the remaining of the audio data
			SDL_mutexV(_mutex);
			return;
		}
	}

	_rbuf.write(finalWave, samples * 2);

	SDL_mutexV(_mutex);
}


bool SoundSDL::init(long sampleRate)
{
	SDL_AudioSpec audio;
	audio.freq = sampleRate;
	audio.format = AUDIO_S16SYS;
	audio.channels = 2;
	audio.samples = sampleRate / 60;
	audio.callback = soundCallback;
	audio.userdata = this;

	if(SDL_OpenAudio(&audio, NULL))
	{
		fprintf(stderr,"Failed to open audio: %s\n", SDL_GetError());
		return false;
	}

	_rbuf.reset(_delay * sampleRate * 2);

	_cond  = SDL_CreateCond();
	_mutex = SDL_CreateMutex();

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
