#!/usr/bin/env python3
"""
xmxres — the device-resident runtime: buffers that persist, dispatches that batch.

`xmx.py` runs one GEMM per submit and hands the result back to the host, which is
why the XMX path does not beat a good CPU BLAS: a 720p frame moves 16.46 GB across
the boundary and 62x the kernel's own time goes into the trip
(`notes/phase11-what-is-left.md`, `notes/phase13-torch-and-blas.md`).

Here the operands are device buffers addressed by pointer, the elementwise passes
run on the GPU too, and a whole chain records into one command buffer with a single
fence at the end. Because the APU's memory is shared and HOST_CACHED, a buffer's
contents are also a numpy array — `Buffer.view()` — so feeding an input or reading
an output is an address, not a transfer.

    rt = Runtime()
    a = rt.buffer_from(activations)          # float32, on device
    rt.begin()
    rt.to_half(a, a16, n)
    rt.gemm(a16, w16, out, M, N, K)
    rt.submit()
"""
from __future__ import annotations

import ctypes
import os
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src"))
import nr_build  # noqa: E402
TM, TN, TK = 8, 16, 16

(E4M3, GATE, HALF, TO_HALF, SCALE, RESIDUAL, FROM_HALF, PARTITION, REVERSE, ADD_BIAS,
 SPLIT_HEADS, MERGE_HEADS, POOL2, UPSAMPLE2, SCALE_CHANNEL, ADD, PAD_END,
 GATE_E4M3_HALF, E4M3_HALF, GATE_HALF) = range(20)
COSINE_PUBLISH, SOFTMAX = 0, 1

# GEMM epilogues, applied to the accumulator on its way out of the kernel
EPI_NONE, EPI_E4M3, EPI_GATE, EPI_GATE_E4M3, EPI_HALF = 0, 1, 2, 3, 4


def _reads(a_half=False, b_half=False):
    """Operand width bits: 15 marks `a` as float16, 16 marks `b`."""
    return (0x8000 if a_half else 0) | (0x10000 if b_half else 0)


def _publish(epilogue, narrow):
    """The publish half of a pass's flags: bits 8-11 the epilogue, bit 12 a half output.

    Every shader reads the same layout, so a pass that ends in a publish never needs a
    second dispatch over the same buffer to apply it.
    """
    return (int(epilogue) << 8) | (0x1000 if narrow else 0)

_lib = None


class DeviceLost(RuntimeError):
    """`VK_ERROR_DEVICE_LOST`: the device went away under this process, normally the
    driver resetting a hung GPU.

    Unlike every other failure here it is final. Each later submit on this device fails
    the same way, so a caller should stop rather than retry; a new process gets a new
    device.
    """


def failure(lib, call):
    """The exception for a call that returned an error, told apart by the library's own
    record of a lost device rather than by the text of the message."""
    message = f"{call}: {lib.xmx_error().decode()}"
    return DeviceLost(message) if lib.xmx_device_lost() else RuntimeError(message)


