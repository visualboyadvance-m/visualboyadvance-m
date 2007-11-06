// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "window.h"

#include <sys/stat.h>

#include <stdio.h>
#include <time.h>

#include <SDL.h>

#include "../GBA.h"
#include "../gb/GB.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbPrinter.h"
#include "../Sound.h"
#include "../Util.h"

#include "menuitem.h"
#include "tools.h"
#include "intl.h"

extern int systemRenderedFrames;
extern int systemFPS;
extern bool debugger;
extern int RGB_LOW_BITS_MASK;
extern void (*dbgMain)();
extern void (*dbgSignal)(int, int);
extern void (*dbgOutput)(char *, u32);
extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int, int);
extern void remoteOutput(char *, u32);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);

#ifdef MMX
extern "C" bool cpu_mmx;
#endif // MMX

namespace VBA
{

using Gnome::Glade::Xml;

Window * Window::m_poInstance = NULL;

Window::Window(GtkWindow * _pstWindow, const Glib::RefPtr<Xml> & _poXml) :
  Gtk::Window       (_pstWindow),
  m_iGBScreenWidth  (160),
  m_iGBScreenHeight (144),
  m_iSGBScreenWidth (256),
  m_iSGBScreenHeight(224),
  m_iGBAScreenWidth (240),
  m_iGBAScreenHeight(160),
  m_iFrameskipMin   (0),
  m_iFrameskipMax   (9),
  m_iThrottleMin    (5),
  m_iThrottleMax    (1000),
  m_iScaleMin       (1),
  m_iScaleMax       (6),
  m_iShowSpeedMin   (ShowNone),
  m_iShowSpeedMax   (ShowDetailed),
  m_iSaveTypeMin    (SaveAuto),
  m_iSaveTypeMax    (SaveNone),
  m_iSoundQualityMin(Sound44K),
  m_iSoundQualityMax(Sound11K),
  m_iSoundVolumeMin (Sound100),
  m_iSoundVolumeMax (Sound50),
  m_iEmulatorTypeMin(EmulatorAuto),
  m_iEmulatorTypeMax(EmulatorSGB2),
  m_iFilter2xMin    (FirstFilter),
  m_iFilter2xMax    (LastFilter),
  m_iFilterIBMin    (FirstFilterIB),
  m_iFilterIBMax    (LastFilterIB),
  m_iJoypadMin      (1),
  m_iJoypadMax      (4)
{
  m_poXml            = _poXml;
  m_poFileOpenDialog = NULL;
  m_iScreenWidth     = m_iGBAScreenWidth;
  m_iScreenHeight    = m_iGBAScreenHeight;
  m_eCartridge       = CartridgeNone;
  m_uiJoypadState    = 0;

  vInitSystem();
  vInitSDL();

  Gtk::Container * poC;
  poC = dynamic_cast<Gtk::Container *>(_poXml->get_widget("ScreenContainer"));
  m_poScreenArea = Gtk::manage(new ScreenArea(m_iScreenWidth, m_iScreenHeight));
  poC->add(*m_poScreenArea);
  vDrawDefaultScreen();
  m_poScreenArea->show();

  // Get config
  //
  vInitConfig();

  m_sUserDataDir = Glib::get_home_dir() + "/.gvba";
  m_sConfigFile  = m_sUserDataDir + "/config";

  if (! Glib::file_test(m_sUserDataDir, Glib::FILE_TEST_EXISTS))
  {
    mkdir(m_sUserDataDir.c_str(), 0777);
  }
  if (Glib::file_test(m_sConfigFile, Glib::FILE_TEST_EXISTS))
  {
    vLoadConfig(m_sConfigFile);
    vCheckConfig();
  }
  else
  {
    vSaveConfig(m_sConfigFile);
  }

  vCreateFileOpenDialog();
  vLoadHistoryFromConfig();
  vLoadJoypadsFromConfig();

  Gtk::MenuItem *      poMI;
  Gtk::CheckMenuItem * poCMI;

  // File menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileOpen"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileOpen));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileLoad"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileLoad));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileSave"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileSave));
  m_listSensitiveWhenPlaying.push_back(poMI);

  for (int i = 0; i < 10; i++)
  {
    char csName[20];
    snprintf(csName, 20, "LoadGameSlot%d", i + 1);
    m_apoLoadGameItem[i] = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget(csName));
    snprintf(csName, 20, "SaveGameSlot%d", i + 1);
    m_apoSaveGameItem[i] = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget(csName));

