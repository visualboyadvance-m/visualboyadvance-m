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

#include "../agb/GBA.h"
#include "../dmg/gb.h"
#include "../dmg/gbGlobals.h"
#include "../dmg/gbPrinter.h"
#include "../Sound.h"
#include "../Util.h"

#include "tools.h"
#include "intl.h"
#include "screenarea-cairo.h"
#include "screenarea-xvideo.h"

#ifdef USE_OPENGL
#include "screenarea-opengl.h"
#endif // USE_OPENGL

extern int systemRenderedFrames;
extern int systemFPS;
extern bool debugger;
extern int RGB_LOW_BITS_MASK;
extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int, int);
extern void remoteOutput(const char *, u32);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);

namespace VBA
{

using Gnome::Glade::Xml;

Window * Window::m_poInstance = NULL;

const Window::SJoypadKey Window::m_astJoypad[SDLBUTTONS_NUM] =
{
	{ "left",    KEY_LEFT           },
	{ "right",   KEY_RIGHT          },
	{ "up",      KEY_UP             },
	{ "down",    KEY_DOWN           },
	{ "A",       KEY_BUTTON_A       },
	{ "B",       KEY_BUTTON_B       },
	{ "select",  KEY_BUTTON_SELECT  },
	{ "start",   KEY_BUTTON_START   },
	{ "L",       KEY_BUTTON_L       },
	{ "R",       KEY_BUTTON_R       },
	{ "speed",   KEY_BUTTON_SPEED   },
	{ "capture", KEY_BUTTON_CAPTURE },
	{ "speed",   KEY_BUTTON_SPEED   },
	{ "capture", KEY_BUTTON_CAPTURE }
};

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
  m_fSoundVolumeMin (0.50f),
  m_fSoundVolumeMax (2.00f),
  m_iEmulatorTypeMin(EmulatorAuto),
  m_iEmulatorTypeMax(EmulatorSGB2),
  m_iFilter2xMin    (FirstFilter),
  m_iFilter2xMax    (LastFilter),
  m_iFilterIBMin    (FirstFilterIB),
  m_iFilterIBMax    (LastFilterIB),
  m_iJoypadMin      (PAD_1),
  m_iJoypadMax      (PAD_4),
  m_iVideoOutputMin (OutputCairo),
  m_iVideoOutputMax (OutputXvideo),
  m_bFullscreen     (false)
{
  m_poXml            = _poXml;
  m_poFileOpenDialog = NULL;
  m_iScreenWidth     = m_iGBAScreenWidth;
  m_iScreenHeight    = m_iGBAScreenHeight;
  m_eCartridge       = CartridgeNone;

  vInitSDL();
  vInitSystem();

  vSetDefaultTitle();

  // Get config
  //
  m_sUserDataDir = Glib::get_home_dir() + "/.gvba";
  m_sConfigFile  = m_sUserDataDir + "/config";

  vInitConfig();

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
  vLoadJoypadsFromConfig();

  Gtk::MenuItem *      poMI;
  Gtk::CheckMenuItem * poCMI;

  // Menu bar
  m_poMenuBar = dynamic_cast<Gtk::MenuBar *>(_poXml->get_widget("MenuBar"));

  // Video output menu
  //
  struct
  {
    const char *       m_csName;
    const EVideoOutput m_eVideoOutput;
  }
  astVideoOutput[] =
  {
    { "VideoOpenGL", OutputOpenGL },
    { "VideoCairo",  OutputCairo  },
    { "VideoXv",     OutputXvideo }
  };
  EVideoOutput eDefaultVideoOutput = (EVideoOutput)m_poDisplayConfig->oGetKey<int>("output");
  for (guint i = 0; i < G_N_ELEMENTS(astVideoOutput); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astVideoOutput[i].m_csName));
    if (astVideoOutput[i].m_eVideoOutput == eDefaultVideoOutput)
    {
      poCMI->set_active();
      vOnVideoOutputToggled(poCMI, eDefaultVideoOutput);
    }
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnVideoOutputToggled),
                                      poCMI, astVideoOutput[i].m_eVideoOutput));
  }

  // File menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileOpen"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileOpen));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileLoad"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileLoad));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileSave"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileSave));
  m_listSensitiveWhenPlaying.push_back(poMI);

  for (int i = 0; i < 10; i++)
  {
    char csName[20];
    snprintf(csName, 20, "LoadGameSlot%d", i + 1);
    m_apoLoadGameItem[i] = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget(csName));
    snprintf(csName, 20, "SaveGameSlot%d", i + 1);
    m_apoSaveGameItem[i] = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget(csName));

    m_apoLoadGameItem[i]->signal_activate().connect(sigc::bind(
                                                      sigc::mem_fun(*this, &Window::vOnLoadGame),
                                                      i + 1));
    m_apoSaveGameItem[i]->signal_activate().connect(sigc::bind(
                                                      sigc::mem_fun(*this, &Window::vOnSaveGame),
                                                      i + 1));
  }
  vUpdateGameSlots();

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("LoadGameMostRecent"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnLoadGameMostRecent));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("LoadGameAuto"));
  poCMI->set_active(m_poCoreConfig->oGetKey<bool>("load_game_auto"));
  vOnLoadGameAutoToggled(poCMI);
  poCMI->signal_toggled().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnLoadGameAutoToggled),
                                    poCMI));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("SaveGameOldest"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnSaveGameOldest));
  m_listSensitiveWhenPlaying.push_back(poMI);

  m_poFilePauseItem = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("FilePause"));
  m_poFilePauseItem->set_active(false);
  vOnFilePauseToggled(m_poFilePauseItem);
  m_poFilePauseItem->signal_toggled().connect(sigc::bind(
                                                sigc::mem_fun(*this, &Window::vOnFilePauseToggled),
                                                m_poFilePauseItem));
  m_listSensitiveWhenPlaying.push_back(m_poFilePauseItem);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileReset"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileReset));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileScreenCapture"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileScreenCapture));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileClose"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileClose));
  m_listSensitiveWhenPlaying.push_back(poMI);

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("FileExit"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnFileExit));

  // Recent menu
  //
  m_poRecentManager = Gtk::RecentManager::get_default();

  Gtk::RecentFilter oRecentFilter;
  oRecentFilter.add_application( Glib::get_application_name() );

  m_poRecentChooserMenu = Gtk::manage( new Gtk::RecentChooserMenu(m_poRecentManager) );
  m_poRecentChooserMenu->set_show_numbers();
  m_poRecentChooserMenu->set_show_tips();
  m_poRecentChooserMenu->set_local_only();
  m_poRecentChooserMenu->add_filter(oRecentFilter);
  m_poRecentChooserMenu->signal_item_activated().connect(
                                                   sigc::mem_fun(*this, &Window::vOnRecentFile));


  m_poRecentMenu = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("RecentMenu"));
  m_poRecentMenu->set_submenu(static_cast<Gtk::Menu &>(*m_poRecentChooserMenu));

  // Import menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("ImportBatteryFile"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnImportBatteryFile));
  m_listSensitiveWhenPlaying.push_back(poMI);

  // Export menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("ExportBatteryFile"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnExportBatteryFile));
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnFrameskipToggled),
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
  poCMI->signal_activate().connect(sigc::bind(
                                     sigc::mem_fun(*this, &Window::vOnThrottleOther),
                                     poCMI));

  int iDefaultThrottle = m_poCoreConfig->oGetKey<int>("throttle");
  for (guint i = 0; i < G_N_ELEMENTS(astThrottle); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astThrottle[i].m_csName));
    if (astThrottle[i].m_iThrottle == iDefaultThrottle)
    {
      poCMI->set_active();
    }
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnThrottleToggled),
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnVideoScaleToggled),
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnLayerToggled),
                                      poCMI, astLayer[i].m_iLayer));
  }

  // Emulator menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("EmulatorDirectories"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnDirectories));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("EmulatorPauseWhenInactive"));
  poCMI->set_active(m_poDisplayConfig->oGetKey<bool>("pause_when_inactive"));
  vOnPauseWhenInactiveToggled(poCMI);
  poCMI->signal_toggled().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnPauseWhenInactiveToggled),
                                    poCMI));

  m_poUseBiosItem = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("EmulatorUseBios"));
  m_poUseBiosItem->set_active(m_poCoreConfig->oGetKey<bool>("use_bios_file"));
  if (m_poCoreConfig->sGetKey("bios_file") == "")
  {
    m_poUseBiosItem->set_sensitive(false);
  }
  m_poUseBiosItem->signal_toggled().connect(sigc::bind(
                                              sigc::mem_fun(*this, &Window::vOnUseBiosToggled),
                                              m_poUseBiosItem));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("EmulatorSelectBios"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnSelectBios));

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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnShowSpeedToggled),
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnSaveTypeToggled),
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnFlashSizeToggled),
                                      poCMI, astFlashSize[i].m_iFlashSize));
  }

  // Sound menu
  //
  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("SoundMute"));
  poCMI->set_active(m_poSoundConfig->oGetKey<bool>("mute"));
  vOnSoundMuteToggled(poCMI);
  poCMI->signal_toggled().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnSoundMuteToggled),
                                    poCMI));

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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnSoundQualityToggled),
                                      poCMI, astSoundQuality[i].m_eSoundQuality));
  }

  // Volume menu
  //
  struct
  {
    const char *       m_csName;
    const float        m_fSoundVolume;
  }
  astSoundVolume[] =
  {
    { "Volume25",   0.25f    },
    { "Volume50",   0.50f    },
    { "Volume100",  1.00f    },
    { "Volume200",  2.00f    }
  };
  float fDefaultSoundVolume = m_poSoundConfig->oGetKey<float>("volume");
  for (guint i = 0; i < G_N_ELEMENTS(astSoundVolume); i++)
  {
    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(astSoundVolume[i].m_csName));
    if (astSoundVolume[i].m_fSoundVolume == fDefaultSoundVolume)
    {
      poCMI->set_active();
      vOnSoundVolumeToggled(poCMI, fDefaultSoundVolume);
    }
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnSoundVolumeToggled),
                                      poCMI, astSoundVolume[i].m_fSoundVolume));
  }

  // Gameboy menu
  //
  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("GameboyBorder"));
  poCMI->set_active(m_poCoreConfig->oGetKey<bool>("gb_border"));
  vOnGBBorderToggled(poCMI);
  poCMI->signal_toggled().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnGBBorderToggled),
                                    poCMI));

  poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget("GameboyPrinter"));
  poCMI->set_active(m_poCoreConfig->oGetKey<bool>("gb_printer"));
  vOnGBPrinterToggled(poCMI);
  poCMI->signal_toggled().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnGBPrinterToggled),
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnEmulatorTypeToggled),
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
    { "FilterAdvanceMame2x", FilterAdMame2x     },
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnFilter2xToggled),
                                      poCMI, astFilter2x[i].m_eFilter2x));
  }

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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnFilterIBToggled),
                                      poCMI, astFilterIB[i].m_eFilterIB));
  }

  // Joypad menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure1"));
  poMI->signal_activate().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnJoypadConfigure), 1));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure2"));
  poMI->signal_activate().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnJoypadConfigure), 2));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure3"));
  poMI->signal_activate().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnJoypadConfigure), 3));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("JoypadConfigure4"));
  poMI->signal_activate().connect(sigc::bind(
                                    sigc::mem_fun(*this, &Window::vOnJoypadConfigure), 4));

  int iDefaultJoypad = m_poInputConfig->oGetKey<int>("active_joypad");
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csName[20];
    snprintf(csName, sizeof(csName), "Joypad%d", i + 1);

    poCMI = dynamic_cast<Gtk::CheckMenuItem *>(_poXml->get_widget(csName));
    if (i == iDefaultJoypad)
    {
      poCMI->set_active();
      vOnJoypadToggled(poCMI, (EPad)iDefaultJoypad);
    }
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnJoypadToggled),
                                      poCMI, (EPad)i));
  }

  // Autofire menu
  //
  /*struct
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
    poCMI->signal_toggled().connect(sigc::bind(
                                      sigc::mem_fun(*this, &Window::vOnAutofireToggled),
                                      poCMI, astAutofire[i].m_eKeyFlag));
  }*/

  // Fullscreen menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("VideoFullscreen"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnVideoFullscreen));

  // GDB menu
  //
