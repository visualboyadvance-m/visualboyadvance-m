// -*- C++ -*-
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

#ifndef __VBA_CONFIGFILE_H__
#define __VBA_CONFIGFILE_H__

#include <list>
#include <string>
#include <sstream>
#include <ostream>

namespace VBA
{
namespace Config
{

class NotFound
{
public:
  virtual ~NotFound() {}

protected:
  NotFound() {}
};

class SectionNotFound : public NotFound
{
public:
  SectionNotFound(const std::string & _rsName) :
    m_sName(_rsName)
    {
    }
  virtual ~SectionNotFound() {}

  inline std::string sGetName() const { return m_sName; }

private:
  std::string m_sName;
};

class KeyNotFound : public NotFound
{
public:
  KeyNotFound(const std::string & _rsSection, const std::string & _rsKey) :
    m_sSection(_rsSection),
    m_sKey(_rsKey)
    {
    }
  virtual ~KeyNotFound() {}

  inline std::string sGetSection() const { return m_sSection; }
  inline std::string sGetKey() const { return m_sKey; }

private:
  std::string m_sSection;
  std::string m_sKey;
};

class Line
{
public:
  Line(const std::string & _rsKey, const std::string & _rsValue);

  std::string m_sKey;
  std::string m_sValue;
};

class Section : private std::list<Line>
{
public:
  explicit Section(const std::string & _rsName);

  inline std::string sGetName() const { return m_sName; }

  bool bKeyExists(const std::string & _rsKey);
  void vSetKey(const std::string & _rsKey, const std::string & _rsValue);
  std::string sGetKey(const std::string & _rsKey) const;
  void vRemoveKey(const std::string & _rsKey);

  template<typename T>
  void vSetKey(const std::string & _rsKey, const T & _rValue);

  template<typename T>
  T oGetKey(const std::string & _rsKey) const;

  // read only
  typedef std::list<Line>::const_iterator const_iterator;
  inline const_iterator begin() const
    {
      return std::list<Line>::begin();
    }
  inline const_iterator end() const
    {
      return std::list<Line>::end();
    }

private:
  inline iterator begin()
    {
      return std::list<Line>::begin();
    }
  inline iterator end()
    {
      return std::list<Line>::end();
    }

  std::string m_sName;
};

class File : private std::list<Section>
{
public:
  File();
  File(const std::string & _rsFile);
  virtual ~File();

  bool bSectionExists(const std::string & _rsName);
  Section * poAddSection(const std::string & _rsName);
  Section * poGetSection(const std::string & _rsName);
  void vRemoveSection(const std::string & _rsName);
  void vLoad(const std::string & _rsFile,
             bool _bAddSection = true,
             bool _bAddKey = true);
  void vSave(const std::string & _rsFile);
  void vClear();

  // read only
  typedef std::list<Section>::const_iterator const_iterator;
  inline const_iterator begin() const
    {
      return std::list<Section>::begin();
    }
  inline const_iterator end() const
    {
      return std::list<Section>::end();
    }

private:
  inline iterator begin()
    {
      return std::list<Section>::begin();
    }
  inline iterator end()
    {
      return std::list<Section>::end();
    }
};

// debug
std::ostream & operator<<(std::ostream & _roOut, const File & _roConfig);

template<typename T>
void Section::vSetKey(const std::string & _rsKey, const T & _rValue)
{
  std::ostringstream oOut;
  oOut << _rValue;
  for (iterator it = begin(); it != end(); it++)
  {
    if (it->m_sKey == _rsKey)
    {
      it->m_sValue = oOut.str();
      return;
    }
  }
  push_back(Line(_rsKey, oOut.str()));
}

template<typename T>
T Section::oGetKey(const std::string & _rsKey) const
{
  T oValue;
  for (const_iterator it = begin(); it != end(); it++)
  {
    if (it->m_sKey == _rsKey)
    {
      std::istringstream oIn(it->m_sValue);
      oIn >> oValue;
      return oValue;
    }
  }
  throw KeyNotFound(m_sName, _rsKey);
}

} // namespace Config
} // namespace VBA


#endif // __VBA_CONFIGFILE_H__
