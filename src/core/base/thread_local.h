#ifndef VBAM_CORE_BASE_THREAD_LOCAL_H_
#define VBAM_CORE_BASE_THREAD_LOCAL_H_

// For block-scope variables only: `VBAM_THREAD_LOCAL T x;` is `thread_local`
// where there are threads and plain `static` in a single-threaded build
// (VBAM_SINGLE_THREADED, see nothreads.h), which is the same thing with one
// thread. Retro68's 68k GCC emits native TLS accesses for thread_local, which
// classic Mac OS has no runtime for.
#ifdef VBAM_SINGLE_THREADED
#define VBAM_THREAD_LOCAL static
#else
#define VBAM_THREAD_LOCAL thread_local
#endif

#endif  // VBAM_CORE_BASE_THREAD_LOCAL_H_
