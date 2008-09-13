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
#include "../dmg/gbSound.h"
#include "../Sound.h"
#include "../Util.h"
#include "../sdl/inputSDL.h"

#include "tools.h"
#include "intl.h"
#include "joypadconfig.h"

namespace VBA
{

void Window::vOnFileOpen()
{
  while (m_poFileOpenDialog->run() == Gtk::RESPONSE_OK)
  {
    if (bLoadROM(m_poFileOpenDialog->get_filename()))
    {
      break;
    }
  }
  m_poFileOpenDialog->hide();
}

void Window::vOnFileLoad()
{
  std::string sSaveDir = m_poDirConfig->sGetKey("saves");

  Gtk::FileChooserDialog oDialog(*this, _("Load game"));
  oDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  oDialog.add_button(Gtk::Stock::OPEN,   Gtk::RESPONSE_OK);

  if (sSaveDir == "")
  {
    oDialog.set_current_folder(Glib::path_get_dirname(m_sRomFile));
  }
  else
  {
    oDialog.set_current_folder(sSaveDir);
    oDialog.add_shortcut_folder(sSaveDir);
  }

  Gtk::FileFilter oSaveFilter;
  oSaveFilter.set_name(_("VisualBoyAdvance save game"));
  oSaveFilter.add_pattern("*.[sS][gG][mM]");

  oDialog.add_filter(oSaveFilter);

  while (oDialog.run() == Gtk::RESPONSE_OK)
  {
    if (m_stEmulator.emuReadState(oDialog.get_filename().c_str()))
    {
      break;
    }
  }
}

void Window::vOnFileSave()
{
  Glib::ustring sSaveDir = m_poDirConfig->sGetKey("saves");

  Gtk::FileChooserDialog oDialog(*this, _("Save game"),
                                 Gtk::FILE_CHOOSER_ACTION_SAVE);
  oDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  oDialog.add_button(Gtk::Stock::SAVE,   Gtk::RESPONSE_OK);

  if (sSaveDir == "")
  {
    oDialog.set_current_folder(Glib::path_get_dirname(m_sRomFile));
  }
  else
  {
    oDialog.set_current_folder(sSaveDir);
    oDialog.add_shortcut_folder(sSaveDir);
  }
  oDialog.set_current_name(sCutSuffix(Glib::path_get_basename(m_sRomFile)));

  Gtk::FileFilter oSaveFilter;
  oSaveFilter.set_name(_("VisualBoyAdvance save game"));
  oSaveFilter.add_pattern("*.[sS][gG][mM]");

  oDialog.add_filter(oSaveFilter);

  while (oDialog.run() == Gtk::RESPONSE_OK)
  {
    Glib::ustring sFile = oDialog.get_filename();
    if (! bHasSuffix(sFile, ".sgm", false))
    {
      sFile += ".sgm";
    }

    if (Glib::file_test(sFile, Glib::FILE_TEST_EXISTS))
    {
      Gtk::MessageDialog oConfirmDialog(*this,
                                        _("File already exists. Overwrite it?"),
                                        false,
                                        Gtk::MESSAGE_QUESTION,
                                        Gtk::BUTTONS_YES_NO);
      if (oConfirmDialog.run() != Gtk::RESPONSE_YES)
      {
        continue;
      }
    }

    if (m_stEmulator.emuWriteState(sFile.c_str()))
    {
      break;
    }
  }
}

void Window::vOnLoadGameMostRecent()
{
  int    iMostRecent = -1;
  time_t uiTimeMax = 0;

  for (int i = 0; i < 10; i++)
  {
    if (! m_astGameSlot[i].m_bEmpty
        && (iMostRecent < 0 || m_astGameSlot[i].m_uiTime > uiTimeMax))
    {
      iMostRecent = i;
      uiTimeMax = m_astGameSlot[i].m_uiTime;
    }
  }

  if (iMostRecent >= 0)
  {
    vOnLoadGame(iMostRecent + 1);
  }
}

void Window::vOnLoadGameAutoToggled(Gtk::CheckMenuItem * _poCMI)
{
  m_poCoreConfig->vSetKey("load_game_auto", _poCMI->get_active());
}

void Window::vOnLoadGame(int _iSlot)
{
  int i = _iSlot - 1;
  if (! m_astGameSlot[i].m_bEmpty)
  {
    m_stEmulator.emuReadState(m_astGameSlot[i].m_sFile.c_str());
    m_poFilePauseItem->set_active(false);
  }
}

void Window::vOnSaveGameOldest()
{
  int    iOldest = -1;
  time_t uiTimeMin = 0;

  for (int i = 0; i < 10; i++)
  {
    if (! m_astGameSlot[i].m_bEmpty
        && (iOldest < 0 || m_astGameSlot[i].m_uiTime < uiTimeMin))
    {
      iOldest = i;
      uiTimeMin = m_astGameSlot[i].m_uiTime;
    }
  }

  if (iOldest >= 0)
  {
    vOnSaveGame(iOldest + 1);
  }
  else
  {
    vOnSaveGame(1);
  }
}

void Window::vOnSaveGame(int _iSlot)
{
  int i = _iSlot - 1;
  m_stEmulator.emuWriteState(m_astGameSlot[i].m_sFile.c_str());
  vUpdateGameSlots();
}

void Window::vOnFilePauseToggled(Gtk::CheckMenuItem * _poCMI)
{
  m_bPaused = _poCMI->get_active();
  if (emulating)
  {
    if (m_bPaused)
    {
      vStopEmu();
      soundPause();
    }
    else
    {
      vStartEmu();
      soundResume();
    }
  }
}

void Window::vOnFileReset()
{
  if (emulating)
  {
    m_stEmulator.emuReset();
    m_poFilePauseItem->set_active(false);
  }
}

void Window::vOnRecentFile()
{
  Glib::ustring sURI = m_poRecentChooserMenu->get_current_uri();

  if (!sURI.empty())
  {
    std::string sFileName = Glib::filename_from_uri(sURI);
    bLoadROM(sFileName);
  }
}

void Window::vOnFileScreenCapture()
{
  std::string sCaptureDir = m_poDirConfig->sGetKey("captures");

  Gtk::FileChooserDialog oDialog(*this, _("Save screenshot"),
                                 Gtk::FILE_CHOOSER_ACTION_SAVE);
  oDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  oDialog.add_button(Gtk::Stock::SAVE,   Gtk::RESPONSE_OK);

  if (sCaptureDir == "")
  {
    oDialog.set_current_folder(Glib::path_get_dirname(m_sRomFile));
  }
  else
  {
    oDialog.set_current_folder(sCaptureDir);
    oDialog.add_shortcut_folder(sCaptureDir);
  }
  oDialog.set_current_name(sCutSuffix(Glib::path_get_basename(m_sRomFile)));

  Gtk::FileFilter oPngFilter;
  oPngFilter.set_name(_("PNG image"));
  oPngFilter.add_pattern("*.[pP][nN][gG]");

  oDialog.add_filter(oPngFilter);

  while (oDialog.run() == Gtk::RESPONSE_OK)
  {
    Glib::ustring sFile = oDialog.get_filename();
    Glib::ustring sExt = ".png";

    if (! bHasSuffix(sFile, sExt, false))
    {
      sFile += sExt;
    }

    if (Glib::file_test(sFile, Glib::FILE_TEST_EXISTS))
    {
      Gtk::MessageDialog oConfirmDialog(*this,
                                        _("File already exists. Overwrite it?"),
                                        false,
                                        Gtk::MESSAGE_QUESTION,
                                        Gtk::BUTTONS_YES_NO);
      if (oConfirmDialog.run() != Gtk::RESPONSE_YES)
      {
        continue;
      }
    }

    if (m_stEmulator.emuWritePNG(sFile.c_str()))
    {
      break;
    }
  }
}

void Window::vOnFileClose()
{
  if (m_eCartridge != CartridgeNone)
  {
    soundPause();
    vStopEmu();
    vSetDefaultTitle();
    vDrawDefaultScreen();
    vSaveBattery();
    m_stEmulator.emuCleanUp();
    m_eCartridge = CartridgeNone;
    emulating = 0;

    vUpdateGameSlots();

    for (std::list<Gtk::Widget *>::iterator it = m_listSensitiveWhenPlaying.begin();
         it != m_listSensitiveWhenPlaying.end();
         it++)
    {
      (*it)->set_sensitive(false);
    }

    m_poFilePauseItem->set_active(false);
  }
}

void Window::vOnFileExit()
{
  hide();
}

void Window::vOnFrameskipToggled(Gtk::CheckMenuItem * _poCMI, int _iValue)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  if (_iValue >= 0 && _iValue <= 9)
  {
    m_poCoreConfig->vSetKey("frameskip", _iValue);
    gbFrameSkip      = _iValue;
    systemFrameSkip  = _iValue;
    m_bAutoFrameskip = false;
  }
  else
  {
    m_poCoreConfig->vSetKey("frameskip", "auto");
    gbFrameSkip      = 0;
    systemFrameSkip  = 0;
    m_bAutoFrameskip = true;
  }
}

