#include "stdafx.h"
#include "VBA.h"
#include "MainWnd.h"

#include "Associate.h"
#include "Directories.h"
#include "FileDlg.h"
#include "GameOverrides.h"
#include "LinkOptions.h"
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
  autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottleNothrottle(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( throttle == 0 );
}


void MainWnd::OnOptionsFrameskipThrottle25()
{
	theApp.updateThrottle( 25 );
  autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle25(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( throttle == 25 );
}


void MainWnd::OnOptionsFrameskipThrottle50()
{
	theApp.updateThrottle( 50 );
  autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle50(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( throttle == 50 );
}


void MainWnd::OnOptionsFrameskipThrottle100()
{
	theApp.updateThrottle( 100 );
  autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle100(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( throttle == 100 );
}


void MainWnd::OnOptionsFrameskipThrottle150()
{
	theApp.updateThrottle( 150 );
autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle150(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( throttle == 150 );
}


void MainWnd::OnOptionsFrameskipThrottle200()
{
	theApp.updateThrottle( 200 );
  autoFrameSkip = false;
}

void MainWnd::OnUpdateOptionsFrameskipThrottle200(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( throttle == 200 );
}


void MainWnd::OnOptionsFrameskipThrottleOther()
{
	Throttle dlg;
	unsigned short v = (unsigned short)dlg.DoModal();

	if( v ) {
		theApp.updateThrottle( v );
		autoFrameSkip = false;
	}
}


void MainWnd::OnUpdateOptionsFrameskipThrottleOther(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(
		( throttle != 0 ) &&
		( throttle != 25 ) &&
		( throttle != 50 ) &&
		( throttle != 100 ) &&
		( throttle != 150 ) &&
		( throttle != 200 ) );
}

void MainWnd::OnOptionsFrameskipAutomatic() 
{
  autoFrameSkip = !autoFrameSkip;
  if(!autoFrameSkip && emulating)
	  theApp.updateFrameSkip();
  else
  {
	  throttle = false;
	  frameSkip = 0;
	  systemFrameSkip = 0;
  }
}

void MainWnd::OnUpdateOptionsFrameskipAutomatic(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(autoFrameSkip);  
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
	vsync = !vsync;
	if( theApp.display ) {
		theApp.display->setOption( _T("vsync"), vsync );
	}
}


void MainWnd::OnUpdateOptionsVideoVsync(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(vsync);
}

void MainWnd::OnUpdateOptionsVideoX1(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_1X);
}

void MainWnd::OnUpdateOptionsVideoX2(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_2X);
}

void MainWnd::OnUpdateOptionsVideoX3(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_3X);
}

void MainWnd::OnUpdateOptionsVideoX4(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_4X);
}

void MainWnd::OnUpdateOptionsVideoX5(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_5X);
}

void MainWnd::OnUpdateOptionsVideoX6(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_6X);
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
	  if( ( mode.width != fsWidth ) ||
		  ( mode.height != fsHeight ) ||
		  ( mode.bitDepth != fsColorDepth ) ||
		  ( mode.frequency != fsFrequency ) ||
		  ( mode.adapter != fsAdapter ) ||
		  ( videoOption != VIDEO_OTHER ) )
	  {
		  fsForceChange = true;
		  fsWidth = mode.width;
		  fsHeight = mode.height;
		  fsFrequency = mode.frequency;
		  fsColorDepth = mode.bitDepth;
		  fsAdapter = mode.adapter;
		  theApp.updateVideoSize( ID_OPTIONS_VIDEO_FULLSCREEN );
	  }
  }
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}


void MainWnd::OnUpdateOptionsVideoFullscreen(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(videoOption == VIDEO_OTHER);
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
	fullScreenStretch = !fullScreenStretch;
	if( theApp.display ) {
		theApp.display->setOption( _T("fullScreenStretch"), fullScreenStretch );
	}
}


void MainWnd::OnUpdateOptionsVideoFullscreenstretchtofit(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(fullScreenStretch);
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
	tripleBuffering = !tripleBuffering;
	if( theApp.display ) {
		theApp.display->setOption( _T("tripleBuffering"), tripleBuffering );
	}
}


void MainWnd::OnUpdateOptionsVideoTriplebuffering(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(tripleBuffering);
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
	glFilter = 0;
	if( theApp.display ) {
		theApp.display->setOption( _T("glFilter"), glFilter );
	}
#endif
}