def _load():
    global _lib
    if _lib is not None:
        return _lib
    import xmx
    lib = xmx._load()                      # shares the instance, device and queue
    for name, args in (
            ("xmx_res_init", [ctypes.c_char_p] * 6),
            ("xmx_buf_create", [ctypes.c_ulonglong]),
            ("xmx_buf_create_kind", [ctypes.c_ulonglong, ctypes.c_int]),
            ("xmx_buf_host_visible", [ctypes.c_int]),
            ("xmx_buf_zero", [ctypes.c_int]),
            ("xmx_staging_mode", []),
            ("xmx_buf_upload", [ctypes.c_int, ctypes.c_void_p,
                               ctypes.c_ulonglong, ctypes.c_ulonglong]),
            ("xmx_buf_download", [ctypes.c_int, ctypes.c_void_p,
                                 ctypes.c_ulonglong, ctypes.c_ulonglong]),
            ("xmx_buf_destroy", [ctypes.c_int]),
            ("xmx_begin", []),
            ("xmx_abort", []),
            ("xmx_sync", [ctypes.c_int]),
            ("xmx_specialize", [ctypes.c_uint]),
            ("xmx_specialized_count", []),
            ("xmx_specialization", []),
            ("xmx_graph_capture", []),
            ("xmx_graph_run", [ctypes.c_int]),
            ("xmx_graph_destroy", [ctypes.c_int]),
            ("xmx_rec_copy", [ctypes.c_int] * 2 + [ctypes.c_ulonglong] * 3),
            ("xmx_submit", []),
            ("xmx_rec_gemm", [ctypes.c_int] * 3 + [ctypes.c_uint] * 14),
            ("xmx_rec_unary", [ctypes.c_uint] + [ctypes.c_int] * 4
             + [ctypes.c_uint, ctypes.c_uint, ctypes.c_float] + [ctypes.c_uint] * 5),
            ("xmx_rec_row", [ctypes.c_uint] + [ctypes.c_int] * 4 + [ctypes.c_uint] * 5
             + [ctypes.c_float]),
            ("xmx_profile", [ctypes.c_int]),
            ("xmx_rec_history", [ctypes.c_int] * 3 + [ctypes.c_uint] * 5),
            ("xmx_device_lost", [])):
        getattr(lib, name).argtypes = args
        getattr(lib, name).restype = ctypes.c_int
    lib.xmx_buf_ptr.argtypes = [ctypes.c_int]
    lib.xmx_buf_ptr.restype = ctypes.c_void_p
    lib.xmx_buf_bytes.argtypes = [ctypes.c_int]
    lib.xmx_buf_bytes.restype = ctypes.c_ulonglong
    lib.xmx_buf_total_bytes.argtypes = []
    lib.xmx_buf_total_bytes.restype = ctypes.c_ulonglong
    lib.xmx_profile_reset.argtypes = []
    lib.xmx_profile_reset.restype = None
    lib.xmx_profile_ms.argtypes = [ctypes.c_uint]
    lib.xmx_profile_ms.restype = ctypes.c_double
    lib.xmx_profile_count.argtypes = [ctypes.c_uint]
    lib.xmx_profile_count.restype = ctypes.c_uint
    # The shader paths take an environment override so a variant can be measured
    # against the shipped one without editing the tree. The GEMM kernels follow the
    # device: without VK_KHR_cooperative_matrix (or with XMX_PORTABLE=1) the portable
    # multiply-add builds take the same dispatches, and the staged kernel — which has no
    # portable twin — is never selected by the runtime, so its slot gets the plain one.
    portable = bool(lib.xmx_portable())
    gemm, tiled, staged = (("gemm_portable.spv", "gemm_portable_tiled.spv", "gemm_portable.spv")
                           if portable else
                           ("gemm_resident.spv", "gemm_tiled.spv", "gemm_staged.spv"))
    spv = [os.environ.get(name) or nr_build.shader_arg(lib, default)
           for name, default in (("XMX_GEMM_SPV", gemm),
                                 ("XMX_UNARY_SPV", "resident.spv"),
                                 ("XMX_ROW_SPV", "attention.spv"),
                                 ("XMX_HISTORY_SPV", "history.spv"),
                                 ("XMX_TILED_SPV", tiled),
                                 ("XMX_STAGED_SPV", staged))]
    if lib.xmx_res_init(*[p.encode() for p in spv]) != 0:
        raise failure(lib, "xmx_res_init")
    _lib = lib
    return lib