void Window::vOnVideoFullscreen()
{
  vToggleFullscreen();
}

void Window::vOnVideoOutputToggled(Gtk::CheckMenuItem * _poCMI, int _iOutput)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  m_poDisplayConfig->vSetKey("output", _iOutput);

  vInitScreenArea((EVideoOutput)_iOutput);
}

void Window::vOnVideoScaleToggled(Gtk::CheckMenuItem * _poCMI, int _iScale)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  m_poDisplayConfig->vSetKey("scale", _iScale);
  vUpdateScreen();
}

void Window::vOnDirectories()
{
  struct
  {
    const char * m_csKey;
    const char * m_csLabel;
    const char * m_csFileChooserButton;
    Gtk::FileChooserButton * m_poFileChooserButton;
  }
  astRow[] =
  {
    { "gba_roms",  "GBA roms :",  "GBARomsDirEntry",   0 },
    { "gb_roms",   "GB roms :",   "GBRomsDirEntry",    0 },
    { "batteries", "Batteries :", "BatteriesDirEntry", 0 },
    { "saves",     "Saves :",     "SavesDirEntry",     0 },
    { "captures",  "Captures :",  "CapturesDirEntry",  0 }
  };

  Gtk::Table oTable(G_N_ELEMENTS(astRow), 2, false);
  oTable.set_border_width(5);
  oTable.set_spacings(5);

  for (guint i = 0; i < G_N_ELEMENTS(astRow); i++)
  {
    Gtk::Label * poLabel = Gtk::manage( new Gtk::Label(astRow[i].m_csLabel, Gtk::ALIGN_RIGHT) );
    astRow[i].m_poFileChooserButton = Gtk::manage( new Gtk::FileChooserButton(Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER) );
    astRow[i].m_poFileChooserButton->set_current_folder(m_poDirConfig->sGetKey(astRow[i].m_csKey));

    oTable.attach(* poLabel, 0, 1, i, i + 1);
    oTable.attach(* astRow[i].m_poFileChooserButton, 1, 2, i, i + 1);
  }

  Gtk::Dialog oDialog("Directories", true, true);
  oDialog.add_button(Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);
  oDialog.get_vbox()->pack_start(oTable);
  oDialog.set_transient_for(*this);
  oDialog.show_all_children();
  oDialog.run();

  for (guint i = 0; i < G_N_ELEMENTS(astRow); i++)
  {
    Glib::ustring sDir = astRow[i].m_poFileChooserButton->get_current_folder();

    m_poDirConfig->vSetKey(astRow[i].m_csKey, sDir);
  }

  // Needed if saves dir changed
  vUpdateGameSlots();
}