    m_apoLoadGameItem[i]->signal_activate().connect(SigC::bind<int>(
                                                      SigC::slot(*this, &Window::vOnLoadGame),
                                                      i + 1));
    m_apoSaveGameItem[i]->signal_activate().connect(SigC::bind<int>(
                                                      SigC::slot(*this, &Window::vOnSaveGame),
                                                      i + 1));
  }
  vUpdateGameSlots();

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("LoadGameMostRecent"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnLoadGameMostRecent));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("LoadGameAuto"));
  poCMI->set_active(m_poCoreConfig->oGetKey<bool>("load_game_auto"));
  vOnLoadGameAutoToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnLoadGameAutoToggled),
                                    poCMI));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("SaveGameOldest"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnSaveGameOldest));
  m_listSensitiveWhenPlaying.push_back(poMI);

  m_poFilePauseItem = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("FilePause"));
  m_poFilePauseItem->set_active(false);
  vOnFilePauseToggled(m_poFilePauseItem);
  m_poFilePauseItem->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                                SigC::slot(*this, &Window::vOnFilePauseToggled),
                                                m_poFilePauseItem));
  m_listSensitiveWhenPlaying.push_back(m_poFilePauseItem);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileReset"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileReset));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileScreenCapture"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileScreenCapture));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileClose"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileClose));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileExit"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnFileExit));

  // Recent menu
  //
  m_poRecentMenu = dynamic_cast<Gtk::Menu *>(_poXml->get_widget("RecentMenu_menu"));
  vUpdateHistoryMenu();

  m_poRecentResetItem = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("RecentReset"));
  m_poRecentResetItem->signal_activate().connect(SigC::slot(*this, &Window::vOnRecentReset));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("RecentFreeze"));
  poCMI->set_active(m_poHistoryConfig->oGetKey<bool>("freeze"));
  vOnRecentFreezeToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnRecentFreezeToggled),
                                    poCMI));

  // Import menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("ImportBatteryFile"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnImportBatteryFile));
  m_listSensitiveWhenPlaying.push_back(poMI);

  // Export menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("ExportBatteryFile"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnExportBatteryFile));
  m_listSensitiveWhenPlaying.push_back(poMI);

  // Frameskip menu
  //
  struct
  {
    const char * m_csName;
    const int    m_iFrameskip;
  }
  astFrameskip[] =
  {
    { "FrameskipAutomatic", -1 },
    { "Frameskip0",          0 },
    { "Frameskip1",          1 },
    { "Frameskip2",          2 },
    { "Frameskip3",          3 },
    { "Frameskip4",          4 },
    { "Frameskip5",          5 },
    { "Frameskip6",          6 },
    { "Frameskip7",          7 },
    { "Frameskip8",          8 },
    { "Frameskip9",          9 }
  };
  int iDefaultFrameskip;
  if (m_poCoreConfig->sGetKey("frameskip") == "auto")
  {
    iDefaultFrameskip = -1;
  }
  else
  {
    iDefaultFrameskip = m_poCoreConfig->oGetKey<int>("frameskip");
  }
  for (guint i = 0; i < G_N_ELEMENTS(astFrameskip); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astFrameskip[i].m_csName));
    if (astFrameskip[i].m_iFrameskip == iDefaultFrameskip)
    {
      poCMI->set_active();
      vOnFrameskipToggled(poCMI, iDefaultFrameskip);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnFrameskipToggled),
                                      poCMI, astFrameskip[i].m_iFrameskip));
  }

  // Throttle menu
  //
  struct
  {
    const char * m_csName;
    const int    m_iThrottle;
  }
  astThrottle[] =
  {
    { "ThrottleNoThrottle",   0 },
    { "Throttle25",          25 },
    { "Throttle50",          50 },
    { "Throttle100",        100 },
    { "Throttle150",        150 },
    { "Throttle200",        200 }
  };
  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("ThrottleOther"));
  poCMI->set_active();
  poCMI->signal_activate().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                     SigC::slot(*this, &Window::vOnThrottleOther),
                                     poCMI));

  int iDefaultThrottle = m_poCoreConfig->oGetKey<int>("throttle");
  for (guint i = 0; i < G_N_ELEMENTS(astThrottle); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astThrottle[i].m_csName));
    if (astThrottle[i].m_iThrottle == iDefaultThrottle)
    {
      poCMI->set_active();
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnThrottleToggled),
                                      poCMI, astThrottle[i].m_iThrottle));
  }
  vSetThrottle(iDefaultThrottle);

  // Video menu
  //
  struct
  {
    const char * m_csName;
    const int    m_iScale;
  }
  astVideoScale[] =
  {
    { "Video1x", 1 },
    { "Video2x", 2 },
    { "Video3x", 3 },
    { "Video4x", 4 },
    { "Video5x", 5 },
    { "Video6x", 6 }
  };
  int iDefaultScale = m_poDisplayConfig->oGetKey<int>("scale");
  for (guint i = 0; i < G_N_ELEMENTS(astVideoScale); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astVideoScale[i].m_csName));
    if (astVideoScale[i].m_iScale == iDefaultScale)
    {
      poCMI->set_active();
      vOnVideoScaleToggled(poCMI, iDefaultScale);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnVideoScaleToggled),
                                      poCMI, astVideoScale[i].m_iScale));
  }

  // Layers menu
  //
  struct
  {
    const char * m_csName;
    const char * m_csKey;
    const int    m_iLayer;
  }
  astLayer[] =
  {
    { "LayersBg0",    "layer_bg0",    0 },
    { "LayersBg1",    "layer_bg1",    1 },
    { "LayersBg2",    "layer_bg2",    2 },
    { "LayersBg3",    "layer_bg3",    3 },
    { "LayersObj",    "layer_obj",    4 },
    { "LayersWin0",   "layer_win0",   5 },
    { "LayersWin1",   "layer_win1",   6 },
    { "LayersObjWin", "layer_objwin", 7 }
  };
  for (guint i = 0; i < G_N_ELEMENTS(astLayer); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astLayer[i].m_csName));
    poCMI->set_active(m_poCoreConfig->oGetKey<bool>(astLayer[i].m_csKey));
    vOnLayerToggled(poCMI, astLayer[i].m_iLayer);
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnLayerToggled),
                                      poCMI, astLayer[i].m_iLayer));
  }

  // Emulator menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("EmulatorDirectories"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnDirectories));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("EmulatorPauseWhenInactive"));
  poCMI->set_active(m_poDisplayConfig->oGetKey<bool>("pause_when_inactive"));
  vOnPauseWhenInactiveToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnPauseWhenInactiveToggled),
                                    poCMI));

  m_poUseBiosItem = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("EmulatorUseBios"));
  m_poUseBiosItem->set_active(m_poCoreConfig->oGetKey<bool>("use_bios_file"));
  if (m_poCoreConfig->sGetKey("bios_file") == "")
  {
    m_poUseBiosItem->set_sensitive(false);
  }
  m_poUseBiosItem->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                              SigC::slot(*this, &Window::vOnUseBiosToggled),
                                              m_poUseBiosItem));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("EmulatorSelectBios"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnSelectBios));

  // Show speed menu
  //
  struct
  {
    const char *     m_csName;
    const EShowSpeed m_eShowSpeed;
  }
  astShowSpeed[] =
  {
    { "ShowSpeedNone",       ShowNone       },
    { "ShowSpeedPercentage", ShowPercentage },
    { "ShowSpeedDetailed",   ShowDetailed   }
  };
  EShowSpeed eDefaultShowSpeed = (EShowSpeed)m_poDisplayConfig->oGetKey<int>("show_speed");
  for (guint i = 0; i < G_N_ELEMENTS(astShowSpeed); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astShowSpeed[i].m_csName));
    if (astShowSpeed[i].m_eShowSpeed == eDefaultShowSpeed)
    {
      poCMI->set_active();
      vOnShowSpeedToggled(poCMI, eDefaultShowSpeed);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnShowSpeedToggled),
                                      poCMI, astShowSpeed[i].m_eShowSpeed));
  }

  // Save type menu
  //
  struct
  {
    const char *    m_csName;
    const ESaveType m_eSaveType;
  }
  astSaveType[] =
  {
    { "SaveTypeAutomatic",    SaveAuto         },
    { "SaveTypeEeprom",       SaveEEPROM       },
    { "SaveTypeSram",         SaveSRAM         },
    { "SaveTypeFlash",        SaveFlash        },
    { "SaveTypeEepromSensor", SaveEEPROMSensor },
    { "SaveTypeNone",         SaveNone         }
  };
  ESaveType eDefaultSaveType = (ESaveType)m_poCoreConfig->oGetKey<int>("save_type");
  for (guint i = 0; i < G_N_ELEMENTS(astSaveType); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astSaveType[i].m_csName));
    if (astSaveType[i].m_eSaveType == eDefaultSaveType)
    {
      poCMI->set_active();
      vOnSaveTypeToggled(poCMI, eDefaultSaveType);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnSaveTypeToggled),
                                      poCMI, astSaveType[i].m_eSaveType));
  }

  // Flash size menu
  //
  struct
  {
    const char * m_csName;
    const int    m_iFlashSize;
  }
  astFlashSize[] =
  {
    { "SaveTypeFlash64K",   64 },
    { "SaveTypeFlash128K", 128 }
  };
  int iDefaultFlashSize = m_poCoreConfig->oGetKey<int>("flash_size");
  for (guint i = 0; i < G_N_ELEMENTS(astFlashSize); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astFlashSize[i].m_csName));
    if (astFlashSize[i].m_iFlashSize == iDefaultFlashSize)
    {
      poCMI->set_active();
      vOnFlashSizeToggled(poCMI, iDefaultFlashSize);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnFlashSizeToggled),
                                      poCMI, astFlashSize[i].m_iFlashSize));
  }

  // Screenshot format menu
  //
  struct
  {
    const char * m_csName;
    const char * m_csScreenshotFormat;
  }
  astScreenshotFormat[] =
  {
    { "ScreenshotFormatPNG", "png" },
    { "ScreenshotFormatBMP", "bmp" }
  };
  std::string sDefaultScreenshotFormat = m_poCoreConfig->sGetKey("screenshot_format");
  for (guint i = 0; i < G_N_ELEMENTS(astScreenshotFormat); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astScreenshotFormat[i].m_csName));
    if (astScreenshotFormat[i].m_csScreenshotFormat == sDefaultScreenshotFormat)
    {
      poCMI->set_active();
      vOnScreenshotFormatToggled(poCMI, sDefaultScreenshotFormat);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, std::string>(
                                      SigC::slot(*this, &Window::vOnScreenshotFormatToggled),
                                      poCMI, std::string(astScreenshotFormat[i].m_csScreenshotFormat)));
  }

  // Sound menu
  //
  std::string sDefaultSoundStatus = m_poSoundConfig->sGetKey("status");

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundOff"));
  if (sDefaultSoundStatus == "off")
  {
    poCMI->set_active();
    vOnSoundStatusToggled(poCMI, SoundOff);
  }
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                    SigC::slot(*this, &Window::vOnSoundStatusToggled),
                                    poCMI, SoundOff));
  m_poSoundOffItem = poCMI;

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundMute"));
  if (sDefaultSoundStatus == "mute")
  {
    poCMI->set_active();
    vOnSoundStatusToggled(poCMI, SoundMute);
  }
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                    SigC::slot(*this, &Window::vOnSoundStatusToggled),
                                    poCMI, SoundMute));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundOn"));
  if (sDefaultSoundStatus == "on")
  {
    poCMI->set_active();
    vOnSoundStatusToggled(poCMI, SoundOn);
  }
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                    SigC::slot(*this, &Window::vOnSoundStatusToggled),
                                    poCMI, SoundOn));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundEcho"));
  poCMI->set_active(m_poSoundConfig->oGetKey<bool>("echo"));
  vOnSoundEchoToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnSoundEchoToggled),
                                    poCMI));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundLowPass"));
  poCMI->set_active(m_poSoundConfig->oGetKey<bool>("low_pass"));
  vOnSoundLowPassToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnSoundLowPassToggled),
                                    poCMI));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundReverseStereo"));
  poCMI->set_active(m_poSoundConfig->oGetKey<bool>("reverse_stereo"));
  vOnSoundReverseToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnSoundReverseToggled),
                                    poCMI));

  struct
  {
    const char * m_csName;
    const char * m_csKey;
    const int    m_iSoundChannel;
  }
  astSoundChannel[] =
  {
    { "SoundChannel1", "channel_1", 0 },
    { "SoundChannel2", "channel_2", 1 },
    { "SoundChannel3", "channel_3", 2 },
    { "SoundChannel4", "channel_4", 3 },
    { "SoundChannelA", "channel_A", 4 },
    { "SoundChannelB", "channel_B", 5 }
  };
  for (guint i = 0; i < G_N_ELEMENTS(astSoundChannel); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astSoundChannel[i].m_csName));
    poCMI->set_active(m_poSoundConfig->oGetKey<bool>(astSoundChannel[i].m_csKey));
    vOnSoundChannelToggled(poCMI, astSoundChannel[i].m_iSoundChannel);
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnSoundChannelToggled),
                                      poCMI, astSoundChannel[i].m_iSoundChannel));
  }

  struct
  {
    const char *        m_csName;
    const ESoundQuality m_eSoundQuality;
  }
  astSoundQuality[] =
  {
    { "Sound11Khz", Sound11K },
    { "Sound22Khz", Sound22K },
    { "Sound44Khz", Sound44K }
  };
  ESoundQuality eDefaultSoundQuality = (ESoundQuality)m_poSoundConfig->oGetKey<int>("quality");
  for (guint i = 0; i < G_N_ELEMENTS(astSoundQuality); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astSoundQuality[i].m_csName));
    if (astSoundQuality[i].m_eSoundQuality == eDefaultSoundQuality)
    {
      poCMI->set_active();
      vOnSoundQualityToggled(poCMI, eDefaultSoundQuality);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnSoundQualityToggled),
                                      poCMI, astSoundQuality[i].m_eSoundQuality));
  }

  // Volume menu
  //
  struct
  {
    const char *       m_csName;
    const ESoundVolume m_eSoundVolume;
  }
  astSoundVolume[] =
  {
    { "Volume25",   Sound25  },
    { "Volume50",   Sound50  },
    { "Volume100",  Sound100 },
    { "Volume200",  Sound200 },
    { "Volume300",  Sound300 },
    { "Volume400",  Sound400 }
  };
  ESoundVolume eDefaultSoundVolume = (ESoundVolume)m_poSoundConfig->oGetKey<int>("volume");
  for (guint i = 0; i < G_N_ELEMENTS(astSoundVolume); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astSoundVolume[i].m_csName));
    if (astSoundVolume[i].m_eSoundVolume == eDefaultSoundVolume)
    {
      poCMI->set_active();
      vOnSoundVolumeToggled(poCMI, eDefaultSoundVolume);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnSoundVolumeToggled),
                                      poCMI, astSoundVolume[i].m_eSoundVolume));
  }

  // Gameboy menu
  //
  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("GameboyBorder"));
  poCMI->set_active(m_poCoreConfig->oGetKey<bool>("gb_border"));
  vOnGBBorderToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnGBBorderToggled),
                                    poCMI));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("GameboyPrinter"));
  poCMI->set_active(m_poCoreConfig->oGetKey<bool>("gb_printer"));
  vOnGBPrinterToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnGBPrinterToggled),
                                    poCMI));

  struct
  {
    const char *        m_csName;
    const EEmulatorType m_eEmulatorType;
  }
  astEmulatorType[] =
  {
    { "GameboyAutomatic", EmulatorAuto },
    { "GameboyGba",       EmulatorGBA  },
    { "GameboyCgb",       EmulatorCGB  },
    { "GameboySgb",       EmulatorSGB  },
    { "GameboySgb2",      EmulatorSGB2 },
    { "GameboyGb",        EmulatorGB   }
  };
  EEmulatorType eDefaultEmulatorType = (EEmulatorType)m_poCoreConfig->oGetKey<int>("emulator_type");
  for (guint i = 0; i < G_N_ELEMENTS(astEmulatorType); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astEmulatorType[i].m_csName));
    if (astEmulatorType[i].m_eEmulatorType == eDefaultEmulatorType)
    {
      poCMI->set_active();
      vOnEmulatorTypeToggled(poCMI, eDefaultEmulatorType);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnEmulatorTypeToggled),
                                      poCMI, astEmulatorType[i].m_eEmulatorType));
  }

  // Filter menu
  //
  struct
  {
    const char *    m_csName;
    const EFilter2x m_eFilter2x;
  }
  astFilter2x[] =
  {
    { "FilterNone",          FilterNone         },
    { "FilterTVMode",        FilterScanlinesTV  },
    { "Filter2xSaI",         Filter2xSaI        },
    { "FilterSuper2xSaI",    FilterSuper2xSaI   },
    { "FilterSuperEagle",    FilterSuperEagle   },
    { "FilterPixelate",      FilterPixelate     },
    { "FilterMotionBlur",    FilterMotionBlur   },
    { "FilterAdvanceMame2x", FilterAdMame2x     },
    { "FilterSimple2x",      FilterSimple2x     },
    { "FilterBilinear",      FilterBilinear     },
    { "FilterBilinearPlus",  FilterBilinearPlus },
    { "FilterScanlines",     FilterScanlines    },
    { "FilterHq2x",          FilterHq2x         },
    { "FilterLq2x",          FilterLq2x         }
  };
  EFilter2x eDefaultFilter2x = (EFilter2x)m_poDisplayConfig->oGetKey<int>("filter2x");
  for (guint i = 0; i < G_N_ELEMENTS(astFilter2x); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astFilter2x[i].m_csName));
    if (astFilter2x[i].m_eFilter2x == eDefaultFilter2x)
    {
      poCMI->set_active();
      vOnFilter2xToggled(poCMI, eDefaultFilter2x);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnFilter2xToggled),
                                      poCMI, astFilter2x[i].m_eFilter2x));
  }

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("FilterDisableMmx"));
#ifdef MMX
  poCMI->set_active(m_poDisplayConfig->oGetKey<bool>("filter_disable_mmx"));
  vOnDisableMMXToggled(poCMI);
  poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *>(
                                    SigC::slot(*this, &Window::vOnDisableMMXToggled),
                                    poCMI));
