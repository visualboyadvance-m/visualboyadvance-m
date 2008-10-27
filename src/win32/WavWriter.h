#if !defined(AFX_WAVWRITER_H__BE6C9DE9_60E7_4192_9797_8C7F55B3CE46__INCLUDED_)
#define AFX_WAVWRITER_H__BE6C9DE9_60E7_4192_9797_8C7F55B3CE46__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <mmreg.h>

class WavWriter
{
 private:
  FILE *m_file;
  int m_len;
  long m_posSize;

 public:
  WavWriter();
  ~WavWriter();

  bool Open(const char *name);
  void SetFormat(const WAVEFORMATEX *format);
  void AddSound(const u8 *data, int len);

 private:
  void Close();
};

#endif // !defined(AFX_WAVWRITER_H__BE6C9DE9_60E7_4192_9797_8C7F55B3CE46__INCLUDED_)
