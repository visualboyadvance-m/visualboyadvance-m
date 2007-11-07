////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1998 by Thierry Maurel
// All rights reserved
//
// Distribute freely, except: don't remove my name from the source or
// documentation (don't take credit for my work), mark your changes (don't
// get me blamed for your possible bugs), don't alter or remove this
// notice.
// No warrantee of any kind, express or implied, is included with this
// software; use at your own risk, responsibility for damages (if any) to
// anyone resulting from the use of this software rests entirely with the
// user.
//
// Send bug reports, bug fixes, enhancements, requests, flames, etc., and
// I'll try to keep a version up to date.  I can be reached as follows:
//    tmaurel@caramail.com   (or tmaurel@hol.fr)
//
////////////////////////////////////////////////////////////////////////////////
// File    : CmdAccelOb.cpp
// Project : AccelsEditor
////////////////////////////////////////////////////////////////////////////////
// Version : 1.0                       * Author : T.Maurel
// Date    : 17.08.98
//
// Remarks : 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CmdAccelOb.h"

////////////////////////////////////////////////////////////////////////
//
//
MAPVIRTKEYS mapVirtKeys[] = {
  {VK_LBUTTON, "VK_LBUTTON"},
  {VK_RBUTTON, "VK_RBUTTON"},
  {VK_CANCEL, "VK_CANCEL"},
  {VK_MBUTTON, "VK_MBUTTON"},
  {VK_BACK, "BACK"},
  {VK_TAB, "TAB"},
  {VK_CLEAR, "VK_CLEAR"},
  {VK_RETURN, "RETURN"},
  {VK_SHIFT, "SHIFT"},
  {VK_CONTROL, "CONTROL"},
  {VK_MENU, "MENU"},
  {VK_PAUSE, "PAUSE"},
  {VK_CAPITAL, "CAPITAL"},
  {VK_ESCAPE, "ESCAPE"},
  {VK_SPACE, "SPACE"},
  {VK_PRIOR, "PRIOR"},
  {VK_NEXT, "NEXT"},
  {VK_END, "END"},
  {VK_HOME, "HOME"},
  {VK_LEFT, "LEFT"},
  {VK_UP, "UP"},
  {VK_RIGHT, "RIGHT"},
  {VK_DOWN, "DOWN"},
  {VK_SELECT, "VK_SELECT"},
  {VK_PRINT, "PRINT"},
  {VK_EXECUTE, "EXECUTE"},
  {VK_SNAPSHOT, "SNAPSHOT"},
  {VK_INSERT, "INSERT"},
  {VK_DELETE, "DELETE"},
  {VK_HELP, "VK_HELP"},
  {WORD('0'), "0"},
  {WORD('1'), "1"},
  {WORD('2'), "2"},
  {WORD('3'), "3"},
  {WORD('4'), "4"},
  {WORD('5'), "5"},
  {WORD('6'), "6"},
  {WORD('7'), "7"},
  {WORD('8'), "8"},
  {WORD('9'), "9"},
  {WORD('A'), "A"},
  {WORD('B'), "B"},
  {WORD('C'), "C"},
  {WORD('D'), "D"},
  {WORD('E'), "E"},
  {WORD('F'), "F"},
  {WORD('G'), "G"},
  {WORD('H'), "H"},
  {WORD('I'), "I"},
  {WORD('J'), "J"},
  {WORD('K'), "K"},
  {WORD('L'), "L"},
  {WORD('M'), "M"},
  {WORD('N'), "N"},
  {WORD('O'), "O"},
  {WORD('P'), "P"},
  {WORD('Q'), "Q"},
  {WORD('R'), "R"},
  {WORD('S'), "S"},
  {WORD('T'), "T"},
  {WORD('U'), "U"},
  {WORD('V'), "V"},
  {WORD('W'), "W"},
  {WORD('X'), "X"},
  {WORD('Y'), "Y"},
  {WORD('Z'), "Z"},
  {VK_LWIN, "VK_LWIN"},
  {VK_RWIN, "VK_RWIN"},
  {VK_APPS, "VK_APPS"},
  {VK_NUMPAD0, "NUMPAD0"},
  {VK_NUMPAD1, "NUMPAD1"},
  {VK_NUMPAD2, "NUMPAD2"},
  {VK_NUMPAD3, "NUMPAD3"},
  {VK_NUMPAD4, "NUMPAD4"},
  {VK_NUMPAD5, "NUMPAD5"},
  {VK_NUMPAD6, "NUMPAD6"},
  {VK_NUMPAD7, "NUMPAD7"},
  {VK_NUMPAD8, "NUMPAD8"},
  {VK_NUMPAD9, "NUMPAD9"},
  {VK_MULTIPLY, "MULTIPLY"},
  {VK_ADD, "ADD"},
  {VK_SEPARATOR, "SEPARATOR"},
  {VK_SUBTRACT, "SUBTRACT"},
  {VK_DECIMAL, "DECIMAL"},
  {VK_DIVIDE, "DIVIDE"},
  {VK_F1, "F1"},
  {VK_F2, "F2"},
  {VK_F3, "F3"},
  {VK_F4, "F4"},
  {VK_F5, "F5"},
  {VK_F6, "F6"},
  {VK_F7, "F7"},
  {VK_F8, "F8"},
  {VK_F9, "F9"},
  {VK_F10, "F10"},
  {VK_F11, "F11"},
  {VK_F12, "F12"},
  {VK_F13, "F13"},
  {VK_F14, "F14"},
  {VK_F15, "F15"},
  {VK_F16, "F16"},
  {VK_F17, "F17"},
  {VK_F18, "F18"},
  {VK_F19, "F19"},
  {VK_F20, "F20"},
  {VK_F21, "F21"},
  {VK_F22, "F22"},
  {VK_F23, "F23"},
  {VK_F24, "F24"},
  {VK_NUMLOCK, "NUMLOCK"},
  {VK_SCROLL, "VK_SCROLL"},
  {VK_ATTN, "VK_ATTN"},
  {VK_CRSEL, "VK_CRSEL"},
  {VK_EXSEL, "VK_EXSEL"},
  {VK_EREOF, "VK_EREOF"},
  {VK_PLAY, "VK_PLAY"},
  {VK_ZOOM, "VK_ZOOM"},
  {VK_NONAME, "VK_NONAME"},
  {VK_PA1, "VK_PA1"},
  {VK_OEM_CLEAR, "VK_OEM_CLEAR"},
};