#else // ! MMX
  poCMI->set_active();
  poCMI->set_sensitive(false);
#endif // ! MMX

  // Interframe blending menu
  //
  struct
  {
    const char *    m_csName;
    const EFilterIB m_eFilterIB;
  }
  astFilterIB[] =
  {
    { "IFBNone",       FilterIBNone       },
    { "IFBSmart",      FilterIBSmart      },
    { "IFBMotionBlur", FilterIBMotionBlur }
  };
  EFilterIB eDefaultFilterIB = (EFilterIB)m_poDisplayConfig->oGetKey<int>("filterIB");
  for (guint i = 0; i < G_N_ELEMENTS(astFilterIB); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astFilterIB[i].m_csName));
    if (astFilterIB[i].m_eFilterIB == eDefaultFilterIB)
    {
      poCMI->set_active();
      vOnFilterIBToggled(poCMI, eDefaultFilterIB);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnFilterIBToggled),
                                      poCMI, astFilterIB[i].m_eFilterIB));
  }

  // Joypad menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure1"));
  poMI->signal_activate().connect(SigC::bind<int>(
                                    SigC::slot(*this, &Window::vOnJoypadConfigure), 1));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure2"));
  poMI->signal_activate().connect(SigC::bind<int>(
                                    SigC::slot(*this, &Window::vOnJoypadConfigure), 2));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure3"));
  poMI->signal_activate().connect(SigC::bind<int>(
                                    SigC::slot(*this, &Window::vOnJoypadConfigure), 3));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure4"));
  poMI->signal_activate().connect(SigC::bind<int>(
                                    SigC::slot(*this, &Window::vOnJoypadConfigure), 4));

  int iDefaultJoypad = m_poInputConfig->oGetKey<int>("active_joypad");
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csName[20];
    snprintf(csName, sizeof(csName), "Joypad%d", i);

    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(csName));
    if (i == iDefaultJoypad)
    {
      poCMI->set_active();
      vOnJoypadToggled(poCMI, iDefaultJoypad);
    }
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, int>(
                                      SigC::slot(*this, &Window::vOnJoypadToggled),
                                      poCMI, i));
  }

  // Autofire menu
  //
  struct
  {
    const char *   m_csName;
    const char *   m_csKey;
    const EKeyFlag m_eKeyFlag;
  }
  astAutofire[] =
  {
    { "AutofireA", "autofire_A", KeyFlagA },
    { "AutofireB", "autofire_B", KeyFlagB },
    { "AutofireL", "autofire_L", KeyFlagL },
    { "AutofireR", "autofire_R", KeyFlagR }
  };
  for (guint i = 0; i < G_N_ELEMENTS(astAutofire); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astAutofire[i].m_csName));
    poCMI->set_active(m_poInputConfig->oGetKey<bool>(astAutofire[i].m_csKey));
    vOnAutofireToggled(poCMI, astAutofire[i].m_eKeyFlag);
    poCMI->signal_toggled().connect(SigC::bind<Gtk::CheckMenuItem *, u32>(
                                      SigC::slot(*this, &Window::vOnAutofireToggled),
                                      poCMI, astAutofire[i].m_eKeyFlag));
  }

  // GDB menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbWait"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnGDBWait));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbLoadAndWait"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnGDBLoadAndWait));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbBreak"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnGDBBreak));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbDisconnect"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnGDBDisconnect));

  // Help menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("HelpAbout"));
  poMI->signal_activate().connect(SigC::slot(*this, &Window::vOnHelpAbout));

  // Init widgets sensitivity
  for (std::list<Gtk::Widget *>::iterator it = m_listSensitiveWhenPlaying.begin();
       it != m_listSensitiveWhenPlaying.end();
       it++)
  {
    (*it)->set_sensitive(false);
  }

  if (m_poInstance == NULL)
  {
    m_poInstance = this;
  }
  else
  {
    abort();
  }
}

