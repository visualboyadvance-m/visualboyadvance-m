// === LOGALL writes very detailed informations to vba-trace.log ===
//#define LOGALL

#ifndef NO_OAL

// for gopts
// also, wx-related
#include "wxvbam.h"

// Interface
#include "../common/ConfigManager.h"
#include "../common/SoundDriver.h"

// OpenAL
#include "openal.h"

// Internals
#include "../gba/Sound.h"
#include "../gba/Globals.h" // for 'speedup' and 'synchronize'

// Debug
#include <assert.h>
#define ASSERT_SUCCESS   assert( AL_NO_ERROR == ALFunction.alGetError() )

#ifndef LOGALL
// replace logging functions with comments
#ifdef winlog
#undef winlog
#endif
#define winlog //
#define debugState() //
#endif

struct OPENALFNTABLE;

class OpenAL : public SoundDriver
{
public:
	OpenAL();
	virtual ~OpenAL();

	static wxDynamicLibrary Lib;
	static bool LoadOAL();
	static bool GetDevices(wxArrayString &names, wxArrayString &ids);
	bool init(long sampleRate);   // initialize the sound buffer queue
	void pause();  // pause the secondary sound buffer
	void reset();  // stop and reset the secondary sound buffer
	void resume(); // play/resume the secondary sound buffer
	void write(u16* finalWave, int length);   // write the emulated sound to a sound buffer

private:
	static OPENALFNTABLE  ALFunction;
	bool           initialized;
	bool           buffersLoaded;
	ALCdevice*     device;
	ALCcontext*    context;
	ALuint*        buffer;
	ALuint         tempBuffer;
	ALuint         source;
	int            freq;
	int            soundBufferLen;

#ifdef LOGALL
	void           debugState();
#endif
};

OpenAL::OpenAL()
{
	initialized = false;
	buffersLoaded = false;
	device = NULL;
	context = NULL;
	buffer = (ALuint*)malloc(gopts.audio_buffers * sizeof(ALuint));
	memset(buffer, 0, gopts.audio_buffers * sizeof(ALuint));
	tempBuffer = 0;
	source = 0;
}


OpenAL::~OpenAL()
{
	if (!initialized) return;

	ALFunction.alSourceStop(source);
	ASSERT_SUCCESS;
	ALFunction.alSourcei(source, AL_BUFFER, 0);
	ASSERT_SUCCESS;
	ALFunction.alDeleteSources(1, &source);
	ASSERT_SUCCESS;
	ALFunction.alDeleteBuffers(gopts.audio_buffers, buffer);
	ASSERT_SUCCESS;
	free(buffer);
	ALFunction.alcMakeContextCurrent(NULL);
	// Wine incorrectly returns ALC_INVALID_VALUE
	// and then fails the rest of these functions as well
	// so there will be a leak under Wine, but that's a bug in Wine, not
	// this code
	//ASSERT_SUCCESS;
	ALFunction.alcDestroyContext(context);
	//ASSERT_SUCCESS;
	ALFunction.alcCloseDevice(device);
	//ASSERT_SUCCESS;
	ALFunction.alGetError(); // reset error state
}

#ifdef LOGALL
void OpenAL::debugState()
{
	ALint value = 0;
	ALFunction.alGetSourcei(source, AL_SOURCE_STATE, &value);
	ASSERT_SUCCESS;
	winlog(" soundPaused = %i\n", soundPaused);
	winlog(" Source:\n");
	winlog("  State: ");

	switch (value)
	{
	case AL_INITIAL:
		winlog("AL_INITIAL\n");
		break;

	case AL_PLAYING:
		winlog("AL_PLAYING\n");
		break;

	case AL_PAUSED:
		winlog("AL_PAUSED\n");
		break;

	case AL_STOPPED:
		winlog("AL_STOPPED\n");
		break;

	default:
		winlog("!unknown!\n");
		break;
	}

	ALFunction.alGetSourcei(source, AL_BUFFERS_QUEUED, &value);
	ASSERT_SUCCESS;
	winlog("  Buffers in queue: %i\n", value);
	ALFunction.alGetSourcei(source, AL_BUFFERS_PROCESSED, &value);
	ASSERT_SUCCESS;
	winlog("  Buffers processed: %i\n", value);
}
#endif