void Window::vOnPauseWhenInactiveToggled(Gtk::CheckMenuItem * _poCMI)
{
  m_poDisplayConfig->vSetKey("pause_when_inactive", _poCMI->get_active());
}

void Window::vOnSelectBios()
{
  Gtk::FileChooserDialog oDialog(*this, _("Select BIOS file"));
  oDialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  oDialog.add_button(Gtk::Stock::OPEN,   Gtk::RESPONSE_OK);

  if (m_poCoreConfig->sGetKey("bios_file") != "")
  {
    oDialog.set_filename(m_poCoreConfig->sGetKey("bios_file"));
  }

  const char * acsPattern[] =
  {
    "*.[bB][iI][nN]", "*.[aA][gG][bB]", "*.[gG][bB][aA]",
    "*.[bB][iI][oO][sS]", "*.[zZ][iI][pP]", "*.[zZ]", "*.[gG][zZ]"
  };

  Gtk::FileFilter oAllFilter;
  oAllFilter.set_name(_("All files"));
  oAllFilter.add_pattern("*");

  Gtk::FileFilter oBiosFilter;
  oBiosFilter.set_name(_("Gameboy Advance BIOS"));
  for (guint i = 0; i < G_N_ELEMENTS(acsPattern); i++)
  {
    oBiosFilter.add_pattern(acsPattern[i]);
  }

  oDialog.add_filter(oAllFilter);
  oDialog.add_filter(oBiosFilter);

  oDialog.set_filter(oBiosFilter);

  while (oDialog.run() == Gtk::RESPONSE_OK)
  {
    if (Glib::file_test(oDialog.get_filename(), Glib::FILE_TEST_IS_REGULAR))
    {
      m_poCoreConfig->vSetKey("bios_file", oDialog.get_filename());
      m_poUseBiosItem->set_sensitive();
      break;
    }
  }
}