Window::~Window()
{
  vOnFileClose();
  vSaveHistoryToConfig();
  vSaveJoypadsToConfig();
  vSaveConfig(m_sConfigFile);

  if (m_poFileOpenDialog != NULL)
  {
    delete m_poFileOpenDialog;
  }

  if (m_poKeymap != NULL)
  {
    delete m_poKeymap;
  }

  m_poInstance = NULL;
}

void Window::vInitSystem()
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  systemRedShift    = 3;
  systemGreenShift  = 11;
  systemBlueShift   = 19;
  RGB_LOW_BITS_MASK = 0x00010101;
#else
  systemRedShift    = 27;
  systemGreenShift  = 19;
  systemBlueShift   = 11;
  RGB_LOW_BITS_MASK = 0x01010100;
#endif

  systemColorDepth = 32;
  systemDebug = 0;
  systemVerbose = 0;
  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
  systemFrameSkip = 0;
  systemSoundOn = false;
  soundOffFlag = true;

  systemRenderedFrames = 0;
  systemFPS = 0;

  emulating = 0;
  debugger = false;

  for (int i = 0; i < 0x10000; i++)
  {
    systemColorMap32[i] = (((i & 0x1f) << systemRedShift)
                           | (((i & 0x3e0) >> 5) << systemGreenShift)
                           | (((i & 0x7c00) >> 10) << systemBlueShift));
  }

  gbFrameSkip = 0;

  for (int i = 0; i < 24; )
  {
    systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
    systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
    systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
    systemGbPalette[i++] = 0;
  }

  Init_2xSaI(32);
}

void Window::vInitSDL()
{
  static bool bDone = false;

  if (bDone)
    return;

  int iFlags = (SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE);

  if (SDL_Init(iFlags) < 0)
  {
    fprintf(stderr, "Failed to init SDL: %s", SDL_GetError());
    abort();
  }

  bDone = true;
}