////////////////////////////////////////////////////////////////////////
//
//
MAPVIRTKEYS mapVirtSysKeys[] = {
  {FCONTROL, "Ctrl"},
  {FALT, "Alt"},
  {FSHIFT, "Shift"},
};


////////////////////////////////////////////////////////////////////////
// helper fct for external access
////////////////////////////////////////////////////////////////////////
//
//
TCHAR* mapVirtKeysStringFromWORD(WORD wKey)
{
  for (int index = 0; index < sizeof(mapVirtKeys)/sizeof(mapVirtKeys[0]); index++) {
    if (mapVirtKeys[index].wKey == wKey)
      return mapVirtKeys[index].szKey;
  }
  return NULL;
}



////////////////////////////////////////////////////////////////////////
//
#define DEFAULT_ACCEL   0x01
#define USER_ACCEL              0x02


////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
//
//
CAccelsOb::CAccelsOb()
{
  m_cVirt = 0;
  m_wKey = 0;
  m_bLocked = false;
}


////////////////////////////////////////////////////////////////////////
//
//
CAccelsOb::CAccelsOb(CAccelsOb* pFrom)
{
  ASSERT(pFrom != NULL);
  
  m_cVirt = pFrom->m_cVirt;
  m_wKey = pFrom->m_wKey;
  m_bLocked = pFrom->m_bLocked;
}


////////////////////////////////////////////////////////////////////////
//
//
CAccelsOb::CAccelsOb(BYTE cVirt, WORD wKey, bool bLocked)
{
  m_cVirt = cVirt;
  m_wKey = wKey;
  m_bLocked = bLocked;
}


