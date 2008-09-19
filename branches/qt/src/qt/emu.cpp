// This file adds all the code required by System.h


#include "emu.h"


bool emulating = false;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
bool systemSoundOn = false;
u32 systemColorMap32[0x10000];
u16 systemColorMap16[0x10000];
u16 systemGbPalette[24];
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 32;
int systemDebug = 0;
int systemVerbose = 0;

void ( *dbgSignal )( int sig, int number ) = debuggerSignal;
void ( *dbgOutput )( const char *s, u32 addr ) = debuggerOutput;


bool systemReadJoypads()
{
    // TODO: implement
    return false;
}

u32 systemReadJoypad( int which )
{
    // TODO: implement
    return 0;
}

void systemUpdateMotionSensor()
{
    // TODO: implement
}


int systemGetSensorX()
{
    // TODO: implement
    return 0;
}


int systemGetSensorY()
{
    // TODO: implement
    return 0;
}


u32 systemGetClock()
{
    // TODO: implement
    return 0;
}


void systemMessage( int number, const char *defaultMsg, ... )
{
    // TODO: implement
}


void systemScreenCapture( int captureNumber )
{
    // TODO: implement
}


void systemDrawScreen()
{
    // TODO: implement
}


void systemShowSpeed( int speed )
{
    // TODO: implement
}


void system10Frames( int rate )
{
    // TODO: implement
}


void systemFrame()
{
    // TODO: implement
}


bool systemPauseOnFrame()
{
    // TODO: implement
    return false;
}


void systemGbBorderOn()
{
    // TODO: implement
}


void debuggerOutput( const char *s, u32 addr )
{
    // TODO: implement
}


void debuggerSignal( int sig, int number )
{
    // TODO: implement
}


void winlog( const char *msg, ... )
{
    // TODO: implement
}


void systemWriteDataToSoundBuffer()
{
    // TODO: implement
}

void systemSoundShutdown()
{
    // TODO: implement
}

void systemSoundPause()
{
    // TODO: implement
}

void systemSoundResume()
{
    // TODO: implement
}

void systemSoundReset()
{
    // TODO: implement
}

bool systemSoundInit()
{
    // TODO: implement
    return false;
}

bool systemCanChangeSoundQuality()
{
    // TODO: implement
    return false;
}

void systemGbPrint( u8 *data,
                    int pages,
                    int feed,
                    int palette,
                    int contrast )
{
    //GBPrinterDlg printer;
    //printer.processData(data);
    //printer.DoModal();
}

