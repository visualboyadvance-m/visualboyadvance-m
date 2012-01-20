#include "stdafx.h"
#include "VBA.h"
#include "MainWnd.h"

#include "Associate.h"
#include "Directories.h"
#include "FileDlg.h"
#include "GameOverrides.h"
#include "LinkOptions.h"
#include "JoybusOptions.h"
#include "GBColorDlg.h"
#include "Joypad.h"
#include "MaxScale.h"
#include "Reg.h"
#include "RewindInterval.h"
#include "Throttle.h"
#include "WinResUtil.h"
#include "SelectPlugin.h"
#include "OALConfig.h"
#include "XAudio2_Config.h"
#include "BIOSDialog.h"
#include "AudioCoreSettingsDlg.h"

#include "../System.h"
#include "../gba/agbprint.h"
#include "../gba/GBA.h"
#include "../gba/Globals.h"
#include "../gba/Sound.h"
#include "../gb/GB.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbPrinter.h"
#include "../gb/gbSound.h"
#include "../gba/GBALink.h"

#include "../version.h"

#include <tchar.h>


void MainWnd::OnOptionsFrameskipThrottleNothrottle()
{
	theApp.updateThrottle( 0 ); // disable
  theApp.autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottleNothrottle(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( theApp.throttle == 0 );
}


void MainWnd::OnOptionsFrameskipThrottle25()
{
	theApp.updateThrottle( 25 );
  theApp.autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle25(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( theApp.throttle == 25 );
}


void MainWnd::OnOptionsFrameskipThrottle50()
{
	theApp.updateThrottle( 50 );
  theApp.autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle50(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( theApp.throttle == 50 );
}


void MainWnd::OnOptionsFrameskipThrottle100()
{
	theApp.updateThrottle( 100 );
  theApp.autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle100(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( theApp.throttle == 100 );
}


void MainWnd::OnOptionsFrameskipThrottle150()
{
	theApp.updateThrottle( 150 );
theApp.autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle150(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( theApp.throttle == 150 );
}


void MainWnd::OnOptionsFrameskipThrottle200()
{
	theApp.updateThrottle( 200 );
  theApp.autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle200(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( theApp.throttle == 200 );
}


void MainWnd::OnOptionsFrameskipThrottleOther()
{
	Throttle dlg;
	unsigned short v = (unsigned short)dlg.DoModal();

	if( v ) {
		theApp.updateThrottle( v );
		theApp.autoFrameSkip = false;
	}
}


void MainWnd::OnUpdateOptionsFrameskipThrottleOther(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(
		( theApp.throttle != 0 ) &&
		( theApp.throttle != 25 ) &&
		( theApp.throttle != 50 ) &&
		( theApp.throttle != 100 ) &&
		( theApp.throttle != 150 ) &&
		( theApp.throttle != 200 ) );
}

void MainWnd::OnOptionsFrameskipAutomatic() 
{
  theApp.autoFrameSkip = !theApp.autoFrameSkip;
  if(!theApp.autoFrameSkip && emulating)
	  theApp.updateFrameSkip();
  else
  {
	  theApp.throttle = false;
	  frameSkip = 0;
	  systemFrameSkip = 0;
  }
}

void MainWnd::OnUpdateOptionsFrameskipAutomatic(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(theApp.autoFrameSkip);  
}

BOOL MainWnd::OnOptionsFrameskip(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_VIDEO_FRAMESKIP_0:
  case ID_OPTIONS_VIDEO_FRAMESKIP_1:
  case ID_OPTIONS_VIDEO_FRAMESKIP_2:
  case ID_OPTIONS_VIDEO_FRAMESKIP_3:
  case ID_OPTIONS_VIDEO_FRAMESKIP_4:
  case ID_OPTIONS_VIDEO_FRAMESKIP_5:
    if(theApp.cartridgeType == IMAGE_GBA) {
      frameSkip = nID - ID_OPTIONS_VIDEO_FRAMESKIP_0;
    } else {
      gbFrameSkip = nID - ID_OPTIONS_VIDEO_FRAMESKIP_0;
    }
    if(emulating)
      theApp.updateFrameSkip();
	theApp.updateThrottle( 0 );
    return TRUE;
    break;
  case ID_OPTIONS_VIDEO_FRAMESKIP_6:
  case ID_OPTIONS_VIDEO_FRAMESKIP_7:
  case ID_OPTIONS_VIDEO_FRAMESKIP_8:
  case ID_OPTIONS_VIDEO_FRAMESKIP_9:
    if(theApp.cartridgeType == IMAGE_GBA) {
      frameSkip = 6 + nID - ID_OPTIONS_VIDEO_FRAMESKIP_6;
    } else {
      gbFrameSkip = 6 + nID - ID_OPTIONS_VIDEO_FRAMESKIP_6;
    }
    if(emulating)
      theApp.updateFrameSkip();
	theApp.updateThrottle( 0 );
    return TRUE;
    break;
  }
  return FALSE;
}

void MainWnd::OnUpdateOptionsVideoFrameskip0(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 0 : gbFrameSkip == 0);
}

void MainWnd::OnUpdateOptionsVideoFrameskip1(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 1 : gbFrameSkip == 1);
}

void MainWnd::OnUpdateOptionsVideoFrameskip2(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 2 : gbFrameSkip == 2);
}

void MainWnd::OnUpdateOptionsVideoFrameskip3(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 3 : gbFrameSkip == 3);
}

void MainWnd::OnUpdateOptionsVideoFrameskip4(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 4 : gbFrameSkip == 4);
}

void MainWnd::OnUpdateOptionsVideoFrameskip5(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 5 : gbFrameSkip == 5);
}

void MainWnd::OnUpdateOptionsVideoFrameskip6(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 6 : gbFrameSkip == 6);
}

void MainWnd::OnUpdateOptionsVideoFrameskip7(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 7 : gbFrameSkip == 7);
}

void MainWnd::OnUpdateOptionsVideoFrameskip8(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 8 : gbFrameSkip == 8);
}

void MainWnd::OnUpdateOptionsVideoFrameskip9(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.cartridgeType == IMAGE_GBA ? frameSkip == 9 : gbFrameSkip == 9);
}


void MainWnd::OnOptionsVideoVsync()
{
	theApp.vsync = !theApp.vsync;
	if( theApp.display ) {
		theApp.display->setOption( _T("vsync"), theApp.vsync );
	}
}


