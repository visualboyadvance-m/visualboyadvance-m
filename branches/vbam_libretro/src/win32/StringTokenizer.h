#if !defined(AFX_STRINGTOKENIZER_H__1AB4CD12_6B7A_49E4_A87F_75D3DC3FF20F__INCLUDED_)
#define AFX_STRINGTOKENIZER_H__1AB4CD12_6B7A_49E4_A87F_75D3DC3FF20F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class StringTokenizer
{
 public:
  const char * next();
  StringTokenizer(CString str, CString token);
  virtual ~StringTokenizer();

 private:
  CString m_token;
  CString m_delim;
  CString m_right;
};

#endif // !defined(AFX_STRINGTOKENIZER_H__1AB4CD12_6B7A_49E4_A87F_75D3DC3FF20F__INCLUDED_)
