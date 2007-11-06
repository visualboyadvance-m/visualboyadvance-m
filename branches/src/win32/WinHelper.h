/*----------------------------------------------------------------------
  Copyright (c) 1998 Gipsysoft. All Rights Reserved.
  Please see the file "licence.txt" for licencing details.
  File:   WinHelper.h
  Owner:  russf@gipsysoft.com
  Purpose:        Windows helper functions, classes, structures and macros
  that make life a little easier
  These should all be zero impact classes etc. that is they 
  should *not* have a cpp file associated with them.
  ----------------------------------------------------------------------*/
#ifndef WINHELPER_H
#define WINHELPER_H

//#ifndef DEBUGHLP_H
//      #include <DebugHlp.h>
//#endif        //      DEBUGHLP_H

#ifndef FASTCALL
#define FASTCALL
#endif  //      FASTCALL

extern void AssertFailed(char *, int, char *);
extern void ApiFailure(char *, int, char *);

#define R_VERIFY(a) R_ASSERT(a)
#define R_ASSERT(a) \
  do {\
    if(!(a)) {\
      AssertFailed(__FILE__, __LINE__, #a);\
    }\
  } while(0);

#define VAPI(a) \
  do { \
    if(!(a)) {\
     ApiFailure(__FILE__, __LINE__, #a); \
    }\
  } while (0);

#define ASSERT_VALID_HWND(a) ASSERT( ::IsWindow(a) )

namespace WinHelper
{

  class CSize : public tagSIZE
    //
    //      Wrapper for the SIZE structure
    {
    public:
      inline CSize() {};
      inline explicit CSize( const SIZE &size ) { cx = size.cx; cy = size.cy; }
      inline explicit CSize( long nSizeX, long nSizeY ) { cx = nSizeX; cy = nSizeY; }
      inline void Set( long nSizeX, long nSizeY ) { cx = nSizeX; cy = nSizeY; }
      inline operator LPSIZE() { return this; };

      inline bool operator !=( const SIZE &size ) const { return cx != size.cx || cy != size.cy;}
      inline CSize & operator =( const SIZE &size ) { cx = size.cx; cy = size.cy; return *this; }
      inline void Empty() { cx = cy = 0; }
    };


  class CRect : public tagRECT
    //
    //      Wrapper for a RECT structure
    {
    public:
      inline CRect() {}
      //      Initialisation constructor
      inline explicit CRect( const RECT& rhs ) { Set( rhs.left, rhs.top, rhs.right, rhs.bottom );}
      inline CRect(int xLeft, int yTop, int xRight, int yBottom) { Set( xLeft, yTop, xRight, yBottom ); }
      //      Get the width of the rectangle
      inline int Width() const { return right - left; }
      //      Get the height of the rectangle
      inline int Height() const { return bottom - top; }
      //      overloaded operator so you don't have to do &rc anymore
      inline operator LPCRECT() const { return this; };
      inline operator LPRECT() { return this; };
      //      Return the SIZE of the rectangle;
      inline CSize Size() const { CSize s( Width(), Height() ); return s; }
      //      Return the top left of the rectangle
      inline POINT TopLeft() const { POINT pt = { left, top }; return pt; }   
      //      Return the bottom right of the rectangle
      inline POINT BottomRight() const { POINT pt = { right, bottom }; return pt; }   
      //      Set the rectangles left, top, right and bottom
      inline void Set( int xLeft, int yTop, int xRight, int yBottom) { top = yTop; bottom = yBottom; right = xRight; left = xLeft; }
      //      Return true if the rectangle contains all zeros
      inline bool IsEmpty() const { return left == 0 && right == 0 && top == 0 && bottom == 0 ? true : false; }
      //      Zero out our rectangle
      inline void Empty() { left = right = top = bottom = 0; }
      //      Set the size of the rect but leave the top left position untouched.
      inline void SetSize( const CSize &size ) { bottom = top + size.cy; right = left + size.cx; }
      inline void SetSize( const SIZE &size ) { bottom = top + size.cy; right = left + size.cx; }
      inline void SetSize( int cx, int cy ) { bottom = top + cy; right = left + cx; }
      //      Move the rectangle by an offset
      inline void Offset( int cx, int cy )
        {
          top+=cy;
          bottom+=cy;
          right+=cx;
          left+=cx;
        }
      //      Inflate the rectangle by the cx and cy, use negative to shrink the rectangle
      inline void Inflate( int cx, int cy )
        {
          top-=cy;
          bottom+=cy;
          right+=cx;
          left-=cx;
        }
      //      Assignment from a RECT
      inline CRect &operator = ( const RECT&rhs )
        {
          left = rhs.left; top = rhs.top;
          right = rhs.right; bottom = rhs.bottom;
          return *this;
        }

      //      Return true if the point passed is within the rectangle
      inline bool PtInRect( const POINT &pt ) const   {       return  ( pt.x >= left && pt.x < right && pt.y >=top && pt.y < bottom ); }
      //      Return true if the rectangle passed overlaps this rectangle
      inline bool Intersect( const RECT &rc ) const { return ( rc.left < right && rc.right > left && rc.top < bottom && rc.bottom > top ); }
    };


  class CPoint : public tagPOINT
    //
    //      Wrapper for the POINT structure
    {
    public:
      inline CPoint() {};
      inline CPoint( LPARAM lParam ) { x = LOWORD( lParam ); y = HIWORD(lParam); }
      inline CPoint( int nX, int nY ) { x = nX; y = nY; }
      inline CPoint( const POINT &pt ) { x = pt.x; y = pt.y; }
      inline bool operator == ( const CPoint &rhs ) const { return x == rhs.x && y == rhs.y; }
      inline bool operator != ( const CPoint &rhs ) const { return x != rhs.x || y != rhs.y; }
      inline operator LPPOINT () { return this; }
    };


  class CScrollInfo : public tagSCROLLINFO
    {
    public:
      CScrollInfo( UINT fPassedMask ) { cbSize = sizeof( tagSCROLLINFO ); fMask = fPassedMask; }
    };


  class CCriticalSection
    //
    //      Simple crtical section handler/wrapper
    {
    public:
      inline CCriticalSection()       { ::InitializeCriticalSection(&m_sect); }
      inline ~CCriticalSection() { ::DeleteCriticalSection(&m_sect); }

      //      Blocking lock.
      inline void Lock()                      { ::EnterCriticalSection(&m_sect); }
      //      Unlock
      inline void Unlock()            { ::LeaveCriticalSection(&m_sect); }

      class CLock
        //
        //      Simple lock class for the critcal section
        {
        public:
          inline CLock( CCriticalSection &sect ) : m_sect( sect ) { m_sect.Lock(); }
          inline ~CLock() { m_sect.Unlock(); }
        private:
          CCriticalSection &m_sect;

          CLock();
          CLock( const CLock &);
          CLock& operator =( const CLock &);
        };

    private:
      CRITICAL_SECTION m_sect;

      CCriticalSection( const CCriticalSection & );
      CCriticalSection& operator =( const CCriticalSection & );
    };


#define ZeroStructure( t ) ZeroMemory( &t, sizeof( t ) )
#define countof( t )    (sizeof( (t) ) / sizeof( (t)[0] ) )
#define UNREF(P) UNREFERENCED_PARAMETER(P)

  inline bool IsShiftPressed()
    {
      return GetKeyState(VK_SHIFT) & 0x8000 ? true : false;
    }

  inline bool IsAltPressed()
    {
      return GetKeyState(VK_MENU) & 0x8000 ? true : false;
    }

  inline bool IsControlPressed()
    {
      return GetKeyState(VK_CONTROL) & 0x8000 ? true : false;
    }

  inline HICON LoadIcon16x16( HINSTANCE hInst, UINT uID )
    //
    //      Load a 16x16 icon from the same resource as the other size icons.
    {
      return reinterpret_cast<HICON>( ::LoadImage( hInst, MAKEINTRESOURCE( uID ), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR ) );
    }


  class CDeferWindowPos
    //
    //      Wrapper for the Begin, Defer and End WindowPos functions. Nothing glamorous.
    {
    public:
      inline CDeferWindowPos( const int nWindows = 1 ) : m_hdlDef( ::BeginDeferWindowPos( nWindows ) ) {}
      inline ~CDeferWindowPos() { R_VERIFY( ::EndDeferWindowPos( m_hdlDef ) ); }
      inline HDWP DeferWindowPos( HWND hWnd, HWND hWndInsertAfter , int x, int y, int cx, int cy, UINT uFlags )
        {
          return ::DeferWindowPos( m_hdlDef, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags );
        }
      inline HDWP DeferWindowPos( HWND hWnd, HWND hWndInsertAfter, const CRect &rc, UINT uFlags )
        {
          return ::DeferWindowPos( m_hdlDef, hWnd, hWndInsertAfter, rc.left, rc.top, rc.Width(), rc.Height(), uFlags );
        }

    private:
      HDWP m_hdlDef;
    };

}       //      WinHelper

#endif //WINHELPER_H