#ifndef NO_DEBUGGER
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbWait"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnGDBWait));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbLoadAndWait"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnGDBLoadAndWait));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbBreak"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnGDBBreak));

  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("GdbDisconnect"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnGDBDisconnect));
#else
  // Hide the whole Tools menu as it only contains the debugger stuff as of now
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("ToolsMenu"));
  poMI->hide();
#endif //NO_DEBUGGER

  // Help menu
  //
  poMI = dynamic_cast<Gtk::MenuItem *>(_poXml->get_widget("HelpAbout"));
  poMI->signal_activate().connect(sigc::mem_fun(*this, &Window::vOnHelpAbout));

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
  vUnInitSystem();
  vSaveJoypadsToConfig();
  vSaveConfig(m_sConfigFile);

  if (m_poFileOpenDialog != NULL)
  {
    delete m_poFileOpenDialog;
  }

  m_poInstance = NULL;
}

void Window::vInitScreenArea(EVideoOutput _eVideoOutput)
{
  Gtk::Alignment * poC;

  poC = dynamic_cast<Gtk::Alignment *>(m_poXml->get_widget("ScreenContainer"));
  poC->remove();
  poC->set(Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER, 1.0, 1.0);

  try
  {
    switch (_eVideoOutput)
    {
#ifdef USE_OPENGL
      case OutputOpenGL:
        m_poScreenArea = Gtk::manage(new ScreenAreaGl(m_iScreenWidth, m_iScreenHeight));
        break;
#endif // USE_OPENGL
      case OutputXvideo:
        m_poScreenArea = Gtk::manage(new ScreenAreaXv(m_iScreenWidth, m_iScreenHeight));
        break;
      case OutputCairo:
      default:
        m_poScreenArea = Gtk::manage(new ScreenAreaCairo(m_iScreenWidth, m_iScreenHeight));
        break;
    }
  }
  catch (std::exception e)
  {
    fprintf(stderr, "Unable to initialise output, falling back to Cairo\n");
    m_poScreenArea = Gtk::manage(new ScreenAreaCairo(m_iScreenWidth, m_iScreenHeight));
  }

  poC->add(*m_poScreenArea);
  vDrawDefaultScreen();
  m_poScreenArea->show();
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
  systemFrameSkip = 2;

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

  soundInit();
}

