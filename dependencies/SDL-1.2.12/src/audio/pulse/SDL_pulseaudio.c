/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2007 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Stéphan Kochen
    stephan@kochen.nl
    
    Based on parts of the ALSA and ESounD output drivers.
*/
#include "SDL_config.h"

/* Allow access to an PulseAudio network stream mixing buffer */

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pulse/simple.h>

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "SDL_pulseaudio.h"

#ifdef SDL_AUDIO_DRIVER_PULSE_DYNAMIC
#include "SDL_name.h"
#include "SDL_loadso.h"
#else
#define SDL_NAME(X)	X
#endif

/* The tag name used by the driver */
#define PULSE_DRIVER_NAME	"pulse"

/* Audio driver functions */
static int PULSE_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void PULSE_WaitAudio(_THIS);
static void PULSE_PlayAudio(_THIS);
static Uint8 *PULSE_GetAudioBuf(_THIS);
static void PULSE_CloseAudio(_THIS);

#ifdef SDL_AUDIO_DRIVER_PULSE_DYNAMIC

static const char *pulse_library = SDL_AUDIO_DRIVER_PULSE_DYNAMIC;
static void *pulse_handle = NULL;
static int pulse_loaded = 0;

static pa_simple* (*SDL_NAME(pa_simple_new))(
	const char *server,
	const char *name,
	pa_stream_direction_t dir,
	const char *dev,
	const char *stream_name,
	const pa_sample_spec *ss,
	const pa_channel_map *map,
	const pa_buffer_attr *attr,
	int *error
);
static void (*SDL_NAME(pa_simple_free))(pa_simple *s);
static int (*SDL_NAME(pa_simple_drain))(pa_simple *s, int *error);
static int (*SDL_NAME(pa_simple_write))(
	pa_simple *s,
	const void *data,
	size_t length,
	int *error 
);
static pa_channel_map* (*SDL_NAME(pa_channel_map_init_auto))(
	pa_channel_map *m,
	unsigned channels,
	pa_channel_map_def_t def
);
	

static struct {
	const char *name;
	void **func;
} pulse_functions[] = {
	{ "pa_simple_new",
		(void **)&SDL_NAME(pa_simple_new)		},
	{ "pa_simple_free",
		(void **)&SDL_NAME(pa_simple_free)		},
	{ "pa_simple_drain",
		(void **)&SDL_NAME(pa_simple_drain)		},
	{ "pa_simple_write",
		(void **)&SDL_NAME(pa_simple_write)		},
	{ "pa_channel_map_init_auto",
		(void **)&SDL_NAME(pa_channel_map_init_auto)	},
};

static void UnloadPulseLibrary()
{
	if ( pulse_loaded ) {
		SDL_UnloadObject(pulse_handle);
		pulse_handle = NULL;
		pulse_loaded = 0;
	}
}

static int LoadPulseLibrary(void)
{
	int i, retval = -1;

	pulse_handle = SDL_LoadObject(pulse_library);
	if ( pulse_handle ) {
		pulse_loaded = 1;
		retval = 0;
		for ( i=0; i<SDL_arraysize(pulse_functions); ++i ) {
			*pulse_functions[i].func = SDL_LoadFunction(pulse_handle, pulse_functions[i].name);
			if ( !*pulse_functions[i].func ) {
				retval = -1;
				UnloadPulseLibrary();
				break;
			}
		}
	}
	return retval;
}

#else

static void UnloadPulseLibrary()
{
	return;
}

static int LoadPulseLibrary(void)
{
	return 0;
}

#endif /* SDL_AUDIO_DRIVER_PULSE_DYNAMIC */

/* Audio driver bootstrap functions */

static int Audio_Available(void)
{
	pa_sample_spec paspec;
	pa_simple *connection;
	int available;

	available = 0;
	if ( LoadPulseLibrary() < 0 ) {
		return available;
	}
	
	/* Connect with a dummy format. */
	paspec.format = PA_SAMPLE_U8;
	paspec.rate = 11025;
	paspec.channels = 1;
	connection = SDL_NAME(pa_simple_new)(
		SDL_getenv("PASERVER"),      /* server */
		"Test stream",               /* application name */
		PA_STREAM_PLAYBACK,          /* playback mode */
		SDL_getenv("PADEVICE"),      /* device on the server */
		"Simple DirectMedia Layer",  /* stream description */
		&paspec,                     /* sample format spec */
		NULL,                        /* channel map */
		NULL,                        /* buffering attributes */
		NULL                         /* error code */
	);
	if ( connection != NULL ) {
		available = 1;
		SDL_NAME(pa_simple_free)(connection);
	}
	
	UnloadPulseLibrary();
	return(available);
}

static void Audio_DeleteDevice(SDL_AudioDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
	UnloadPulseLibrary();
}

static SDL_AudioDevice *Audio_CreateDevice(int devindex)
{
	SDL_AudioDevice *this;

	/* Initialize all variables that we clean on shutdown */
	LoadPulseLibrary();
	this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		SDL_memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
				SDL_malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			SDL_free(this);
		}
		return(0);
	}
	SDL_memset(this->hidden, 0, (sizeof *this->hidden));

	/* Set the function pointers */
	this->OpenAudio = PULSE_OpenAudio;
	this->WaitAudio = PULSE_WaitAudio;
	this->PlayAudio = PULSE_PlayAudio;
	this->GetAudioBuf = PULSE_GetAudioBuf;
	this->CloseAudio = PULSE_CloseAudio;

	this->free = Audio_DeleteDevice;

	return this;
}

