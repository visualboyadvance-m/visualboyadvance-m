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

#include "stdafx.h"
#include "MainWnd.h"

#include "AboutDialog.h"
#include "BugReport.h"

extern int emulating;

void MainWnd::OnHelpAbout() 
{
  theApp.winCheckFullscreen();
  AboutDialog dlg;

  dlg.DoModal();
}

void MainWnd::OnHelpFaq() 
{
  ::ShellExecute(0, _T("open"), "http://vba.ngemu.com/faq.shtml", 
                 0, 0, SW_SHOWNORMAL);
}

void MainWnd::OnHelpBugreport() 
{
  BugReport dlg(theApp.m_pMainWnd);

  dlg.DoModal();
}

void MainWnd::OnHelpGnupubliclicense() 
{
  ::ShellExecute(0, _T("open"), "http://www.gnu.org/licenses/gpl.html", 
                 0, 0, SW_SHOWNORMAL);	
}
