"""Optional native CPU image passes; NumPy remains the reference and fallback.

Once the network runs on a small enough frame it stops being the frame, and what is left
is a stack of full-frame passes over the *output* resolution — feature assembly, the two
resizes, the composition, the codec — none of which shrinks with the render scale. This
is those passes in C.

Written and measured in the parallel ProjectsCodex tree (`notes/phase57`); the two
functions this tree needs and that one does not — history in the feature channels and the
temporal composition — are added here. Every function is a transcription of the NumPy
beside it, and the output is required to be byte-identical: `src/ref/test_native_image.py`
runs both and compares.

`make` builds the library for this host, with `-march=native`, so rebuild it rather than
copying it. Every pass is split by rows across the library's own thread pool, one thread
per core by default (`NR_HOST_THREADS`, else `OMP_NUM_THREADS`, to change it; read once,
when the library first runs a pass); a row's arithmetic does not depend on which thread
does it, so the output is the same bytes at any thread count. The pool waits passively,
as upstream's OpenMP build does with `OMP_WAIT_POLICY=passive`.
`NR_HOST_NATIVE=0` selects the NumPy path for a paired measurement without
changing any model or shader setting. With no library at all everything still runs.
"""
import ctypes as C
from functools import lru_cache
import os
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'src'))
import nr_build  # noqa: E402


@lru_cache(maxsize=1)
def _library():
    try:
        lib = C.CDLL(str(nr_build.library('nr_image')))
    except OSError:
        return None
    ptr, stride, size = C.c_void_p, C.c_ssize_t, C.c_size_t
    lib.nr_features.argtypes = [ptr, stride, stride, stride, ptr, stride, stride, stride,
                                ptr, ptr, size, size, ptr, ptr, ptr]
    lib.nr_features.restype = None
    lib.nr_compose_temporal.argtypes = [
        ptr, stride, stride, stride, ptr, stride, stride, stride,
        ptr, stride, stride, stride, ptr, stride, stride, stride,
        ptr, stride, stride, ptr, C.c_float, ptr, stride, stride,
        size, size, C.c_float, C.c_float, C.c_float, C.c_float, ptr]
    lib.nr_compose_temporal.restype = None
    lib.nr_resize_axis.argtypes = [ptr, stride, stride, stride, size, size, size,
                                  C.c_int, ptr, ptr, ptr, ptr]
    lib.nr_resize_axis.restype = None
    lib.nr_compose.argtypes = [ptr, stride, stride, stride, ptr, stride, stride, stride,
                              size, size, C.c_float, ptr]
    lib.nr_compose.restype = None
    lib.nr_decode8.argtypes = [ptr, size, C.c_int, ptr]
    lib.nr_decode8.restype = None
    lib.nr_encode8.argtypes = [ptr, stride, stride, stride, ptr, size, size, C.c_int, ptr]
    lib.nr_encode8.restype = None
    return lib


def library():
    return None if os.environ.get('NR_HOST_NATIVE') == '0' else _library()