void Window::vInitConfig()
{
  m_oConfig.vClear();

  // History section
  //
  m_poHistoryConfig = m_oConfig.poAddSection("History");
  m_poHistoryConfig->vSetKey("freeze", false );
  m_poHistoryConfig->vSetKey("0",      ""    );
  m_poHistoryConfig->vSetKey("1",      ""    );
  m_poHistoryConfig->vSetKey("2",      ""    );
  m_poHistoryConfig->vSetKey("3",      ""    );
  m_poHistoryConfig->vSetKey("4",      ""    );
  m_poHistoryConfig->vSetKey("5",      ""    );
  m_poHistoryConfig->vSetKey("6",      ""    );
  m_poHistoryConfig->vSetKey("7",      ""    );
  m_poHistoryConfig->vSetKey("8",      ""    );
  m_poHistoryConfig->vSetKey("9",      ""    );

  // Directories section
  //
  m_poDirConfig = m_oConfig.poAddSection("Directories");
  m_poDirConfig->vSetKey("gb_roms",   "" );
  m_poDirConfig->vSetKey("gba_roms",  "" );
  m_poDirConfig->vSetKey("batteries", "" );
  m_poDirConfig->vSetKey("saves",     "" );
  m_poDirConfig->vSetKey("captures",  "" );

  // Core section
  //
  m_poCoreConfig = m_oConfig.poAddSection("Core");
  m_poCoreConfig->vSetKey("load_game_auto",    false        );
  m_poCoreConfig->vSetKey("frameskip",         "auto"       );
  m_poCoreConfig->vSetKey("throttle",          0            );
  m_poCoreConfig->vSetKey("layer_bg0",         true         );
  m_poCoreConfig->vSetKey("layer_bg1",         true         );
  m_poCoreConfig->vSetKey("layer_bg2",         true         );
  m_poCoreConfig->vSetKey("layer_bg3",         true         );
  m_poCoreConfig->vSetKey("layer_obj",         true         );
  m_poCoreConfig->vSetKey("layer_win0",        true         );
  m_poCoreConfig->vSetKey("layer_win1",        true         );
  m_poCoreConfig->vSetKey("layer_objwin",      true         );
  m_poCoreConfig->vSetKey("use_bios_file",     false        );
  m_poCoreConfig->vSetKey("bios_file",         ""           );
  m_poCoreConfig->vSetKey("save_type",         SaveAuto     );
  m_poCoreConfig->vSetKey("flash_size",        64           );
  m_poCoreConfig->vSetKey("gb_border",         true         );
  m_poCoreConfig->vSetKey("gb_printer",        false        );
  m_poCoreConfig->vSetKey("emulator_type",     EmulatorAuto );
  m_poCoreConfig->vSetKey("screenshot_format", "png"        );

  // Display section
  //
  m_poDisplayConfig = m_oConfig.poAddSection("Display");
  m_poDisplayConfig->vSetKey("scale",               1              );
  m_poDisplayConfig->vSetKey("show_speed",          ShowPercentage );
  m_poDisplayConfig->vSetKey("pause_when_inactive", true           );
  m_poDisplayConfig->vSetKey("filter2x",            FilterNone     );
  m_poDisplayConfig->vSetKey("filterIB",            FilterIBNone   );
#ifdef MMX
  m_poDisplayConfig->vSetKey("filter_disable_mmx",  false          );
#endif // MMX

  // Sound section
  //
  m_poSoundConfig = m_oConfig.poAddSection("Sound");
  m_poSoundConfig->vSetKey("status",         "on"     );
  m_poSoundConfig->vSetKey("echo",           false    );
  m_poSoundConfig->vSetKey("low_pass",       false    );
  m_poSoundConfig->vSetKey("reverse_stereo", false    );
  m_poSoundConfig->vSetKey("channel_1",      true     );
  m_poSoundConfig->vSetKey("channel_2",      true     );
  m_poSoundConfig->vSetKey("channel_3",      true     );
  m_poSoundConfig->vSetKey("channel_4",      true     );
  m_poSoundConfig->vSetKey("channel_A",      true     );
  m_poSoundConfig->vSetKey("channel_B",      true     );
  m_poSoundConfig->vSetKey("quality",        Sound22K );
  m_poSoundConfig->vSetKey("volume",         Sound100 );

  // Input section
  //
  JoypadConfig oJoypadConfig;
  oJoypadConfig.vSetDefault();
  m_poInputConfig = m_oConfig.poAddSection("Input");
  m_poInputConfig->vSetKey("active_joypad", m_iJoypadMin );
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csPrefix[20];
    snprintf(csPrefix, sizeof(csPrefix), "joypad%d_", i);
    std::string sPrefix(csPrefix);
    m_poInputConfig->vSetKey(sPrefix + "up",      oJoypadConfig.m_uiUp      );
    m_poInputConfig->vSetKey(sPrefix + "down",    oJoypadConfig.m_uiDown    );
    m_poInputConfig->vSetKey(sPrefix + "left",    oJoypadConfig.m_uiLeft    );
    m_poInputConfig->vSetKey(sPrefix + "right",   oJoypadConfig.m_uiRight   );
    m_poInputConfig->vSetKey(sPrefix + "A",       oJoypadConfig.m_uiA       );
    m_poInputConfig->vSetKey(sPrefix + "B",       oJoypadConfig.m_uiB       );
    m_poInputConfig->vSetKey(sPrefix + "L",       oJoypadConfig.m_uiL       );
    m_poInputConfig->vSetKey(sPrefix + "R",       oJoypadConfig.m_uiR       );
    m_poInputConfig->vSetKey(sPrefix + "select",  oJoypadConfig.m_uiSelect  );
    m_poInputConfig->vSetKey(sPrefix + "start",   oJoypadConfig.m_uiStart   );
    m_poInputConfig->vSetKey(sPrefix + "speed",   oJoypadConfig.m_uiSpeed   );
    m_poInputConfig->vSetKey(sPrefix + "capture", oJoypadConfig.m_uiCapture );
  }
  m_poInputConfig->vSetKey("autofire_A", false );
  m_poInputConfig->vSetKey("autofire_B", false );
  m_poInputConfig->vSetKey("autofire_L", false );
  m_poInputConfig->vSetKey("autofire_R", false );
}

void Window::vCheckConfig()
{
  int iValue;
  int iAdjusted;
  std::string sValue;

  // Directories section
  //
  sValue = m_poDirConfig->sGetKey("gb_roms");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("gb_roms", "");
  }
  sValue = m_poDirConfig->sGetKey("gba_roms");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("gba_roms", "");
  }
  sValue = m_poDirConfig->sGetKey("batteries");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("batteries", "");
  }
  sValue = m_poDirConfig->sGetKey("saves");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("saves", "");
  }
  sValue = m_poDirConfig->sGetKey("captures");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("captures", "");
  }

  // Core section
  //
  if (m_poCoreConfig->sGetKey("frameskip") != "auto")
  {
    iValue = m_poCoreConfig->oGetKey<int>("frameskip");
    iAdjusted = CLAMP(iValue, m_iFrameskipMin, m_iFrameskipMax);
    if (iValue != iAdjusted)
    {
      m_poCoreConfig->vSetKey("frameskip", iAdjusted);
    }
  }

  iValue = m_poCoreConfig->oGetKey<int>("throttle");
  if (iValue != 0)
  {
    iAdjusted = CLAMP(iValue, m_iThrottleMin, m_iThrottleMax);
    if (iValue != iAdjusted)
    {
      m_poCoreConfig->vSetKey("throttle", iAdjusted);
    }
  }

  sValue = m_poCoreConfig->sGetKey("bios_file");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_REGULAR))
  {
    m_poCoreConfig->vSetKey("bios_file", "");
  }
  if (m_poCoreConfig->sGetKey("bios_file") == "")
  {
    m_poCoreConfig->vSetKey("use_bios_file", false);
  }

  iValue = m_poCoreConfig->oGetKey<int>("save_type");
  if (iValue != 0)
  {
    iAdjusted = CLAMP(iValue, m_iSaveTypeMin, m_iSaveTypeMax);
    if (iValue != iAdjusted)
    {
      m_poCoreConfig->vSetKey("save_type", iAdjusted);
    }
  }

  iValue = m_poCoreConfig->oGetKey<int>("flash_size");
  if (iValue != 64 && iValue != 128)
  {
    m_poCoreConfig->vSetKey("flash_size", 64);
  }

  iValue = m_poCoreConfig->oGetKey<int>("emulator_type");
  iAdjusted = CLAMP(iValue, m_iEmulatorTypeMin, m_iEmulatorTypeMax);
  if (iValue != iAdjusted)
  {
    m_poCoreConfig->vSetKey("emulator_type", iAdjusted);
  }

  sValue = m_poCoreConfig->sGetKey("screenshot_format");
  if (sValue != "png" && sValue != "bmp")
  {
    sValue = "png";
  }

  // Display section
  //
  iValue = m_poDisplayConfig->oGetKey<int>("scale");
  iAdjusted = CLAMP(iValue, m_iScaleMin, m_iScaleMax);
  if (iValue != iAdjusted)
  {
    m_poDisplayConfig->vSetKey("scale", iAdjusted);
  }

  iValue = m_poDisplayConfig->oGetKey<int>("show_speed");
  iAdjusted = CLAMP(iValue, m_iShowSpeedMin, m_iShowSpeedMax);
  if (iValue != iAdjusted)
  {
    m_poDisplayConfig->vSetKey("show_speed", iAdjusted);
  }

  iValue = m_poDisplayConfig->oGetKey<int>("filter2x");
  iAdjusted = CLAMP(iValue, m_iFilter2xMin, m_iFilter2xMax);
  if (iValue != iAdjusted)
  {
    m_poDisplayConfig->vSetKey("filter2x", iAdjusted);
  }

  iValue = m_poDisplayConfig->oGetKey<int>("filterIB");
  iAdjusted = CLAMP(iValue, m_iFilterIBMin, m_iFilterIBMax);
  if (iValue != iAdjusted)
  {
    m_poDisplayConfig->vSetKey("filterIB", iAdjusted);
  }

  // Sound section
  //
  sValue = m_poSoundConfig->sGetKey("status");
  if (sValue != "off" && sValue != "on" && sValue != "mute")
  {
    m_poSoundConfig->vSetKey("status", "on");
  }

  iValue = m_poSoundConfig->oGetKey<int>("quality");
  iAdjusted = CLAMP(iValue, m_iSoundQualityMin, m_iSoundQualityMax);
  if (iValue != iAdjusted)
  {
    m_poSoundConfig->vSetKey("quality", iAdjusted);
  }

  iValue = m_poSoundConfig->oGetKey<int>("volume");
  iAdjusted = CLAMP(iValue, m_iSoundVolumeMin, m_iSoundVolumeMax);
  if (iValue != iAdjusted)
  {
    m_poSoundConfig->vSetKey("volume", iAdjusted);
  }

  // Input section
  //
  iValue = m_poInputConfig->oGetKey<int>("active_joypad");
  iAdjusted = CLAMP(iValue, m_iJoypadMin, m_iJoypadMax);
  if (iValue != iAdjusted)
  {
    m_poInputConfig->vSetKey("active_joypad", iAdjusted);
  }
}