void MainWnd::OnUpdateOptionsVideoRenderoptionsGlnearest(CCmdUI* pCmdUI)
{
#ifndef NO_OGL
	pCmdUI->SetCheck(glFilter == 0);
#else
	pCmdUI->Enable( FALSE );
#endif
}


void MainWnd::OnOptionsVideoRenderoptionsGlbilinear()
{
#ifndef NO_OGL
	glFilter = 1;
	if( theApp.display ) {
		theApp.display->setOption( _T("glFilter"), glFilter );
	}
#endif
}


void MainWnd::OnUpdateOptionsVideoRenderoptionsGlbilinear(CCmdUI* pCmdUI)
{
#ifndef NO_OGL
	pCmdUI->SetCheck(glFilter == 1);
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
  disableStatusMessages = !disableStatusMessages;
}

void MainWnd::OnUpdateOptionsEmulatorDisablestatusmessages(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(disableStatusMessages);
}

void MainWnd::OnOptionsEmulatorPausewheninactive()
{
  pauseWhenInactive = !pauseWhenInactive;
}

void MainWnd::OnUpdateOptionsEmulatorPausewheninactive(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(pauseWhenInactive);
}

void MainWnd::OnOptionsEmulatorSpeeduptoggle()
{
  speedupToggle = !speedupToggle;
}

void MainWnd::OnUpdateOptionsEmulatorSpeeduptoggle(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(speedupToggle);
}

void MainWnd::OnOptionsEmulatorAutomaticallyApplyPatchFiles()
{
  autoPatch = !autoPatch;
}

void MainWnd::OnUpdateOptionsEmulatorAutomaticallyipspatch(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(autoPatch);
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
  rtcEnabled = !rtcEnabled;
}

void MainWnd::OnUpdateOptionsEmulatorRealtimeclock(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(rtcEnabled);
}