void Window::vOnUseBiosToggled(Gtk::CheckMenuItem * _poCMI)
{
  m_poCoreConfig->vSetKey("use_bios_file", _poCMI->get_active());
}

void Window::vOnShowSpeedToggled(Gtk::CheckMenuItem * _poCMI, int _iShowSpeed)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  m_eShowSpeed = (EShowSpeed)_iShowSpeed;
  if (m_eShowSpeed == ShowNone)
  {
    vSetDefaultTitle();
  }
  m_poDisplayConfig->vSetKey("show_speed", _iShowSpeed);
}

void Window::vOnSaveTypeToggled(Gtk::CheckMenuItem * _poCMI, int _iSaveType)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  cpuSaveType = _iSaveType;
  m_poCoreConfig->vSetKey("save_type", _iSaveType);
}

void Window::vOnFlashSizeToggled(Gtk::CheckMenuItem * _poCMI, int _iFlashSize)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  if (_iFlashSize == 64)
  {
    flashSetSize(0x10000);
  }
  else
  {
    flashSetSize(0x20000);
  }
  m_poCoreConfig->vSetKey("flash_size", _iFlashSize);
}

void Window::vOnSoundMuteToggled(Gtk::CheckMenuItem * _poCMI)
{
  bool bMute = _poCMI->get_active();
  if (bMute)
  {
    soundSetEnable(0x000);
  }
  else
  {
    soundSetEnable(0x30f);
  }
  m_poSoundConfig->vSetKey("mute", bMute);
}

void Window::vOnSoundQualityToggled(Gtk::CheckMenuItem * _poCMI, int _iSoundQuality)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  m_eSoundQuality = (ESoundQuality)_iSoundQuality;
  if (m_eCartridge == CartridgeGBA)
  {
    soundSetQuality(_iSoundQuality);
  }
  else if (m_eCartridge == CartridgeGB)
  {
    gbSoundSetQuality(_iSoundQuality);
  }
  m_poSoundConfig->vSetKey("quality", _iSoundQuality);
}

void Window::vOnSoundVolumeToggled(Gtk::CheckMenuItem * _poCMI, float _fSoundVolume)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  soundSetVolume(_fSoundVolume);
  m_poSoundConfig->vSetKey("volume", _fSoundVolume);
}

void Window::vOnGBBorderToggled(Gtk::CheckMenuItem * _poCMI)
{
  gbBorderOn = _poCMI->get_active();
  if (emulating && m_eCartridge == CartridgeGB && _poCMI->get_active())
  {
    gbSgbRenderBorder();
  }
  vUpdateScreen();
  m_poCoreConfig->vSetKey("gb_border", _poCMI->get_active());
}

void Window::vOnGBPrinterToggled(Gtk::CheckMenuItem * _poCMI)
{
  if (_poCMI->get_active())
  {
    gbSerialFunction = gbPrinterSend;
  }
  else
  {
    gbSerialFunction = NULL;
  }
  m_poCoreConfig->vSetKey("gb_printer", _poCMI->get_active());
}

void Window::vOnEmulatorTypeToggled(Gtk::CheckMenuItem * _poCMI, int _iEmulatorType)
{
  gbEmulatorType = _iEmulatorType;
  m_poCoreConfig->vSetKey("emulator_type", _iEmulatorType);
}

void Window::vOnFilter2xToggled(Gtk::CheckMenuItem * _poCMI, int _iFilter2x)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  m_poScreenArea->vSetFilter2x((EFilter2x)_iFilter2x);
  if (emulating)
  {
    vDrawScreen();
  }
  m_poDisplayConfig->vSetKey("filter2x", _iFilter2x);
}