bool OpenAL::init(long sampleRate)
{
	winlog("OpenAL::init\n");
	assert(initialized == false);

	if (!LoadOAL())
	{
		wxLogError(_("OpenAL library could not be found on your system.  Please install the runtime from http://openal.org"));
		return false;
	}

	if (!gopts.audio_dev.empty())
	{
		device = ALFunction.alcOpenDevice(gopts.audio_dev.mb_str());
	}
	else
	{
		device = ALFunction.alcOpenDevice(NULL);
	}

	assert(device != NULL);
	context = ALFunction.alcCreateContext(device, NULL);
	assert(context != NULL);
	ALCboolean retVal = ALFunction.alcMakeContextCurrent(context);
	assert(ALC_TRUE == retVal);
	ALFunction.alGenBuffers(gopts.audio_buffers, buffer);
	ASSERT_SUCCESS;
	ALFunction.alGenSources(1, &source);
	ASSERT_SUCCESS;
	freq = sampleRate;
	// calculate the number of samples per frame first
	// then multiply it with the size of a sample frame (16 bit * stereo)
	soundBufferLen = (freq / 60) * 4;
	initialized = true;
	return true;
}


void OpenAL::resume()
{
	if (!initialized) return;

	winlog("OpenAL::resume\n");

	if (!buffersLoaded) return;

	debugState();
	ALint sourceState = 0;
	ALFunction.alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
	ASSERT_SUCCESS;

	if (sourceState != AL_PLAYING)
	{
		ALFunction.alSourcePlay(source);
		ASSERT_SUCCESS;
	}

	debugState();
}


void OpenAL::pause()
{
	if (!initialized) return;

	winlog("OpenAL::pause\n");

	if (!buffersLoaded) return;

	debugState();
	ALint sourceState = 0;
	ALFunction.alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
	ASSERT_SUCCESS;

	if (sourceState == AL_PLAYING)
	{
		ALFunction.alSourcePause(source);
		ASSERT_SUCCESS;
	}

	debugState();
}


void OpenAL::reset()
{
	if (!initialized) return;

	winlog("OpenAL::reset\n");

	if (!buffersLoaded) return;

	debugState();
	ALint sourceState = 0;
	ALFunction.alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
	ASSERT_SUCCESS;

	if (sourceState != AL_STOPPED)
	{
		ALFunction.alSourceStop(source);
		ASSERT_SUCCESS;
	}

	debugState();
}


void OpenAL::write(u16* finalWave, int length)
{
	if (!initialized) return;

	winlog("OpenAL::write\n");
	debugState();
	ALint sourceState = 0;
	ALint nBuffersProcessed = 0;

	if (!buffersLoaded)
	{
		// ==initial buffer filling==
		winlog(" initial buffer filling\n");

		for (int i = 0 ; i < gopts.audio_buffers ; i++)
		{
			// Filling the buffers explicitly with silence would be cleaner,
			// but the very first sample is usually silence anyway.
			ALFunction.alBufferData(buffer[i], AL_FORMAT_STEREO16, finalWave, soundBufferLen, freq);
			ASSERT_SUCCESS;
		}

		ALFunction.alSourceQueueBuffers(source, gopts.audio_buffers, buffer);
		ASSERT_SUCCESS;
		buffersLoaded = true;
	}
	else
	{
		// ==normal buffer refreshing==
		nBuffersProcessed = 0;
		ALFunction.alGetSourcei(source, AL_BUFFERS_PROCESSED, &nBuffersProcessed);
		ASSERT_SUCCESS;

		if (nBuffersProcessed == gopts.audio_buffers)
		{
			// we only want to know about it when we are emulating at full speed or faster:
			if ((throttle >= 100) || (throttle == 0))
			{
				if (systemVerbose & VERBOSE_SOUNDOUTPUT)
				{
					static unsigned int i = 0;
					log("OpenAL: Buffers were not refilled fast enough (i=%i)\n", i++);
				}
			}
		}

		if (!speedup && throttle && !gba_joybus_active)
		{
			// wait until at least one buffer has finished
			while (nBuffersProcessed == 0)
			{
				winlog(" waiting...\n");
				// wait for about half the time one buffer needs to finish
				// unoptimized: ( sourceBufferLen * 1000 ) / ( freq * 2 * 2 ) * 1/2
				wxMilliSleep(soundBufferLen / (freq >> 7));
				ALFunction.alGetSourcei(source, AL_BUFFERS_PROCESSED, &nBuffersProcessed);
				ASSERT_SUCCESS;
			}
		}
		else
		{
			if (nBuffersProcessed == 0) return;
		}

		assert(nBuffersProcessed > 0);
		// unqueue buffer
		tempBuffer = 0;
		ALFunction.alSourceUnqueueBuffers(source, 1, &tempBuffer);
		ASSERT_SUCCESS;
		// refill buffer
		ALFunction.alBufferData(tempBuffer, AL_FORMAT_STEREO16, finalWave, soundBufferLen, freq);
		ASSERT_SUCCESS;
		// requeue buffer
		ALFunction.alSourceQueueBuffers(source, 1, &tempBuffer);
		ASSERT_SUCCESS;
	}

	// start playing the source if necessary
	ALFunction.alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
	ASSERT_SUCCESS;

	if (!soundPaused && (sourceState != AL_PLAYING))
	{
		ALFunction.alSourcePlay(source);
		ASSERT_SUCCESS;
	}
}