void Window::vLoadConfig(const std::string & _rsFile)
{
  try
  {
    m_oConfig.vLoad(_rsFile, false, false);
  }
  catch (const Glib::Error & e)
  {
    vPopupError(e.what().c_str());
  }
}

void Window::vSaveConfig(const std::string & _rsFile)
{
  try
  {
    m_oConfig.vSave(_rsFile);
  }
  catch (const Glib::Error & e)
  {
    vPopupError(e.what().c_str());
  }
}

void Window::vLoadHistoryFromConfig()
{
  char csKey[] = "0";
  for (int i = 0; i < 10; i++, csKey[0]++)
  {
    std::string sFile = m_poHistoryConfig->sGetKey(csKey);
    if (sFile == "")
    {
      break;
    }
    m_listHistory.push_back(sFile);
  }
}

void Window::vSaveHistoryToConfig()
{
  char csKey[] = "0";
  for (std::list<std::string>::const_iterator it = m_listHistory.begin();
       it != m_listHistory.end();
       it++, csKey[0]++)
  {
    m_poHistoryConfig->vSetKey(csKey, *it);
  }
}

void Window::vHistoryAdd(const std::string & _rsFile)
{
  if (m_poHistoryConfig->oGetKey<bool>("freeze"))
  {
    return;
  }

  m_listHistory.remove(_rsFile);
  m_listHistory.push_front(_rsFile);
  if (m_listHistory.size() > 10)
  {
    m_listHistory.pop_back();
  }

  vUpdateHistoryMenu();
}

void Window::vClearHistoryMenu()
{
  Gtk::Menu_Helpers::MenuList::iterator it = m_poRecentMenu->items().begin();
  for (int i = 0; i < 3; i++, it++)
    ;

  m_poRecentMenu->items().erase(it, m_poRecentMenu->items().end());
}

void Window::vUpdateHistoryMenu()
{
  vClearHistoryMenu();

  guint uiAccelKey = GDK_F1;
  for (std::list<std::string>::const_iterator it = m_listHistory.begin();
       it != m_listHistory.end();
       it++, uiAccelKey++)
  {
    Gtk::Image * poImage = Gtk::manage(new Gtk::Image(Gtk::Stock::OPEN, Gtk::ICON_SIZE_MENU));
    Glib::ustring sLabel = Glib::path_get_basename(*it);
    VBA::ImageMenuItem * poIMI = Gtk::manage(new VBA::ImageMenuItem(*poImage, sLabel));

    m_oTooltips.set_tip(*poIMI, *it);

    poIMI->signal_activate().connect(SigC::bind<std::string>(
                                      SigC::slot(*this, &Window::vOnRecentFile),
                                      *it));

    poIMI->set_accel_key(Gtk::AccelKey(uiAccelKey, Gdk::CONTROL_MASK));
    poIMI->accelerate(*this);

    poIMI->show();
    m_poRecentMenu->items().push_back(*poIMI);
  }
}

void Window::vLoadJoypadsFromConfig()
{
  m_oJoypads.clear();

  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csPrefix[20];
    snprintf(csPrefix, sizeof(csPrefix), "joypad%d_", i);
    std::string sPrefix(csPrefix);

    JoypadConfig oJoypadConfig;
    oJoypadConfig.m_uiUp      = m_poInputConfig->oGetKey<guint>(sPrefix + "up");
    oJoypadConfig.m_uiDown    = m_poInputConfig->oGetKey<guint>(sPrefix + "down");
    oJoypadConfig.m_uiLeft    = m_poInputConfig->oGetKey<guint>(sPrefix + "left");
    oJoypadConfig.m_uiRight   = m_poInputConfig->oGetKey<guint>(sPrefix + "right");
    oJoypadConfig.m_uiA       = m_poInputConfig->oGetKey<guint>(sPrefix + "A");
    oJoypadConfig.m_uiB       = m_poInputConfig->oGetKey<guint>(sPrefix + "B");
    oJoypadConfig.m_uiL       = m_poInputConfig->oGetKey<guint>(sPrefix + "L");
    oJoypadConfig.m_uiR       = m_poInputConfig->oGetKey<guint>(sPrefix + "R");
    oJoypadConfig.m_uiSelect  = m_poInputConfig->oGetKey<guint>(sPrefix + "select");
    oJoypadConfig.m_uiStart   = m_poInputConfig->oGetKey<guint>(sPrefix + "start");
    oJoypadConfig.m_uiSpeed   = m_poInputConfig->oGetKey<guint>(sPrefix + "speed");
    oJoypadConfig.m_uiCapture = m_poInputConfig->oGetKey<guint>(sPrefix + "capture");

    m_oJoypads.push_back(oJoypadConfig);
  }
}

void Window::vSaveJoypadsToConfig()
{
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csPrefix[20];
    snprintf(csPrefix, sizeof(csPrefix), "joypad%d_", i);
    std::string sPrefix(csPrefix);

    m_poInputConfig->vSetKey(sPrefix + "up",      m_oJoypads[i - 1].m_uiUp      );
    m_poInputConfig->vSetKey(sPrefix + "down",    m_oJoypads[i - 1].m_uiDown    );
    m_poInputConfig->vSetKey(sPrefix + "left",    m_oJoypads[i - 1].m_uiLeft    );
    m_poInputConfig->vSetKey(sPrefix + "right",   m_oJoypads[i - 1].m_uiRight   );
    m_poInputConfig->vSetKey(sPrefix + "A",       m_oJoypads[i - 1].m_uiA       );
    m_poInputConfig->vSetKey(sPrefix + "B",       m_oJoypads[i - 1].m_uiB       );
    m_poInputConfig->vSetKey(sPrefix + "L",       m_oJoypads[i - 1].m_uiL       );
    m_poInputConfig->vSetKey(sPrefix + "R",       m_oJoypads[i - 1].m_uiR       );
    m_poInputConfig->vSetKey(sPrefix + "select",  m_oJoypads[i - 1].m_uiSelect  );
    m_poInputConfig->vSetKey(sPrefix + "start",   m_oJoypads[i - 1].m_uiStart   );
    m_poInputConfig->vSetKey(sPrefix + "speed",   m_oJoypads[i - 1].m_uiSpeed   );
    m_poInputConfig->vSetKey(sPrefix + "capture", m_oJoypads[i - 1].m_uiCapture );
  }
}

