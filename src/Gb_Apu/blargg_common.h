
// Sets up common environment for Shay Green's libraries.
//
// To change configuration options, modify blargg_config.h, not this file.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

// HAVE_CONFIG_H: If defined, include user's "config.h" first (which *can*
// re-include blargg_common.h if it needs to)
#ifdef HAVE_CONFIG_H
	#undef BLARGG_COMMON_H
	#include "config.h"
	#define BLARGG_COMMON_H
#endif

// BLARGG_NONPORTABLE: If defined to 1, platform-specific (and possibly non-portable)
// optimizations are used. Defaults to off. Report any problems that occur only when
// this is enabled.
#ifndef BLARGG_NONPORTABLE
	#define BLARGG_NONPORTABLE 0
#endif

// BLARGG_BIG_ENDIAN, BLARGG_LITTLE_ENDIAN: Determined automatically, otherwise only
// one must be #defined to 1. Only needed if something actually depends on byte order.
#if !defined (BLARGG_BIG_ENDIAN) && !defined (BLARGG_LITTLE_ENDIAN)
	#if defined (MSB_FIRST) || defined (__powerc) || defined (macintosh) || \
			defined (WORDS_BIGENDIAN) || defined (__BIG_ENDIAN__)
		#define BLARGG_BIG_ENDIAN 1
	#else
		#define BLARGG_LITTLE_ENDIAN 1
	#endif
#endif

// Determine compiler's language support

// Metrowerks CodeWarrior
#if defined (__MWERKS__)
	#define BLARGG_COMPILER_HAS_NAMESPACE 1
	#if !__option(bool)
		#define BLARGG_COMPILER_HAS_BOOL 0
	#endif
	#define STATIC_CAST(T,expr) static_cast< T > (expr)

// Microsoft Visual C++
#elif defined (_MSC_VER)
	#if _MSC_VER < 1100
		#define BLARGG_COMPILER_HAS_BOOL 0
	#endif

// GNU C++
#elif defined (__GNUC__)
	#if __GNUC__ > 2
		#define BLARGG_COMPILER_HAS_NAMESPACE 1
	#endif

// Mingw
#elif defined (__MINGW32__)
	// empty

// Pre-ISO C++ compiler
#elif __cplusplus < 199711
	#ifndef BLARGG_COMPILER_HAS_BOOL
		#define BLARGG_COMPILER_HAS_BOOL 0
	#endif

#endif

/* BLARGG_COMPILER_HAS_BOOL: If 0, provides bool support for old compilers.
   If errors occur here, add the following line to your config.h file:
	#define BLARGG_COMPILER_HAS_BOOL 0
*/
#if defined (BLARGG_COMPILER_HAS_BOOL) && !BLARGG_COMPILER_HAS_BOOL
	typedef int bool;
	const bool true  = 1;
	const bool false = 0;
#endif

// BLARGG_USE_NAMESPACE: If 1, use <cxxx> headers rather than <xxxx.h>
#if BLARGG_USE_NAMESPACE || (!defined (BLARGG_USE_NAMESPACE) && BLARGG_COMPILER_HAS_NAMESPACE)
	#include <cstddef>
	#include <cstdlib>
	#include <cassert>
	#include <climits>
	#define STD std
#else
	#include <stddef.h>
	#include <stdlib.h>
	#include <assert.h>
	#include <limits.h>
	#define STD
#endif

// BLARGG_NEW is used in place of 'new' to create objects. By default, plain new is used.
// To prevent an exception if out of memory, #define BLARGG_NEW new (std::nothrow)
#ifndef BLARGG_NEW
	#define BLARGG_NEW new
#endif

// BOOST::int8_t etc.

// HAVE_STDINT_H: If defined, use <stdint.h> for int8_t etc.
#if defined (HAVE_STDINT_H)
	#include <stdint.h>
	#define BOOST

// HAVE_INTTYPES_H: If defined, use <stdint.h> for int8_t etc.
#elif defined (HAVE_INTTYPES_H)
	#include <inttypes.h>
	#define BOOST