AudioBootStrap PULSE_bootstrap = {
	PULSE_DRIVER_NAME, "PulseAudio",
	Audio_Available, Audio_CreateDevice
};

/* This function waits until it is possible to write a full sound buffer */
static void PULSE_WaitAudio(_THIS)
{
	/* Check to see if the thread-parent process is still alive */
	{ static int cnt = 0;
		/* Note that this only works with thread implementations 
		   that use a different process id for each thread.
		*/
		if (parent && (((++cnt)%10) == 0)) { /* Check every 10 loops */
			if ( kill(parent, 0) < 0 ) {
				this->enabled = 0;
			}
		}
	}
}

static void PULSE_PlayAudio(_THIS)
{
	/* Write the audio data */
	if ( SDL_NAME(pa_simple_write)(stream, mixbuf, mixlen, NULL) != 0 )
	{
		this->enabled = 0;
	}
}

static Uint8 *PULSE_GetAudioBuf(_THIS)
{
	return(mixbuf);
}

static void PULSE_CloseAudio(_THIS)
{
	if ( mixbuf != NULL ) {
		SDL_FreeAudioMem(mixbuf);
		mixbuf = NULL;
	}
	if ( stream != NULL ) {
		SDL_NAME(pa_simple_drain)(stream, NULL);
		SDL_NAME(pa_simple_free)(stream);
		stream = NULL;
	}
}

/* Try to get the name of the program */
static char *get_progname(void)
{
	char *progname = NULL;
#ifdef __LINUX__
	FILE *fp;
	static char temp[BUFSIZ];

	SDL_snprintf(temp, SDL_arraysize(temp), "/proc/%d/cmdline", getpid());
	fp = fopen(temp, "r");
	if ( fp != NULL ) {
		if ( fgets(temp, sizeof(temp)-1, fp) ) {
			progname = SDL_strrchr(temp, '/');
			if ( progname == NULL ) {
				progname = temp;
			} else {
				progname = progname+1;
			}
		}
		fclose(fp);
	}
#endif
	return(progname);
}

static int PULSE_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	Uint16          test_format;
	pa_sample_spec  paspec;
	pa_buffer_attr  paattr;
	pa_channel_map  pacmap;
	
	paspec.format = PA_SAMPLE_INVALID;
	for ( test_format = SDL_FirstAudioFormat(spec->format); test_format; ) {
		switch ( test_format ) {
			case AUDIO_U8:
				paspec.format = PA_SAMPLE_U8;
				break;
			case AUDIO_S16LSB:
				paspec.format = PA_SAMPLE_S16LE;
				break;
			case AUDIO_S16MSB:
				paspec.format = PA_SAMPLE_S16BE;
				break;
		}
		if ( paspec.format != PA_SAMPLE_INVALID )
			break;
	}
	if (paspec.format == PA_SAMPLE_INVALID ) {
		SDL_SetError("Couldn't find any suitable audio formats");
		return(-1);
	}
	spec->format = test_format;
	
	paspec.channels = spec->channels;
	paspec.rate = spec->freq;

	/* Calculate the final parameters for this audio specification */
	SDL_CalculateAudioSpec(spec);

	/* Allocate mixing buffer */
	mixlen = spec->size;
	mixbuf = (Uint8 *)SDL_AllocAudioMem(mixlen);
	if ( mixbuf == NULL ) {
		return(-1);
	}
	SDL_memset(mixbuf, spec->silence, spec->size);
	
	/* Reduced prebuffering compared to the defaults. */
	paattr.tlength = mixlen;
	paattr.minreq = mixlen;
	paattr.fragsize = mixlen;
	paattr.prebuf = mixlen;
	paattr.maxlength = mixlen * 4;
	
	/* The SDL ALSA output hints us that we use Windows' channel mapping */
	/* http://bugzilla.libsdl.org/show_bug.cgi?id=110 */
	SDL_NAME(pa_channel_map_init_auto)(
		&pacmap, spec->channels, PA_CHANNEL_MAP_WAVEEX);
	
	/* Connect to the PulseAudio server */
	stream = SDL_NAME(pa_simple_new)(
		SDL_getenv("PASERVER"),      /* server */
		get_progname(),              /* application name */
		PA_STREAM_PLAYBACK,          /* playback mode */
		SDL_getenv("PADEVICE"),      /* device on the server */
		"Simple DirectMedia Layer",  /* stream description */
		&paspec,                     /* sample format spec */
		&pacmap,                     /* channel map */
		&paattr,                     /* buffering attributes */
		NULL                         /* error code */
	);
	if ( stream == NULL ) {
		PULSE_CloseAudio(this);
		SDL_SetError("Could not connect to PulseAudio");
		return(-1);
	}

	/* Get the parent process id (we're the parent of the audio thread) */
	parent = getpid();
	
	return(0);
}