void MainWnd::OnUpdateOptionsVideoVsync(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.vsync);
}

void MainWnd::OnUpdateOptionsVideoX1(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_1X);
}

void MainWnd::OnUpdateOptionsVideoX2(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_2X);
}

void MainWnd::OnUpdateOptionsVideoX3(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_3X);
}

void MainWnd::OnUpdateOptionsVideoX4(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_4X);
}

void MainWnd::OnUpdateOptionsVideoX5(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_5X);
}

void MainWnd::OnUpdateOptionsVideoX6(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_6X);
}

BOOL MainWnd::OnOptionVideoSize(UINT nID)
{
	theApp.updateVideoSize(nID);
	return TRUE;
}

void MainWnd::OnOptionsVideoFullscreen()
{
  IDisplay::VIDEO_MODE mode;
  ZeroMemory( &mode, sizeof(IDisplay::VIDEO_MODE) );

  if( theApp.display->selectFullScreenMode( mode ) ) {
	  if( ( mode.width != theApp.fsWidth ) ||
		  ( mode.height != theApp.fsHeight ) ||
		  ( mode.bitDepth != theApp.fsColorDepth ) ||
		  ( mode.frequency != theApp.fsFrequency ) ||
		  ( mode.adapter != theApp.fsAdapter ) ||
		  ( theApp.videoOption != VIDEO_OTHER ) )
	  {
		  theApp.fsForceChange = true;
		  theApp.fsWidth = mode.width;
		  theApp.fsHeight = mode.height;
		  theApp.fsFrequency = mode.frequency;
		  theApp.fsColorDepth = mode.bitDepth;
		  theApp.fsAdapter = mode.adapter;
		  theApp.updateVideoSize( ID_OPTIONS_VIDEO_FULLSCREEN );
	  }
  }
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}


void MainWnd::OnUpdateOptionsVideoFullscreen(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.videoOption == VIDEO_OTHER);
}

void MainWnd::OnOptionsVideoDisablesfx()
{
  cpuDisableSfx = !cpuDisableSfx;
  if(emulating && theApp.cartridgeType == IMAGE_GBA)
    CPUUpdateRender();
}

void MainWnd::OnUpdateOptionsVideoDisablesfx(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuDisableSfx);
}


void MainWnd::OnOptionsVideoFullscreenstretchtofit()
{
	theApp.fullScreenStretch = !theApp.fullScreenStretch;
	if( theApp.display ) {
		theApp.display->setOption( _T("fullScreenStretch"), theApp.fullScreenStretch );
	}
}


void MainWnd::OnUpdateOptionsVideoFullscreenstretchtofit(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.fullScreenStretch);
}

BOOL MainWnd::OnVideoLayer(UINT nID)
{
  layerSettings ^= 0x0100 <<
    ((nID & 0xFFFF) - ID_OPTIONS_VIDEO_LAYERS_BG0);
  layerEnable = DISPCNT & layerSettings;
  CPUUpdateRenderBuffers(false);

  // inform the user about the change
  CString msg;
  int thisLayer = -1;
  switch( nID ) {
	  case ID_OPTIONS_VIDEO_LAYERS_BG0:
		  msg = "BG0";
		  thisLayer = 256;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_BG1:
		  msg = "BG1";
		  thisLayer = 512;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_BG2:
		  msg = "BG2";
		  thisLayer = 1024;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_BG3:
		  msg = "BG3";
		  thisLayer = 2048;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_OBJ:
		  msg = "OBJ";
		  thisLayer = 4096;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_WIN0:
		  msg = "WIN0";
		  thisLayer = 8192;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_WIN1:
		  msg = "WIN1";
		  thisLayer = 16384;
		  break;
	  case ID_OPTIONS_VIDEO_LAYERS_OBJWIN:
		  msg = "OBJWIN";
		  thisLayer = 32768;
		  break;
	  default:
		  ASSERT( false );
  }
  if( layerSettings & thisLayer ) {
	  msg.Append( " enabled" );
  } else {
	  msg.Append( " disabled" );
  }
  systemScreenMessage( msg );

  return TRUE;
}

void MainWnd::OnVideoLayerReset()
{
	layerSettings = 0xFF00;
	layerEnable = DISPCNT & layerSettings;
	CPUUpdateRenderBuffers(false);
	systemScreenMessage( "All layers enabled" );
}

void MainWnd::OnUpdateVideoLayer(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck((layerSettings >> (8 + pCmdUI->m_nID - ID_OPTIONS_VIDEO_LAYERS_BG0)) & 1);
  switch(pCmdUI->m_nID) {
  case ID_OPTIONS_VIDEO_LAYERS_BG1:
  case ID_OPTIONS_VIDEO_LAYERS_BG2:
  case ID_OPTIONS_VIDEO_LAYERS_BG3:
  case ID_OPTIONS_VIDEO_LAYERS_WIN1:
  case ID_OPTIONS_VIDEO_LAYERS_OBJWIN:
    pCmdUI->Enable(theApp.cartridgeType == IMAGE_GBA);
    break;
  }
}


