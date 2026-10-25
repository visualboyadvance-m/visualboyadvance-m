// DJGPP: the toolchain's libstdc++ was built with GNU Pth's pthread.h, whose
// _POSIX_THREAD_SYSCALL_SOFT macros rename the read, write, writev, sleep,
// usleep and detach calls in its file streams and <thread> to __pthread_*.
// Those live in libpthread.a's pthread.o, which also defines pthread_key_create
// and friends. Once that object is linked, libgcc believes threads are active
// and locks a mutex in __register_frame_info(), which crt0 calls before
// __crt1_startup(). Pth then initializes itself, calls gettimeofday(), and
// DJGPP's tzset() opens a file through the DOS transfer buffer that has not
// been set up yet: a page fault before main() with CWSDPMI's register dump
// instead of a traceback. Satisfy the references from libc so that no Pth
// object is linked and the program stays single-threaded.
//
// No <sys/uio.h>: Watt-32 installs a dummy one over DJGPP's, so writev() and
// struct iovec come from libc by hand (the layout is Pth's, which libstdc++
// was built against: base pointer, size_t length).
#include <errno.h>
#include <stddef.h>
#include <unistd.h>

struct vbam_dos_iovec {
    void* iov_base;
    size_t iov_len;
};

extern ssize_t writev(int fd, const struct vbam_dos_iovec* iov, int iovcnt);

ssize_t __pthread_read(int fd, void* buf, size_t count)
{
    return read(fd, buf, count);
}

ssize_t __pthread_write(int fd, const void* buf, size_t count)
{
    return write(fd, buf, count);
}

ssize_t __pthread_writev(int fd, const void* iov, int iovcnt)
{
    return writev(fd, (const struct vbam_dos_iovec*)iov, iovcnt);
}

unsigned int __pthread_sleep(unsigned int seconds)
{
    return sleep(seconds);
}

int __pthread_usleep(unsigned int useconds)
{
    return (int)usleep(useconds);
}

// std::thread::detach(); no thread can ever be created here.
int __pthread_detach(void* thread)
{
    (void)thread;
    return ESRCH;
}