void Window::vOnFilterIBToggled(Gtk::CheckMenuItem * _poCMI, int _iFilterIB)
{
  if (! _poCMI->get_active())
  {
    return;
  }

  m_poScreenArea->vSetFilterIB((EFilterIB)_iFilterIB);
  if (emulating)
  {
    vDrawScreen();
  }
  m_poDisplayConfig->vSetKey("filterIB", _iFilterIB);
}

void Window::vOnJoypadConfigure()
{
  JoypadConfigDialog oDialog;
  oDialog.set_transient_for(*this);
  oDialog.run();

  m_poInputConfig->vSetKey("active_joypad", inputGetDefaultJoypad());
}

void Window::vOnAutofireToggled(Gtk::CheckMenuItem * _poCMI, EKey _eKey)
{
  if (_poCMI->get_active() == inputGetAutoFire(_eKey))
  {
    inputToggleAutoFire(_eKey);
  }

  std::string sKey;
  if (_eKey == KEY_BUTTON_A)
  {
    sKey = "autofire_A";
  }
  else if (_eKey == KEY_BUTTON_B)
  {
    sKey = "autofire_B";
  }
  else if (_eKey == KEY_BUTTON_L)
  {
    sKey = "autofire_L";
  }
  else if (_eKey == KEY_BUTTON_R)
  {
    sKey = "autofire_R";
  }
  m_poInputConfig->vSetKey(sKey, _poCMI->get_active());
}

void Window::vOnHelpAbout()
{
  Gtk::AboutDialog oAboutDialog;

  oAboutDialog.set_transient_for(*this);

  oAboutDialog.set_name("VBA-M");
  oAboutDialog.set_version(VERSION);
  oAboutDialog.set_comments(_("Nintendo GameBoy Advance emulator."));
  oAboutDialog.set_license("GPL");
  oAboutDialog.set_logo_icon_name("vbam");

  oAboutDialog.set_website("http://vba-m.ngemu.com/");

  std::list<Glib::ustring> list_authors;
  list_authors.push_back("Forgotten");
  list_authors.push_back("kxu");
  list_authors.push_back("Pokemonhacker");
  list_authors.push_back("Spacy51");
  list_authors.push_back("mudlord");
  list_authors.push_back("Nach");
  list_authors.push_back("jbo_85");
  list_authors.push_back("bgK");
  oAboutDialog.set_authors(list_authors);

  std::list<Glib::ustring> list_artists;
  list_artists.push_back("Matteo Drera");
  list_artists.push_back("Jakub Steiner");
  list_artists.push_back("Jones Lee");
  oAboutDialog.set_artists(list_artists);

  oAboutDialog.run();
}

bool Window::bOnEmuIdle()
{
  vSDLPollEvents();

  m_stEmulator.emuMain(m_stEmulator.emuCount);
  return true;
}

bool Window::on_focus_in_event(GdkEventFocus * _pstEvent)
{
  if (emulating
      && ! m_bPaused
      && m_poDisplayConfig->oGetKey<bool>("pause_when_inactive"))
  {
    vStartEmu();
    soundResume();
  }
  return false;
}

bool Window::on_focus_out_event(GdkEventFocus * _pstEvent)
{
  if (emulating
      && ! m_bPaused
      && m_poDisplayConfig->oGetKey<bool>("pause_when_inactive"))
  {
    vStopEmu();
    soundPause();
  }
  return false;
}

bool Window::on_key_press_event(GdkEventKey * _pstEvent)
{
  // The menu accelerators are disabled when it is hidden
  if (_pstEvent->keyval == GDK_F11 && !m_poMenuBar->is_visible())
  {
    vToggleFullscreen();
    return true;
  }

  // Forward the keyboard event to the input module by faking a SDL event
  SDL_Event event;
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = (SDLKey)_pstEvent->keyval;
  inputProcessSDLEvent(event);

  return Gtk::Window::on_key_press_event(_pstEvent);
}

bool Window::on_key_release_event(GdkEventKey * _pstEvent)
{
  // Forward the keyboard event to the input module by faking a SDL event
  SDL_Event event;
  event.type = SDL_KEYUP;
  event.key.keysym.sym = (SDLKey)_pstEvent->keyval;
  inputProcessSDLEvent(event);


  return Gtk::Window::on_key_release_event(_pstEvent);
}

bool Window::on_window_state_event(GdkEventWindowState* _pstEvent)
{
  if (_pstEvent->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
  {
    m_bFullscreen = _pstEvent->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;
  }

  return true;
}

} // namespace VBA