void MainWnd::OnOptionsVideoRendermethodDirect3d()
{
#ifndef NO_D3D
  theApp.renderMethod = DIRECT_3D;
  theApp.updateRenderMethod(false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
#endif
}

void MainWnd::OnUpdateOptionsVideoRendermethodDirect3d(CCmdUI* pCmdUI)
{
#ifndef NO_D3D
	pCmdUI->SetCheck(theApp.renderMethod == DIRECT_3D);
#else
	pCmdUI->Enable( FALSE );
#endif
}

void MainWnd::OnOptionsVideoRendermethodOpengl()
{
#ifndef NO_OGL
  theApp.renderMethod = OPENGL;
  theApp.updateRenderMethod(false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
#endif
}

void MainWnd::OnUpdateOptionsVideoRendermethodOpengl(CCmdUI* pCmdUI)
{
#ifndef NO_OGL
	pCmdUI->SetCheck(theApp.renderMethod == OPENGL);
#else
	pCmdUI->Enable( FALSE );
#endif
}


void MainWnd::OnOptionsVideoTriplebuffering()
{
	theApp.tripleBuffering = !theApp.tripleBuffering;
	if( theApp.display ) {
		theApp.display->setOption( _T("tripleBuffering"), theApp.tripleBuffering );
	}
}


void MainWnd::OnUpdateOptionsVideoTriplebuffering(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(theApp.tripleBuffering);
}


void MainWnd::OnOptionsVideoRenderoptionsD3dnofilter()
{
#ifndef NO_D3D
	theApp.d3dFilter = 0;
	if( theApp.display ) {
		theApp.display->setOption( _T("d3dFilter"), theApp.d3dFilter );
	}
#endif
}

void MainWnd::OnUpdateOptionsVideoRenderoptionsD3dnofilter(CCmdUI* pCmdUI)
{
#ifndef NO_D3D
	pCmdUI->SetCheck(theApp.d3dFilter == 0);
#else
	pCmdUI->Enable( FALSE );
#endif
}


void MainWnd::OnOptionsVideoRenderoptionsD3dbilinear()
{
#ifndef NO_D3D
	theApp.d3dFilter = 1;
	if( theApp.display ) {
		theApp.display->setOption( _T("d3dFilter"), theApp.d3dFilter );
	}
#endif
}


void MainWnd::OnUpdateOptionsVideoRenderoptionsD3dbilinear(CCmdUI* pCmdUI)
{
#ifndef NO_D3D
	pCmdUI->SetCheck(theApp.d3dFilter == 1);
#else
	pCmdUI->Enable( FALSE );
#endif
}


void MainWnd::OnOptionsVideoRenderoptionsGlnearest()
{
#ifndef NO_OGL
	theApp.glFilter = 0;
	if( theApp.display ) {
		theApp.display->setOption( _T("glFilter"), theApp.glFilter );
	}
#endif
}


void MainWnd::OnUpdateOptionsVideoRenderoptionsGlnearest(CCmdUI* pCmdUI)
{
#ifndef NO_OGL
	pCmdUI->SetCheck(theApp.glFilter == 0);
#else
	pCmdUI->Enable( FALSE );
#endif
}


void MainWnd::OnOptionsVideoRenderoptionsGlbilinear()
{
#ifndef NO_OGL
	theApp.glFilter = 1;
	if( theApp.display ) {
		theApp.display->setOption( _T("glFilter"), theApp.glFilter );
	}
#endif
}


void MainWnd::OnUpdateOptionsVideoRenderoptionsGlbilinear(CCmdUI* pCmdUI)
{
#ifndef NO_OGL
	pCmdUI->SetCheck(theApp.glFilter == 1);
#else
	pCmdUI->Enable( FALSE );
#endif
}


void MainWnd::OnOptionsEmulatorAssociate()
{
  Associate dlg;
  dlg.DoModal();
}

void MainWnd::OnOptionsEmulatorDirectories()
{
  Directories dlg;
  dlg.DoModal();
}

void MainWnd::OnOptionsEmulatorDisablestatusmessages()
{
  theApp.disableStatusMessage = !theApp.disableStatusMessage;
}

void MainWnd::OnUpdateOptionsEmulatorDisablestatusmessages(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.disableStatusMessage);
}

void MainWnd::OnOptionsEmulatorSynchronize()
{
  synchronize = !synchronize;
  if (synchronize)
	  theApp.throttle = false;
}

void MainWnd::OnUpdateOptionsEmulatorSynchronize(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(synchronize);
}

void MainWnd::OnOptionsEmulatorPausewheninactive()
{
  theApp.pauseWhenInactive = !theApp.pauseWhenInactive;
}

void MainWnd::OnUpdateOptionsEmulatorPausewheninactive(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.pauseWhenInactive);
}

void MainWnd::OnOptionsEmulatorSpeeduptoggle()
{
  theApp.speedupToggle = !theApp.speedupToggle;
}

void MainWnd::OnUpdateOptionsEmulatorSpeeduptoggle(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.speedupToggle);
}

void MainWnd::OnOptionsEmulatorAutomaticallyApplyPatchFiles()
{
  theApp.autoPatch = !theApp.autoPatch;
}

void MainWnd::OnUpdateOptionsEmulatorAutomaticallyipspatch(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.autoPatch);
}

void MainWnd::OnOptionsEmulatorAgbprint()
{
  agbPrintEnable(!agbPrintIsEnabled());
}

void MainWnd::OnUpdateOptionsEmulatorAgbprint(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(agbPrintIsEnabled());
}

void MainWnd::OnOptionsEmulatorRealtimeclock()
{
  theApp.winRtcEnable = !theApp.winRtcEnable;
}

void MainWnd::OnUpdateOptionsEmulatorRealtimeclock(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winRtcEnable);
}

void MainWnd::OnOptionsEmulatorRewindinterval()
{
	RewindInterval dlg(theApp.rewindTimer/6);
	int v = (int)dlg.DoModal();

	if( v >= 0 ) {
		theApp.rewindTimer = v*6; // convert to a multiple of 10 frames
		regSetDwordValue("rewindTimer", v);
		if(v == 0) {
			if(theApp.rewindMemory) {
				free(theApp.rewindMemory);
				theApp.rewindMemory = NULL;
			}
			theApp.rewindCount = 0;
			theApp.rewindCounter = 0;
			theApp.rewindSaveNeeded = false;
		} else {
			if(theApp.rewindMemory == NULL) {
				theApp.rewindMemory = (char *)malloc(8*REWIND_SIZE);
			}
		}
	}
}


BOOL MainWnd::OnOptionsEmulatorShowSpeed(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_EMULATOR_SHOWSPEED_NONE:
    theApp.showSpeed = 0;
    systemSetTitle(VBA_NAME_AND_SUBVERSION);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_PERCENTAGE:
    theApp.showSpeed = 1;
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_DETAILED:
    theApp.showSpeed = 2;
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_TRANSPARENT:
    theApp.showSpeedTransparent = !theApp.showSpeedTransparent;
    break;
  default:
    return FALSE;
  }
  return TRUE;
}

void MainWnd::OnUpdateOptionsEmulatorShowSpeed(CCmdUI *pCmdUI)
{
  switch(pCmdUI->m_nID) {
  case ID_OPTIONS_EMULATOR_SHOWSPEED_NONE:
    pCmdUI->SetCheck(theApp.showSpeed == 0);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_PERCENTAGE:
    pCmdUI->SetCheck(theApp.showSpeed == 1);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_DETAILED:
    pCmdUI->SetCheck(theApp.showSpeed == 2);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_TRANSPARENT:
    pCmdUI->SetCheck(theApp.showSpeedTransparent);
    break;
  }
}

void MainWnd::OnOptionsEmulatorSavetypeAutomatic()
{
  theApp.winSaveType = 0;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeAutomatic(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winSaveType == 0);
}

