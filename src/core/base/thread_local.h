#ifndef VBAM_CORE_BASE_THREAD_LOCAL_H_
#define VBAM_CORE_BASE_THREAD_LOCAL_H_

// For block-scope variables only: `VBAM_THREAD_LOCAL T x;` is `thread_local`,
// or plain `static` where the compiler's thread_local has no runtime
// (VBAM_NO_THREAD_LOCAL, from nothreads.h or pthread_std.h): Retro68's 68k GCC
// emits native TLS accesses for it, which classic Mac OS has nothing for.
//
// With one thread that is the same thing. With the cooperative
// pthreads-retro68 threads it is one variable shared by all of them, which
// holds for the users here -- filter scratch buffers and caches -- because a
// thread only gives up the CPU where it blocks or yields, which none of them
// does while it uses the variable, and the wx port filters on one thread
// there (wxThread::GetCPUCount() is 1).
#ifdef VBAM_NO_THREAD_LOCAL
#define VBAM_THREAD_LOCAL static
#else
#define VBAM_THREAD_LOCAL thread_local
#endif

#endif  // VBAM_CORE_BASE_THREAD_LOCAL_H_