def _strides(array):
    return tuple(value // array.itemsize for value in array.strides)


def decode8(raw, width, height, bgra):
    lib = library()
    if lib is None:
        return None
    pixels = np.frombuffer(raw, np.uint8).reshape(height, width, 4)
    output = np.empty((height, width, 3), np.float32)
    lib.nr_decode8(pixels.ctypes.data, height * width, bgra, output.ctypes.data)
    return output


def encode8(image, raw, bgra):
    lib = library()
    if lib is None:
        return None
    image = np.require(image, dtype=np.float32, requirements=['A'])
    if image.ndim != 3 or image.shape[2] != 3:
        raise ValueError('encode expects RGB colour')
    height, width = image.shape[:2]
    pixels = np.frombuffer(raw, np.uint8).reshape(height, width, 4)
    output = np.empty((height, width, 4), np.uint8)
    lib.nr_encode8(image.ctypes.data, *_strides(image), pixels.ctypes.data,
                   height, width, bgra, output.ctypes.data)
    return output.tobytes()


def compose(head, colour, intensity):
    lib = library()
    if lib is None:
        return None
    head = np.require(head, dtype=np.float32, requirements=['A'])
    colour = np.require(colour, dtype=np.float32, requirements=['A'])
    if (head.ndim != 3 or colour.ndim != 3 or head.shape[:2] != colour.shape[:2]
            or head.shape[2] < 3 or colour.shape[2] != 3):
        raise ValueError('head and colour must share height and width')
    output = np.empty(colour.shape, np.float32)
    lib.nr_compose(head.ctypes.data, *_strides(head), colour.ctypes.data, *_strides(colour),
                   *colour.shape[:2], intensity, output.ctypes.data)
    return output


def features(colour, rows, columns, noise, controls, history=None):
    lib = library()
    if lib is None:
        return None
    colour = np.require(colour, dtype=np.float32, requirements=['A'])
    rows = np.require(rows, dtype=np.int32, requirements=['C', 'A'])
    columns = np.require(columns, dtype=np.int32, requirements=['C', 'A'])
    noise = np.require(noise, dtype=np.float32, requirements=['C', 'A'])
    controls = np.require(controls, dtype=np.float32, requirements=['C', 'A'])
    if rows.ndim != 1 or columns.ndim != 1:
        raise ValueError('native feature coordinates must be one-dimensional')
    height, width = len(rows), len(columns)
    if (colour.ndim != 3 or colour.shape[2] != 3 or controls.shape != (5,)
            or noise.shape != (height, width, 3) or height == 0 or width == 0
            or rows.min() < 0 or rows.max() >= colour.shape[0]
            or columns.min() < 0 or columns.max() >= colour.shape[1]):
        raise ValueError('invalid native feature inputs')
    if history is not None:
        history = np.require(history, dtype=np.float32, requirements=['A'])
        if history.shape != colour.shape:
            raise ValueError('history must match the colour it stands beside')
    output = np.empty((height, width, 16), np.float32)
    lib.nr_features(colour.ctypes.data, *_strides(colour),
                    history.ctypes.data if history is not None else None,
                    *(_strides(history) if history is not None else (0, 0, 0)),
                    rows.ctypes.data, columns.ctypes.data, height, width,
                    noise.ctypes.data, controls.ctypes.data, output.ctypes.data)
    return output


def compose_temporal(head, colour, history, previous, gate, mask, *, intensity,
                     blend_scale, hold, slope, table=None, confidence=1.0):
    """`nr_frame.compose` with a history, a floor and an optional control mask.

    With `table` — `nr_frame.gate_table`, NumPy's sigmoid on every half value — the gate
    and its `confidence` are computed in the pass, and `gate` is not read. Without it,
    `gate` is the model's own weight already through its sigmoid and its confidence in
    NumPy. Either way the sigmoid is NumPy's: `expf` and NumPy's float32 exponential
    disagree in the last bit, and the contract here is byte-identical output rather than
    nearly. `slope` is the folded constant of the floor, for the same reason — see the C.
    """
    lib = library()
    if lib is None:
        return None
    head = np.require(head, dtype=np.float32, requirements=['A'])
    colour = np.require(colour, dtype=np.float32, requirements=['A'])
    history = np.require(history, dtype=np.float32, requirements=['A'])
    if table is not None:
        # the gate from its 65536-entry table, in the pass itself (`nr_frame.gate_table`)
        table = np.require(table, dtype=np.float32, requirements=['C', 'A'])
        if table.shape != (1 << 16,):
            raise ValueError('the gate table has one entry for each of the 65536 half values')
        gate = np.zeros((1, 1, 1), np.float32)          # unread
    else:
        gate = np.require(gate, dtype=np.float32, requirements=['A'])
    if (head.ndim != 3 or head.shape[2] < 4 or colour.ndim != 3 or colour.shape[2] != 3
            or history.shape != colour.shape or head.shape[:2] != colour.shape[:2]
            or (table is None and (gate.shape[:2] != colour.shape[:2] or gate.ndim != 3
                                   or gate.shape[2] != 1))):
        raise ValueError('the temporal composition needs a four-channel head, a colour, '
                         'a history of the same shape and a single-channel gate')
    if previous is not None:
        previous = np.require(previous, dtype=np.float32, requirements=['A'])
        if previous.shape != colour.shape:
            raise ValueError('the previous frame must match the colour')
    if mask is not None:
        mask = np.require(mask, dtype=np.float32, requirements=['A'])
        if mask.ndim != 3 or mask.shape[:2] != colour.shape[:2]:
            raise ValueError('the control mask must match the colour')
    output = np.empty(colour.shape, np.float32)
    lib.nr_compose_temporal(
        head.ctypes.data, *_strides(head),
        colour.ctypes.data, *_strides(colour),
        history.ctypes.data, *_strides(history),
        previous.ctypes.data if previous is not None else None,
        *(_strides(previous) if previous is not None else (0, 0, 0)),
        gate.ctypes.data, *_strides(gate)[:2],
        table.ctypes.data if table is not None else None, confidence,
        mask.ctypes.data if mask is not None else None,
        *(_strides(mask)[:2] if mask is not None else (0, 0)),
        *colour.shape[:2], intensity, blend_scale, hold, slope, output.ctypes.data)
    return output


@lru_cache(maxsize=32)
def _axis_plan(extent, count):
    centres = (np.arange(count, dtype=np.float32) + 0.5) * (extent / count) - 0.5
    low = np.clip(np.floor(centres), 0, extent - 1).astype(np.int32)
    high = np.clip(low + 1, 0, extent - 1)
    weight = np.clip(centres - low, 0.0, 1.0).astype(np.float32)
    for array in (low, high, weight):
        array.flags.writeable = False
    return low, high, weight


def bilinear(image, size):
    lib = library()
    if lib is None:
        return None
    source = np.require(image, dtype=np.float32, requirements=['A'])
    if (source.ndim != 3 or min(source.shape) < 1 or len(size) != 2
            or any(not isinstance(count, (int, np.integer)) for count in size)
            or min(size) < 1):
        raise ValueError('bilinear expects nonempty HWC and positive target dimensions')
    for axis, count in enumerate(size):
        if source.shape[axis] == count:
            continue
        low, high, weight = _axis_plan(source.shape[axis], count)
        shape = list(source.shape)
        shape[axis] = count
        output = np.empty(shape, np.float32)
        lib.nr_resize_axis(source.ctypes.data, *_strides(source), *shape, axis,
                           low.ctypes.data, high.ctypes.data, weight.ctypes.data,
                           output.ctypes.data)
        source = output
    return source
