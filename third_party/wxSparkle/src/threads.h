/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#ifndef _threads_h_
#define _threads_h_

#include "error.h"
#include "platform.h"

#include <atomic>
#include <memory>
#include <string>

// Native Win32 primitives are used on Windows (as in WinSparkle), so that
// toolchains whose C++ library lacks std::thread (e.g. some MinGW builds)
// are supported too. Classic Mac OS's C++ library has no std::thread either,
// so POSIX threads (pthreads-retro68, on top of the Thread Manager) are used
// for the synchronization objects there, while the threads themselves are
// wxThreads, see Thread::Start(). Standard C++ primitives are used elsewhere.
#ifdef _WIN32
    #include <windows.h>
#elif WXSPARKLE_CLASSIC_MAC
    #define WXSPARKLE_PTHREADS 1
    #include <pthread.h>
    class wxThread;
#else
    #include <condition_variable>
    #include <mutex>
    #include <thread>
#endif

namespace winsparkle
{

/// Event object, auto-resetting (default) or manual-reset.
class Event
{
public:
    static const unsigned INFINITE_WAIT = 0xFFFFFFFF;

    explicit Event(bool manualReset = false);
    ~Event();

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    /// Signal the event
    void Signal();

    /// Wait until the event is signalled (true) or timeout ellapses (false)
    bool WaitUntilSignaled(unsigned timeoutMilliseconds = INFINITE_WAIT);

    bool CheckIfSignaled()
    {
        return WaitUntilSignaled(0);
    }

private:
#ifdef _WIN32
    HANDLE m_handle;
#elif defined(WXSPARKLE_PTHREADS)
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    bool m_signaled;
    bool m_manualReset;
#else
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_signaled;
    bool m_manualReset;
#endif
};


/**
    Recursive mutex, equivalent of the Win32 critical section object
    (which is recursive as well -- some code relies on this).
*/
class CriticalSection
{
public:
#ifdef _WIN32
    CriticalSection() { InitializeCriticalSection(&m_cs); }
    ~CriticalSection() { DeleteCriticalSection(&m_cs); }

    void Enter() { EnterCriticalSection(&m_cs); }
    void Leave() { LeaveCriticalSection(&m_cs); }
#elif defined(WXSPARKLE_PTHREADS)
    CriticalSection();
    ~CriticalSection() { pthread_mutex_destroy(&m_cs); }

    void Enter() { pthread_mutex_lock(&m_cs); }
    void Leave() { pthread_mutex_unlock(&m_cs); }
#else
    CriticalSection() {}

    void Enter() { m_cs.lock(); }
    void Leave() { m_cs.unlock(); }
#endif

    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

private:
#ifdef _WIN32
    CRITICAL_SECTION m_cs;
#elif defined(WXSPARKLE_PTHREADS)
    pthread_mutex_t m_cs;
#else
    std::recursive_mutex m_cs;
#endif
};


/**
Locks a critical section as RIIA. Use this instead of manually calling
CriticalSection::Enter() and CriticalSection::Leave().
*/
class CriticalSectionLocker
{
public:
    CriticalSectionLocker(CriticalSection& cs) : m_cs(cs) { cs.Enter(); }
    ~CriticalSectionLocker() { m_cs.Leave(); }

    CriticalSectionLocker(const CriticalSectionLocker&) = delete;
    CriticalSectionLocker& operator=(const CriticalSectionLocker&) = delete;

private:
    CriticalSection& m_cs;
};


/**
    Lightweight thread class.

    It's a base class; derived class must implement at least Run().

    Create the thread on heap, then call Start() on it. If the thread is joinable
    (see IsJoinable()), then you must call Join() or TerminateAndJoin() on it
    before destroying it. Otherwise, it self-destructs when Run() finishes.
 */
class Thread
{
public:
    /**
        Creates the thread object.

        Note that you must explicitly call Start() to start it.

        @param name Descriptive name of the thread, shown in debuggers.
     */
    Thread(const char *name);

    virtual ~Thread();

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /**
        Launch the thread.

        Calls Run() in the new thread's context.

        This method doesn't return until Run() calls SignalReady().

        Throws on error.
     */
    void Start();

    /**
        Wait for the thread to terminate.
     */
    void Join();

    /**
        Signal the thread to terminate and call Join().

        @note The thread must support this and call
              CheckShouldTerminate() frequently.
     */
    void TerminateAndJoin();

    /// Check if the thread should terminate and throw TerminateThreadException if so.
    void CheckShouldTerminate();

    /// Non-throwing variant of CheckShouldTerminate().
    bool IsTerminationRequested() const { return m_terminate.load(); }

protected:
    /// Signals Start() that the thread is up and ready.
    void SignalReady();

    /**
        Sleeps for up to @a seconds, waking up early if termination was
        requested. Returns true if the thread should terminate.
     */
    bool WaitForTermination(unsigned seconds);

    /**
        Code to run in the thread's context.

        This method @a must call SignalReady() as soon as it is initialized.
     */
    virtual void Run() = 0;

    /// Is the thread joinable?
    virtual bool IsJoinable() const = 0;

    /// Requests termination without waiting for it.
    void RequestTermination();

public:
    /// This exception is thrown when the thread was terminated.
    struct TerminateThreadException
    {
    };

private:
    static void ThreadEntryPoint(Thread *thread);
#ifdef _WIN32
    static unsigned __stdcall Win32ThreadEntryPoint(void *data);
#elif defined(WXSPARKLE_PTHREADS)
    static void *PosixThreadEntryPoint(void *data);
    friend class ThreadRunner;
#endif

    // returns true if called from this thread
    bool IsCurrentThread() const;

    std::string            m_name;

    // Start()/SignalReady() handshake (manual-reset event). It is
    // reference-counted, because a self-destructing thread may delete
    // itself before Start() returns.
    std::shared_ptr<Event> m_ready;

    std::atomic<bool>      m_terminate;
    Event                  m_terminateEvent; // manual-reset

#ifdef _WIN32
    HANDLE                 m_handle;
    unsigned               m_id;
#elif defined(WXSPARKLE_PTHREADS)
    wxThread              *m_thread;    // joinable threads only
#else
    std::thread            m_thread;
#endif
};

} // namespace winsparkle

#endif // _threads_h_
