#include "stdafx.h"
#include "MainWnd.h"

#include "AboutDialog.h"

extern int emulating;

void MainWnd::OnHelpAbout()
{
  AboutDialog dlg;

  dlg.DoModal();
}

void MainWnd::OnHelpFaq()
{
  ::ShellExecute(0, _T("open"), "http://vba-m.ngemu.com/forum/",
                 0, 0, SW_SHOWNORMAL);
}

void MainWnd::OnHelpBugreport()
{
  ::ShellExecute(0, _T("open"), "http://sourceforge.net/tracker/?atid=1023154&group_id=212795&func=browse",
                 0, 0, SW_SHOWNORMAL);
}

void MainWnd::OnHelpGnupubliclicense()
{
  ::ShellExecute(0, _T("open"), "http://www.gnu.org/licenses/old-licenses/gpl-2.0.html",
                 0, 0, SW_SHOWNORMAL);
}
