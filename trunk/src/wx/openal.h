// on win32 and mac, pointer typedefs only happen with AL_NO_PROTOTYPES
// on mac, ALC_NO_PROTOTYPES as well

#define AL_NO_PROTOTYPES 1

// on mac, alc pointer typedefs ony happen for ALC if ALC_NO_PROTOTYPES
// unfortunately, there is a bug in the system headers (use of ALCvoid when
// void should be used; shame on Apple for introducing this error, and shame
// on Creative for making a typedef to void in the first place)
//#define ALC_NO_PROTOTYPES 1

#include <al.h>
#include <alc.h>

// since the ALC typedefs are broken on Mac:

#ifdef __WXMAC__
typedef ALCcontext*    (ALC_APIENTRY* LPALCCREATECONTEXT)(ALCdevice* device, const ALCint* attrlist);
typedef ALCboolean(ALC_APIENTRY* LPALCMAKECONTEXTCURRENT)(ALCcontext* context);
typedef void (ALC_APIENTRY* LPALCDESTROYCONTEXT)(ALCcontext* context);
typedef ALCdevice*     (ALC_APIENTRY* LPALCOPENDEVICE)(const ALCchar* devicename);
typedef ALCboolean(ALC_APIENTRY* LPALCCLOSEDEVICE)(ALCdevice* device);
typedef ALCboolean(ALC_APIENTRY* LPALCISEXTENSIONPRESENT)(ALCdevice* device, const ALCchar* extname);
typedef const ALCchar* (ALC_APIENTRY* LPALCGETSTRING)(ALCdevice* device, ALCenum param);
#endif

// no more use of copyrighted OpenAL code just to load the stupid library
struct OPENALFNTABLE
{
	//LPALENABLE alEnable;
	//LPALDISABLE alDisable;
	//LPALISENABLED alIsEnabled;
	//LPALGETSTRING alGetString;
	//LPALGETBOOLEANV alGetBooleanv;
	//LPALGETINTEGERV alGetIntegerv;
	//LPALGETFLOATV alGetFloatv;
	//LPALGETDOUBLEV alGetDoublev;
	//LPALGETBOOLEAN alGetBoolean;
	//LPALGETINTEGER alGetInteger;
	//LPALGETFLOAT alGetFloat;
	//LPALGETDOUBLE alGetDouble;
	LPALGETERROR alGetError;
	LPALISEXTENSIONPRESENT alIsExtensionPresent;
	//LPALGETPROCADDRESS alGetProcAddress;
	//LPALGETENUMVALUE alGetEnumValue;
	//LPALLISTENERF alListenerf;
	//LPALLISTENER3F alListener3f;
	//LPALLISTENERFV alListenerfv;
	//LPALLISTENERI alListeneri;
	//LPALLISTENER3I alListener3i;
	//LPALLISTENERIV alListeneriv;
	//LPALGETLISTENERF alGetListenerf;
	//LPALGETLISTENER3F alGetListener3f;
	//LPALGETLISTENERFV alGetListenerfv;
	//LPALGETLISTENERI alGetListeneri;
	//LPALGETLISTENER3I alGetListener3i;
	//LPALGETLISTENERIV alGetListeneriv;
	LPALGENSOURCES alGenSources;
	LPALDELETESOURCES alDeleteSources;
	//LPALISSOURCE alIsSource;
	//LPALSOURCEF alSourcef;
	//LPALSOURCE3F alSource3f;
	//LPALSOURCEFV alSourcefv;
	LPALSOURCEI alSourcei;
	//LPALSOURCE3I alSource3i;
	//LPALSOURCEIV alSourceiv;
	//LPALGETSOURCEF alGetSourcef;
	//LPALGETSOURCE3F alGetSource3f;
	//LPALGETSOURCEFV alGetSourcefv;
	LPALGETSOURCEI alGetSourcei;
	//LPALGETSOURCE3I alGetSource3i;
	//LPALGETSOURCEIV alGetSourceiv;
	//LPALSOURCEPLAYV alSourcePlayv;
	//LPALSOURCESTOPV alSourceStopv;
	//LPALSOURCEREWINDV alSourceRewindv;
	//LPALSOURCEPAUSEV alSourcePausev;
	LPALSOURCEPLAY alSourcePlay;
	LPALSOURCESTOP alSourceStop;
	//LPALSOURCEREWIND alSourceRewind;
	LPALSOURCEPAUSE alSourcePause;
	LPALSOURCEQUEUEBUFFERS alSourceQueueBuffers;
	LPALSOURCEUNQUEUEBUFFERS alSourceUnqueueBuffers;
	LPALGENBUFFERS alGenBuffers;
	LPALDELETEBUFFERS alDeleteBuffers;
	//LPALISBUFFER alIsBuffer;
	LPALBUFFERDATA alBufferData;
	//LPALBUFFERF alBufferf;
	//LPALBUFFER3F alBuffer3f;
	//LPALBUFFERFV alBufferfv;
	//LPALBUFFERI alBufferi;
	//LPALBUFFER3I alBuffer3i;
	//LPALBUFFERIV alBufferiv;
	//LPALGETBUFFERF alGetBufferf;
	//LPALGETBUFFER3F alGetBuffer3f;
	//LPALGETBUFFERFV alGetBufferfv;
	//LPALGETBUFFERI alGetBufferi;
	//LPALGETBUFFER3I alGetBuffer3i;
	//LPALGETBUFFERIV alGetBufferiv;
	//LPALDOPPLERFACTOR alDopplerFactor;
	//LPALDOPPLERVELOCITY alDopplerVelocity;
	//LPALSPEEDOFSOUND alSpeedOfSound;
	//LPALDISTANCEMODEL alDistanceModel;

	LPALCCREATECONTEXT alcCreateContext;
	LPALCMAKECONTEXTCURRENT alcMakeContextCurrent;
	//LPALCPROCESSCONTEXT alcProcessContext;
	//LPALCSUSPENDCONTEXT alcSuspendContext;
	LPALCDESTROYCONTEXT alcDestroyContext;
	//LPALCGETCURRENTCONTEXT alcGetCurrentContext;
	//LPALCGETCONTEXTSDEVICE alcGetContextsDevice;
	LPALCOPENDEVICE alcOpenDevice;
	LPALCCLOSEDEVICE alcCloseDevice;
	//LPALCGETERROR alcGetError;
	LPALCISEXTENSIONPRESENT alcIsExtensionPresent;
	//LPALCGETPROCADDRESS alcGetProcAddress;
	//LPALCGETENUMVALUE alcGetEnumValue;
	LPALCGETSTRING alcGetString;
	//LPALCGETINTEGERV alcGetIntegerv;
	//LPALCCAPTUREOPENDEVICE alcCaptureOpenDevice;
	//LPALCCAPTURECLOSEDEVICE alcCaptureCloseDevice;
	//LPALCCAPTURESTART alcCaptureStart;
	//LPALCCAPTURESTOP alcCaptureStop;
	//LPALCCAPTURESAMPLES alcCaptureSamples;
};