void MainWnd::OnOptionsEmulatorSavetypeEeprom()
{
  theApp.winSaveType = 1;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeEeprom(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winSaveType == 1);
}

void MainWnd::OnOptionsEmulatorSavetypeSram()
{
  theApp.winSaveType = 2;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeSram(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winSaveType == 2);
}

void MainWnd::OnOptionsEmulatorSavetypeFlash()
{
  theApp.winSaveType = 3;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeFlash(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winSaveType == 3);
}

void MainWnd::OnOptionsEmulatorSavetypeEepromsensor()
{
  theApp.winSaveType = 4;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeEepromsensor(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winSaveType == 4);
}

void MainWnd::OnOptionsEmulatorSavetypeNone()
{
  theApp.winSaveType = 5;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeNone(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winSaveType == 5);
}

void MainWnd::OnOptionsEmulatorSavetypeFlash512k()
{
  flashSetSize(0x10000);
  theApp.winFlashSize = 0x10000;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeFlash512k(CCmdUI* pCmdUI)
{
  // changed theApp.winFlashSize to flashSize to reflect the actual
  // flashsize value used by the emu (it can change upon battery loading)
  pCmdUI->SetCheck(flashSize == 0x10000);
}

void MainWnd::OnOptionsEmulatorSavetypeFlash1m()
{
  flashSetSize(0x20000);
  theApp.winFlashSize = 0x20000;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeFlash1m(CCmdUI* pCmdUI)
{
  // changed theApp.winFlashSize to flashSize to reflect the actual
  // flashsize value used by the emu (it can change upon battery loading)
  pCmdUI->SetCheck(flashSize == 0x20000);
}

void MainWnd::OnOptionsEmulatorSavetypeDetectNow()
{
    if( theApp.cartridgeType != IMAGE_GBA ) return;
    const int address_max = theApp.romSize - 10;
    char temp[11]; temp[10] = '\0';
    CString answer( _T( "This cartridge has probably no backup media." ) );

    const u32 EEPR = 'E' | ( 'E' << 8 ) | ( 'P' << 16 ) | ( 'R' << 24 );
    const u32 SRAM = 'S' | ( 'R' << 8 ) | ( 'A' << 16 ) | ( 'M' << 24 );
    const u32 FLAS = 'F' | ( 'L' << 8 ) | ( 'A' << 16 ) | ( 'S' << 24 );

    for( int address = 0; address < address_max; address += 4 ) {
        const u32 check = *((u32*)&rom[address]);

        if( EEPR == check ) {
            memcpy( temp, &rom[address], 10 );
            if( 0 == strncmp( temp, "EEPROM_V", 8 ) ) {
                answer = _T( "This cartridge uses EEPROM." );
                break;
            }
        }

        if( SRAM == check ) {
            memcpy( temp, &rom[address], 10 );
            if( ( 0 == strncmp( temp, "SRAM_V", 6 ) ) || ( 0 == strncmp( temp, "SRAM_F_V", 8 ) ) ) {
                answer = _T( "This cartridge uses SRAM." );
                break;
            }
        }

        if( FLAS == check ) {
            memcpy( temp, &rom[address], 10 );
            if( ( 0 == strncmp( temp, "FLASH_V", 7 ) ) || ( 0 == strncmp( temp, "FLASH512_V", 10 ) ) ) {
                answer = _T( "This cartridge uses FLASH (64 KiB)." );
                break;
            }
            if( 0 == strncmp( temp, "FLASH1M_V", 9 ) ) {
                answer = _T( "This cartridge uses FLASH (128 KiB)." );
                break;
            }
        }
    }

    MessageBox( answer );
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeDetectNow(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnOptionsEmulatorPngformat()
{
  theApp.captureFormat = 0;
}

void MainWnd::OnUpdateOptionsEmulatorPngformat(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.captureFormat == 0);
}

void MainWnd::OnOptionsEmulatorBmpformat()
{
  theApp.captureFormat = 1;
}

void MainWnd::OnUpdateOptionsEmulatorBmpformat(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.captureFormat == 1);
}

void MainWnd::OnAudioCoreSettings()
{
	AudioCoreSettingsDlg dlg;

	dlg.m_enabled = gb_effects_config.enabled;
	dlg.m_surround = gb_effects_config.surround;
	dlg.m_echo = gb_effects_config.echo;
	dlg.m_stereo = gb_effects_config.stereo;
	dlg.m_volume = soundGetVolume();
	dlg.m_declicking = gbSoundGetDeclicking();
	dlg.m_sound_interpolation = soundInterpolation;
	dlg.m_sound_filtering = soundFiltering;
	dlg.m_sample_rate = soundGetSampleRate();

	if( IDOK == dlg.DoModal() ) {
		gb_effects_config_t _new;

		_new.enabled = dlg.m_enabled;
		_new.surround = dlg.m_surround;
		_new.echo = dlg.m_echo;
		_new.stereo = dlg.m_stereo;

		gbSoundConfigEffects( _new );

		soundSetVolume( dlg.m_volume );

		gbSoundSetDeclicking( dlg.m_declicking );

		soundInterpolation = dlg.m_sound_interpolation;

		soundFiltering = dlg.m_sound_filtering;
		
		if( theApp.cartridgeType == IMAGE_GBA ) {
			soundSetSampleRate( dlg.m_sample_rate );
		} else if( theApp.cartridgeType == IMAGE_GB ) {
			gbSoundSetSampleRate( dlg.m_sample_rate );
		}
	}
}

void MainWnd::OnUpdateAudioCoreSettings(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( ( !theApp.aviRecording && !theApp.soundRecording ) ? TRUE : FALSE );
}

void MainWnd::updateSoundChannels(UINT id)
{
  int flag = 0;

  if(id == ID_OPTIONS_SOUND_CHANNEL1)
    flag = 1;

  if(id == ID_OPTIONS_SOUND_CHANNEL2)
    flag = 2;

  if(id == ID_OPTIONS_SOUND_CHANNEL3)
    flag = 4;

  if(id == ID_OPTIONS_SOUND_CHANNEL4)
    flag = 8;

  if(id == ID_OPTIONS_SOUND_DIRECTSOUNDA)
    flag = 256;

  if(id == ID_OPTIONS_SOUND_DIRECTSOUNDB)
    flag = 512;

  int active = soundGetEnable() & 0x30f;

  if(active & flag)
    active &= (~flag);
  else
    active |= flag;

  soundSetEnable(active);
}

void MainWnd::OnOptionsSoundChannel1()
{
  updateSoundChannels(ID_OPTIONS_SOUND_CHANNEL1);
}

void MainWnd::OnUpdateOptionsSoundChannel1(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(soundGetEnable() & 1);
}

void MainWnd::OnOptionsSoundChannel2()
{
  updateSoundChannels(ID_OPTIONS_SOUND_CHANNEL2);
}

void MainWnd::OnUpdateOptionsSoundChannel2(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(soundGetEnable() & 2);
}

void MainWnd::OnOptionsSoundChannel3()
{
  updateSoundChannels(ID_OPTIONS_SOUND_CHANNEL3);
}

void MainWnd::OnUpdateOptionsSoundChannel3(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(soundGetEnable() & 4);
}

void MainWnd::OnOptionsSoundChannel4()
{
  updateSoundChannels(ID_OPTIONS_SOUND_CHANNEL4);
}

void MainWnd::OnUpdateOptionsSoundChannel4(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(soundGetEnable() & 8);
}

void MainWnd::OnOptionsSoundDirectsounda()
{
  updateSoundChannels(ID_OPTIONS_SOUND_DIRECTSOUNDA);
}

void MainWnd::OnUpdateOptionsSoundDirectsounda(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(soundGetEnable() & 256);
  pCmdUI->Enable(theApp.cartridgeType == 0);
}

void MainWnd::OnOptionsSoundDirectsoundb()
{
  updateSoundChannels(ID_OPTIONS_SOUND_DIRECTSOUNDB);
}

void MainWnd::OnUpdateOptionsSoundDirectsoundb(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(soundGetEnable() & 512);
  pCmdUI->Enable(theApp.cartridgeType == 0);
}

void MainWnd::OnOptionsGameboyBorder()
{
  theApp.winGbBorderOn = !theApp.winGbBorderOn;
  gbBorderOn = theApp.winGbBorderOn;
  if(emulating && theApp.cartridgeType == 1 && gbBorderOn) {
    gbSgbRenderBorder();
  }
  theApp.updateWindowSize(theApp.videoOption);
}

void MainWnd::OnUpdateOptionsGameboyBorder(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.winGbBorderOn);
}

void MainWnd::OnOptionsGameboyPrinter()
{
  theApp.winGbPrinterEnabled = !theApp.winGbPrinterEnabled;
  if(theApp.winGbPrinterEnabled)
    gbSerialFunction = gbPrinterSend;
  else
    gbSerialFunction = NULL;
}

void MainWnd::OnUpdateOptionsGameboyPrinter(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbSerialFunction == gbPrinterSend);
}