////////////////////////////////////////////////////////////////////////
//
//
CAccelsOb::CAccelsOb(LPACCEL pACCEL)
{
  ASSERT(pACCEL != NULL);
  
  m_cVirt = pACCEL->fVirt;
  m_wKey = pACCEL->key;
  m_bLocked = false;
}


////////////////////////////////////////////////////////////////////////
//
//
CAccelsOb& CAccelsOb::operator=(const CAccelsOb& from)
{
  m_cVirt = from.m_cVirt;
  m_wKey = from.m_wKey;
  m_bLocked = from.m_bLocked;
  
  return *this;
}


////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
//
//
void CAccelsOb::GetString(CString& szBuffer)
{
  szBuffer = "";
  // in case of the object is not assigned, we avoid error messages
  if (m_wKey == 0)
    return;

  // modifiers part
  int i;
  for (i = 0; i < sizetable(mapVirtSysKeys); i++) {
    if (m_cVirt & mapVirtSysKeys[i].wKey) {
      szBuffer += mapVirtSysKeys[i].szKey;
      szBuffer += "+";
    }
  }
  // and virtual key part
  for (i = 0; i < sizetable(mapVirtKeys); i++) {
    if (m_wKey == mapVirtKeys[i].wKey) {
      szBuffer += mapVirtKeys[i].szKey;
      return;
    }
  }
  AfxMessageBox("Internal error : (CAccelsOb::GetString) m_wKey invalid");
}


////////////////////////////////////////////////////////////////////////
//
//
bool CAccelsOb::IsEqual(WORD wKey, bool bCtrl, bool bAlt, bool bShift)
{
  //        CString szTemp;
  //        GetString(szTemp);

  
  bool m_bCtrl = (m_cVirt & FCONTROL) ? true : false;
  bool bRet = (bCtrl == m_bCtrl);
  
  bool m_bAlt = (m_cVirt & FALT) ? true : false;
  bRet &= (bAlt == m_bAlt);
  
  bool m_bShift = (m_cVirt & FSHIFT) ? true : false;
  bRet &= (bShift == m_bShift);
  
  bRet &= static_cast<bool>(m_wKey == wKey);
  
  return bRet;
}


////////////////////////////////////////////////////////////////////////
//
//
DWORD CAccelsOb::GetData()
{
  BYTE cLocalCodes = 0;
  if (m_bLocked)
    cLocalCodes = DEFAULT_ACCEL;
  else
    cLocalCodes = USER_ACCEL;
  
  WORD bCodes = MAKEWORD(m_cVirt, cLocalCodes);
  return MAKELONG(m_wKey, bCodes);
}


////////////////////////////////////////////////////////////////////////
//
//
bool CAccelsOb::SetData(DWORD dwDatas)
{
  m_wKey = LOWORD(dwDatas);
  
  WORD bCodes = HIWORD(dwDatas);
  m_cVirt = LOBYTE(bCodes);
  
  BYTE cLocalCodes = HIBYTE(bCodes);
  m_bLocked = static_cast<bool>(cLocalCodes == DEFAULT_ACCEL);
  return true;
}

////////////////////////////////////////////////////////////////////////
//
#ifdef _DEBUG
////////////////////////////////////////////////////////////////////////
//
//
void CAccelsOb::AssertValid() const
{
  CObject::AssertValid();
}

////////////////////////////////////////////////////////////////////////
//
//
void CAccelsOb::Dump(CDumpContext& dc) const
{
  dc << "\t\t";
  CObject::Dump(dc);
  dc << "\t\tlocked=" << m_bLocked << ", cVirt=" << m_cVirt << ", wKey=" << m_wKey << "\n\n";

}
#endif

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
//
//
CCmdAccelOb::CCmdAccelOb()
{
}


////////////////////////////////////////////////////////////////////////
//
//
CCmdAccelOb::CCmdAccelOb(WORD wIDCommand, LPCTSTR szCommand)
{
  ASSERT(szCommand != NULL);

  m_wIDCommand = wIDCommand;
  m_szCommand = szCommand;
}


