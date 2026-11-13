#ifndef VBAM_WX_WX_NOTHREADS_H_
#define VBAM_WX_WX_NOTHREADS_H_

// Single-threaded stand-ins for wx's thread classes, for a wx built with
// wxUSE_THREADS=0 (wxRetro68: classic Mac OS has no preemptive threads), where
// <wx/thread.h> declares none of them.
//
// The frontend only starts threads to run filters in bands and for background
// input, and the filter code already runs everything on the calling thread
// when it is given one band. Here GetCPUCount() says there is one CPU, so it
// always is, and a thread can be neither created nor run: Create() and Run()
// fail, which every caller handles. Locking never contends and never waits.

#include <wx/defs.h>

#if !wxUSE_THREADS

#include <wx/thread.h>
#include <wx/utils.h>

enum wxMutexError {
    wxMUTEX_NO_ERROR = 0,
    wxMUTEX_INVALID,
    wxMUTEX_DEAD_LOCK,
    wxMUTEX_BUSY,
    wxMUTEX_UNLOCKED,
    wxMUTEX_TIMEOUT,
    wxMUTEX_MISC_ERROR
};

enum wxCondError { wxCOND_NO_ERROR = 0, wxCOND_INVALID, wxCOND_TIMEOUT, wxCOND_MISC_ERROR };

enum wxSemaError {
    wxSEMA_NO_ERROR = 0,
    wxSEMA_INVALID,
    wxSEMA_BUSY,
    wxSEMA_TIMEOUT,
    wxSEMA_OVERFLOW,
    wxSEMA_MISC_ERROR
};

enum wxThreadError {
    wxTHREAD_NO_ERROR = 0,
    wxTHREAD_NO_RESOURCE,
    wxTHREAD_RUNNING,
    wxTHREAD_NOT_RUNNING,
    wxTHREAD_KILLED,
    wxTHREAD_MISC_ERROR
};

enum wxThreadKind { wxTHREAD_DETACHED, wxTHREAD_JOINABLE };

enum wxMutexType { wxMUTEX_DEFAULT, wxMUTEX_RECURSIVE };

class wxMutex {
public:
    explicit wxMutex(wxMutexType = wxMUTEX_DEFAULT) {}
    wxMutex(const wxMutex&) = delete;
    wxMutex& operator=(const wxMutex&) = delete;

    bool IsOk() const { return true; }
    wxMutexError Lock() { return wxMUTEX_NO_ERROR; }
    wxMutexError LockTimeout(unsigned long) { return wxMUTEX_NO_ERROR; }
    wxMutexError TryLock() { return wxMUTEX_NO_ERROR; }
    wxMutexError Unlock() { return wxMUTEX_NO_ERROR; }
};

class wxMutexLocker {
public:
    explicit wxMutexLocker(wxMutex&) {}
    bool IsOk() const { return true; }
};

class wxCondition {
public:
    explicit wxCondition(wxMutex&) {}
    wxCondition(const wxCondition&) = delete;
    wxCondition& operator=(const wxCondition&) = delete;

    bool IsOk() const { return true; }
    // Nothing else can signal; a wait may return spuriously, which is all a
    // single thread can do.
    wxCondError Wait() { return wxCOND_NO_ERROR; }
    wxCondError WaitTimeout(unsigned long) { return wxCOND_TIMEOUT; }
    wxCondError Signal() { return wxCOND_NO_ERROR; }
    wxCondError Broadcast() { return wxCOND_NO_ERROR; }
};

class wxSemaphore {
public:
    explicit wxSemaphore(int initialcount = 0, int maxcount = 0)
        : count_(initialcount), max_(maxcount) {}
    wxSemaphore(const wxSemaphore&) = delete;
    wxSemaphore& operator=(const wxSemaphore&) = delete;

    bool IsOk() const { return true; }
    // Never blocks: a count of zero would wait for a Post() that no other
    // thread can make.
    wxSemaError Wait() { return TryWait(); }
    wxSemaError TryWait() {
        if (count_ == 0)
            return wxSEMA_BUSY;
        --count_;
        return wxSEMA_NO_ERROR;
    }
    wxSemaError WaitTimeout(unsigned long) {
        return TryWait() == wxSEMA_NO_ERROR ? wxSEMA_NO_ERROR : wxSEMA_TIMEOUT;
    }
    wxSemaError Post() {
        if (max_ > 0 && count_ >= max_)
            return wxSEMA_OVERFLOW;
        ++count_;
        return wxSEMA_NO_ERROR;
    }

private:
    int count_;
    int max_;
};

class wxThread {
public:
    typedef void* ExitCode;

    explicit wxThread(wxThreadKind kind = wxTHREAD_DETACHED) : kind_(kind) {}
    virtual ~wxThread() = default;

    static int GetCPUCount() { return 1; }
    static bool IsMain() { return true; }
    static wxThread* This() { return nullptr; }
    static void Yield() {}
    static void Sleep(unsigned long milliseconds) { wxMilliSleep(milliseconds); }

    wxThreadError Create(unsigned int = 0) { return wxTHREAD_NO_RESOURCE; }
    wxThreadError Run() { return wxTHREAD_NO_RESOURCE; }
    wxThreadError Delete(ExitCode* rc = nullptr) {
        if (rc)
            *rc = nullptr;
        return wxTHREAD_NOT_RUNNING;
    }
    ExitCode Wait() { return nullptr; }
    wxThreadError Kill() { return wxTHREAD_NOT_RUNNING; }
    wxThreadError Pause() { return wxTHREAD_NOT_RUNNING; }
    wxThreadError Resume() { return wxTHREAD_NOT_RUNNING; }

    bool IsAlive() const { return false; }
    bool IsRunning() const { return false; }
    bool IsPaused() const { return false; }
    bool IsDetached() const { return kind_ == wxTHREAD_DETACHED; }
    virtual bool TestDestroy() { return true; }

    // The filter code calls the joinable FilterThread's Entry() itself when it
    // runs on one thread, hence public.
    virtual ExitCode Entry() = 0;

private:
    wxThreadKind kind_;
};

#endif  // !wxUSE_THREADS

#endif  // VBAM_WX_WX_NOTHREADS_H_
