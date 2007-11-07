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

static char buffer[2048];
static HKEY vbKey = NULL;
static CString *regVbaPath = NULL;

#define VBA_PREF "preferences"

bool regEnabled = true;

void regInit(const char *path)
{
  DWORD disp = 0;
  LONG res = RegCreateKeyEx(HKEY_CURRENT_USER,
                            "Software\\Emulators\\VisualBoyAdvance",
                            0,
                            "",
                            REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS,
                            NULL,
                            &vbKey,
                            &disp);
  if( regVbaPath != NULL ) {
	  delete regVbaPath;
	  regVbaPath = NULL;
  }
  regVbaPath = new CString();
  regVbaPath->Format("%s\\vba.ini", path);
}

void regShutdown()
{
  LONG res = RegCloseKey(vbKey);
}

const char *regGetINIPath()
{
  return *regVbaPath;
}

char *regQueryStringValue(const char * key, char *def)
{
  if(regEnabled) {
    DWORD type = 0;
    DWORD size = 2048;
    
    LONG res = RegQueryValueEx(vbKey,
                               key,
                               NULL,
                               &type,
                               (UCHAR *)buffer,
                               &size);
    
    if(res == ERROR_SUCCESS && type == REG_SZ)
      return buffer;

    return def;
  }

  DWORD res = GetPrivateProfileString(VBA_PREF,
                                      key,
                                      def,
                                      (LPTSTR)buffer,
                                      2048,
                                      *regVbaPath);

  if(res)
    return buffer;

  return def;
}

DWORD regQueryDwordValue(const char * key, DWORD def, bool force)
{
  if(regEnabled || force) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD result = 0;
    
    LONG res = RegQueryValueEx(vbKey,
                               key,
                               NULL,
                               &type,
                               (UCHAR *)&result,
                               &size);
    
    if(res == ERROR_SUCCESS && type == REG_DWORD)
      return result;

    return def;
  }

  return GetPrivateProfileInt(VBA_PREF,
                              key,
                              def,
                              *regVbaPath);
}

BOOL regQueryBinaryValue(const char * key, char *value, int count)
{
  if(regEnabled) {
    DWORD type = 0;
    DWORD size = count;
    DWORD result = 0;
    
    
    LONG res = RegQueryValueEx(vbKey,
                               key,
                               NULL,
                               &type,
                               (UCHAR *)value,
                               &size);
    
    if(res == ERROR_SUCCESS && type == REG_BINARY)
      return TRUE;

    return FALSE;
  }
  CString k = key;
  k += "Count";
  int size = GetPrivateProfileInt(VBA_PREF,
                                  k,
                                  -1,
                                  *regVbaPath);
  if(size >= 0 && size < count)
    count = size;
  return GetPrivateProfileStruct(VBA_PREF,
                                 key,
                                 value,
                                 count,
                                 *regVbaPath);
}

void regSetStringValue(const char * key, const char * value)
{
  if(regEnabled) {
    LONG res = RegSetValueEx(vbKey,
                             key,
                             NULL,
                             REG_SZ,
                             (const UCHAR *)value,
                             lstrlen(value)+1);
  } else {
    WritePrivateProfileString(VBA_PREF,
                              key,
                              value,
                              *regVbaPath);
  }
}

void regSetDwordValue(const char * key, DWORD value, bool force)
{
  if(regEnabled || force) {
    LONG res = RegSetValueEx(vbKey,
                             key,
                             NULL,
                             REG_DWORD,
                             (const UCHAR *)&value,
                             sizeof(DWORD));
  } else {
    wsprintf(buffer, "%u", value);
    WritePrivateProfileString(VBA_PREF,
                              key,
                              buffer,
                              *regVbaPath);
  }
}

void regSetBinaryValue(const char *key, char *value, int count)
{
  if(regEnabled) {
    LONG res = RegSetValueEx(vbKey,
                             key,
                             NULL,
                             REG_BINARY,
                             (const UCHAR *)value,
                             count);
  } else {
    CString k = key;
    k += "Count";
    wsprintf(buffer, "%u", count);
    
    WritePrivateProfileString(VBA_PREF,
                              k,
                              buffer,
                              *regVbaPath);
                           
    WritePrivateProfileStruct(VBA_PREF,
                              key,
                              value,
                              count,
                              *regVbaPath);
  }
}

