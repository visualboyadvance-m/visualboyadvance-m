#include "stdafx.h"
#include "vba.h"
#include "WavWriter.h"

#include "../System.h"
#include "../Util.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

WavWriter::WavWriter()
{
  m_file = NULL;
  m_len = 0;
  m_posSize = 0;
}

WavWriter::~WavWriter()
{
  if(m_file)
    Close();
}

void WavWriter::Close()
{
  // calculate the total file length
  u32 len = ftell(m_file)-8;
  fseek(m_file, 4, SEEK_SET);
  u8 data[4];
  utilPutDword(data, len);
  fwrite(data, 1, 4, m_file);
  // write out the size of the data section
  fseek(m_file, m_posSize, SEEK_SET);
  utilPutDword(data, m_len);
  fwrite(data, 1, 4, m_file);
  fclose(m_file);
  m_file = NULL;
}

bool WavWriter::Open(const char *name)
{
  if(m_file)
    Close();
  m_file = fopen(name, "wb");

  if(!m_file)
    return false;
  // RIFF header
  u8 data[4] = { 'R', 'I', 'F', 'F' };
  fwrite(data, 1, 4, m_file);
  utilPutDword(data, 0);
  // write 0 for now. Will get filled during close
  fwrite(data, 1, 4, m_file);
  // write WAVE header
  u8 data2[4] = { 'W', 'A', 'V', 'E' };
  fwrite(data2, 1, 4, m_file);
  return true;
}

void WavWriter::SetFormat(const WAVEFORMATEX *format)
{
  if(m_file == NULL)
    return;
  // write fmt header
  u8 data[4] = { 'f', 'm', 't', ' ' };
  fwrite(data, 1, 4, m_file);
  u32 value = sizeof(WAVEFORMATEX);
  utilPutDword(data, value);
  fwrite(data, 1, 4, m_file);
  fwrite(format, 1, sizeof(WAVEFORMATEX), m_file);
  // start data header
  u8 data2[4] = { 'd', 'a', 't', 'a' };
  fwrite(data2, 1, 4, m_file);

  m_posSize = ftell(m_file);
  // write 0 for data chunk size. Filled out during Close()
  utilPutDword(data, 0);
  fwrite(data, 1, 4, m_file);
}

void WavWriter::AddSound(const u8 *data, int len)
{
  if(m_file == NULL)
    return;
  // write a block of sound data
  fwrite(data, 1, len, m_file);
  m_len += len;
}