////////////////////////////////////////////////////////////////////////
//
//
CCmdAccelOb::CCmdAccelOb(BYTE cVirt, WORD wIDCommand, WORD wKey, LPCTSTR szCommand, bool bLocked)
{
  ASSERT(szCommand != NULL);
  
  m_wIDCommand = wIDCommand;
  m_szCommand = szCommand;
  
  CAccelsOb* pAccel = DEBUG_NEW CAccelsOb(cVirt, wKey, bLocked);
  ASSERT(pAccel != NULL);
  m_Accels.AddTail(pAccel);
}


////////////////////////////////////////////////////////////////////////
//
//
CCmdAccelOb::~CCmdAccelOb()
{
  POSITION pos = m_Accels.GetHeadPosition();
  while (pos != NULL)
    delete m_Accels.GetNext(pos);
  m_Accels.RemoveAll();
}


////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
//
//
void CCmdAccelOb::Add(BYTE cVirt, WORD wKey, bool bLocked)
{
  CAccelsOb* pAccel = DEBUG_NEW CAccelsOb(cVirt, wKey, bLocked);
  ASSERT(pAccel != NULL);
  m_Accels.AddTail(pAccel);
}


////////////////////////////////////////////////////////////////////////
//
//
void CCmdAccelOb::Add(CAccelsOb* pAccel)
{
  ASSERT(pAccel != NULL);
  m_Accels.AddTail(pAccel);
}


////////////////////////////////////////////////////////////////////////
//
//
CCmdAccelOb& CCmdAccelOb::operator=(const CCmdAccelOb& from)
{
  Reset();
  
  m_wIDCommand = from.m_wIDCommand;
  m_szCommand = from.m_szCommand;
  
  CAccelsOb* pAccel;
  POSITION pos = from.m_Accels.GetHeadPosition();
  while (pos != NULL) {
    pAccel = DEBUG_NEW CAccelsOb(from.m_Accels.GetNext(pos));
    ASSERT(pAccel != NULL);
    m_Accels.AddTail(pAccel);
  }
  return *this;
}


////////////////////////////////////////////////////////////////////////
//
//
void CCmdAccelOb::DeleteUserAccels()
{
  CAccelsOb* pAccel;
  POSITION prevPos;
  POSITION pos = m_Accels.GetHeadPosition();
  while (pos != NULL) {
    prevPos = pos;
    pAccel = m_Accels.GetNext(pos);
    if (!pAccel->m_bLocked) {
      delete pAccel;
      m_Accels.RemoveAt(prevPos);
    }
  }
}


////////////////////////////////////////////////////////////////////////
//
//
void CCmdAccelOb::Reset()
{
  m_wIDCommand = 0;
  m_szCommand = "Empty command";
  
  CAccelsOb* pAccel;
  POSITION pos = m_Accels.GetHeadPosition();
  while (pos != NULL) {
    pAccel = m_Accels.GetNext(pos);
    delete pAccel;
  }
}

////////////////////////////////////////////////////////////////////////
//
#ifdef _DEBUG
////////////////////////////////////////////////////////////////////////
//
//
void CCmdAccelOb::AssertValid() const
{
  // call base class function first
  CObject::AssertValid();
}


////////////////////////////////////////////////////////////////////////
//
//
void CCmdAccelOb::Dump( CDumpContext& dc ) const
{
  // call base class function first
  dc << "\t";
  CObject::Dump( dc );

  // now do the stuff for our specific class
  dc << "\tIDCommand = " << m_wIDCommand;
  dc << "\n\tszCommand = " << m_szCommand;
  dc << "\n\tAccelerators = {\n";

  CAccelsOb* pAccel;
  POSITION pos = m_Accels.GetHeadPosition();
  while (pos != NULL) {
    pAccel = m_Accels.GetNext(pos);
    dc << pAccel;
  }
  dc << "\t}\n";
}
#endif
