#include <stdio.h>
#include <string.h>
#include <portaudio.h>

#ifndef _WIN32
# include <unistd.h>
#else
# include <Windows.h>
#endif

#include "../System.h"
#include "../Sound.h"
#include "../Globals.h"

extern int emulating;

bool systemSoundOn = false;

static PaStreamParameters outputParameters;
static PaStream *stream;
static PaError err;

static u16 *stereodata16;
static u32 soundoffset;
static volatile u32 soundpos;
static u32 soundbufsize;

static unsigned int audioFree()
{
   if (soundoffset > soundpos)
      return soundbufsize - soundoffset + soundpos;
   else
      return soundpos - soundoffset;
}

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
{
    const unsigned int outBufferLength = framesPerBuffer * sizeof(u16) * 2;
    u8 *out = (u8*)outputBuffer;
    
    if (!emulating || soundPaused)
    {
        memset(out, 0, outBufferLength);
        return paContinue;
    }

    if (soundbufsize - audioFree() < outBufferLength)
        fprintf(stderr, "** PortAudio : buffer underflow **\n");

    u8 *soundbuf = (u8 *)stereodata16;
    
    for (unsigned int i = 0; i < outBufferLength; i++)
    {
        if (soundpos >= soundbufsize)
            soundpos = 0;

        out[i] = soundbuf[soundpos];
        soundpos++;
    }

    return paContinue;    
}

void systemWriteDataToSoundBuffer()
{
    u32 copy1size = 0, copy2size = 0;

    while (emulating && !speedup & !systemThrottle && (audioFree() < (unsigned int)soundBufferLen))
    {
#ifndef _WIN32
        usleep(1000);
#else
        Sleep(1);
#endif
    }

    if ((soundbufsize - soundoffset) < (unsigned int)soundBufferLen)
    {
        copy1size = (soundbufsize - soundoffset);
        copy2size = soundBufferLen - copy1size;
    }
    else
    {
        copy1size = soundBufferLen;
        copy2size = 0;
    }

    memcpy((((u8 *)stereodata16)+soundoffset), soundFinalWave, copy1size);

    if (copy2size)
        memcpy(stereodata16, ((u8 *)soundFinalWave)+copy1size, copy2size);

    soundoffset += copy1size + copy2size;
    soundoffset %= soundbufsize;
}

bool systemSoundInit()
{
    int sampleRate;
    
    err = Pa_Initialize();
    if (err != paNoError) goto error;
    
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = 2;       /* stereo output */
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    switch(soundQuality) {
        case 1:
            sampleRate = 44100;
            soundBufferLen = 1470*2;
        break;
        default:
        case 2:
            sampleRate = 22050;
            soundBufferLen = 736*2;
        break;
        case 4:
            sampleRate = 11025;
            soundBufferLen = 368*2;
        break;
    }

    soundbufsize = (soundBufferLen + 1) * 4;

    stereodata16 = new u16[soundbufsize];
    memset(stereodata16, 0, soundbufsize);
    soundpos = 0;

    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              sampleRate,
              0,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              patestCallback,
              NULL );
    if (err != paNoError) goto error;

    err = Pa_StartStream( stream );
    if (err != paNoError) goto error;
    
    systemSoundOn = true;
    
    return true;
error:
    Pa_Terminate();
    fprintf(stderr, "An error occured while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    return false;
}

void systemSoundShutdown()
{
    Pa_CloseStream(stream);
    Pa_Terminate();

    delete[] stereodata16;
}

void systemSoundPause()
{
}

void systemSoundResume()
{
}

void systemSoundReset()
{
}