void Window::vUnInitSystem()
{
  systemSoundShutdown();
}

void Window::vInitSDL()
{
  static bool bDone = false;

  if (bDone)
    return;

  int iFlags = (SDL_INIT_EVERYTHING | SDL_INIT_NOPARACHUTE);

  if (SDL_Init(iFlags) < 0)
  {
    fprintf(stderr, "Failed to init SDL: %s", SDL_GetError());
    abort();
  }

  inputSetKeymap(PAD_DEFAULT, KEY_LEFT, GDK_Left);
  inputSetKeymap(PAD_DEFAULT, KEY_RIGHT, GDK_Right);
  inputSetKeymap(PAD_DEFAULT, KEY_UP, GDK_Up);
  inputSetKeymap(PAD_DEFAULT, KEY_DOWN, GDK_Down);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_A, GDK_z);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_B, GDK_x);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_START, GDK_Return);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_SELECT, GDK_BackSpace);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_L, GDK_a);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_R, GDK_s);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_SPEED, GDK_space);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_CAPTURE, GDK_F12);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_AUTO_A, GDK_q);
  inputSetKeymap(PAD_DEFAULT, KEY_BUTTON_AUTO_B, GDK_w);

  // TODO : remove
  int sdlNumDevices = SDL_NumJoysticks();
  for (int i = 0; i < sdlNumDevices; i++)
    SDL_JoystickOpen(i);

  inputInitJoysticks();

  bDone = true;
}