void MainWnd::OnOptionsEmulatorRewindinterval()
{
	RewindInterval dlg(rewindTimer/6);
	int v = (int)dlg.DoModal();

	if( v >= 0 ) {
		rewindTimer = v*6; // convert to a multiple of 10 frames
		regSetDwordValue("rewindTimer", v);
		if(v == 0) {
			if(theApp.rewindMemory) {
				free(theApp.rewindMemory);
				theApp.rewindMemory = NULL;
			}
			rewindCount = 0;
			rewindCounter = 0;
			rewindSaveNeeded = false;
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
    showSpeed = 0;
    systemSetTitle(VBA_NAME_AND_SUBVERSION);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_PERCENTAGE:
    showSpeed = 1;
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_DETAILED:
    showSpeed = 2;
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_TRANSPARENT:
    showSpeedTransparent = !showSpeedTransparent;
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
    pCmdUI->SetCheck(showSpeed == 0);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_PERCENTAGE:
    pCmdUI->SetCheck(showSpeed == 1);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_DETAILED:
    pCmdUI->SetCheck(showSpeed == 2);
    break;
  case ID_OPTIONS_EMULATOR_SHOWSPEED_TRANSPARENT:
    pCmdUI->SetCheck(showSpeedTransparent);
    break;
  }
}

void MainWnd::OnOptionsEmulatorSavetypeAutomatic()
{
  cpuSaveType = 0;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeAutomatic(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuSaveType == 0);
}

void MainWnd::OnOptionsEmulatorSavetypeEeprom()
{
  cpuSaveType = 1;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeEeprom(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuSaveType == 1);
}

void MainWnd::OnOptionsEmulatorSavetypeSram()
{
  cpuSaveType = 2;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeSram(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuSaveType == 2);
}

void MainWnd::OnOptionsEmulatorSavetypeFlash()
{
  cpuSaveType = 3;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeFlash(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuSaveType == 3);
}

void MainWnd::OnOptionsEmulatorSavetypeEepromsensor()
{
  cpuSaveType = 4;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeEepromsensor(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuSaveType == 4);
}

void MainWnd::OnOptionsEmulatorSavetypeNone()
{
  cpuSaveType = 5;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeNone(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(cpuSaveType == 5);
}

void MainWnd::OnOptionsEmulatorSavetypeFlash512k()
{
  flashSetSize(0x10000);
  winFlashSize = 0x10000;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeFlash512k(CCmdUI* pCmdUI)
{
  // changed winFlashSize to flashSize to reflect the actual
  // flashsize value used by the emu (it can change upon battery loading)
  pCmdUI->SetCheck(flashSize == 0x10000);
}

void MainWnd::OnOptionsEmulatorSavetypeFlash1m()
{
  flashSetSize(0x20000);
  winFlashSize = 0x20000;
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeFlash1m(CCmdUI* pCmdUI)
{
  // changed winFlashSize to flashSize to reflect the actual
  // flashsize value used by the emu (it can change upon battery loading)
  pCmdUI->SetCheck(flashSize == 0x20000);
}

void MainWnd::OnOptionsEmulatorSavetypeDetectNow()
{
    if( theApp.cartridgeType != IMAGE_GBA ) return;

    const int address_max = theApp.romSize - 10;
    char temp[11]; temp[10] = '\0';
    CString answer( _T( "This cartridge has probably no backup media." ) );

	utilGBAFindSave(theApp.romSize);
	switch (saveType)
	{
	case 0:
		answer = _T("This cartridge has probably no backup media.");
		break;
	case 1:
		answer = _T( "This cartridge uses SRAM." );
		break;
	case 2:
		if (flashSize == 0x10000)
			answer = _T( "This cartridge uses FLASH (64 KiB)." );
		else if (flashSize = 0x20000)
			answer = _T( "This cartridge uses FLASH (128 KiB)." );
		break;
	case 3:
		answer = _T("This cartridge uses EEPROM.");
		break;
	default:
		answer = _T("This cartridge has probably no backup media.");
		break;
    }

    MessageBox( answer );
}

void MainWnd::OnUpdateOptionsEmulatorSavetypeDetectNow(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnOptionsEmulatorPngformat()
{
  captureFormat = 0;
}

void MainWnd::OnUpdateOptionsEmulatorPngformat(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(captureFormat == 0);
}

void MainWnd::OnOptionsEmulatorBmpformat()
{
  captureFormat = 1;
}

void MainWnd::OnUpdateOptionsEmulatorBmpformat(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(captureFormat == 1);
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
	pCmdUI->Enable( ( !aviRecording && !soundRecording ) ? TRUE : FALSE );
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
  winGbBorderOn = !winGbBorderOn;
  gbBorderOn = winGbBorderOn;
  if(emulating && theApp.cartridgeType == 1 && gbBorderOn) {
    gbSgbRenderBorder();
  }
  theApp.updateWindowSize(videoOption);
}

void MainWnd::OnUpdateOptionsGameboyBorder(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(winGbBorderOn);
}

void MainWnd::OnOptionsGameboyPrinter()
{
  winGbPrinterEnabled = !winGbPrinterEnabled;
  if(winGbPrinterEnabled)
    gbSerialFunction = gbPrinterSend;
  else
#ifndef NO_LINK
	gbSerialFunction = gbStartLink;
#else
	gbSerialFunction = NULL;
#endif
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
    theApp.updateWindowSize(videoOption);
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
    threadPriority = 0;
    break;
  case ID_OPTIONS_PRIORITY_ABOVENORMAL:
    threadPriority = 1;
    break;
  case ID_OPTIONS_PRIORITY_NORMAL:
    threadPriority = 2;
    break;
  case ID_OPTIONS_PRIORITY_BELOWNORMAL:
    threadPriority = 3;
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
    pCmdUI->SetCheck(threadPriority == 0);
    break;
  case ID_OPTIONS_PRIORITY_ABOVENORMAL:
    pCmdUI->SetCheck(threadPriority == 1);
    break;
  case ID_OPTIONS_PRIORITY_NORMAL:
    pCmdUI->SetCheck(threadPriority == 2);
    break;
  case ID_OPTIONS_PRIORITY_BELOWNORMAL:
    pCmdUI->SetCheck(threadPriority == 3);
    break;
  }
}

BOOL MainWnd::OnOptionsFilter(UINT nID)
{
	switch(nID)
	{
	case ID_OPTIONS_FILTER_NORMAL:
		filter = FILTER_NONE;
		break;
	case ID_OPTIONS_FILTER_TVMODE:
		filter = FILTER_TVMODE;
		break;
	case ID_OPTIONS_FILTER_PLUGIN:
		filter = FILTER_PLUGIN;
		if( strcmp( theApp.pluginName, "" ) == 0 ) {
			// open select plugin dialog when none selected
			OnOptionsSelectPlugin();
		}
		break;
	case ID_OPTIONS_FILTER_2XSAI:
		filter = FILTER_2XSAI;
		break;
	case ID_OPTIONS_FILTER_SUPER2XSAI:
		filter = FILTER_SUPER2XSAI;
		break;
	case ID_OPTIONS_FILTER_SUPEREAGLE:
		filter = FILTER_SUPEREAGLE;
		break;
	case ID_OPTIONS_FILTER16BIT_PIXELATEEXPERIMENTAL:
		filter = FILTER_PIXELATE;
		break;
	case ID_OPTIONS_FILTER16BIT_ADVANCEMAMESCALE2X:
		filter = FILTER_MAMESCALE2X;
		break;
	case ID_OPTIONS_FILTER16BIT_SIMPLE2X:
		filter = FILTER_SIMPLE2X;
		break;
	case ID_OPTIONS_FILTER_BILINEAR:
		filter = FILTER_BILINEAR;
		break;
	case ID_OPTIONS_FILTER_BILINEARPLUS:
		filter = FILTER_BILINEARPLUS;
		break;
	case ID_OPTIONS_FILTER_SCANLINES:
		filter = FILTER_SCANLINES;
		break;
	case ID_OPTIONS_FILTER_HQ2X:
		filter = FILTER_HQ2X;
		break;
	case ID_OPTIONS_FILTER_LQ2X:
		filter = FILTER_LQ2X;
		break;
	case ID_OPTIONS_FILTER_SIMPLE3X:
		filter = FILTER_SIMPLE3X;
		break;
	case ID_OPTIONS_FILTER_SIMPLE4X:
		filter = FILTER_SIMPLE4X;
		break;
	case ID_OPTIONS_FILTER_HQ3X:
		filter = FILTER_HQ3X;
		break;
	case ID_OPTIONS_FILTER_HQ4X:
		filter = FILTER_HQ4X;
		break;
	case ID_OPTIONS_FILTER_XBRZ2X:
		filter = FILTER_XBRZ2X;
		break;
	case ID_OPTIONS_FILTER_XBRZ3X:
		filter = FILTER_XBRZ3X;
		break;
	case ID_OPTIONS_FILTER_XBRZ4X:
		filter = FILTER_XBRZ4X;
		break;
	case ID_OPTIONS_FILTER_XBRZ5X:
		filter = FILTER_XBRZ5X;
		break;
	case ID_OPTIONS_FILTER_XBRZ6X:
		filter = FILTER_XBRZ6X;
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
    pCmdUI->SetCheck(filter == FILTER_NONE);
    break;
  case ID_OPTIONS_FILTER_TVMODE:
    pCmdUI->SetCheck(filter == FILTER_TVMODE);
    break;
  case ID_OPTIONS_FILTER_2XSAI:
    pCmdUI->SetCheck(filter == FILTER_2XSAI);
    break;
  case ID_OPTIONS_FILTER_SUPER2XSAI:
    pCmdUI->SetCheck(filter == FILTER_SUPER2XSAI);
    break;
  case ID_OPTIONS_FILTER_PLUGIN:
    pCmdUI->Enable( !filterMT );
    pCmdUI->SetCheck(filter == FILTER_PLUGIN);
    break;
  case ID_OPTIONS_FILTER_SUPEREAGLE:
    pCmdUI->SetCheck(filter == FILTER_SUPEREAGLE);
    break;
  case ID_OPTIONS_FILTER16BIT_PIXELATEEXPERIMENTAL:
    pCmdUI->SetCheck(filter == FILTER_PIXELATE);
    break;
  case ID_OPTIONS_FILTER16BIT_ADVANCEMAMESCALE2X:
    pCmdUI->SetCheck(filter == FILTER_MAMESCALE2X);
    break;
  case ID_OPTIONS_FILTER16BIT_SIMPLE2X:
    pCmdUI->SetCheck(filter == FILTER_SIMPLE2X);
    break;
  case ID_OPTIONS_FILTER_BILINEAR:
    pCmdUI->SetCheck(filter == FILTER_BILINEAR);
    break;
  case ID_OPTIONS_FILTER_BILINEARPLUS:
    pCmdUI->SetCheck(filter == FILTER_BILINEARPLUS);
    break;
  case ID_OPTIONS_FILTER_SCANLINES:
    pCmdUI->SetCheck(filter == FILTER_SCANLINES);
    break;
  case ID_OPTIONS_FILTER_HQ2X:
    pCmdUI->SetCheck(filter == FILTER_HQ2X);
    break;
  case ID_OPTIONS_FILTER_LQ2X:
    pCmdUI->SetCheck(filter == FILTER_LQ2X);
    break;
  case ID_OPTIONS_FILTER_SIMPLE3X:
    pCmdUI->SetCheck(filter == FILTER_SIMPLE3X);
    break;
  case ID_OPTIONS_FILTER_SIMPLE4X:
    pCmdUI->SetCheck(filter == FILTER_SIMPLE4X);
    break;
  case ID_OPTIONS_FILTER_HQ3X:
    pCmdUI->SetCheck(filter == FILTER_HQ3X);
    break;
  case ID_OPTIONS_FILTER_HQ4X:
    pCmdUI->SetCheck(filter == FILTER_HQ4X);
    break;
  case ID_OPTIONS_FILTER_XBRZ2X:
    pCmdUI->SetCheck(filter == FILTER_XBRZ2X);
    break;
  case ID_OPTIONS_FILTER_XBRZ3X:
    pCmdUI->SetCheck(filter == FILTER_XBRZ3X);
    break;
  case ID_OPTIONS_FILTER_XBRZ4X:
    pCmdUI->SetCheck(filter == FILTER_XBRZ4X);
    break;
  case ID_OPTIONS_FILTER_XBRZ5X:
    pCmdUI->SetCheck(filter == FILTER_XBRZ5X);
    break;
  case ID_OPTIONS_FILTER_XBRZ6X:
    pCmdUI->SetCheck(filter == FILTER_XBRZ6X);
    break;
  }
}

void MainWnd::OnUpdateOptionsSelectPlugin(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( !filterMT );
}

BOOL MainWnd::OnOptionsFilterIFB(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_NONE:
    ifbType = kIFBNone;
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_MOTIONBLUR:
	ifbType = kIBMotionBlur;
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_SMART:
    ifbType = kIBSmart;
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
    pCmdUI->SetCheck(ifbType == 0);
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_MOTIONBLUR:
    pCmdUI->SetCheck(ifbType == 1);
    break;
  case ID_OPTIONS_FILTER_INTERFRAMEBLENDING_SMART:
    pCmdUI->SetCheck(ifbType == 2);
    break;
  }
}

void MainWnd::OnOptionsFilterDisablemmx()
{
#ifdef MMX
  disableMMX = !disableMMX;
  if(!disableMMX)
    cpu_mmx = theApp.detectMMX();
  else
    cpu_mmx = 0;
#endif
}

void MainWnd::OnUpdateOptionsFilterDisablemmx(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(disableMMX);
}

void MainWnd::OnOptionsLanguageSystem()
{
  theApp.winSetLanguageOption(0, false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}

void MainWnd::OnUpdateOptionsLanguageSystem(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(languageOption == 0);
}

void MainWnd::OnOptionsLanguageEnglish()
{
  theApp.winSetLanguageOption(1, false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}

void MainWnd::OnUpdateOptionsLanguageEnglish(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(languageOption == 1);
}

void MainWnd::OnOptionsLanguageOther()
{
  theApp.winSetLanguageOption(2, false);
  theApp.winAccelMgr.UpdateMenu(theApp.menu);
}

void MainWnd::OnUpdateOptionsLanguageOther(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(languageOption == 2);
}


void MainWnd::OnOptionsJoypadConfigure1()
{
  JoypadConfig dlg(0);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure1(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(videoOption != VIDEO_320x240);
}

void MainWnd::OnOptionsJoypadConfigure2()
{
  JoypadConfig dlg(1);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure2(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(videoOption != VIDEO_320x240);
}

void MainWnd::OnOptionsJoypadConfigure3()
{
  JoypadConfig dlg(2);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure3(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(videoOption != VIDEO_320x240);
}

void MainWnd::OnOptionsJoypadConfigure4()
{
  JoypadConfig dlg(3);
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadConfigure4(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(videoOption != VIDEO_320x240);
}

BOOL MainWnd::OnOptionsJoypadDefault(UINT nID)
{
  joypadDefault = nID - ID_OPTIONS_JOYPAD_DEFAULTJOYPAD_1;
  return TRUE;
}

void MainWnd::OnUpdateOptionsJoypadDefault(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(joypadDefault == (int)(pCmdUI->m_nID - ID_OPTIONS_JOYPAD_DEFAULTJOYPAD_1));
}

void MainWnd::OnOptionsJoypadMotionconfigure()
{
  MotionConfig dlg;
  dlg.DoModal();
}

void MainWnd::OnUpdateOptionsJoypadMotionconfigure(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(videoOption != VIDEO_320x240);
}

BOOL MainWnd::OnOptionsJoypadAutofire(UINT nID)
{
  switch(nID) {
  case ID_OPTIONS_JOYPAD_AUTOFIRE_A:
    if(autoFire & 1) {
      autoFire &= ~1;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_A_DISABLED));
    } else {
      autoFire |= 1;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_A));
    }
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_B:
    if(autoFire & 2) {
      autoFire &= ~2;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_B_DISABLED));
    } else {
      autoFire |= 2;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_B));
    }
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_L:
    if(autoFire & 512) {
      autoFire &= ~512;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_L_DISABLED));
    } else {
      autoFire |= 512;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_L));
    }
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_R:
    if(autoFire & 256) {
      autoFire &= ~256;
      systemScreenMessage(winResLoadString(IDS_AUTOFIRE_R_DISABLED));
    } else {
      autoFire |= 256;
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
    check = (autoFire & 1) != 0;
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_B:
    check = (autoFire & 2) != 0;
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_L:
    check = (autoFire & 512) != 0;
    break;
  case ID_OPTIONS_JOYPAD_AUTOFIRE_R:
    check = (autoFire & 256) != 0;
    break;
  }
  pCmdUI->SetCheck(check);
}