void MainWnd::OnOptionsGameboyBorderAutomatic()
{
  gbBorderAutomatic = !gbBorderAutomatic;
  if(emulating && theApp.cartridgeType == 1 && gbBorderOn) {
    gbSgbRenderBorder();
    theApp.updateWindowSize(theApp.videoOption);
  }
}

void MainWnd::OnUpdateOptionsGameboyBorderAutomatic(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbBorderAutomatic);
}

void MainWnd::OnOptionsGameboyAutomatic()
{
  gbEmulatorType = 0;
}

void MainWnd::OnUpdateOptionsGameboyAutomatic(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbEmulatorType == 0);
}

void MainWnd::OnOptionsGameboyGba()
{
  gbEmulatorType = 4;
}

void MainWnd::OnUpdateOptionsGameboyGba(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbEmulatorType == 4);
}

void MainWnd::OnOptionsGameboyCgb()
{
  gbEmulatorType = 1;
}

void MainWnd::OnUpdateOptionsGameboyCgb(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbEmulatorType == 1);
}

void MainWnd::OnOptionsGameboySgb()
{
  gbEmulatorType = 2;
}

void MainWnd::OnUpdateOptionsGameboySgb(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbEmulatorType == 2);
}

void MainWnd::OnOptionsGameboySgb2()
{
  gbEmulatorType = 5;
}

void MainWnd::OnUpdateOptionsGameboySgb2(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbEmulatorType == 5);
}

void MainWnd::OnOptionsGameboyGb()
{
  gbEmulatorType = 3;
}

void MainWnd::OnUpdateOptionsGameboyGb(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbEmulatorType == 3);
}

void MainWnd::OnOptionsGameboyRealcolors()
{
  gbColorOption = 0;
  utilUpdateSystemColorMaps(theApp.cartridgeType == IMAGE_GBA && gbColorOption == 1);
}

void MainWnd::OnUpdateOptionsGameboyRealcolors(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbColorOption == 0);
}

void MainWnd::OnOptionsGameboyGameboycolors()
{
  gbColorOption = 1;
  utilUpdateSystemColorMaps(theApp.cartridgeType == IMAGE_GBA && gbColorOption == 1);
}

void MainWnd::OnUpdateOptionsGameboyGameboycolors(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(gbColorOption == 1);
}


void MainWnd::OnOptionsGameboyColors()
{
  GBColorDlg dlg;
  if(dlg.DoModal()) {
    gbPaletteOption = dlg.getWhich();
    memcpy(systemGbPalette, dlg.getColors(), 24*sizeof(u16));
    if(emulating && theApp.cartridgeType == 1) {
      memcpy(gbPalette, &systemGbPalette[dlg.getWhich()*8], 8*sizeof(u16));
    }
  }
}

BOOL MainWnd::OnOptionsPriority(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_PRIORITY_HIGHEST:
    theApp.threadPriority = 0;
    break;
  case ID_OPTIONS_PRIORITY_ABOVENORMAL:
    theApp.threadPriority = 1;
    break;
  case ID_OPTIONS_PRIORITY_NORMAL:
    theApp.threadPriority = 2;
    break;
  case ID_OPTIONS_PRIORITY_BELOWNORMAL:
    theApp.threadPriority = 3;
    break;
  default:
    return FALSE;
  }
  theApp.updatePriority();

  return TRUE;
}

void MainWnd::OnUpdateOptionsPriority(CCmdUI *pCmdUI)
{
  switch(pCmdUI->m_nID) {
  case ID_OPTIONS_PRIORITY_HIGHEST:
    pCmdUI->SetCheck(theApp.threadPriority == 0);
    break;
  case ID_OPTIONS_PRIORITY_ABOVENORMAL:
    pCmdUI->SetCheck(theApp.threadPriority == 1);
    break;
  case ID_OPTIONS_PRIORITY_NORMAL:
    pCmdUI->SetCheck(theApp.threadPriority == 2);
    break;
  case ID_OPTIONS_PRIORITY_BELOWNORMAL:
    pCmdUI->SetCheck(theApp.threadPriority == 3);
    break;
  }
}