#else
	struct BOOST
	{
		#if UCHAR_MAX == 0xFF && SCHAR_MAX == 0x7F
			typedef signed char     int8_t;
			typedef unsigned char   uint8_t;
		#else
			// No suitable 8-bit type available
			typedef struct see_blargg_common_h int8_t;
			typedef struct see_blargg_common_h uint8_t;
		#endif
		
		#if USHRT_MAX == 0xFFFF
			typedef short           int16_t;
			typedef unsigned short  uint16_t;
		#else
			// No suitable 16-bit type available
			typedef struct see_blargg_common_h int16_t;
			typedef struct see_blargg_common_h uint16_t;
		#endif
		
		#if ULONG_MAX == 0xFFFFFFFF
			typedef long            int32_t;
			typedef unsigned long   uint32_t;
		#elif UINT_MAX == 0xFFFFFFFF
			typedef int             int32_t;
			typedef unsigned int    uint32_t;
		#else
			// No suitable 32-bit type available
			typedef struct see_blargg_common_h int32_t;
			typedef struct see_blargg_common_h uint32_t;
		#endif
	};
#endif

// BLARGG_SOURCE_BEGIN: Library sources #include this after other #includes.
#ifndef BLARGG_SOURCE_BEGIN
	#define BLARGG_SOURCE_BEGIN "blargg_source.h"
#endif

// BLARGG_ENABLE_OPTIMIZER: Library sources #include this for speed-critical code
#ifndef BLARGG_ENABLE_OPTIMIZER
	#define BLARGG_ENABLE_OPTIMIZER "blargg_common.h"
#endif

// BLARGG_CPU_*: Used to select between some optimizations
#if !defined (BLARGG_CPU_POWERPC) && !defined (BLARGG_CPU_X86)
	#if defined (__powerc)
		#define BLARGG_CPU_POWERPC 1
	#elif defined (_MSC_VER) && defined (_M_IX86)
		#define BLARGG_CPU_X86 1
	#endif
#endif

// BOOST_STATIC_ASSERT( expr ): Generates compile error if expr is 0.
#ifndef BOOST_STATIC_ASSERT
	#ifdef _MSC_VER
		// MSVC6 (_MSC_VER < 1300) fails for use of __LINE__ when /Zl is specified
		#define BOOST_STATIC_ASSERT( expr ) \
			void blargg_failed_( int (*arg) [2 / ((expr) ? 1 : 0) - 1] )
	#else
		// Some other compilers fail when declaring same function multiple times in class,
		// so differentiate them by line
		#define BOOST_STATIC_ASSERT( expr ) \
			void blargg_failed_( int (*arg) [2 / ((expr) ? 1 : 0) - 1] [__LINE__] )
	#endif
#endif

// STATIC_CAST(T,expr): Used in place of static_cast<T> (expr)
#ifndef STATIC_CAST
	#define STATIC_CAST(T,expr) ((T) (expr))
#endif

// blargg_err_t (NULL on success, otherwise error string)
#ifndef blargg_err_t
	typedef const char* blargg_err_t;
#endif
const char* const blargg_success = 0;

// blargg_vector: Simple array that does *not* work for types with a constructor (non-POD).
template<class T>
class blargg_vector {
	T* begin_;
	STD::size_t size_;
public:
	blargg_vector() : begin_( 0 ), size_( 0 ) { }
	~blargg_vector() { STD::free( begin_ ); }
	
	typedef STD::size_t size_type;
	
	blargg_err_t resize( size_type n )
	{
		void* p = STD::realloc( begin_, n * sizeof (T) );
		if ( !p && n )
			return "Out of memory";
		begin_ = (T*) p;
		size_ = n;
		return 0;
	}
	
	void clear()
	{
		void* p = begin_;
		begin_ = 0;
		size_ = 0;
		STD::free( p );
	}
	
	size_type size() const { return size_; }
	
	T* begin() { return begin_; }
	T* end()   { return begin_ + size_; }
	
	const T* begin() const { return begin_; }
	const T* end() const   { return begin_ + size_; }
	
	T& operator [] ( size_type n )
	{
		assert( n <= size_ ); // allow for past-the-end value
		return begin_ [n];
	}
	
	const T& operator [] ( size_type n ) const
	{
		assert( n <= size_ ); // allow for past-the-end value
		return begin_ [n];
	}
};

#endif