void Window::vUpdateScreen()
{
  if (m_eCartridge == CartridgeGB)
  {
    if (gbBorderOn)
    {
      m_iScreenWidth     = m_iSGBScreenWidth;
      m_iScreenHeight    = m_iSGBScreenHeight;
      gbBorderLineSkip   = m_iSGBScreenWidth;
      gbBorderColumnSkip = (m_iSGBScreenWidth - m_iGBScreenWidth) / 2;
      gbBorderRowSkip    = (m_iSGBScreenHeight - m_iGBScreenHeight) / 2;
    }
    else
    {
      m_iScreenWidth     = m_iGBScreenWidth;
      m_iScreenHeight    = m_iGBScreenHeight;
      gbBorderLineSkip   = m_iGBScreenWidth;
      gbBorderColumnSkip = 0;
      gbBorderRowSkip    = 0;
    }
  }
  else if (m_eCartridge == CartridgeGBA)
  {
    m_iScreenWidth  = m_iGBAScreenWidth;
    m_iScreenHeight = m_iGBAScreenHeight;
  }

  g_return_if_fail(m_iScreenWidth >= 1 && m_iScreenHeight >= 1);

  m_poScreenArea->vSetSize(m_iScreenWidth, m_iScreenHeight);
  m_poScreenArea->vSetScale(m_poDisplayConfig->oGetKey<int>("scale"));

  resize(1, 1);

  if (emulating)
  {
    vDrawScreen();
  }
  else
  {
    vDrawDefaultScreen();
  }
}

bool Window::bLoadROM(const std::string & _rsFile)
{
  vOnFileClose();

  m_sRomFile = _rsFile;
  const char * csFile = _rsFile.c_str();

  IMAGE_TYPE eType = utilFindType(csFile);
  if (eType == IMAGE_UNKNOWN)
  {
    vPopupError(_("Unknown file type %s"), csFile);
    return false;
  }

  bool bLoaded = false;
  if (eType == IMAGE_GB)
  {
    bLoaded = gbLoadRom(csFile);
    if (bLoaded)
    {
      m_eCartridge = CartridgeGB;
      m_stEmulator = GBSystem;
    }
  }
  else if (eType == IMAGE_GBA)
  {
    int iSize = CPULoadRom(csFile);
    bLoaded = (iSize > 0);
    if (bLoaded)
    {
      m_eCartridge = CartridgeGBA;
      m_stEmulator = GBASystem;

      useBios = m_poCoreConfig->oGetKey<bool>("use_bios_file");
      CPUInit(m_poCoreConfig->sGetKey("bios_file").c_str(), useBios);
      CPUReset();

      // If the bios file was rejected by CPUInit
      if (m_poCoreConfig->oGetKey<bool>("use_bios_file") && ! useBios)
      {
        m_poUseBiosItem->set_active(false);
        m_poUseBiosItem->set_sensitive(false);
        m_poCoreConfig->vSetKey("bios_file", "");
      }
    }
  }

  if (! bLoaded)
  {
    return false;
  }

  vLoadBattery();
  vUpdateScreen();

  debugger = false; // May cause conflicts
  emulating = 1;
  m_bWasEmulating = false;
  m_uiThrottleDelay = 0;

  if (m_eCartridge == CartridgeGBA)
  {
    soundSetQuality(m_eSoundQuality);
  }
  else
  {
    gbSoundSetQuality(m_eSoundQuality);
  }

  vUpdateGameSlots();
  vHistoryAdd(_rsFile);

  for (std::list<Gtk::Widget *>::iterator it = m_listSensitiveWhenPlaying.begin();
       it != m_listSensitiveWhenPlaying.end();
       it++)
  {
    (*it)->set_sensitive();
  }

  if (m_poCoreConfig->oGetKey<bool>("load_game_auto"))
  {
    vOnLoadGameMostRecent();
  }

  vStartEmu();

  return true;
}

void Window::vPopupError(const char * _csFormat, ...)
{
  va_list args;
  va_start(args, _csFormat);
  char * csMsg = g_strdup_vprintf(_csFormat, args);
  va_end(args);

  Gtk::MessageDialog oDialog(*this,
                             csMsg,
#ifndef GTKMM20
                             false,
#endif // ! GTKMM20
                             Gtk::MESSAGE_ERROR,
                             Gtk::BUTTONS_OK);
  oDialog.run();
  g_free(csMsg);
}

void Window::vPopupErrorV(const char * _csFormat, va_list _args)
{
  char * csMsg = g_strdup_vprintf(_csFormat, _args);

  Gtk::MessageDialog oDialog(*this,
                             csMsg,
#ifndef GTKMM20
                             false,
#endif // ! GTKMM20
                             Gtk::MESSAGE_ERROR,
                             Gtk::BUTTONS_OK);
  oDialog.run();
  g_free(csMsg);
}

void Window::vDrawScreen()
{
  m_poScreenArea->vDrawPixels(pix);
}

void Window::vDrawDefaultScreen()
{
  m_poScreenArea->vDrawColor(0x000000); // Black
}

void Window::vSetDefaultTitle()
{
  set_title("VBA");
}

void Window::vShowSpeed(int _iSpeed)
{
  char csTitle[50];

  if (m_eShowSpeed == ShowPercentage)
  {
    snprintf(csTitle, 50, "VBA - %d%%", _iSpeed);
    set_title(csTitle);
  }
  else if (m_eShowSpeed == ShowDetailed)
  {
    snprintf(csTitle, 50, "VBA - %d%% (%d, %d fps)",
             _iSpeed, systemFrameSkip, systemFPS);
    set_title(csTitle);
  }
}

void Window::vComputeFrameskip(int _iRate)
{
  static u32 uiLastTime = 0;
  static int iFrameskipAdjust = 0;

  u32 uiTime = SDL_GetTicks();

  if (m_bWasEmulating)
  {
    int iWantedSpeed = 100;

    if (m_iThrottle > 0)
    {
      if (! speedup)
      {
        u32 uiDiff  = uiTime - m_uiThrottleLastTime;
        int iTarget = 1000000 / (_iRate * m_iThrottle);
        int iDelay  = iTarget - uiDiff;
        if (iDelay > 0)
        {
          m_uiThrottleDelay = iDelay;
        }
      }
      iWantedSpeed = m_iThrottle;
    }

    if (m_bAutoFrameskip)
    {
      u32 uiDiff = uiTime - uiLastTime;
      int iSpeed = iWantedSpeed;

      if (uiDiff != 0)
      {
        iSpeed = (1000000 / _iRate) / uiDiff;
      }

      if (iSpeed >= iWantedSpeed - 2)
      {
        iFrameskipAdjust++;
        if (iFrameskipAdjust >= 3)
        {
          iFrameskipAdjust = 0;
          if (systemFrameSkip > 0)
          {
            systemFrameSkip--;
          }
        }
      }
      else
      {
        if (iSpeed < iWantedSpeed - 20)
        {
          iFrameskipAdjust -= ((iWantedSpeed - 10) - iSpeed) / 5;
        }
        else if (systemFrameSkip < 9)
        {
          iFrameskipAdjust--;
        }

        if (iFrameskipAdjust <= -2)
        {
          iFrameskipAdjust += 2;
          if (systemFrameSkip < 9)
          {
            systemFrameSkip++;
          }
        }
      }
    }
  }
  else
  {
    m_bWasEmulating = true;
  }

  uiLastTime = uiTime;
  m_uiThrottleLastTime = uiTime;
}

void Window::vCaptureScreen(int _iNum)
{
  std::string sBaseName;
  std::string sCaptureDir = m_poDirConfig->sGetKey("captures");
  if (sCaptureDir == "")
  {
    sBaseName = sCutSuffix(m_sRomFile);
  }
  else
  {
    sBaseName = sCaptureDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile));
  }
  std::string sFormat = m_poCoreConfig->sGetKey("screenshot_format");

  char * csFile = g_strdup_printf("%s_%02d.%s",
                                  sBaseName.c_str(),
                                  _iNum,
                                  sFormat.c_str());
  if (sFormat == "png")
  {
    m_stEmulator.emuWritePNG(csFile);
  }
  else
  {
    m_stEmulator.emuWriteBMP(csFile);
  }
  g_free(csFile);
}

u32 Window::uiReadJoypad()
{
  u32 uiJoypad = m_uiJoypadState;

  if (m_uiAutofireState != 0)
  {
    uiJoypad &= ~m_uiAutofireState;
    if (m_bAutofireToggle)
    {
      uiJoypad |= m_uiAutofireState;
    }
    m_bAutofireToggle = ! m_bAutofireToggle;
  }

  return uiJoypad;
}

