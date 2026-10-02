"""nr_pipe — the Windows named-pipe transport for the layer/daemon pair.

The Linux build speaks over a Unix socket: the daemon `bind()`s, the layer
`connect()`s, and both use the byte stream unchanged. On Windows the two ends
meet on a named pipe instead — Winsock's `AF_UNIX` cannot bind here and CPython
has no `socket.AF_UNIX` at all, so a Win32 named pipe is the only transport
both sides can reach (see the t15 build notes, section A.7).

The bytes are unchanged: 16-byte header, then the colour, then the mask for a
masked frame. Only the endpoint changes, so this module reproduces just enough
of the `socket` interface for `nr_daemon` — `.accept()` on the server, and
`.recv()` / `.sendall()` / `.settimeout()` / `.close()` on a connection — so the
daemon's frame loop stays as it is.

`CreateNamedPipe` is reached through `ctypes` on kernel32: the machine has no
`pywin32`, and the standard library is enough (measured in section A.7.9).
"""
import ctypes
import errno
import time
from ctypes import wintypes

_k32 = ctypes.WinDLL("kernel32", use_last_error=True)

PIPE_ACCESS_DUPLEX = 0x00000003
PIPE_TYPE_BYTE = 0x00000000
PIPE_READMODE_BYTE = 0x00000000
PIPE_WAIT = 0x00000000

ERROR_PIPE_CONNECTED = 535
ERROR_PIPE_BUSY = 231        # every instance is in use; retry, it is not a failure
ERROR_BROKEN_PIPE = 109
ERROR_NO_DATA = 232

INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value

_k32.CreateNamedPipeW.restype = wintypes.HANDLE
_k32.CreateNamedPipeW.argtypes = [
    wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
    wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
_k32.ConnectNamedPipe.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
_k32.ConnectNamedPipe.restype = wintypes.BOOL
_k32.ReadFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                          ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
_k32.ReadFile.restype = wintypes.BOOL
_k32.WriteFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                           ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
_k32.WriteFile.restype = wintypes.BOOL
_k32.CloseHandle.argtypes = [wintypes.HANDLE]
_k32.CloseHandle.restype = wintypes.BOOL
_k32.DisconnectNamedPipe.argtypes = [wintypes.HANDLE]
_k32.DisconnectNamedPipe.restype = wintypes.BOOL


class NamedPipeConnection:
    """One connected byte-mode pipe endpoint, shaped like a socket."""

    def __init__(self, handle, timeout=None):
        self._handle = int(handle)
        self._timeout = timeout
        self._open = True

    def settimeout(self, seconds):
        self._timeout = seconds

    def recv(self, count):
        """Read up to `count` bytes; b'' when the peer closed cleanly."""
        if not self._open:
            return b""
        while True:
            buf = ctypes.create_string_buffer(count)
            got = wintypes.DWORD(0)
            ok = _k32.ReadFile(self._handle, buf, count, ctypes.byref(got), None)
            if ok:
                return buf.raw[:got.value]
            err = ctypes.get_last_error()
            if err in (ERROR_BROKEN_PIPE, ERROR_NO_DATA):
                # The layer closed its end: clean EOF, not a failure.
                self._open = False
                return b""
            # A byte-mode pipe on a blocking ReadFile returns only when data
            # arrives or the peer closes; this path is unexpected rather than a
            # timeout, so it surfaces as an error the daemon's frame loop can
            # already swallow ("frame rejected/failed").
            raise OSError(err, "ReadFile")

    def sendall(self, data):
        view = memoryview(data)
        sent = 0
        while sent < len(view):
            buf = ctypes.create_string_buffer(bytes(view[sent:]))
            written = wintypes.DWORD(0)
            ok = _k32.WriteFile(self._handle, buf, len(view) - sent,
                                ctypes.byref(written), None)
            if not ok:
                err = ctypes.get_last_error()
                if err == ERROR_BROKEN_PIPE:
                    self._open = False
                    raise BrokenPipeError(err, "WriteFile")
                raise OSError(err, "WriteFile")
            if written.value == 0:
                raise OSError(errno.EPIPE, "WriteFile wrote nothing")
            sent += written.value

    def close(self):
        if self._open:
            _k32.CloseHandle(self._handle)
            self._open = False

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


class NamedPipeServer:
    """A byte-mode named pipe the layer connects to, shaped like a listening socket."""

    def __init__(self, path, backlog=4, buffer_size=1 << 20, timeout=None):
        self.path = path
        self.backlog = backlog
        self.buffer_size = buffer_size
        self._timeout = timeout
        self._handle = self._make()

    def _make(self, attempts=50):
        """Create one pipe instance.

        ERROR_PIPE_BUSY means the instance count is momentarily at `backlog` — the
        normal state while accepted connections are still open. It is not fatal: wait
        and retry, and only give up when the caller really cannot get an endpoint.
        """
        for attempt in range(attempts):
            handle = _k32.CreateNamedPipeW(
                self.path, PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                self.backlog, self.buffer_size, self.buffer_size, 0, None)
            value = int(handle)
            if value != INVALID_HANDLE_VALUE:
                return value
            err = ctypes.get_last_error()
            if err != ERROR_PIPE_BUSY:
                raise OSError(err, "CreateNamedPipeW")
            time.sleep(0.02 * (attempt + 1))
        raise OSError(ctypes.get_last_error(), "CreateNamedPipeW")

    def accept(self):
        """Wait for one client, then return a connected `NamedPipeConnection`."""
        if not self._handle:
            # Every instance was in use last time round; make a new one now.
            self._handle = self._make()
        ok = _k32.ConnectNamedPipe(self._handle, None)
        if not ok:
            err = ctypes.get_last_error()
            if err != ERROR_PIPE_CONNECTED:
                raise OSError(err, "ConnectNamedPipe")
        connection = NamedPipeConnection(self._handle, self._timeout)
        # A fresh pipe instance replaces the one just handed over, so the next
        # `accept` waits on a new endpoint rather than the one in use. When the
        # backlog is momentarily full this leaves `_handle` empty and the next
        # accept() creates the instance instead - so a busy pipe costs a retry,
        # not the daemon.
        try:
            self._handle = self._make()
        except OSError:
            self._handle = 0
        return connection, None

    def close(self):
        if self._handle:
            _k32.DisconnectNamedPipe(self._handle)
            _k32.CloseHandle(self._handle)
            self._handle = None


def server(path, backlog=4, timeout=None):
    return NamedPipeServer(path, backlog=backlog, timeout=timeout)