BOOL MainWnd::OnOptionsFilter(UINT nID)
{
	switch(nID)
	{
	case ID_OPTIONS_FILTER_NORMAL:
		theApp.filterType = FILTER_NONE;
		break;
	case ID_OPTIONS_FILTER_TVMODE:
		theApp.filterType = FILTER_TVMODE;
		break;
	case ID_OPTIONS_FILTER_PLUGIN:
		theApp.filterType = FILTER_PLUGIN;
		if( strcmp( theApp.pluginName, "" ) == 0 ) {
			// open select plugin dialog when none selected
			OnOptionsSelectPlugin();
		}
		break;
	case ID_OPTIONS_FILTER_2XSAI:
		theApp.filterType = FILTER_2XSAI;
		break;
	case ID_OPTIONS_FILTER_SUPER2XSAI:
		theApp.filterType = FILTER_SUPER2XSAI;
		break;
	case ID_OPTIONS_FILTER_SUPEREAGLE:
		theApp.filterType = FILTER_SUPEREAGLE;
		break;
	case ID_OPTIONS_FILTER16BIT_PIXELATEEXPERIMENTAL:
		theApp.filterType = FILTER_PIXELATE;
		break;
	case ID_OPTIONS_FILTER16BIT_ADVANCEMAMESCALE2X:
		theApp.filterType = FILTER_MAMESCALE2X;
		break;
	case ID_OPTIONS_FILTER16BIT_SIMPLE2X:
		theApp.filterType = FILTER_SIMPLE2X;
		break;
	case ID_OPTIONS_FILTER_BILINEAR:
		theApp.filterType = FILTER_BILINEAR;
		break;
	case ID_OPTIONS_FILTER_BILINEARPLUS:
		theApp.filterType = FILTER_BILINEARPLUS;
		break;
	case ID_OPTIONS_FILTER_SCANLINES:
		theApp.filterType = FILTER_SCANLINES;
		break;
	case ID_OPTIONS_FILTER_HQ2X:
		theApp.filterType = FILTER_HQ2X;
		break;
	case ID_OPTIONS_FILTER_LQ2X:
		theApp.filterType = FILTER_LQ2X;
		break;
	case ID_OPTIONS_FILTER_SIMPLE3X:
		theApp.filterType = FILTER_SIMPLE3X;
		break;
	case ID_OPTIONS_FILTER_SIMPLE4X:
		theApp.filterType = FILTER_SIMPLE4X;
		break;
	case ID_OPTIONS_FILTER_HQ3X:
		theApp.filterType = FILTER_HQ3X;
		break;
	case ID_OPTIONS_FILTER_HQ4X:
		theApp.filterType = FILTER_HQ4X;
		break;
	default:
		return FALSE;
	}
	theApp.updateFilter();
	return TRUE;
}

void MainWnd::OnUpdateOptionsFilter(CCmdUI *pCmdUI)
{
  pCmdUI->Enable( systemColorDepth == 16 || systemColorDepth == 32 );

  switch(pCmdUI->m_nID) {
  case ID_OPTIONS_FILTER_NORMAL:
    pCmdUI->SetCheck(theApp.filterType == FILTER_NONE);
    break;
  case ID_OPTIONS_FILTER_TVMODE:
    pCmdUI->SetCheck(theApp.filterType == FILTER_TVMODE);
    break;
  case ID_OPTIONS_FILTER_2XSAI:
    pCmdUI->SetCheck(theApp.filterType == FILTER_2XSAI);
    break;
  case ID_OPTIONS_FILTER_SUPER2XSAI:
    pCmdUI->SetCheck(theApp.filterType == FILTER_SUPER2XSAI);
    break;
  case ID_OPTIONS_FILTER_PLUGIN:
    pCmdUI->Enable( !theApp.filterMT );
    pCmdUI->SetCheck(theApp.filterType == FILTER_PLUGIN);
    break;
  case ID_OPTIONS_FILTER_SUPEREAGLE:
    pCmdUI->SetCheck(theApp.filterType == FILTER_SUPEREAGLE);
    break;
  case ID_OPTIONS_FILTER16BIT_PIXELATEEXPERIMENTAL:
    pCmdUI->SetCheck(theApp.filterType == FILTER_PIXELATE);
    break;
  case ID_OPTIONS_FILTER16BIT_ADVANCEMAMESCALE2X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_MAMESCALE2X);
    break;
  case ID_OPTIONS_FILTER16BIT_SIMPLE2X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_SIMPLE2X);
    break;
  case ID_OPTIONS_FILTER_BILINEAR:
    pCmdUI->SetCheck(theApp.filterType == FILTER_BILINEAR);
    break;
  case ID_OPTIONS_FILTER_BILINEARPLUS:
    pCmdUI->SetCheck(theApp.filterType == FILTER_BILINEARPLUS);
    break;
  case ID_OPTIONS_FILTER_SCANLINES:
    pCmdUI->SetCheck(theApp.filterType == FILTER_SCANLINES);
    break;
  case ID_OPTIONS_FILTER_HQ2X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_HQ2X);
    break;
  case ID_OPTIONS_FILTER_LQ2X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_LQ2X);
    break;
  case ID_OPTIONS_FILTER_SIMPLE3X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_SIMPLE3X);
    break;
  case ID_OPTIONS_FILTER_SIMPLE4X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_SIMPLE4X);
    break;
  case ID_OPTIONS_FILTER_HQ3X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_HQ3X);
    break;
  case ID_OPTIONS_FILTER_HQ4X:
    pCmdUI->SetCheck(theApp.filterType == FILTER_HQ4X);
    break;
  }
}

void MainWnd::OnUpdateOptionsSelectPlugin(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( !theApp.filterMT );
}

BOOL MainWnd::OnOptionsFilterIFB(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_NONE:
    theApp.ifbType = 0;
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_MOTIONBLUR:
    theApp.ifbType = 1;
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_SMART:
    theApp.ifbType = 2;
    break;
  default:
    return FALSE;
  }
  theApp.updateIFB();
  return TRUE;
}

