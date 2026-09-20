#!/usr/bin/env python3
"""
nr_frame_resident — the whole 71-block graph on the device.

`nr_resident` records a block; this records a frame. Activations never come back to
the host: the stem writes a device buffer, every block, transition and skip reads and
writes device buffers, and only the head is read out.

Levels, for a network extent (H, W). The encoder halves five times and the decoder
mirrors it; the two deepest transitions pad to a multiple of the window first.

    L0  H     x W      C=32     block 0, and block 70 at the end
    L1  H/2   x W/2    C=32     blocks 1-3,   67-69
    L2  H/4   x W/4    C=64     blocks 5-7,   63-65
    L3  H/8   x W/8    C=128    blocks 9-13,  57-61
    L4  H/16  x W/16   C=256    blocks 15-21, 49-55
    L5  pad8(L4)/2     C=512    blocks 23-30, 40-47
    L6  pad8(L5)/2     C=1024   blocks 31-38, every token attending to every other

A block is safe writing over its own input — its target is touched only by the final
residual, after every read of the source — so a level needs one value buffer, not two.
"""
from __future__ import annotations

import os
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "ref"))

import nr_model  # noqa: E402
import nr_resident as R  # noqa: E402
import xmxres  # noqa: E402


host_copy = xmxres.host_view          # diagnostics read buffers the graph never returns

ENCODER = ((range(1, 4), 4, 1), (range(5, 8), 8, 2),
           (range(9, 14), 14, 4), (range(15, 22), 22, 8))
DECODER = ((48, range(49, 56), 4, 8), (56, range(57, 62), 3, 4),
           (62, range(63, 66), 2, 2), (66, range(67, 70), 1, 1))