void regDeleteValue(char *key)
{
  if(regEnabled) {
    LONG res = RegDeleteValue(vbKey,
                              key);
  } else {
    WritePrivateProfileString(VBA_PREF,
                              key,
                              NULL,
                              *regVbaPath);
  }
}

bool regCreateFileType(const char *ext, const char *type)
{
  DWORD disp = 0;
  HKEY key;
  LONG res = RegCreateKeyEx(HKEY_CLASSES_ROOT,
                            ext,
                            0,
                            "",
                            REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS,
                            NULL,
                            &key,
                            &disp);
  if(res == ERROR_SUCCESS) {
    res = RegSetValueEx(key,
                        "",
                        0,
                        REG_SZ,
                        (const UCHAR *)type,
                        lstrlen(type)+1);
    RegCloseKey(key);
    return true;
  }
  return false;
}

bool regAssociateType(const char *type, const char *desc, const char *application)
{
  DWORD disp = 0;
  HKEY key;
  LONG res = RegCreateKeyEx(HKEY_CLASSES_ROOT,
                            type,
                            0,
                            "",
                            REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS,
                            NULL,
                            &key,
                            &disp);
  if(res == ERROR_SUCCESS) {
    res = RegSetValueEx(key,
                        "",
                        0,
                        REG_SZ,
                        (const UCHAR *)desc,
                        lstrlen(desc)+1);
    HKEY key2;
    res = RegCreateKeyEx(key,
                         "Shell\\Open\\Command",
                         0,
                         "",
                         REG_OPTION_NON_VOLATILE,
                         KEY_ALL_ACCESS,
                         NULL,
                         &key2,
                         &disp);
    if(res == ERROR_SUCCESS) {
      res = RegSetValueEx(key2,
                          "",
                          0,
                          REG_SZ,
                          (const UCHAR *)application,
                          lstrlen(application)+1);
      RegCloseKey(key2);
      RegCloseKey(key);
      return true;
    }
    
    RegCloseKey(key);
  }
  return false;
}

static void regExportSettingsToINI(HKEY key, const char *section)
{
  char valueName[256];
  int index = 0;
  while(1) {
    DWORD nameSize = 256;
    DWORD size = 2048;
    DWORD type;
    LONG res = RegEnumValue(key,
                            index,
                            valueName,
                            &nameSize,
                            NULL,
                            &type,
                            (LPBYTE)buffer,
                            &size);
      
    if(res == ERROR_SUCCESS) {
      switch(type) {
      case REG_DWORD:
        {
          char temp[256];
          wsprintf(temp, "%u", *((DWORD *)buffer));
          WritePrivateProfileString(section,
                                    valueName,
                                    temp,
                                    *regVbaPath);
        }
        break;
      case REG_SZ:
        WritePrivateProfileString(section,
                                  valueName,
                                  buffer,
                                  *regVbaPath);
        break;
      case REG_BINARY:
        {
          char temp[256];
          
          wsprintf(temp, "%u", size);
          CString k = valueName;
          k += "Count";
          WritePrivateProfileString(section,
                                    k,
                                    temp,
                                    *regVbaPath);
          WritePrivateProfileStruct(section,
                                    valueName,
                                    buffer,
                                    size,
                                    *regVbaPath);
        }
        break;
      }
      index++;
    } else
      break;
  }
}

void regExportSettingsToINI()
{ 
  if(vbKey != NULL) {
    regExportSettingsToINI(vbKey, VBA_PREF);
  }

  HKEY key;

  if(RegOpenKey(HKEY_CURRENT_USER, 
                "Software\\Emulators\\VisualBoyAdvance\\Viewer", &key) ==
     ERROR_SUCCESS) {
    regExportSettingsToINI(key, "Viewer");
    RegCloseKey(key);
  }
}