SoundDriver* newOpenAL()
{
	winlog("newOpenAL\n");
	return new OpenAL();
}

// no more use of copyrighted OpenAL code just to load the stupid library
// this is for compatibility with MFC version
// positive:  make an OpenAL-capable binary which does not require OpenAL
// negative:  openal lib may not be in library path
//            openal lib name is OS-dependent, and may even change based
//            on where it was installed from
// On UNIX, it would probably be better to just hard link with libopenal

OPENALFNTABLE OpenAL::ALFunction = {NULL};
wxDynamicLibrary OpenAL::Lib;

bool OpenAL::LoadOAL()
{
	if (!Lib.IsLoaded() &&
#ifdef __WXMSW__
	        // on win32, it's openal32.dll
	        !Lib.Load(wxT("openal32")) &&
#else
#ifdef __WXMAC__
	        // on macosx, it's just plain OpenAL
	        !Lib.Load(wxT("OpenAL"), wxDL_NOW | wxDL_VERBATIM) &&
#endif
#endif
	        // on linux, it's libopenal.so
	        // try standard name on all platforms
	        !Lib.Load(wxDynamicLibrary::CanonicalizeName(wxT("openal"))))
		return false;

#define loadfn(t, n) do { \
    if(!(ALFunction.n = (t)Lib.GetSymbol(wxT(#n)))) \
        return false; \
} while(0)
	//loadfn(LPALENABLE, alEnable);
	//loadfn(LPALDISABLE, alDisable);
	//loadfn(LPALISENABLED, alIsEnabled);
	//loadfn(LPALGETSTRING, alGetString);
	//loadfn(LPALGETBOOLEANV, alGetBooleanv);
	//loadfn(LPALGETINTEGERV, alGetIntegerv);
	//loadfn(LPALGETFLOATV, alGetFloatv);
	//loadfn(LPALGETDOUBLEV, alGetDoublev);
	//loadfn(LPALGETBOOLEAN, alGetBoolean);
	//loadfn(LPALGETINTEGER, alGetInteger);
	//loadfn(LPALGETFLOAT, alGetFloat);
	//loadfn(LPALGETDOUBLE, alGetDouble);
	loadfn(LPALGETERROR, alGetError);
	//loadfn(LPALISEXTENSIONPRESENT, alIsExtensionPresent);
	//loadfn(LPALGETPROCADDRESS, alGetProcAddress);
	//loadfn(LPALGETENUMVALUE, alGetEnumValue);
	//loadfn(LPALLISTENERF, alListenerf);
	//loadfn(LPALLISTENER3F, alListener3f);
	//loadfn(LPALLISTENERFV, alListenerfv);
	//loadfn(LPALLISTENERI, alListeneri);
	//loadfn(LPALLISTENER3I, alListener3i);
	//loadfn(LPALLISTENERIV, alListeneriv);
	//loadfn(LPALGETLISTENERF, alGetListenerf);
	//loadfn(LPALGETLISTENER3F, alGetListener3f);
	//loadfn(LPALGETLISTENERFV, alGetListenerfv);
	//loadfn(LPALGETLISTENERI, alGetListeneri);
	//loadfn(LPALGETLISTENER3I, alGetListener3i);
	//loadfn(LPALGETLISTENERIV, alGetListeneriv);
	loadfn(LPALGENSOURCES, alGenSources);
	loadfn(LPALDELETESOURCES, alDeleteSources);
	//loadfn(LPALISSOURCE, alIsSource);
	//loadfn(LPALSOURCEF, alSourcef);
	//loadfn(LPALSOURCE3F, alSource3f);
	//loadfn(LPALSOURCEFV, alSourcefv);
	loadfn(LPALSOURCEI, alSourcei);
	//loadfn(LPALSOURCE3I, alSource3i);
	//loadfn(LPALSOURCEIV, alSourceiv);
	//loadfn(LPALGETSOURCEF, alGetSourcef);
	//loadfn(LPALGETSOURCE3F, alGetSource3f);
	//loadfn(LPALGETSOURCEFV, alGetSourcefv);
	loadfn(LPALGETSOURCEI, alGetSourcei);
	//loadfn(LPALGETSOURCE3I, alGetSource3i);
	//loadfn(LPALGETSOURCEIV, alGetSourceiv);
	//loadfn(LPALSOURCEPLAYV, alSourcePlayv);
	//loadfn(LPALSOURCESTOPV, alSourceStopv);
	//loadfn(LPALSOURCEREWINDV, alSourceRewindv);
	//loadfn(LPALSOURCEPAUSEV, alSourcePausev);
	loadfn(LPALSOURCEPLAY, alSourcePlay);
	loadfn(LPALSOURCESTOP, alSourceStop);
	//loadfn(LPALSOURCEREWIND, alSourceRewind);
	loadfn(LPALSOURCEPAUSE, alSourcePause);
	loadfn(LPALSOURCEQUEUEBUFFERS, alSourceQueueBuffers);
	loadfn(LPALSOURCEUNQUEUEBUFFERS, alSourceUnqueueBuffers);
	loadfn(LPALGENBUFFERS, alGenBuffers);
	loadfn(LPALDELETEBUFFERS, alDeleteBuffers);
	//loadfn(LPALISBUFFER, alIsBuffer);
	loadfn(LPALBUFFERDATA, alBufferData);
	//loadfn(LPALBUFFERF, alBufferf);
	//loadfn(LPALBUFFER3F, alBuffer3f);
	//loadfn(LPALBUFFERFV, alBufferfv);
	//loadfn(LPALBUFFERI, alBufferi);
	//loadfn(LPALBUFFER3I, alBuffer3i);
	//loadfn(LPALBUFFERIV, alBufferiv);
	//loadfn(LPALGETBUFFERF, alGetBufferf);
	//loadfn(LPALGETBUFFER3F, alGetBuffer3f);
	//loadfn(LPALGETBUFFERFV, alGetBufferfv);
	//loadfn(LPALGETBUFFERI, alGetBufferi);
	//loadfn(LPALGETBUFFER3I, alGetBuffer3i);
	//loadfn(LPALGETBUFFERIV, alGetBufferiv);
	//loadfn(LPALDOPPLERFACTOR, alDopplerFactor);
	//loadfn(LPALDOPPLERVELOCITY, alDopplerVelocity);
	//loadfn(LPALSPEEDOFSOUND, alSpeedOfSound);
	//loadfn(LPALDISTANCEMODEL, alDistanceModel);
	loadfn(LPALCCREATECONTEXT, alcCreateContext);
	loadfn(LPALCMAKECONTEXTCURRENT, alcMakeContextCurrent);
	//loadfn(LPALCPROCESSCONTEXT, alcProcessContext);
	//loadfn(LPALCSUSPENDCONTEXT, alcSuspendContext);
	loadfn(LPALCDESTROYCONTEXT, alcDestroyContext);
	//loadfn(LPALCGETCURRENTCONTEXT, alcGetCurrentContext);
	//loadfn(LPALCGETCONTEXTSDEVICE, alcGetContextsDevice);
	loadfn(LPALCOPENDEVICE, alcOpenDevice);
	loadfn(LPALCCLOSEDEVICE, alcCloseDevice);
	//loadfn(LPALCGETERROR, alcGetError);
	loadfn(LPALCISEXTENSIONPRESENT, alcIsExtensionPresent);
	//loadfn(LPALCGETPROCADDRESS, alcGetProcAddress);
	//loadfn(LPALCGETENUMVALUE, alcGetEnumValue);
	loadfn(LPALCGETSTRING, alcGetString);
	//loadfn(LPALCGETINTEGERV, alcGetIntegerv);
	//loadfn(LPALCCAPTUREOPENDEVICE, alcCaptureOpenDevice);
	//loadfn(LPALCCAPTURECLOSEDEVICE, alcCaptureCloseDevice);
	//loadfn(LPALCCAPTURESTART, alcCaptureStart);
	//loadfn(LPALCCAPTURESTOP, alcCaptureStop);
	//loadfn(LPALCCAPTURESAMPLES, alcCaptureSamples);
	return true;
}

bool GetOALDevices(wxArrayString &names, wxArrayString &ids)
{
	return OpenAL::GetDevices(names, ids);
}

bool OpenAL::GetDevices(wxArrayString &names, wxArrayString &ids)
{
	if (!OpenAL::LoadOAL())
		return false;

#ifdef ALC_DEVICE_SPECIFIER

	if (ALFunction.alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_FALSE)
		// this extension isn't critical to OpenAL operating
		return true;

	const char* devs = ALFunction.alcGetString(NULL, ALC_DEVICE_SPECIFIER);

	while (*devs)
	{
		names.push_back(wxString(devs, wxConvLibc));
		ids.push_back(names[names.size() - 1]);
		devs += strlen(devs) + 1;
	}

#else
	// should work anyway, but must always use default driver
	return true;
#endif
}

#endif