def pad8(extent):
    return -(-extent // 8) * 8


class Edge:
    """A transition's weights: a projection and, for the decoder, a skip scale."""

    def __init__(self, runtime, weight, sine=None):
        self.weight0 = runtime.buffer_from(weight, np.float16)
        self.out_channels = weight.shape[1]
        self.sine = None if sine is None else runtime.buffer_from(sine)


class DeviceWeights:
    """Every weight buffer, and nothing that depends on the extent.

    They used to be the frame's, and a frame belongs to one extent — so moving the render
    scale rebuilt them, ~292 MB written again for a knob. Nothing in them knows the extent:
    blocks are keyed by index, edges by index and direction. So they live here, one set per
    backend, and a new extent rebuilds the graph and its scratch alone.
    """

    def __init__(self, runtime, weights):
        self.rt, self.weights = runtime, weights
        self._blocks, self._edges = {}, {}
        self._upload_edges()

    def _upload_edges(self):
        take = lambda name: self.weights[name]
        self.adapter = self.rt.buffer_from(take("block0.layer0.input_adapter_weight"),
                                           np.float16)
        self.bottleneck = Edge(self.rt, take("block30.layer4.weight"))
        self.decoder_input = Edge(self.rt, take("block39.layer0.conv_weight"),
                                  take("block39.layer0.inp_upsample_sin"))
        self.merge_sin = self.rt.buffer_from(take("block70.layer0.inp_merge_sin"))
        self.merge_cos = self.rt.buffer_from(take("block70.layer0.inp_merge_cos"))
        # the head is 32 -> 4, and the cooperative matrix wants a multiple of 16
        # columns; both halves go into one padded matrix and the first four columns
        # of the product are the head
        head = np.zeros((32, 16), dtype=np.float32)
        head[:16, :4] = take("block70.layer0.out_gain")
        head[16:, :4] = take("block70.layer0.out_conv_weight")
        self.head = self.rt.buffer_from(head, np.float16)

    def block(self, index, heads, family="window"):
        if (index, family) not in self._blocks:
            builder = {"split": lambda: R.SplitBlockWeights(self.rt, self.weights, index),
                       "global": lambda: R.GlobalBlockWeights(self.rt, self.weights, index),
                       "window": lambda: R.BlockWeights(self.rt, self.weights, index,
                                                        heads=heads)}[family]
            self._blocks[(index, family)] = builder()
        return self._blocks[(index, family)]

    def edge(self, index, kind):
        if (index, kind) not in self._edges:
            prefix = f"block{index}.layer0"
            self._edges[(index, kind)] = Edge(
                self.rt, self.weights[f"{prefix}.weight0"],
                self.weights.get(f"{prefix}.sin") if kind == "up" else None)
        return self._edges[(index, kind)]

    def close(self):
        self._blocks.clear()
        self._edges.clear()
        for name in ResidentFrame.WEIGHT_NAMES:
            if hasattr(self, name):
                delattr(self, name)


class ResidentFrame:
    """The graph and its buffers, for one network extent. The weights are shared."""

    # the six named weight buffers live on `DeviceWeights` now; the body of a frame still
    # says `self.adapter`, because where they are kept is not that code's business
    WEIGHT_NAMES = ("adapter", "bottleneck", "decoder_input", "merge_sin", "merge_cos", "head")

    def __getattr__(self, name):
        if name in ResidentFrame.WEIGHT_NAMES:
            return getattr(self.__dict__["w"], name)
        raise AttributeError(name)

    def __init__(self, runtime, weights, height, width):
        self.split = (0.0, 0.0, 0.0)      # host write, graph, host read, of the last frame
        # a caller may hand over shared weights or the raw tensors; the benches and tests
        # hand over tensors, and then this frame owns the upload as it always did
        self.w = weights if isinstance(weights, DeviceWeights) else DeviceWeights(runtime, weights)
        self._owns_weights = self.w is not weights
        self.rt, self.weights = runtime, self.w.weights
        self.height, self.width = height, width
        self.levels = self._plan(height, width)
        self._scratch, self._blocks, self._buffers, self._edges = {}, {}, {}, {}
        self._graphs = {}
        self._arena = xmxres.ScratchArena(runtime) if os.environ.get("NR_SCRATCH_ARENA", "1") != "0" else None
        self._closed = False

    @staticmethod
    def _plan(height, width):
        levels = [(height, width, 32), (height // 2, width // 2, 32),
                  (height // 4, width // 4, 64), (height // 8, width // 8, 128),
                  (height // 16, width // 16, 256)]
        h, w = pad8(levels[4][0]) // 2, pad8(levels[4][1]) // 2
        levels.append((h, w, 512))
        levels.append((pad8(h) // 2, pad8(w) // 2, 1024))
        return levels


    # -- lazily built and reused -----------------------------------------

    def block(self, index, heads, family="window"):
        return self.w.block(index, heads, family)

    def scratch(self, block, height, width, tokens=None):
        key = (height, width, tokens, block.channels, block.heads,
               getattr(block, "split", False), getattr(block, "branched", False))
        if key not in self._scratch:
            self._scratch[key] = (R.GlobalScratch(self.rt, block, tokens, arena=self._arena) if tokens
                                  else R.BlockScratch(self.rt, block, height, width, arena=self._arena))
        return self._scratch[key]

    def transition_scratch(self, elements):
        rounded = 1 << max(1, int(elements - 1)).bit_length()
        if rounded not in self._edges:
            self._edges[rounded] = R.TransitionScratch(self.rt, rounded, 0, arena=self._arena)
        return self._edges[rounded]

    # Two buffers in the whole graph are touched by the host, once each per frame: the
    # features go in and the head comes back. They are named here so they can be put where
    # the host can reach them — cached for the strided read of the head — while everything
    # else follows the device, which on a discrete card means the card's own memory.
    HOST_SIDE = {"features": xmxres.HOST_WRITE, "head": xmxres.HOST_READ}

    def buffer(self, name, elements, dtype=np.float32):
        existing = self._buffers.get(name)
        if existing is None or existing.nbytes < elements * np.dtype(dtype).itemsize:
            if existing is not None:
                existing.free()
            existing = self.rt.buffer(elements, dtype,
                                      kind=self.HOST_SIDE.get(name, xmxres.GRAPH))
            self._buffers[name] = existing
        return existing

    def edge(self, index, kind):
        return self.w.edge(index, kind)

    def _prepare_scratch(self):
        """Plan every role's maximum size before a buffer address can be recorded."""
        if self._arena is None or self._arena.sealed:
            return
        for index in (0, 70):
            self.scratch(self.block(index, 1), self.height, self.width)
        for level, (regular, transition, heads) in enumerate(ENCODER, 1):
            h, w, channels = self.levels[level]
            for index in (*regular, transition):
                self.scratch(self.block(index, heads), h, w)
            self.transition_scratch(pad8(h) * pad8(w) * channels)
        h, w, channels = self.levels[5]
        for index in (*range(23, 31), *range(40, 48)):
            self.scratch(self.block(index, 16, "split"), h, w)
        self.transition_scratch(pad8(h) * pad8(w) * channels)
        self.transition_scratch(h * w * channels)
        gh, gw, _ = self.levels[6]
        for index in range(31, 39):
            self.scratch(self.block(index, 32, "global"), gh, gw, tokens=gh * gw)
        for transition, regular, level, heads in DECODER:
            sh, sw, schannels = self.levels[level]
            self.transition_scratch(sh * sw * max(channels, schannels))
            for index in (transition, *regular):
                self.scratch(self.block(index, heads), sh, sw)
            channels = schannels
        self._arena.seal()

    def close(self):
        """Release recorded commands before any buffers they reference."""
        for graph in self._graphs.values():
            graph.free()
        self._graphs.clear()
        self._closed = True
        for cache in (self._scratch, self._blocks, self._buffers, self._edges):
            cache.clear()
        if self._arena is not None:
            self._arena.free()
        # shared weights outlive the frame; ones this frame uploaded itself do not
        if self._owns_weights:
            self.w.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    # -- the frame --------------------------------------------------------

    def run(self, features, submits=None, capture=None, timing=None, execution=None):
        try:
            return self._run(features, submits, capture, timing, execution)
        except Exception:
            self.rt.abort()
            raise

    def _run(self, features, submits, capture, timing, execution):
        rt = self.rt
        import time as _time

        if self._closed:
            raise RuntimeError("ResidentFrame is closed")
        if features.shape != (self.height, self.width, 16):
            raise ValueError("features must match the frame's (height, width, 16)")
        self._prepare_scratch()
        execution = execution or os.environ.get("NR_FRAME_MODE", "replay")
        if execution not in ("block", "single", "replay"):
            raise ValueError("NR_FRAME_MODE must be block, single or replay")
        # Diagnostic reads require a fence after each block.
        if capture is not None or timing is not None:
            execution = "block"
        batched = execution != "block"
        staged = rt.staging
        key = rt.graph_key()
        recording = False

        def begin():
            nonlocal recording
            if not batched or not recording:
                rt.begin()
                recording = True

        def keep(name, buffer, count, shape=None, dtype=np.float32):
            if capture is not None:
                data = host_copy(buffer, dtype, count).astype(np.float32)
                capture[name] = data if shape is None else data.reshape(shape)

        height, width = self.height, self.width
        pixels = height * width
        counter = [0]

        stage = ["stem"]

        def submit():
            if batched:
                return
            started = _time.perf_counter()
            counter[0] += rt.submit()
            if timing is not None:
                timing.setdefault(stage[0], [0.0, 0])
                timing[stage[0]][0] += _time.perf_counter() - started
                timing[stage[0]][1] += 1

        stem = self.buffer("stem", pixels * 32)
        source = self.buffer("features", pixels * 16)
        # the three parts of a frame, timed separately: on a card the two transfers are
        # PCIe and the middle one is the GPU, and a single total cannot tell them apart
        mark = _time.perf_counter()
        xmxres.host_write(source, features.reshape(-1, 16), rows=(pixels, 16))
        carried = _time.perf_counter() - mark
        if execution == "replay" and key in self._graphs:
            mark = _time.perf_counter()
            passes = self._graphs[key].run()
            ran = _time.perf_counter() - mark
            if submits is not None:
                submits.append(passes)
            mark = _time.perf_counter()
            out = np.array(xmxres.host_view(self.buffer("head", pixels * 16),
                                            shape=(pixels, 16))[:, :4], copy=True)
            self.split = (carried, ran, _time.perf_counter() - mark)
            return out.reshape(height, width, 4)
        begin()
        rt.to_half(source, self.buffer("features16", pixels * 16, np.float16), pixels * 16)
        rt.gemm(self.buffer("features16", pixels * 16, np.float16), self.adapter, stem,
                pixels, 32, 16)
        submit()

        # block 0 runs at full resolution; its output is both the skip the post block
        # merges and, pooled, the encoder's input
        stage[0] = "block0 + pool"
        block0 = self.block(0, 1)
        raw = self.buffer("block0", pixels * 32)
        # Everything the graph publishes is E4M3, which is exact in float16, so every
        # published buffer is stored narrow: half the traffic, and the widening pass in
        # front of each block's first GEMM disappears. `src/bench/bf16_check.py`.
        full_skip = self.buffer("full_skip", pixels * 32, np.float16)
        h, w, channels = self.levels[1]
        value = self.buffer("l1", h * w * 32, np.float16)
        begin()
        R.record_block(rt, block0, self.scratch(block0, height, width),
                       source=stem, target=raw)
        # the post block's skip is block 0 published; the encoder pools the
        # *unpublished* output, so both come from `raw` and neither from the other
        with rt.independent():
            rt.e4m3_half(raw, full_skip, pixels * 32)
            rt.pool2(raw, value, height, width, 32, epilogue=xmxres.EPI_E4M3, narrow=True)
        submit()
        keep("stem", stem, pixels * 32, (1, height, width, 32))
        keep("block0", raw, pixels * 32, (1, height, width, 32))
        keep("full_skip", full_skip, pixels * 32, (1, height, width, 32), np.float16)
        keep("l1_in", value, h * w * 32, (1, h, w, 32), np.float16)

        skips, level = {}, 1
        for regular, transition, heads in ENCODER:
            h, w, channels = self.levels[level]
            for index in regular:
                stage[0] = f"encoder L{level} blocks (C={channels})"
                block = self.block(index, heads)
                begin()
                R.record_block(rt, block, self.scratch(block, h, w), source=value,
                               target=value, publish=xmxres.EPI_E4M3,
                               source_half=True, target_half=True)
                submit()
            keep(f"l{level}", value, h * w * channels, (1, h, w, channels), np.float16)
            skips[level] = self.buffer(f"skip{level}", h * w * channels, np.float16)
            if batched or staged:
                begin()
                rt.copy(value, skips[level], h * w * channels * 2)
                submit()
            else:
                skips[level].view(np.float16)[:h * w * channels] = \
                    value.view(np.float16)[:h * w * channels]

            block = self.block(transition, heads)
            edge = self.edge(transition, "down")
            nh, nw, nchannels = self.levels[level + 1]
            unpublished = self.buffer("unpublished", h * w * channels)
            nxt = self.buffer(f"l{level + 1}", nh * nw * nchannels, np.float16)
            padded = pad8(h) * pad8(w) * channels
            stage[0] = f"downsample L{level}->L{level + 1}"
            begin()
            R.record_block(rt, block, self.scratch(block, h, w), source=value,
                           target=unpublished, source_half=True)
            R.record_downsample(rt, edge, self.transition_scratch(padded), unpublished,
                                nxt, h, w, channels, pad_to=8 if transition == 22 else 0,
                                target_half=True)
            submit()
            value, level = nxt, level + 1
            keep(f"l{level}_in", value, nh * nw * nchannels, (1, nh, nw, nchannels),
                 np.float16)

        # the split family, then the bottleneck
        h, w, channels = self.levels[5]
        stage[0] = "split blocks 23-30 (C=512)"
        for index in range(23, 31):
            block = self.block(index, 16, "split")
            begin()
            R.record_block(rt, block, self.scratch(block, h, w), source=value,
                           target=value, publish=xmxres.EPI_E4M3,
                           source_half=True, target_half=True)
            submit()
        keep("l5", value, h * w * channels, (1, h, w, channels), np.float16)
        split_skip = self.buffer("split_skip", h * w * channels, np.float16)
        if batched or staged:
            begin()
            rt.copy(value, split_skip, h * w * channels * 2)
            submit()
        else:
            split_skip.view(np.float16)[:h * w * channels] = \
                value.view(np.float16)[:h * w * channels]

        gh, gw, gchannels = self.levels[6]
        deep = self.buffer("l6", gh * gw * gchannels, np.float16)
        begin()
        R.record_plain_downsample(rt, self.bottleneck, self.transition_scratch(
            pad8(h) * pad8(w) * channels), value, deep, h, w, channels, pad_to=8,
            source_half=True, target_half=True)
        submit()

        tokens = gh * gw
        stage[0] = "global blocks 31-38 (C=1024)"
        for index in range(31, 39):
            block = self.block(index, 32, "global")
            scratch = self.scratch(block, gh, gw, tokens=tokens)
            begin()
            # Published values are exact in both widths. Convert on-device when
            # batching; the block mode retains the original host-copy reference.
            if batched or staged:
                rt.from_half(deep, scratch.value, tokens * gchannels)
            else:
                scratch.value.view()[:tokens * gchannels] = \
                    deep.view(np.float16)[:tokens * gchannels]
            R.record_global_block(rt, block, scratch)
            rt.e4m3(scratch.out, scratch.out, scratch.padded * gchannels)
            if batched or staged:
                rt.to_half(scratch.out, deep, tokens * gchannels)
            submit()
            if not batched and not staged:
                deep.view(np.float16)[:tokens * gchannels] = \
                    scratch.out.view()[:tokens * gchannels]

        # the decoder input merge, then the split family again
        begin()
        R.record_upsample_merge(rt, self.decoder_input, self.transition_scratch(
            h * w * channels), deep, split_skip, value, gh, gw, h, w, gchannels, channels,
            source_half=True, skip_half=True, target_half=True)
        submit()
        stage[0] = "split blocks 40-47 (C=512)"
        for index in range(40, 48):
            block = self.block(index, 16, "split")
            begin()
            R.record_block(rt, block, self.scratch(block, h, w), source=value,
                           target=value, publish=xmxres.EPI_E4M3,
                           source_half=True, target_half=True)
            submit()

        for transition, regular, skip_level, heads in DECODER:
            stage[0] = f"decoder upsample -> L{skip_level}"
            sh, sw, schannels = self.levels[skip_level]
            edge = self.edge(transition, "up")
            target = self.buffer(f"d{skip_level}", sh * sw * schannels, np.float16)
            begin()
            R.record_upsample_merge(rt, edge, self.transition_scratch(
                sh * sw * max(channels, schannels)), value, skips[skip_level], target,
                h, w, sh, sw, channels, schannels,
                source_half=True, skip_half=True, target_half=True)
            block = self.block(transition, heads)
            R.record_block(rt, block, self.scratch(block, sh, sw), source=target,
                           target=target, publish=xmxres.EPI_E4M3,
                           source_half=True, target_half=True)
            submit()
            value, h, w, channels = target, sh, sw, schannels
            for index in regular:
                stage[0] = f"decoder L{skip_level} blocks (C={channels})"
                block = self.block(index, heads)
                begin()
                R.record_block(rt, block, self.scratch(block, h, w), source=value,
                               target=value, publish=xmxres.EPI_E4M3,
                               source_half=True, target_half=True)
                submit()

        # back to full resolution, merged with block 0's output, then the head
        stage[0] = "block70 + head"
        merged = self.buffer("merged", pixels * 32)
        upsampled = self.buffer("upsampled", pixels * 32)
        block70 = self.block(70, 1)
        out = self.buffer("out", pixels * 32)
        begin()
        rt.upsample2(value, upsampled, w, height, width, 32, a_half=True)
        rt.scale_channel(upsampled, self.merge_sin, merged, pixels * 32, 32)
        rt.residual(merged, full_skip, self.merge_cos, merged, pixels * 32, 32,
                    b_half=True)
        R.record_block(rt, block70, self.scratch(block70, height, width),
                       source=merged, target=out)
        rt.to_half(out, self.buffer("out16", pixels * 32, np.float16), pixels * 32)
        rt.gemm(self.buffer("out16", pixels * 32, np.float16), self.head,
                self.buffer("head", pixels * 16), pixels, 16, 32)
        submit()

        if execution == "replay":
            self._graphs[key] = rt.capture()
            counter[0] = self._graphs[key].run()
        elif execution == "single":
            counter[0] = rt.submit()
        if submits is not None:
            submits.append(counter[0])
        return np.array(xmxres.host_view(self.buffer("head", pixels * 16),
                                        shape=(pixels, 16))[:, :4],
                        copy=True).reshape(height, width, 4)