void MainWnd::OnUpdateOptionsFilterIFB(CCmdUI *pCmdUI)
{
  switch(pCmdUI->m_nID) {
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_NONE:
    pCmdUI->SetCheck(theApp.ifbType == 0);
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_MOTIONBLUR:
    pCmdUI->SetCheck(theApp.ifbType == 1);
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_SMART:
    pCmdUI->SetCheck(theApp.ifbType == 2);
    break;
  }
}

void MainWnd::OnOptionsFilterDisablemmx()
{
#ifdef MMX
  theApp.disableMMX = !theApp.disableMMX;
  if(!theApp.disableMMX)
    cpu_mmx = theApp.detectMMX();
  else
    cpu_mmx = 0;
#endif
}

void MainWnd::OnUpdateOptionsFilterDisablemmx(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.disableMMX);
}

void MainWnd::OnOptionsLanguageSystem()
{
  theApp.winSetLanguageOption(0, false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}

void MainWnd::OnUpdateOptionsLanguageSystem(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.languageOption == 0);
}

void MainWnd::OnOptionsLanguageEnglish()
{
  theApp.winSetLanguageOption(1, false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}

void MainWnd::OnUpdateOptionsLanguageEnglish(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.languageOption == 1);
}

void MainWnd::OnOptionsLanguageOther()
{
  theApp.winSetLanguageOption(2, false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}

void MainWnd::OnUpdateOptionsLanguageOther(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(theApp.languageOption == 2);
}


void MainWnd::OnOptionsJoypadConfigure1()
{
  JoypadConfig dlg(0);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure1(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(theApp.videoOption != VIDEO_320x240);
}

void MainWnd::OnOptionsJoypadConfigure2()
{
  JoypadConfig dlg(1);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure2(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(theApp.videoOption != VIDEO_320x240);
}

void MainWnd::OnOptionsJoypadConfigure3()
{
  JoypadConfig dlg(2);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure3(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(theApp.videoOption != VIDEO_320x240);
}

void MainWnd::OnOptionsJoypadConfigure4()
{
  JoypadConfig dlg(3);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure4(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(theApp.videoOption != VIDEO_320x240);
}

BOOL MainWnd::OnOptionsJoypadDefault(UINT nID)
{
  theApp.joypadDefault = nID - ID_OPTIONS_JOYPAD_DEFAULTJOYPAD_1;
  return TRUE;
}

void MainWnd::OnUpdateOptionsJoypadDefault(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(theApp.joypadDefault == (int)(pCmdUI->m_nID - ID_OPTIONS_JOYPAD_DEFAULTJOYPAD_1));
}

void MainWnd::OnOptionsJoypadMotionconfigure()
{
  MotionConfig dlg;
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadMotionconfigure(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(theApp.videoOption != VIDEO_320x240);
}

BOOL MainWnd::OnOptionsJoypadAutofire(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_JOYPAD_AUTOFIRE_A:
    if(theApp.autoFire & 1) {
      theApp.autoFire &= ~1;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_A_DISABLED));
    } else {
      theApp.autoFire |= 1;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_A));
    }
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_B:
    if(theApp.autoFire & 2) {
      theApp.autoFire &= ~2;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_B_DISABLED));
    } else {
      theApp.autoFire |= 2;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_B));
    }
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_L:
    if(theApp.autoFire & 512) {
      theApp.autoFire &= ~512;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_L_DISABLED));
    } else {
      theApp.autoFire |= 512;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_L));
    }
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_R:
    if(theApp.autoFire & 256) {
      theApp.autoFire &= ~256;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_R_DISABLED));
    } else {
      theApp.autoFire |= 256;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_R));
    }
    break;
  default:
    return FALSE;
  }
  return TRUE;
}

void MainWnd::OnUpdateOptionsJoypadAutofire(CCmdUI *pCmdUI)
{
  bool check = true;
  switch(pCmdUI->m_nID) {
  case ID_OPTIONS_JOYPAD_AUTOFIRE_A:
    check = (theApp.autoFire & 1) != 0;
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_B:
    check = (theApp.autoFire & 2) != 0;
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_L:
    check = (theApp.autoFire & 512) != 0;
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_R:
    check = (theApp.autoFire & 256) != 0;
    break;
  }
  pCmdUI->SetCheck(check);
}

void MainWnd::OnOptionsVideoFullscreenmaxscale()
{
  MaxScale dlg;

  dlg.DoModal();

  if( theApp.display ) {
	  theApp.display->setOption( _T("maxScale"), theApp.maxScale );
  }
}

#ifndef NO_LINK
void MainWnd::OnLinkOptions()
{
	LinkOptions dlg;

	dlg.DoModal();
}

void MainWnd::OnOptionsLinkRFU()
{
	if(rfu_enabled) rfu_enabled = false;
	else {
		rfu_enabled = true;
		MessageBox("Please note this is the first version\nof RFU emulation code and it's not 100% bug free.\nAlso only 2 players single computer are supported at this time.", "Warning", MB_OK);
	}
}

void MainWnd::OnUpdateOptionsLinkEnable(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(gba_link_enabled);
}

void MainWnd::OnOptionsLinkEnable()
{
	gba_link_enabled = !gba_link_enabled;
}

void MainWnd::OnUpdateOptionsLinkRFU(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(rfu_enabled);
}

void MainWnd::OnOptionsJoybus()
{
	JoybusOptions dlg;
	dlg.DoModal();
}
#else
void MainWnd::OnLinkOptions() { }
void MainWnd::OnOptionsLinkRFU() { }
void MainWnd::OnUpdateOptionsLinkEnable(CCmdUI*) { }
void MainWnd::OnOptionsLinkEnable() { }
void MainWnd::OnUpdateOptionsLinkRFU(CCmdUI*) { }
void MainWnd::OnOptionsJoybus() { }
#endif

void MainWnd::OnOptionsEmulatorGameoverrides()
{
  if(emulating && theApp.cartridgeType == 0) {
    GameOverrides dlg(this);
    dlg.DoModal();
  }
}

void MainWnd::OnUpdateOptionsEmulatorGameoverrides(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating && (theApp.cartridgeType == 0));
}

void MainWnd::OnOptionsSoundHardwareacceleration()
{
  theApp.dsoundDisableHardwareAcceleration = !theApp.dsoundDisableHardwareAcceleration;
  soundShutdown();
  soundInit();
}

void MainWnd::OnUpdateOptionsSoundHardwareacceleration(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(!theApp.dsoundDisableHardwareAcceleration);
}

