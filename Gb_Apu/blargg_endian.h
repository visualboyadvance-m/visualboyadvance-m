
// CPU Byte Order Utilities

// Game_Music_Emu 0.3.0

#ifndef BLARGG_ENDIAN
#define BLARGG_ENDIAN

#include "blargg_common.h"

#if 0
	// Read 16/32-bit little-endian integer from memory
	unsigned      GET_LE16( void const* );
	unsigned long GET_LE32( void const* );

	// Read 16/32-bit big-endian integer from memory
	unsigned      GET_BE16( void const* );
	unsigned long GET_BE32( void const* );

	// Write 16/32-bit integer to memory in little-endian format
	void SET_LE16( void*, unsigned );
	void SET_LE32( void*, unsigned );
	
	// Write 16/32-bit integer to memory in big-endian format
	void SET_BE16( void*, unsigned long );
	void SET_BE32( void*, unsigned long );
#endif

inline unsigned get_le16( void const* p )
{
	return  ((unsigned char*) p) [1] * 0x100 +
			((unsigned char*) p) [0];
}

inline unsigned get_be16( void const* p )
{
	return  ((unsigned char*) p) [0] * 0x100 +
			((unsigned char*) p) [1];
}

inline unsigned long get_le32( void const* p )
{
	return  ((unsigned char*) p) [3] * 0x01000000 +
			((unsigned char*) p) [2] * 0x00010000 +
			((unsigned char*) p) [1] * 0x00000100 +
			((unsigned char*) p) [0];
}

inline unsigned long get_be32( void const* p )
{
	return  ((unsigned char*) p) [0] * 0x01000000 +
			((unsigned char*) p) [1] * 0x00010000 +
			((unsigned char*) p) [2] * 0x00000100 +
			((unsigned char*) p) [3];
}

inline void set_le16( void* p, unsigned n )
{
	((unsigned char*) p) [1] = (unsigned char) (n >> 8);
	((unsigned char*) p) [0] = (unsigned char) n;
}

inline void set_be16( void* p, unsigned n )
{
	((unsigned char*) p) [0] = (unsigned char) (n >> 8);
	((unsigned char*) p) [1] = (unsigned char) n;
}

inline void set_le32( void* p, unsigned long n )
{
	((unsigned char*) p) [3] = (unsigned char) (n >> 24);
	((unsigned char*) p) [2] = (unsigned char) (n >> 16);
	((unsigned char*) p) [1] = (unsigned char) (n >> 8);
	((unsigned char*) p) [0] = (unsigned char) n;
}

inline void set_be32( void* p, unsigned long n )
{
	((unsigned char*) p) [0] = (unsigned char) (n >> 24);
	((unsigned char*) p) [1] = (unsigned char) (n >> 16);
	((unsigned char*) p) [2] = (unsigned char) (n >> 8);
	((unsigned char*) p) [3] = (unsigned char) n;
}

#ifndef GET_LE16
	// Optimized implementation if byte order is known
	#if BLARGG_NONPORTABLE && BLARGG_LITTLE_ENDIAN
		#define GET_LE16( addr )        (*(BOOST::uint16_t*) (addr))
		#define GET_LE32( addr )        (*(BOOST::uint32_t*) (addr))
		#define SET_LE16( addr, data )  (void (*(BOOST::uint16_t*) (addr) = (data)))
		#define SET_LE32( addr, data )  (void (*(BOOST::uint32_t*) (addr) = (data)))

	#elif BLARGG_NONPORTABLE && BLARGG_CPU_POWERPC
		// PowerPC has special byte-reversed instructions
		// to do: assumes that PowerPC is running in big-endian mode
		#define GET_LE16( addr )        (__lhbrx( (addr), 0 ))
		#define GET_LE32( addr )        (__lwbrx( (addr), 0 ))
		#define SET_LE16( addr, data )  (__sthbrx( (data), (addr), 0 ))
		#define SET_LE32( addr, data )  (__stwbrx( (data), (addr), 0 ))

		#define GET_BE16( addr )        (*(BOOST::uint16_t*) (addr))
		#define GET_BE32( addr )        (*(BOOST::uint32_t*) (addr))
		#define SET_BE16( addr, data )  (void (*(BOOST::uint16_t*) (addr) = (data)))
		#define SET_BE32( addr, data )  (void (*(BOOST::uint32_t*) (addr) = (data)))
		
	#endif
#endif

#ifndef GET_LE16
	#define GET_LE16( addr )        get_le16( addr )
#endif

#ifndef GET_LE32
	#define GET_LE32( addr )        get_le32( addr )
#endif

#ifndef SET_LE16
	#define SET_LE16( addr, data )  set_le16( addr, data )
#endif

#ifndef SET_LE32
	#define SET_LE32( addr, data )  set_le32( addr, data )
#endif

#ifndef GET_BE16
	#define GET_BE16( addr )        get_be16( addr )
#endif

#ifndef GET_BE32
	#define GET_BE32( addr )        get_be32( addr )
#endif

#ifndef SET_BE16
	#define SET_BE16( addr, data )  set_be16( addr, data )
#endif

#ifndef SET_BE32
	#define SET_BE32( addr, data )  set_be32( addr, data )
#endif

// auto-selecting versions

inline void set_le( BOOST::uint16_t* p, unsigned      n ) { SET_LE16( p, n ); }
inline void set_le( BOOST::uint32_t* p, unsigned long n ) { SET_LE32( p, n ); }

inline void set_be( BOOST::uint16_t* p, unsigned      n ) { SET_BE16( p, n ); }
inline void set_be( BOOST::uint32_t* p, unsigned long n ) { SET_BE32( p, n ); }

inline unsigned      get_le( BOOST::uint16_t* p ) { return GET_LE16( p ); }
inline unsigned long get_le( BOOST::uint32_t* p ) { return GET_LE32( p ); }

inline unsigned      get_be( BOOST::uint16_t* p ) { return GET_BE16( p ); }
inline unsigned long get_be( BOOST::uint32_t* p ) { return GET_BE32( p ); }

#endif

