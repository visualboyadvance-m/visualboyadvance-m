// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2015 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SoundSDL.h"
#include "ConfigManager.h"
#include "../gba/Globals.h"
#include "../gba/Sound.h"

extern int emulating;

// Hold up to 32 ms of data in the ring buffer
const float SoundSDL::_delay = 0.032f;

SoundSDL::SoundSDL():
	_rbuf(0),
	_dev(-1),
	current_rate(0),
	_initialized(false)
{

}

void SoundSDL::soundCallback(void *data, u8 *stream, int len)
{
	reinterpret_cast<SoundSDL*>(data)->read(reinterpret_cast<u16 *>(stream), len);
}

void SoundSDL::read(u16 * stream, int length)
{
	if (!_initialized || length <= 0 || !emulating)
		return;


	/* since this is running in a different thread, speedup and
	 * throttle can change at any time; save the value so locks
	 * stay in sync */
	bool lock = (emulating && !speedup && throttle && !gba_joybus_active) ? true : false;

	if (lock)
		SDL_SemWait (_semBufferFull);

	SDL_mutexP(_mutex);

	_rbuf.read(stream, std::min(static_cast<std::size_t>(length) / 2, _rbuf.used()));

	SDL_mutexV(_mutex);

	SDL_SemPost (_semBufferEmpty);
}

void SoundSDL::write(u16 * finalWave, int length)
{
	if (!_initialized)
		return;

	if (SDL_GetAudioDeviceStatus(_dev) != SDL_AUDIO_PLAYING)
		SDL_PauseAudioDevice(_dev, 0);

	SDL_mutexP(_mutex);

	unsigned int samples = length / 4;

	std::size_t avail;
	while ((avail = _rbuf.avail() / 2) < samples)
	{
		bool lock = (emulating && !speedup && throttle && !gba_joybus_active) ? true : false;

		_rbuf.write(finalWave, avail * 2);

		finalWave += avail * 2;
		samples -= avail;

		SDL_mutexV(_mutex);
		SDL_SemPost(_semBufferFull);
		if (lock)
		{
			SDL_SemWait(_semBufferEmpty);
			if (throttle > 0 && throttle != current_rate)
			{
				SDL_CloseAudio();
				init(soundGetSampleRate() * throttle / 100);
				current_rate = throttle;
			}
		}
		else
		{
			// Drop the remaining of the audio data
			return;
		}
		SDL_mutexP(_mutex);
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
	audio.samples = 1024;
	audio.callback = soundCallback;
	audio.userdata = this;

	if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_Init(SDL_INIT_AUDIO);

	_dev = SDL_OpenAudioDevice(NULL, 0, &audio, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
	if(_dev < 0)
	{
		fprintf(stderr,"Failed to open audio: %s\n", SDL_GetError());
		return false;
	}

	_rbuf.reset(_delay * sampleRate * 2);

	if (!_initialized)
	{
		_mutex = SDL_CreateMutex();
		_semBufferFull  = SDL_CreateSemaphore (0);
		_semBufferEmpty = SDL_CreateSemaphore (1);
		_initialized    = true;
	}

	return true;
}

SoundSDL::~SoundSDL()
{
	if (!_initialized)
		return;

	SDL_mutexP(_mutex);
	int iSave = emulating;
	emulating = 0;
	SDL_SemPost(_semBufferFull);
	SDL_SemPost(_semBufferEmpty);
	SDL_mutexV(_mutex);

	SDL_DestroySemaphore(_semBufferFull);
	SDL_DestroySemaphore(_semBufferEmpty);
	_semBufferFull = NULL;
	_semBufferEmpty = NULL;

	SDL_DestroyMutex(_mutex);
	_mutex = NULL;

	SDL_CloseAudioDevice(_dev);

	emulating = iSave;

	_initialized = false;
}

void SoundSDL::pause()
{
	if (!_initialized)
		return;

	SDL_PauseAudioDevice(_dev, 1);
}

void SoundSDL::resume()
{
	if (!_initialized)
		return;

	SDL_PauseAudioDevice(_dev, 0);
}

void SoundSDL::reset()
{
}