void MainWnd::OnOptionsSelectPlugin()
{
  SelectPlugin dlg;

  if (dlg.DoModal() == IDOK)
  {
	theApp.filterType = FILTER_PLUGIN;
	theApp.updateFilter();
  }
}


void MainWnd::OnOutputapiDirectsound()
{
	if( theApp.audioAPI != DIRECTSOUND ) {
		theApp.audioAPI = DIRECTSOUND;
		soundShutdown();
		soundInit();
	}
}

void MainWnd::OnUpdateOutputapiDirectsound(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck( ( theApp.audioAPI == DIRECTSOUND ) ? 1 : 0 );
	pCmdUI->Enable(!theApp.aviRecording && !theApp.soundRecording);
}

void MainWnd::OnOutputapiXaudio2()
{
#ifndef NO_XAUDIO2
	if( theApp.audioAPI != XAUDIO2 ) {
		theApp.audioAPI = XAUDIO2;
		soundShutdown();
		soundInit();
	}
#endif
}

void MainWnd::OnUpdateOutputapiXaudio2(CCmdUI *pCmdUI)
{
#ifndef NO_XAUDIO2
	pCmdUI->SetCheck( ( theApp.audioAPI == XAUDIO2 ) ? 1 : 0 );
	pCmdUI->Enable(!theApp.aviRecording && !theApp.soundRecording);
#else
	pCmdUI->Enable( FALSE );
#endif
}

void MainWnd::OnOutputapiOpenal()
{
#ifndef NO_OAL
	if( theApp.audioAPI != OPENAL_SOUND ) {
		theApp.audioAPI = OPENAL_SOUND;
		soundShutdown();
		soundInit();
	}
#endif
}

void MainWnd::OnUpdateOutputapiOpenal(CCmdUI *pCmdUI)
{
#ifndef NO_OAL
	pCmdUI->SetCheck( ( theApp.audioAPI == OPENAL_SOUND ) ? 1 : 0 );
	pCmdUI->Enable(!theApp.aviRecording && !theApp.soundRecording);
#else
	pCmdUI->Enable( FALSE );
#endif
}

void MainWnd::OnOutputapiOalconfiguration()
{
#ifndef NO_OAL
	OALConfig dlg;

	dlg.selectedDevice = theApp.oalDevice;
	dlg.bufferCount = theApp.oalBufferCount;

	if( dlg.DoModal() == IDOK ) {
		soundShutdown();
		// do this before changing any values OpenAL
		// might need for successful cleanup

		if( theApp.oalDevice ) {
			free( theApp.oalDevice );
			theApp.oalDevice = NULL;
		}

		theApp.oalDevice = (TCHAR*)malloc( (dlg.selectedDevice.GetLength() + 1) * sizeof( TCHAR ) );
		_tcscpy( theApp.oalDevice, dlg.selectedDevice.GetBuffer() );
		theApp.oalBufferCount = dlg.bufferCount;

		soundInit();
	}
#endif
}

void MainWnd::OnUpdateOutputapiOalconfiguration(CCmdUI *pCmdUI)
{
#ifndef NO_OAL
	pCmdUI->Enable(!theApp.aviRecording && !theApp.soundRecording);
#else
	pCmdUI->Enable( FALSE );
#endif
}

void MainWnd::OnOutputapiXaudio2config()
{
#ifndef NO_XAUDIO2
	XAudio2_Config dlg;

	dlg.m_selected_device_index = theApp.xa2Device;
	dlg.m_buffer_count = theApp.xa2BufferCount;
	dlg.m_enable_upmixing = theApp.xa2Upmixing;

	if( dlg.DoModal() == IDOK ) {
		soundShutdown();

		theApp.xa2Device = dlg.m_selected_device_index;
		theApp.xa2BufferCount = dlg.m_buffer_count;
		theApp.xa2Upmixing = dlg.m_enable_upmixing;

		soundInit();
	}
#endif
}

void MainWnd::OnUpdateOutputapiXaudio2config(CCmdUI *pCmdUI)
{
#ifndef NO_XAUDIO2
	pCmdUI->Enable(!theApp.aviRecording && !theApp.soundRecording);
#else
	pCmdUI->Enable( FALSE );
#endif
}

void MainWnd::OnRenderapiD3dmotionblur()
{
#ifndef NO_D3D
	theApp.d3dMotionBlur = !theApp.d3dMotionBlur;
	if( theApp.display ) {
		theApp.display->setOption( _T("motionBlur"), theApp.d3dMotionBlur ? 1 : 0 );
	}
#endif
}

void MainWnd::OnUpdateRenderapiD3dmotionblur(CCmdUI *pCmdUI)
{
#ifndef NO_D3D
	pCmdUI->SetCheck( theApp.d3dMotionBlur ? 1 : 0 );
#else
	pCmdUI->Enable( FALSE );
#endif
}

void MainWnd::OnEmulatorBiosfiles()
{
	BIOSDialog dlg;
	dlg.m_enableBIOS_GBA = theApp.useBiosFileGBA ? TRUE : FALSE;
	dlg.m_enableBIOS_GBC = theApp.useBiosFileGBC ? TRUE : FALSE;
	dlg.m_enableBIOS_GB = theApp.useBiosFileGB ? TRUE : FALSE;
	dlg.m_skipLogo = theApp.skipBiosFile ? TRUE : FALSE;
	dlg.m_pathGBA = theApp.biosFileNameGBA;
	dlg.m_pathGBC = theApp.biosFileNameGBC;
	dlg.m_pathGB = theApp.biosFileNameGB;

	if( IDOK == dlg.DoModal() ) {
		theApp.useBiosFileGBA = dlg.m_enableBIOS_GBA == TRUE;
		theApp.useBiosFileGBC = dlg.m_enableBIOS_GBC == TRUE;
		theApp.useBiosFileGB = dlg.m_enableBIOS_GB == TRUE;
		theApp.skipBiosFile = dlg.m_skipLogo == TRUE;
		theApp.biosFileNameGBA = dlg.m_pathGBA;
		theApp.biosFileNameGBC = dlg.m_pathGBC;
		theApp.biosFileNameGB = dlg.m_pathGB;
	}
}

void MainWnd::OnPixelfilterMultiThreading()
{
	theApp.filterMT = !theApp.filterMT;
}

void MainWnd::OnUpdatePixelfilterMultiThreading(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( theApp.filterType != FILTER_PLUGIN );
	pCmdUI->SetCheck( theApp.filterMT ? 1 : 0 );
}