void MainWnd::OnOptionsVideoFullscreenmaxscale()
{
  MaxScale dlg;

  dlg.DoModal();

  if( theApp.display ) {
	  theApp.display->setOption( _T("maxScale"), maxScale );
  }
}

#ifndef NO_LINK
void MainWnd::OnLinkOptions()
{
	LinkOptions dlg;

	dlg.DoModal();
}

#else
void MainWnd::OnLinkOptions() { }
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
  dsoundDisableHardwareAcceleration = !dsoundDisableHardwareAcceleration;
  soundShutdown();
  soundInit();
}

void MainWnd::OnUpdateOptionsSoundHardwareacceleration(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(!dsoundDisableHardwareAcceleration);
}

void MainWnd::OnOptionsSelectPlugin()
{
  SelectPlugin dlg;

  if (dlg.DoModal() == IDOK)
  {
	filter = FILTER_PLUGIN;
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
	pCmdUI->Enable(!aviRecording && !soundRecording);
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
	pCmdUI->Enable(!aviRecording && !soundRecording);
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
	pCmdUI->Enable(!aviRecording && !soundRecording);
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
	pCmdUI->Enable(!aviRecording && !soundRecording);
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
	pCmdUI->Enable(!aviRecording && !soundRecording);
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
	dlg.m_enableBIOS_GBA = useBiosFileGBA ? TRUE : FALSE;
	dlg.m_enableBIOS_GBC = useBiosFileGBC ? TRUE : FALSE;
	dlg.m_enableBIOS_GB = useBiosFileGB ? TRUE : FALSE;
	dlg.m_skipLogo = skipBios ? TRUE : FALSE;
	dlg.m_pathGBA = theApp.biosFileNameGBA;
	dlg.m_pathGBC = theApp.biosFileNameGBC;
	dlg.m_pathGB = theApp.biosFileNameGB;

	if( IDOK == dlg.DoModal() ) {
		useBiosFileGBA = dlg.m_enableBIOS_GBA == TRUE;
		useBiosFileGBC = dlg.m_enableBIOS_GBC == TRUE;
		useBiosFileGB = dlg.m_enableBIOS_GB == TRUE;
		skipBios = dlg.m_skipLogo == TRUE;
		theApp.biosFileNameGBA = dlg.m_pathGBA;
		theApp.biosFileNameGBC = dlg.m_pathGBC;
		theApp.biosFileNameGB = dlg.m_pathGB;
	}
}

void MainWnd::OnPixelfilterMultiThreading()
{
	filterMT = !filterMT;
}

void MainWnd::OnUpdatePixelfilterMultiThreading(CCmdUI *pCmdUI)
{
	pCmdUI->Enable( filter != FILTER_PLUGIN );
	pCmdUI->SetCheck( filterMT ? 1 : 0 );
}