def align(value, multiple):
    return -(-value // multiple) * multiple


# what a buffer is for, which decides where it lives
GRAPH, HOST_READ, HOST_WRITE = 0, 1, 2


class Buffer:
    """A device buffer, addressable as a numpy array when the host can see it.

    `kind` says what it is for. `HOST_READ` and `HOST_WRITE` are the buffers the host
    touches — always mapped, cached for reading. `GRAPH` follows the device: mapped where
    the host can see the card's memory, device-local and **unmapped** where it cannot, and
    then `view()` refuses rather than handing back memory the device will not see.

    That refusal is the point. The alternative — a host shadow that someone must remember
    to upload — was tried by somebody else's port of this and produced frames rendered from
    data that never left the host, with no error anywhere. A buffer either is addressable or
    says so.
    """

    __slots__ = ("id", "nbytes", "kind", "_lib")

    def __init__(self, nbytes, kind=GRAPH):
        self._lib = _load()
        self.id = self._lib.xmx_buf_create_kind(int(nbytes), int(kind))
        if self.id < 0:
            raise failure(self._lib, "xmx_buf_create")
        self.nbytes = int(nbytes)
        self.kind = int(kind)

    @property
    def mapped(self):
        return bool(self._lib.xmx_buf_host_visible(self.id))

    def view(self, dtype=np.float32, shape=None):
        pointer = self._lib.xmx_buf_ptr(self.id)
        if not pointer:
            raise RuntimeError(
                "this buffer is in device memory the host cannot address: use upload() or "
                "download(), or a HOST_READ/HOST_WRITE buffer the graph copies through")
        count = self.nbytes // np.dtype(dtype).itemsize
        array = np.ctypeslib.as_array(
            ctypes.cast(pointer, ctypes.POINTER(ctypes.c_uint8)), shape=(self.nbytes,))
        array = array.view(dtype)[:count]
        return array if shape is None else array[:int(np.prod(shape))].reshape(shape)

    def upload(self, array, offset=0):
        """Host data into this buffer: a memcpy when mapped, a staged copy when not."""
        array = np.ascontiguousarray(array)
        if self._lib.xmx_buf_upload(self.id, array.ctypes.data, int(offset),
                                    int(array.nbytes)) != 0:
            raise failure(self._lib, "xmx_buf_upload")
        return self

    def download(self, dtype=np.float32, count=None, offset=0):
        out = np.empty(count if count is not None
                       else self.nbytes // np.dtype(dtype).itemsize, dtype)
        if self._lib.xmx_buf_download(self.id, out.ctypes.data, int(offset),
                                      int(out.nbytes)) != 0:
            raise failure(self._lib, "xmx_buf_download")
        return out

    def zero(self):
        # the library decides how: a memset when mapped, a recorded fill when not, because
        # the scratch arena clears roles in the middle of the recording that will use them
        if self._lib.xmx_buf_zero(self.id) != 0:
            raise failure(self._lib, "xmx_buf_zero")
        return self

    def free(self):
        if self.id >= 0:
            self._lib.xmx_buf_destroy(self.id)
            self.id = -1

    def __del__(self):
        try:
            self.free()
        except Exception:
            pass


def host_view(buffer, dtype=np.float32, shape=None, count=None):
    """Read a buffer from the host, addressable or not.

    A mapped buffer hands back its own memory, as it always did. An unmapped one — the
    graph's buffers on a card whose memory the host cannot see — is copied out through
    staging. Tests and diagnostics read buffers the graph never sends back, and should not
    have to know which kind they are holding.
    """
    if getattr(buffer, "mapped", True):
        array = buffer.view(dtype, shape)
        return array if count is None else array[:count]
    array = buffer.download(dtype, count)
    return array if shape is None else array[:int(np.prod(shape))].reshape(shape)


def host_write(buffer, array, rows=None):
    """Put host data into a buffer, addressable or not.

    `rows` is for the padded scratch layouts: the first rows of a `(padded, channels)`
    buffer are contiguous at its start, so writing them is one copy either way.
    """
    array = np.ascontiguousarray(array)
    if getattr(buffer, "mapped", True):
        if rows is None:
            buffer.view(dtype=array.dtype, shape=array.shape)[...] = array
        else:
            buffer.view(dtype=array.dtype, shape=rows)[:array.shape[0]] = array
        return buffer
    return buffer.upload(array)


class CommandGraph:
    """Replayable commands. Referenced buffers must outlive this object."""

    def __init__(self, lib):
        self._lib = lib
        self.id = lib.xmx_graph_capture()
        if self.id < 0:
            raise failure(lib, "xmx_graph_capture")

    def run(self):
        passes = self._lib.xmx_graph_run(self.id)
        if passes < 0:
            raise failure(self._lib, "xmx_graph_run")
        return passes

    def free(self):
        if self.id >= 0:
            self._lib.xmx_graph_destroy(self.id)
            self.id = -1

    def __del__(self):
        try:
            self.free()
        except Exception:
            pass


class ScratchArena:
    """Plan shared scratch by role, then freeze sizes before commands are recorded.

    Blocks run sequentially and may share each role's storage. Input, output and
    encoder skips belong to the frame separately. Unused roles allocate no memory.
    """

    # Within a block these roles have disjoint live intervals. Barriers already
    # separate their producers/last consumers in nr_resident. Transition scratch
    # runs between blocks and reuses those same allocations. Keep FFN residuals
    # separate: they stay live until the closing attention residual.
    ALIASES = {
        **dict.fromkeys(("hidden16", "proj", "attended", "attention",
                         "transition.padded", "transition.projected"), "projection"),
        **dict.fromkeys(("branch", "v16"), "branch_value"),
        **dict.fromkeys(("value16", "win16", "ffn16", "k16",
                         "transition.pooled16", "transition.projected16"), "input_key"),
        **dict.fromkeys(("heads16", "core16", "q16", "probs16", "merged16"), "query_probability"),
        **dict.fromkeys(("scores", "context", "transition.upsampled"), "scores_context"),
        **dict.fromkeys(("ffn", "transition.scaled"), "residual"),
    }

    def __init__(self, runtime):
        self.runtime = runtime
        self.buffers = {}
        self.sealed = False
        self._plan_state = [False]

    def buffer(self, name, count, dtype=np.float32):
        dtype = np.dtype(dtype)
        key = (self.ALIASES[name], "bytes") if name in self.ALIASES else (name, dtype.str)
        size = int(count) * dtype.itemsize
        if key not in self.buffers:
            if self.sealed:
                raise RuntimeError(f"scratch role was not planned: {name}")
            self.buffers[key] = _ScratchBuffer(self, size)
        buffer = self.buffers[key]
        if size > buffer.nbytes:
            if self.sealed:
                raise RuntimeError(f"scratch role exceeds its plan: {name}")
            buffer.nbytes = size
        return buffer

    def seal(self):
        self.sealed = True
        self._plan_state[0] = True

    def free(self):
        for buffer in self.buffers.values():
            buffer.free()
        self.buffers.clear()


class _ScratchBuffer:
    def __init__(self, arena, nbytes):
        # Keep no reference back to the arena, so dropping a frame releases it.
        self.runtime = arena.runtime
        self._plan_state = arena._plan_state
        self.nbytes = nbytes
        self._buffer = None
        self._zero = False
        self._closed = False

    def _get(self):
        if self._closed:
            raise RuntimeError("scratch buffer is closed")
        if not self._plan_state[0]:
            raise RuntimeError("scratch plan must be sealed before allocation")
        if self._buffer is None:
            self._buffer = self.runtime.buffer(self.nbytes, np.uint8)
            if self._zero:
                self._buffer.zero()
        return self._buffer

    @property
    def id(self):
        return self._get().id

    @property
    def mapped(self):
        return self._get().mapped

    def view(self, dtype=np.float32, shape=None):
        return self._get().view(dtype, shape)

    def upload(self, array, offset=0):
        return self._get().upload(array, offset)

    def download(self, dtype=np.float32, count=None, offset=0):
        return self._get().download(dtype, count, offset)

    def zero(self):
        self._zero = True
        if self._buffer is not None:
            self._buffer.zero()
        return self

    def free(self):
        if self._buffer is not None:
            self._buffer.free()
        self._closed = True


# The families a profiled pass can belong to; a kind is `family * 32 + subkind`, so a
# unary or row pass is attributed to the specific operation rather than to its family.
PROFILE_FAMILIES = ("gemm", "gemm tiled", "gemm staged", "unary", "row", "history", "copy",
                    "start")


def profile(on=True):
    """Turn GPU timestamping on. Off by default and free when off."""
    lib = _load()
    if lib.xmx_profile(1 if on else 0):
        raise RuntimeError("xmx_profile: " + lib.xmx_error().decode())


def profile_reset():
    _load().xmx_profile_reset()


def profile_totals():
    """Milliseconds and pass count per kind, as {(family, subkind): (ms, passes)}.

    Only kinds that actually ran appear. `start` is the zero point of the first frame
    recorded and is not a pass, so it is dropped."""
    lib = _load()
    out = {}
    for kind in range(256):
        passes = lib.xmx_profile_count(kind)
        if not passes:
            continue
        family, sub = PROFILE_FAMILIES[kind // 32], kind % 32
        if family == "start":
            continue
        out[(family, sub)] = (lib.xmx_profile_ms(kind), passes)
    return out


class Runtime:
    """Records a chain of GPU passes and submits it once."""

    def __init__(self):
        self.lib = _load()
        self.recorded = 0
        self.fuse_qk = os.environ.get("NR_FUSE_QK", "1") != "0"

    def graph_key(self):
        return self.lib.xmx_specialization() | (int(self.fuse_qk) << 3)

    @property
    def buffer_bytes(self):
        """Live resident buffer sizes, excluding driver allocations and host weights."""
        return self.lib.xmx_buf_total_bytes()

    # -- allocation ----------------------------------------------------

    @property
    def staging(self):
        """Whether the graph's buffers are unmapped, so the host reaches them by copies."""
        return bool(self.lib.xmx_staging_mode())

    def buffer(self, count, dtype=np.float32, kind=GRAPH):
        return Buffer(int(count) * np.dtype(dtype).itemsize, kind=kind)

    # the same two helpers as methods, for callers outside this package: `src/ref` reaches
    # a runtime through an object, not through an import path
    def read(self, buffer, dtype=np.float32, shape=None, count=None):
        return host_view(buffer, dtype, shape, count)

    def write(self, buffer, array, rows=None):
        return host_write(buffer, array, rows)

    def buffer_from(self, array, dtype=np.float32, pad=0):
        """A device buffer holding `array`, optionally padded with zeros at the end."""
        array = np.ascontiguousarray(array, dtype=dtype)
        buffer = Buffer((array.size + int(pad)) * array.dtype.itemsize)
        if buffer.mapped:
            flat = buffer.view(dtype)
            flat[:array.size] = array.reshape(-1)
            if pad:
                flat[array.size:] = 0
        else:
            # the weights go up once per extent, not once per frame; a staged copy is the
            # right cost here and the only one available with no mapping
            if pad:
                buffer.zero()
            buffer.upload(array.reshape(-1))
        return buffer

    # -- recording -----------------------------------------------------

    def begin(self):
        if self.lib.xmx_begin() != 0:
            raise RuntimeError("xmx_begin: " + self.lib.xmx_error().decode())
        self.recorded = 0
        return self

    def independent(self):
        """A `with` block whose dispatches are known not to depend on one another.

        Everything inside records without a barrier between; leaving the block emits
        one. The graph has several such runs — a branched feed-forward's per-head GEMMs
        write disjoint slices of one buffer, the three head splits read one buffer and
        write three — and on small dispatches the overlap is worth having.
        """
        return _Independent(self)

    def capture(self):
        """Finish recording without executing; return reusable commands."""
        return CommandGraph(self.lib)

    def abort(self):
        """Discard unfinished recording after an error; completed graphs survive."""
        if self.lib.xmx_abort() != 0:
            raise RuntimeError("xmx_abort: " + self.lib.xmx_error().decode())

    def copy(self, source, target, nbytes, source_offset=0, target_offset=0):
        if min(nbytes, source_offset, target_offset) < 0:
            raise ValueError("copy sizes and offsets must be nonnegative")
        if self.lib.xmx_rec_copy(source.id, target.id, nbytes,
                                  source_offset, target_offset) != 0:
            raise RuntimeError("xmx_rec_copy: " + self.lib.xmx_error().decode())
        self.recorded += 1
        return self

    def specialize(self, mask=7):
        """Select cached shader variants: GEMM=1, elementwise=2, rows=4.

        The default enables all three. Zero retains the generic shaders for exact
        A/B tests on the same buffers. Switch only between command buffers.
        """
        if self.lib.xmx_specialize(mask) != 0:
            raise RuntimeError("xmx_specialize: " + self.lib.xmx_error().decode())
        return self

    def gemm(self, a, b, c, rows, cols, inner, *, batch=1, strides=None, transpose_b=False,
             leading=None, offsets=(0, 0, 0), epilogue=0, narrow=False):
        """C = A @ B for tile-aligned extents; A and B are float16, C float32.

        `strides` are element counts per batch item, defaulting to the dense packing;
        `leading` overrides the row strides of A, B and C, and `offsets` shifts each
        operand's base in elements, so a GEMM can read or write a slice of a wider
        buffer — which is how the branched and split feed-forwards place their heads.
        Extents must already be multiples of 8 / 16 / 16: cooperative-matrix loads are
        not bounds-checked on this device (`cooperativeMatrixRobustBufferAccess` is
        false), so the padding has to be in the buffer, not in a guard.
        """
        for extent, multiple, name in ((rows, TM, "rows"), (cols, TN, "cols"), (inner, TK, "inner")):
            if extent % multiple:
                raise ValueError(f"{name}={extent} must be a multiple of {multiple}")
        if strides is None:
            strides = (rows * inner, cols * inner if transpose_b else inner * cols, rows * cols)
        lda, ldb, ldc = leading or (0, 0, 0)
        flags = (1 if transpose_b else 0) | _publish(epilogue, narrow)
        if self.lib.xmx_rec_gemm(a.id, b.id, c.id, rows, cols, inner, batch,
                                 strides[0], strides[1], strides[2], flags,
                                 lda, ldb, ldc, *offsets) != 0:
            raise RuntimeError("xmx_rec_gemm: " + self.lib.xmx_error().decode())
        self.recorded += 1
        return self

    def unary(self, kind, source, target, count, *, scale=1.0, second=None,
              third=None, channels=0, epilogue=0, narrow=False, a_half=False,
              b_half=False, _dims=None, _pad=0):
        second = second if second is not None else source
        third = third if third is not None else source
        batch, height, width, across = _dims or (0, 0, 0, 0)
        if self.lib.xmx_rec_unary(kind | _publish(epilogue, narrow) | _reads(a_half, b_half),
                                  source.id, second.id, target.id, third.id,
                                  int(count), int(channels), float(scale),
                                  int(batch), int(height), int(width), int(across),
                                  int(_pad)) != 0:
            raise RuntimeError("xmx_rec_unary: " + self.lib.xmx_error().decode())
        self.recorded += 1
        return self

    def e4m3(self, source, target, count, scale=1.0):
        return self.unary(E4M3, source, target, count, scale=scale)

    def gate(self, source, target, count, scale=1.0):
        return self.unary(GATE, source, target, count, scale=scale)

    def half(self, source, target, count, scale=1.0):
        return self.unary(HALF, source, target, count, scale=scale)

    def gate_e4m3_half(self, source, target, count):
        """gate, publish and narrow in one read and one write.

        The graph writes a float32 hidden buffer, gates it, publishes it and narrows
        it to half — four trips over 503 MB at 720p in block 0 alone. This is one.
        """
        return self.unary(GATE_E4M3_HALF, source, target, count)

    def e4m3_half(self, source, target, count):
        return self.unary(E4M3_HALF, source, target, count)

    def gate_half(self, source, target, count):
        return self.unary(GATE_HALF, source, target, count)

    def to_half(self, source, target, count, scale=1.0):
        """float32 -> float16, for a GEMM operand."""
        return self.unary(TO_HALF, source, target, count, scale=scale)

    def from_half(self, source, target, count, scale=1.0):
        return self.unary(FROM_HALF, source, target, count, scale=scale)

    def scale(self, source, target, count, factor):
        return self.unary(SCALE, source, target, count, scale=factor)

    def residual(self, branch, skip, cosine, target, count, channels, *, reverse=None,
                 epilogue=0, narrow=False, a_half=False, b_half=False):
        """target = branch + skip * cosine, one cosine per channel."""
        if reverse is not None:
            height, width, size, origin = reverse
            _, pw, (top, left) = self.window_extent(height, width, origin, size)
            return self.unary(RESIDUAL | 0x4000, branch, target, count, second=skip,
                              third=cosine, channels=channels, epilogue=epilogue,
                              narrow=narrow, a_half=a_half, b_half=b_half,
                              _dims=(size, height, width, pw // size),
                              _pad=(top << 16) | left)
        return self.unary(RESIDUAL, branch, target, count, second=skip, third=cosine,
                          channels=channels, epilogue=epilogue, narrow=narrow,
                          a_half=a_half, b_half=b_half)

    @staticmethod
    def window_extent(height, width, origin=(0, 0), size=8):
        """-> (padded height, padded width, pads) for a shifted-window partition."""
        pad_top, pad_left = -origin[0], -origin[1]
        padded_height = pad_top + height + (-(height + pad_top)) % size
        padded_width = pad_left + width + (-(width + pad_left)) % size
        return padded_height, padded_width, (pad_top, pad_left)

    def partition(self, source, target, height, width, channels, size=8, origin=(0, 0),
                  *, epilogue=0, narrow=False, a_half=False):
        """NHWC -> (windows, tokens, channels), the shifted-window origin folded in.

        The vendor pads by up to one window before partitioning; rather than write that
        padded copy, the gather reads zero outside the image.
        """
        ph, pw, (top, left) = self.window_extent(height, width, origin, size)
        return self.unary(PARTITION, source, target, ph * pw * channels,
                          channels=channels, scale=1.0, epilogue=epilogue, narrow=narrow,
                          a_half=a_half,
                          _dims=(size, height, width, pw // size),
                          _pad=(top << 16) | left)

    def reverse(self, source, target, height, width, channels, size=8, origin=(0, 0)):
        """(windows, tokens, channels) -> NHWC, cropping the shifted-window pad away."""
        ph, pw, (top, left) = self.window_extent(height, width, origin, size)
        return self.unary(REVERSE, source, target, height * width * channels,
                          channels=channels, scale=1.0,
                          _dims=(size, height, width, pw // size),
                          _pad=(top << 16) | left)

    def split_heads(self, source, target, windows, tokens, channels, heads, part,
                    *, epilogue=0, narrow=False):
        """(windows, tokens, 3C) -> Q, K or V as (windows, heads, tokens, 32)."""
        return self.unary(SPLIT_HEADS, source, target, windows * tokens * channels,
                          channels=channels, epilogue=epilogue, narrow=narrow,
                          _dims=(heads, tokens, part, 0))

    def merge_heads(self, source, target, windows, tokens, channels, heads,
                    *, epilogue=0, narrow=False):
        """(windows, heads, tokens, 32) -> (windows, tokens, C)."""
        return self.unary(MERGE_HEADS, source, target, windows * tokens * channels,
                          channels=channels, epilogue=epilogue, narrow=narrow,
                          _dims=(heads, tokens, 0, 0))

    def pool2(self, source, target, height, width, channels, *, epilogue=0, narrow=False,
              a_half=False):
        """2x2 average pool, NHWC."""
        return self.unary(POOL2, source, target, (height // 2) * (width // 2) * channels,
                          channels=channels, epilogue=epilogue, narrow=narrow, a_half=a_half,
                          _dims=(0, height, width, 0))

    def upsample2(self, source, target, source_width, height, width, channels, *,
                  a_half=False, narrow=False):
        """Nearest 2x upsample, cropped to (height, width)."""
        return self.unary(UPSAMPLE2, source, target, height * width * channels,
                          channels=channels, a_half=a_half, narrow=narrow,
                          _dims=(0, width, source_width, 0))

    def pad_end(self, source, target, height, width, padded_height, padded_width, channels,
                *, a_half=False, narrow=False):
        """Extend to a larger extent with zeros, as `pad_spatial_end` does."""
        return self.unary(PAD_END, source, target,
                          padded_height * padded_width * channels, channels=channels,
                          a_half=a_half, narrow=narrow, _dims=(0, height, width, padded_width))

    def scale_channel(self, source, factors, target, count, channels, *, a_half=False):
        """target = source * factors, one factor per channel."""
        return self.unary(SCALE_CHANNEL, source, target, count, channels=channels,
                          third=factors, a_half=a_half)

    def add(self, left, right, target, count, *, epilogue=0, narrow=False,
            a_half=False, b_half=False):
        return self.unary(ADD, left, target, count, second=right, epilogue=epilogue,
                          narrow=narrow, a_half=a_half, b_half=b_half)

    def add_bias(self, source, bias, target, count, tokens, heads):
        """scores + the per-head attention bias."""
        return self.unary(ADD_BIAS, source, target, count, channels=tokens,
                          third=bias, _dims=(heads, 0, 0, 0))

    def cosine_publish(self, source, target, rows, *, tokens=0, heads=0, scale=None,
                       narrow=False, from_half=False, qkv_part=None):
        """Normalise rows of 32 through the kernel's fragment tree, then publish as E4M3.

        With `scale` the query path also multiplies by its head's `attn_scale`; rows are
        ordered (batch, head, token), so the head follows from the row index.
        """
        third = scale if scale is not None else source
        flags = COSINE_PUBLISH | _publish(0, narrow) | (0x8000 if from_half else 0)
        if qkv_part is not None:
            if qkv_part not in (0, 1) or tokens <= 0 or heads <= 0 or from_half:
                raise ValueError("QKV gather needs part 0/1, positive tokens/heads and float32 input")
            flags |= 0x20000 | (qkv_part << 18)
        if self.lib.xmx_rec_row(flags,
                                source.id, source.id, target.id, third.id,
                                int(rows), int(tokens), int(heads),
                                1 if scale is not None else 0, 0, 0.0) != 0:
            raise RuntimeError("xmx_rec_row: " + self.lib.xmx_error().decode())
        self.recorded += 1
        return self

    def softmax(self, source, target, rows, width, *, stride=0, cap=0.0, narrow=False,
                bias=None, heads=0):
        """The bit-affine softmax, one row per invocation.

        `stride` lets a row be wider than its token count, which the global blocks
        need: their token count is the bottleneck's pixel count and need not be a
        multiple of the tile. `cap` is the symmetric logit clamp the vit_1d kernels
        apply.
        """
        flags = SOFTMAX | _publish(0, narrow) | (0x2000 if bias is not None else 0)
        if self.lib.xmx_rec_row(flags, source.id, (bias or source).id, target.id, source.id,
                                int(rows), int(width), int(heads), 0,
                                int(stride), float(cap)) != 0:
            raise RuntimeError("xmx_rec_row: " + self.lib.xmx_error().decode())
        self.recorded += 1
        return self

    def sample_history(self, history, motion, target, height, width, channels=3,
                       absolute=False):
        """Reproject `history` with the recovered five-tap Catmull-Rom filter.

        `motion` holds either the offsets, or the sample coordinates themselves when
        `absolute`, which is the form `sample_history` in the reference takes.
        """
        if self.lib.xmx_rec_history(history.id, motion.id, target.id,
                                    height * width, channels, height, width,
                                    1 if absolute else 0) != 0:
            raise RuntimeError("xmx_rec_history: " + self.lib.xmx_error().decode())
        self.recorded += 1
        return self

    def submit(self):
        count = self.lib.xmx_submit()
        if count < 0:
            raise RuntimeError("xmx_submit: " + self.lib.xmx_error().decode())
        return count


class _Independent:
    __slots__ = ("runtime",)

    def __init__(self, runtime):
        self.runtime = runtime

    def __enter__(self):
        self.runtime.lib.xmx_sync(0)
        return self.runtime

    def __exit__(self, *exc):
        self.runtime.lib.xmx_sync(1)
        return False
