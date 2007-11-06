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

#include "configfile.h"

#include <string.h>
#include <glib.h>

#include <glibmm/fileutils.h>
#include <glibmm/iochannel.h>

namespace VBA
{
namespace Config
{

using std::string;
using Glib::IOChannel;

Line::Line(const string & _rsKey, const string & _rsValue) :
  m_sKey(_rsKey),
  m_sValue(_rsValue)
{
}

Section::Section(const string & _rsName) :
  m_sName(_rsName)
{
}

bool Section::bKeyExists(const string & _rsKey)
{
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->m_sKey == _rsKey)
    {
      return true;
    }
  }
  return false;
}

void Section::vSetKey(const string & _rsKey, const string & _rsValue)
{
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->m_sKey == _rsKey)
    {
      it->m_sValue = _rsValue;
      return;
    }
  }
  push_back(Line(_rsKey, _rsValue));
}

string Section::sGetKey(const string & _rsKey) const
{
  for (const_iterator it = begin(); it != end(); it++)
  {
    if (it->m_sKey == _rsKey)
    {
      return it->m_sValue;
    }
  }
  throw KeyNotFound(m_sName, _rsKey);
}

void Section::vRemoveKey(const string & _rsKey)
{
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->m_sKey == _rsKey)
    {
      erase(it);
      return;
    }
  }
}

File::File()
{
}

File::File(const string & _rsFile)
{
  vLoad(_rsFile);
}

File::~File()
{
}

bool File::bSectionExists(const string & _rsName)
{
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->sGetName() == _rsName)
    {
      return true;
    }
  }
  return false;
}

Section * File::poAddSection(const string & _rsName)
{
  Section * poSection = NULL;
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->sGetName() == _rsName)
    {
      poSection = &(*it);
    }
  }
  if (poSection == NULL)
  {
    push_back(Section(_rsName));
    poSection = &back();
  }
  return poSection;
}

Section * File::poGetSection(const string & _rsName)
{
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->sGetName() == _rsName)
    {
      return &(*it);
    }
  }
  throw SectionNotFound(_rsName);
}

void File::vRemoveSection(const string & _rsName)
{
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->sGetName() == _rsName)
    {
      erase(it);
      return;
    }
  }
}

void File::vLoad(const string & _rsFile,
                 bool _bAddSection,
                 bool _bAddKey)
{
  string sBuffer = Glib::file_get_contents(_rsFile);
  Section * poSection = NULL;
  char ** lines = g_strsplit(sBuffer.c_str(), "\n", 0);
  char * tmp;
  int i = 0;
  while (lines[i])
  {
    if (lines[i][0] == '[')
    {
      if ((tmp = strchr(lines[i], ']')))
      {
        *tmp = '\0';
        if (_bAddSection)
        {
          poSection = poAddSection(&lines[i][1]);
        }
        else
        {
          try
          {
            poSection = poGetSection(&lines[i][1]);
          }
          catch (...)
          {
            poSection = NULL;
          }
        }
      }
    }
    else if (lines[i][0] != '#' && poSection != NULL)
    {
      if ((tmp = strchr(lines[i], '=')))
      {
        *tmp = '\0';
        tmp++;
        if (_bAddKey || poSection->bKeyExists(lines[i]))
        {
          poSection->vSetKey(lines[i], tmp);
        }
      }
    }
    i++;
  }
  g_strfreev(lines);
}

void File::vSave(const string & _rsFile)
{
  Glib::RefPtr<IOChannel> poFile = IOChannel::create_from_file(_rsFile, "w");
  poFile->set_encoding("");

  for (const_iterator poSection = begin();
       poSection != end();
       poSection++)
  {
    string sName = "[" + poSection->sGetName() + "]\n";
    poFile->write(sName);

    for (Section::const_iterator poLine = poSection->begin();
         poLine != poSection->end();
         poLine++)
    {
      string sLine = poLine->m_sKey + "=" + poLine->m_sValue + "\n";
      poFile->write(sLine);
    }
    poFile->write("\n");
  }
}

void File::vClear()
{
  clear();
}

std::ostream & operator<<(std::ostream & _roOut, const File & _roFile)
{
  for (File::const_iterator poSection = _roFile.begin();
       poSection != _roFile.end();
       poSection++)
  {
    string sName = "[" + poSection->sGetName() + "]\n";
    _roOut << sName;

    for (Section::const_iterator poLine = poSection->begin();
         poLine != poSection->end();
         poLine++)
    {
      string sLine = poLine->m_sKey + "=" + poLine->m_sValue + "\n";
      _roOut << sLine;
    }
    _roOut << "\n";
  }
  return _roOut;
}

} // namespace Config
} // namespace VBA