void Window::vCreateFileOpenDialog()
{
  if (m_poFileOpenDialog != NULL)
  {
    return;
  }

  std::string sGBDir  = m_poDirConfig->sGetKey("gb_roms");
  std::string sGBADir = m_poDirConfig->sGetKey("gba_roms");

#ifdef GTKMM20

  Gtk::FileSelection * poDialog = new Gtk::FileSelection(_("Open"));
  poDialog->set_transient_for(*this);

  if (sGBADir != "")
  {
    poDialog->set_filename(sGBADir + "/");
  }
  else if (sGBDir != "")
  {
    poDialog->set_filename(sGBDir + "/");
  }

#else // ! GTKMM20

  Gtk::FileChooserDialog * poDialog = new Gtk::FileChooserDialog(*this, _("Open"));
  poDialog->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  poDialog->add_button(Gtk::Stock::OPEN,   Gtk::RESPONSE_OK);

  if (sGBDir != "")
  {
    poDialog->add_shortcut_folder(sGBDir);
    poDialog->set_current_folder(sGBDir);
  }

  if (sGBADir != "" && sGBADir != sGBDir)
  {
    poDialog->add_shortcut_folder(sGBADir);
    poDialog->set_current_folder(sGBADir);
  }

  const char * acsPattern[] =
  {
    // GBA
    "*.[bB][iI][nN]", "*.[aA][gG][bB]", "*.[gG][bB][aA]",
    // GB
    "*.[gG][bB]", "*.[sS][gG][bB]", "*.[cC][gG][bB]", "*.[gG][bB][cC]",
    // Both
    "*.[mM][bB]", "*.[eE][lL][fF]", "*.[zZ][iI][pP]", "*.[zZ]", "*.[gG][zZ]"
  };

  Gtk::FileFilter oAllGBAFilter;
  oAllGBAFilter.set_name(_("All Gameboy Advance files"));
  for (guint i = 0; i < G_N_ELEMENTS(acsPattern); i++)
  {
    oAllGBAFilter.add_pattern(acsPattern[i]);
  }

  Gtk::FileFilter oGBAFilter;
  oGBAFilter.set_name(_("Gameboy Advance files"));
  for (int i = 0; i < 3; i++)
  {
    oGBAFilter.add_pattern(acsPattern[i]);
  }

  Gtk::FileFilter oGBFilter;
  oGBFilter.set_name(_("Gameboy files"));
  for (int i = 3; i < 7; i++)
  {
    oGBFilter.add_pattern(acsPattern[i]);
  }

  poDialog->add_filter(oAllGBAFilter);
  poDialog->add_filter(oGBAFilter);
  poDialog->add_filter(oGBFilter);

#endif // ! GTKMM20

  m_poFileOpenDialog = poDialog;
}

void Window::vLoadBattery()
{
  std::string sBattery;
  std::string sDir = m_poDirConfig->sGetKey("batteries");
  if (sDir == "")
  {
    sBattery = sCutSuffix(m_sRomFile) + ".sav";
  }
  else
  {
    sBattery = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile)) + ".sav";
  }

  if (m_stEmulator.emuReadBattery(sBattery.c_str()))
  {
    systemScreenMessage(_("Loaded battery"));
  }
}

void Window::vSaveBattery()
{
  std::string sBattery;
  std::string sDir = m_poDirConfig->sGetKey("batteries");
  if (sDir == "")
  {
    sBattery = sCutSuffix(m_sRomFile) + ".sav";
  }
  else
  {
    sBattery = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile)) + ".sav";
  }

  if (m_stEmulator.emuWriteBattery(sBattery.c_str()))
  {
    systemScreenMessage(_("Saved battery"));
  }
}

void Window::vStartEmu()
{
  if (m_oEmuSig.connected())
  {
    return;
  }

  m_oEmuSig = Glib::signal_idle().connect(SigC::slot(*this, &Window::bOnEmuIdle),
                                          Glib::PRIORITY_DEFAULT_IDLE);
}

void Window::vStopEmu()
{
  m_oEmuSig.disconnect();
  m_bWasEmulating = false;
}

void Window::vSetThrottle(int _iPercent)
{
  m_iThrottle = _iPercent;
  m_poCoreConfig->vSetKey("throttle", _iPercent);
}

void Window::vSelectBestThrottleItem()
{
  struct
  {
    const char * m_csName;
    const int    m_iThrottle;
  }
  astThrottle[] =
  {
    { "ThrottleNoThrottle",   0 },
    { "Throttle25",          25 },
    { "Throttle50",          50 },
    { "Throttle100",        100 },
    { "Throttle150",        150 },
    { "Throttle200",        200 }
  };
  for (guint i = 0; i < G_N_ELEMENTS(astThrottle); i++)
  {
    Gtk::CheckMenuItem * poCMI;
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(m_poXml->get_widget(astThrottle[i].m_csName));
    if (astThrottle[i].m_iThrottle == m_iThrottle)
    {
      poCMI->set_active();
    }
  }
}

void Window::vUpdateGameSlots()
{
  if (m_eCartridge == CartridgeNone)
  {
    std::string sDateTime = _("----/--/-- --:--:--");

    for (int i = 0; i < 10; i++)
    {
      char csPrefix[10];
      snprintf(csPrefix, sizeof(csPrefix), "%2d ", i + 1);

      Gtk::Label * poLabel;
      poLabel = dynamic_cast<Gtk::Label *>(m_apoLoadGameItem[i]->get_child());
      poLabel->set_text(csPrefix + sDateTime);
      m_apoLoadGameItem[i]->set_sensitive(false);

      poLabel = dynamic_cast<Gtk::Label *>(m_apoSaveGameItem[i]->get_child());
      poLabel->set_text(csPrefix + sDateTime);
      m_apoSaveGameItem[i]->set_sensitive(false);

      m_astGameSlot[i].m_bEmpty = true;
    }
  }
  else
  {
    std::string sFileBase;
    std::string sDir = m_poDirConfig->sGetKey("saves");
    if (sDir == "")
    {
      sFileBase = sCutSuffix(m_sRomFile);
    }
    else
    {
      sFileBase = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile));
    }

    const char * csDateFormat = _("%Y/%m/%d %H:%M:%S");

    for (int i = 0; i < 10; i++)
    {
      char csPrefix[10];
      snprintf(csPrefix, sizeof(csPrefix), "%2d ", i + 1);

      char csSlot[10];
      snprintf(csSlot, sizeof(csSlot), "%d", i + 1);
      m_astGameSlot[i].m_sFile = sFileBase + csSlot + ".sgm";

      std::string sDateTime;
      struct stat stStat;
      if (stat(m_astGameSlot[i].m_sFile.c_str(), &stStat) == -1)
      {
        sDateTime = _("----/--/-- --:--:--");
        m_astGameSlot[i].m_bEmpty = true;
      }
      else
      {
        char csDateTime[30];
        strftime(csDateTime, sizeof(csDateTime), csDateFormat,
                 localtime(&stStat.st_mtime));
        sDateTime = csDateTime;
        m_astGameSlot[i].m_bEmpty = false;
        m_astGameSlot[i].m_uiTime = stStat.st_mtime;
      }

      Gtk::Label * poLabel;
      poLabel = dynamic_cast<Gtk::Label *>(m_apoLoadGameItem[i]->get_child());
      poLabel->set_text(csPrefix + sDateTime);
      m_apoLoadGameItem[i]->set_sensitive(! m_astGameSlot[i].m_bEmpty);

      poLabel = dynamic_cast<Gtk::Label *>(m_apoSaveGameItem[i]->get_child());
      poLabel->set_text(csPrefix + sDateTime);
      m_apoSaveGameItem[i]->set_sensitive();
    }
  }
}

} // VBA namespace