void Window::vInitConfig()
{
  m_oConfig.vClear();

  // Directories section
  //
  m_poDirConfig = m_oConfig.poAddSection("Directories");
  m_poDirConfig->vSetKey("gb_roms",   Glib::get_home_dir());
  m_poDirConfig->vSetKey("gba_roms",  Glib::get_home_dir());
  m_poDirConfig->vSetKey("batteries", m_sUserDataDir);
  m_poDirConfig->vSetKey("saves",     m_sUserDataDir);
  m_poDirConfig->vSetKey("captures",  m_sUserDataDir);

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

  // Display section
  //
  m_poDisplayConfig = m_oConfig.poAddSection("Display");
  m_poDisplayConfig->vSetKey("scale",               1              );
  m_poDisplayConfig->vSetKey("show_speed",          ShowPercentage );
  m_poDisplayConfig->vSetKey("pause_when_inactive", true           );
  m_poDisplayConfig->vSetKey("filter2x",            FilterNone     );
  m_poDisplayConfig->vSetKey("filterIB",            FilterIBNone   );
#ifdef USE_OPENGL
  m_poDisplayConfig->vSetKey("output",              OutputOpenGL   );
#else
  m_poDisplayConfig->vSetKey("output",              OutputCairo    );
#endif // USE_OPENGL


  // Sound section
  //
  m_poSoundConfig = m_oConfig.poAddSection("Sound");
  m_poSoundConfig->vSetKey("mute",           false    );
  m_poSoundConfig->vSetKey("quality",        Sound22K );
  m_poSoundConfig->vSetKey("volume",         1.00f    );

  // Input section
  //
  m_poInputConfig = m_oConfig.poAddSection("Input");
  m_poInputConfig->vSetKey("active_joypad", m_iJoypadMin );
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csPrefix[20];
    snprintf(csPrefix, sizeof(csPrefix), "joypadSDL%d_", i);
    std::string sPrefix(csPrefix);

    for (int j = 0; j < SDLBUTTONS_NUM; j++)
    {
    	m_poInputConfig->vSetKey(sPrefix + m_astJoypad[j].m_csKey,
    			inputGetKeymap(PAD_DEFAULT, m_astJoypad[j].m_eKeyFlag));
    }
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
  float fValue;
  float fAdjusted;
  std::string sValue;

  // Directories section
  //
  sValue = m_poDirConfig->sGetKey("gb_roms");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("gb_roms", Glib::get_home_dir());
  }
  sValue = m_poDirConfig->sGetKey("gba_roms");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("gba_roms", Glib::get_home_dir());
  }
  sValue = m_poDirConfig->sGetKey("batteries");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("batteries", m_sUserDataDir);
  }
  sValue = m_poDirConfig->sGetKey("saves");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("saves", m_sUserDataDir);
  }
  sValue = m_poDirConfig->sGetKey("captures");
  if (sValue != "" && ! Glib::file_test(sValue, Glib::FILE_TEST_IS_DIR))
  {
    m_poDirConfig->vSetKey("captures", m_sUserDataDir);
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

  iValue = m_poDisplayConfig->oGetKey<int>("output");
  iAdjusted = CLAMP(iValue, m_iVideoOutputMin, m_iVideoOutputMax);
  if (iValue != iAdjusted)
  {
    m_poDisplayConfig->vSetKey("output", iAdjusted);
  }

  // Sound section
  //
  iValue = m_poSoundConfig->oGetKey<int>("quality");
  iAdjusted = CLAMP(iValue, m_iSoundQualityMin, m_iSoundQualityMax);
  if (iValue != iAdjusted)
  {
    m_poSoundConfig->vSetKey("quality", iAdjusted);
  }

  fValue = m_poSoundConfig->oGetKey<float>("volume");
  fAdjusted = CLAMP(fValue, m_fSoundVolumeMin, m_fSoundVolumeMax);
  if (fValue != fAdjusted)
  {
    m_poSoundConfig->vSetKey("volume", fAdjusted);
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

void Window::vHistoryAdd(const std::string & _rsFile)
{
  std::string sURL = "file://" + _rsFile;

  m_poRecentManager->add_item(sURL);
}

void Window::vLoadJoypadsFromConfig()
{
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
    char csPrefix[20];
    snprintf(csPrefix, sizeof(csPrefix), "joypadSDL%d_", i);
    std::string sPrefix(csPrefix);

    for (int j = 0; j < SDLBUTTONS_NUM; j++)
    {
    	inputSetKeymap((EPad)i, m_astJoypad[j].m_eKeyFlag,
    			m_poInputConfig->oGetKey<guint>(sPrefix + m_astJoypad[j].m_csKey));
    }
  }
}

void Window::vSaveJoypadsToConfig()
{
  for (int i = m_iJoypadMin; i <= m_iJoypadMax; i++)
  {
	char csPrefix[20];
	snprintf(csPrefix, sizeof(csPrefix), "joypadSDL%d_", i);
	std::string sPrefix(csPrefix);

	for (int j = 0; j < SDLBUTTONS_NUM; j++)
	{
		m_poInputConfig->vSetKey(sPrefix + m_astJoypad[j].m_csKey,
				inputGetKeymap((EPad)i, m_astJoypad[j].m_eKeyFlag));
	}
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
      gbReset();
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
  m_uiThrottleDelay = Glib::TimeVal(0, 0);

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
                             false,
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
                             false,
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
  m_poScreenArea->vDrawBlackScreen();
}

void Window::vSetDefaultTitle()
{
  set_title("VBA-M");
}

void Window::vShowSpeed(int _iSpeed)
{
  char csTitle[50];

  if (m_eShowSpeed == ShowPercentage)
  {
    snprintf(csTitle, 50, "VBA-M - %d%%", _iSpeed);
    set_title(csTitle);
  }
  else if (m_eShowSpeed == ShowDetailed)
  {
    snprintf(csTitle, 50, "VBA-M - %d%% (%d, %d fps)",
             _iSpeed, systemFrameSkip, systemFPS);
    set_title(csTitle);
  }
}

void Window::vComputeFrameskip(int _iRate)
{
  static Glib::TimeVal uiLastTime;
  static int iFrameskipAdjust = 0;

  Glib::TimeVal uiTime;
  uiTime.assign_current_time();

  if (m_bWasEmulating)
  {
    int iWantedSpeed = 100;

    if (systemThrottle > 0)
    {
      if (! speedup)
      {
        Glib::TimeVal uiDiff  = uiTime - m_uiThrottleLastTime;
        Glib::TimeVal iTarget = Glib::TimeVal(0, 1000 / (_iRate * systemThrottle));
        Glib::TimeVal iDelay  = iTarget - uiDiff;
        if (iDelay > Glib::TimeVal(0, 0))
        {
          m_uiThrottleDelay = iDelay;
        }
      }
      iWantedSpeed = systemThrottle;
    }

    if (m_bAutoFrameskip)
    {
      Glib::TimeVal uiDiff = uiTime - uiLastTime;
      int iSpeed = iWantedSpeed;

      if (uiDiff != Glib::TimeVal(0, 0))
      {
        iSpeed = (1000000 / _iRate) / (uiDiff.as_double() * 1000);
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
  std::string sDir = m_poDirConfig->sGetKey("captures");
  if (sDir == "")
  {
    sDir = m_sUserDataDir;
  }

  sBaseName = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile));


  char * csFile = g_strdup_printf("%s_%02d.png",
                                  sBaseName.c_str(),
                                  _iNum);

  m_stEmulator.emuWritePNG(csFile);

  g_free(csFile);
}

void Window::vCreateFileOpenDialog()
{
  if (m_poFileOpenDialog != NULL)
  {
    return;
  }

  std::string sGBDir  = m_poDirConfig->sGetKey("gb_roms");
  std::string sGBADir = m_poDirConfig->sGetKey("gba_roms");

  Gtk::FileChooserDialog * poDialog = new Gtk::FileChooserDialog(*this, _("Open"));
  poDialog->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  poDialog->add_button(Gtk::Stock::OPEN,   Gtk::RESPONSE_OK);

  try
  {
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
  }
  catch (const Gtk::FileChooserError& e)
  {
    // Most likely the shortcut already exists, so do nothing
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

  m_poFileOpenDialog = poDialog;
}

void Window::vLoadBattery()
{
  std::string sBattery;
  std::string sDir = m_poDirConfig->sGetKey("batteries");
  if (sDir == "")
  {
    sDir = m_sUserDataDir;
  }

  sBattery = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile)) + ".sav";

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
    sDir = m_sUserDataDir;
  }

  sBattery = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile)) + ".sav";

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

  m_oEmuSig = Glib::signal_idle().connect(sigc::mem_fun(*this, &Window::bOnEmuIdle),
                                          Glib::PRIORITY_DEFAULT_IDLE);
}

void Window::vStopEmu()
{
  m_oEmuSig.disconnect();
  m_bWasEmulating = false;
}

void Window::vSetThrottle(int _iPercent)
{
  systemThrottle = _iPercent;
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
    if (astThrottle[i].m_iThrottle == systemThrottle)
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
      sDir = m_sUserDataDir;
    }

    sFileBase = sDir + "/" + sCutSuffix(Glib::path_get_basename(m_sRomFile));

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

void Window::vToggleFullscreen()
{
  if(!m_bFullscreen)
  {
    fullscreen();
    m_poMenuBar->hide();
  }
  else
  {
    unfullscreen();
    m_poMenuBar->show();
  }
}

void Window::vSDLPollEvents()
{
  SDL_Event event;
  while(SDL_PollEvent(&event))
  {
    switch(event.type)
    {
      case SDL_JOYHATMOTION:
      case SDL_JOYBUTTONDOWN:
      case SDL_JOYBUTTONUP:
      case SDL_JOYAXISMOTION:
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        inputProcessSDLEvent(event);
        break;
    }
  }
}

} // VBA namespace
