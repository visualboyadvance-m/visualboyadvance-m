#!/usr/bin/env python3
"""
nr_resident — whole blocks of the graph recorded as one device submit.

`xmxres` gives the primitives; this assembles them into the shapes the recovered
graph actually has. One block is a single command buffer: its feed-forward, its
window attention and both residuals, with nothing crossing back to the host in
between.

The E4M3 publishes are what make the layout work. Because the publish is
elementwise, a branched feed-forward can write each of its heads straight into a
slice of one wide buffer and publish the whole thing in a single dense pass — no
concatenation, no strided elementwise kernel.
"""
from __future__ import annotations

import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "ref"))

import nr_model  # noqa: E402
import xmxres  # noqa: E402


class SplitBlockWeights:
    """A split-family block (23-30, 40-47): four layers, sixteen heads, C=512."""

    def __init__(self, runtime, weights, index):
        self.index, self.heads = index, 16
        self.origin = nr_model.recovered_window_origin(index)
        self.projection = weights[f"block{index}.layer3.projection_weight"]
        self.channels = self.projection.shape[0]
        self.groups = self.channels // 64
        bias = weights[f"block{index}.layer2.attn_bias"]
        if nr_model.uses_fragment_swizzle(index, 16):
            bias = nr_model.recover_attention_bias_layout(bias)
        self.first = runtime.buffer_from(weights[f"block{index}.layer0.first_projection_weight"],
                                         np.float16)
        self.expand = runtime.buffer_from(weights[f"block{index}.layer0.group_expand_weight"],
                                          np.float16)
        self.project = runtime.buffer_from(weights[f"block{index}.layer0.group_project_weight"],
                                           np.float16)
        self.weight3 = runtime.buffer_from(weights[f"block{index}.layer1.weight3"], np.float16)
        self.ffn_cos = runtime.buffer_from(weights[f"block{index}.layer1.ffn_cos_skip"])
        self.qkv = runtime.buffer_from(weights[f"block{index}.layer2.qkv_weight"], np.float16)
        self.scale = runtime.buffer_from(weights[f"block{index}.layer2.attn_scale"])
        self.bias = runtime.buffer_from(bias)
        self.out = runtime.buffer_from(self.projection, np.float16)
        self.attn_cos = runtime.buffer_from(weights[f"block{index}.layer3.attn_cos_skip"])
        self.branched = False
        self.split = True


class BlockWeights:
    """One block's weights, uploaded once and kept on the device."""

    def __init__(self, runtime, weights, index, *, heads):
        prefix = f"block{index}.layer0"
        self.split = False
        self.index, self.heads = index, heads
        self.origin = nr_model.recovered_window_origin(index)
        self.projection = weights[f"{prefix}.projection_weight"]
        self.channels = self.projection.shape[0]

        bias = weights[f"{prefix}.attn_bias"]
        if nr_model.uses_fragment_swizzle(index, heads):
            bias = nr_model.recover_attention_bias_layout(bias)

        take = lambda name, dtype=np.float16: runtime.buffer_from(
            weights[f"{prefix}.{name}"], dtype)
        self.qkv = take("qkv_weight")
        self.out = take("projection_weight")
        self.bias = runtime.buffer_from(bias, np.float32)
        self.scale = runtime.buffer_from(weights[f"{prefix}.attn_scale"], np.float32)
        self.attn_cos = runtime.buffer_from(weights[f"{prefix}.attn_cos_skip"])
        self.ffn_cos = runtime.buffer_from(weights[f"{prefix}.ffn_cos_skip"])

        self.branched = f"{prefix}.ffn_expand_weight" in weights
        if self.branched:
            expansion, branch = nr_model._fused_branched_weights(
                weights[f"{prefix}.ffn_expand_weight"],
                weights[f"{prefix}.ffn_branch_projection_weight"])
            self.groups = expansion.shape[0]
            self.expand = runtime.buffer_from(expansion, np.float16)
            self.branch = runtime.buffer_from(branch, np.float16)
            self.ffn_out = take("ffn_output_projection_weight")
        else:
            self.groups = 0
            self.expand = take("weight1")
            self.branch = take("weight2")
            self.ffn_out = None


class GlobalBlockWeights:
    """A bottleneck block (31-38): every token attends to every other, 32 heads, C=1024."""

    def __init__(self, runtime, weights, index):
        import math
        self.index, self.heads, self.split, self.branched = index, 32, False, False
        self.projection = weights[f"block{index}.layer4.projection_weight"]
        self.channels = self.projection.shape[0]
        self.expand = runtime.buffer_from(weights[f"block{index}.layer0.weight"], np.float16)
        self.ffn_proj = runtime.buffer_from(weights[f"block{index}.layer1.weight"], np.float16)
        self.hidden_width = weights[f"block{index}.layer0.weight"].shape[1]
        self.ffn_cos = runtime.buffer_from(weights[f"block{index}.layer1.ffn_cos_skip"])
        self.qkv = runtime.buffer_from(weights[f"block{index}.layer2.qkv_weight"], np.float16)
        # the global kernels fold sqrt(head_dim) into the per-head scale
        scale = (weights[f"block{index}.layer2.attn_scale"]
                 * np.float32(math.sqrt(self.channels // self.heads)))
        self.scale = runtime.buffer_from(scale)
        self.out = runtime.buffer_from(self.projection, np.float16)
        self.attn_cos = runtime.buffer_from(weights[f"block{index}.layer4.attn_cos_skip"])
        self.logit_cap = nr_model.GLOBAL_ATTENTION_LOGIT_CAP


class GlobalScratch:
    """Working buffers for one bottleneck block over `tokens` tokens."""

    def __init__(self, runtime, weights, tokens, arena=None):
        channels, heads = weights.channels, weights.heads
        # the token count is the bottleneck's pixel count and need not be tile-aligned
        self.tokens, self.padded = tokens, xmxres.align(tokens, 16)
        padded, hidden = self.padded, weights.hidden_width
        make = (arena.buffer if arena is not None else
                lambda name, count, dtype=np.float32: runtime.buffer(count, dtype))
        self.value = make("global.value", padded * channels).zero()
        self.value16 = make("value16", padded * channels, np.float16)
        self.hidden16 = make("hidden16", padded * hidden, np.float16)
        self.branch = make("branch", padded * channels)
        self.ffn = make("ffn", padded * channels)
        self.ffn16 = make("ffn16", padded * channels, np.float16)
        self.proj = make("proj", padded * channels * 3)
        self.q16, self.k16, self.v16 = (make(name, padded * channels, np.float16) for name in ("q16", "k16", "v16"))
        self.scores = make("scores", heads * padded * padded)
        self.probs16 = make("probs16", heads * padded * padded, np.float16)
        self.context = make("context", heads * padded * 32)
        self.merged16 = make("merged16", padded * channels, np.float16)
        self.attention = make("attention", padded * channels)
        self.out = make("global.out", padded * channels)

    def free(self):
        for name in dir(self):
            value = getattr(self, name)
            if isinstance(value, xmxres.Buffer):
                value.free()


def record_qkv(runtime, w, s, windows, tokens, channels, heads):
    """Split V; optionally normalize Q/K directly from the projection buffer."""
    if runtime.fuse_qk:
        with runtime.independent():
            runtime.cosine_publish(s.proj, s.q16, windows * heads * tokens,
                                   tokens=tokens, heads=heads, scale=w.scale,
                                   narrow=True, qkv_part=0)
            runtime.cosine_publish(s.proj, s.k16, windows * heads * tokens,
                                   tokens=tokens, heads=heads, narrow=True, qkv_part=1)
            runtime.split_heads(s.proj, s.v16, windows, tokens, channels, heads, 2,
                                epilogue=xmxres.EPI_E4M3, narrow=True)
        return
    with runtime.independent():
        for index, part in enumerate((s.q16, s.k16)):
            runtime.split_heads(s.proj, part, windows, tokens, channels, heads, index,
                                epilogue=xmxres.EPI_HALF, narrow=True)
        runtime.split_heads(s.proj, s.v16, windows, tokens, channels, heads, 2,
                            epilogue=xmxres.EPI_E4M3, narrow=True)
    with runtime.independent():
        runtime.cosine_publish(s.q16, s.q16, windows * heads * tokens,
                               tokens=tokens, heads=heads, scale=w.scale,
                               narrow=True, from_half=True)
        runtime.cosine_publish(s.k16, s.k16, windows * heads * tokens,
                               tokens=tokens, heads=heads, narrow=True, from_half=True)


def record_global_block(runtime, w, s, source=None, target=None):
    """A bottleneck block: the wide feed-forward, then attention over every token."""
    source = source or s.value
    target = target or s.out
    channels, heads, padded = w.channels, w.heads, s.padded
    runtime.to_half(source, s.value16, padded * channels)
    runtime.gemm(s.value16, w.expand, s.hidden16, padded, w.hidden_width, channels,
                 epilogue=xmxres.EPI_GATE_E4M3, narrow=True)
    runtime.gemm(s.hidden16, w.ffn_proj, s.branch, padded, channels, w.hidden_width)
    runtime.residual(s.branch, source, w.ffn_cos, s.ffn, padded * channels, channels)

    runtime.to_half(s.ffn, s.ffn16, padded * channels)
    runtime.gemm(s.ffn16, w.qkv, s.proj, padded, 3 * channels, channels)
    record_qkv(runtime, w, s, 1, padded, channels, heads)
    runtime.gemm(s.q16, s.k16, s.scores, padded, padded, 32, batch=heads,
                 strides=(padded * 32, padded * 32, padded * padded), transpose_b=True)
    # no attention bias here, and the logits are clamped symmetrically
    runtime.softmax(s.scores, s.probs16, heads * padded, s.tokens,
                    stride=padded, cap=w.logit_cap, narrow=True)
    runtime.gemm(s.probs16, s.v16, s.context, padded, 32, padded, batch=heads,
                 strides=(padded * padded, padded * 32, padded * 32))
    runtime.merge_heads(s.context, s.merged16, 1, padded, channels, heads,
                        epilogue=xmxres.EPI_E4M3, narrow=True)
    runtime.gemm(s.merged16, w.out, s.attention, padded, channels, channels)
    runtime.residual(s.attention, s.ffn, w.attn_cos, target, padded * channels, channels)


def run_global_block(runtime, w, s, value):
    """Host convenience: `value` is (tokens, channels)."""
    tokens, channels = value.shape
    xmxres.host_write(s.value, value, rows=(s.padded, channels))
    runtime.begin()
    record_global_block(runtime, w, s)
    passes = runtime.submit()
    return xmxres.host_view(s.out, shape=(s.padded, channels))[:tokens].copy(), passes


class BlockScratch:
    """Working buffers for one block at one extent, allocated once and reused."""

    def __init__(self, runtime, weights, height, width, arena=None):
        channels, heads = weights.channels, weights.heads
        # Sized for the largest window count any origin can produce, so blocks at the
        # same level share one scratch even though their shifts differ. Getting this
        # wrong is silent: a shifted block has more windows than an unshifted one and
        # would write past the end of buffers cut to the unshifted size.
        padded_height, padded_width, _ = runtime.window_extent(height, width, (-4, -4))
        self.height, self.width = height, width
        self.tokens = 64
        self.windows = (padded_height // 8) * (padded_width // 8)
        self.batch = self.windows * heads
        pixels = height * width
        windowed = self.windows * self.tokens * channels
        hidden = weights.groups * 128 if weights.branched else weights.expand.nbytes // 2 // channels

        if getattr(weights, "split", False):
            hidden = weights.groups * 256
        # Every buffer a block wrote in float32 only to read it straight back went
        # away when the publishes moved into the pass that produces the value.
        make = (arena.buffer if arena is not None else
                lambda name, count, dtype=np.float32: runtime.buffer(count, dtype))
        self.value = make("value", pixels * channels)
        self.value16 = make("value16", pixels * channels, np.float16)
        self.hidden16 = make("hidden16", pixels * hidden, np.float16)
        self.heads16 = (make("heads16", pixels * channels, np.float16)
                        if weights.branched or getattr(weights, "split", False) else None)
        self.branch = make("branch", pixels * channels)
        self.ffn = make("ffn", pixels * channels)
        self.win16 = make("win16", windowed, np.float16)
        self.proj = make("proj", windowed * 3)
        self.q16, self.k16, self.v16 = (make(name, windowed, np.float16) for name in ("q16", "k16", "v16"))
        self.scores = make("scores", self.batch * self.tokens * self.tokens)
        self.probs16 = make("probs16", self.batch * self.tokens * self.tokens, np.float16)
        self.context = make("context", self.batch * self.tokens * 32)
        self.merged16 = make("merged16", windowed, np.float16)
        self.attended = make("attended", windowed)
        self.out = make("out", pixels * channels)
        self.core16 = (make("core16", pixels * channels, np.float16)
                       if getattr(weights, "split", False) else None)
        self.hidden_width = hidden

    def free(self):
        for name in dir(self):
            value = getattr(self, name)
            if isinstance(value, xmxres.Buffer):
                value.free()


def record_feed_forward(runtime, w, s, source, source_half=False):
    """The block's feed-forward, into `s.ffn`. Branched or plain, as the block is.

    `source_half` says the block's input is already float16 — true whenever it is an
    E4M3 publish, which is exact in half — so the widening pass in front of the first
    GEMM is not needed and the residual reads the narrow buffer directly.
    """
    pixels, channels = s.height * s.width, w.channels
    value16 = source if source_half else s.value16
    if not source_half:
        runtime.to_half(source, s.value16, pixels * channels)
    if w.branched:
        with runtime.independent():
            for head in range(w.groups):
                runtime.gemm(value16, w.expand, s.hidden16, pixels, 128, channels,
                             leading=(0, 0, s.hidden_width),
                             offsets=(0, head * channels * 128, head * 128),
                             epilogue=xmxres.EPI_GATE_E4M3, narrow=True)
        with runtime.independent():
            for head in range(w.groups):
                runtime.gemm(s.hidden16, w.branch, s.heads16, pixels, 32, 128,
                             leading=(s.hidden_width, 0, channels),
                             offsets=(head * 128, head * 128 * 32, head * 32),
                             epilogue=xmxres.EPI_E4M3, narrow=True)
        runtime.gemm(s.heads16, w.ffn_out, s.branch, pixels, channels, channels)
        # the fused multi-head kernels publish the residual before attention reads it,
        # which the residual now does on its way out
        runtime.residual(s.branch, source, w.ffn_cos, s.ffn, pixels * channels, channels,
                         epilogue=xmxres.EPI_E4M3, b_half=source_half)
    else:
        runtime.gemm(value16, w.expand, s.hidden16, pixels, s.hidden_width, channels,
                     epilogue=xmxres.EPI_GATE_E4M3, narrow=True)
        runtime.gemm(s.hidden16, w.branch, s.branch, pixels, channels, s.hidden_width)
        runtime.residual(s.branch, source, w.ffn_cos, s.ffn, pixels * channels, channels,
                         b_half=source_half)


def record_split_feed_forward(runtime, w, s, source, source_half=False):
    """The split family's core: e4m3(x @ first), then a per-64-group 64 -> 256 -> 64 MLP.

    The gate sits between the two group GEMMs with no publish, so the wide buffer is
    gated in one dense pass; the group outputs are published once, together.
    """
    pixels, channels, groups = s.height * s.width, w.channels, w.groups
    wide = groups * 256
    value16 = source if source_half else s.value16
    if not source_half:
        runtime.to_half(source, s.value16, pixels * channels)
    runtime.gemm(value16, w.first, s.heads16, pixels, channels, channels,
                 epilogue=xmxres.EPI_E4M3, narrow=True)
    with runtime.independent():
        for group in range(groups):
            runtime.gemm(s.heads16, w.expand, s.hidden16, pixels, 256, 64,
                         leading=(channels, 0, wide),
                         offsets=(group * 64, group * 64 * 256, group * 256),
                         epilogue=xmxres.EPI_GATE, narrow=True)
    with runtime.independent():
        for group in range(groups):
            runtime.gemm(s.hidden16, w.project, s.core16, pixels, 64, 256,
                         leading=(wide, 0, channels),
                         offsets=(group * 256, group * 256 * 64, group * 64),
                         epilogue=xmxres.EPI_E4M3, narrow=True)
    runtime.gemm(s.core16, w.weight3, s.branch, pixels, channels, channels)
    runtime.residual(s.branch, source, w.ffn_cos, s.ffn, pixels * channels, channels,
                     b_half=source_half)


def record_window_attention(runtime, w, s, source):
    """Window attention over `source`, into `s.attended` — in window order.

    The reverse back to image order is the following residual's own gather, so nothing
    is written in image order here. The window count follows this block's own origin,
    not the scratch's worst case.
    """
    channels, heads, tokens = w.channels, w.heads, s.tokens
    padded_height, padded_width, _ = runtime.window_extent(s.height, s.width, w.origin)
    windows = (padded_height // 8) * (padded_width // 8)
    batch = windows * heads
    windowed = windows * tokens * channels
    runtime.partition(source, s.win16, s.height, s.width, channels, origin=w.origin,
                      narrow=True)
    runtime.gemm(s.win16, w.qkv, s.proj, windows * tokens, 3 * channels, channels)
    record_qkv(runtime, w, s, windows, tokens, channels, heads)
    runtime.gemm(s.q16, s.k16, s.scores, tokens, tokens, 32, batch=batch,
                 strides=(tokens * 32, tokens * 32, tokens * tokens), transpose_b=True)
    runtime.softmax(s.scores, s.probs16, batch * tokens, tokens, narrow=True,
                    bias=w.bias, heads=heads)
    runtime.gemm(s.probs16, s.v16, s.context, tokens, 32, tokens, batch=batch,
                 strides=(tokens * tokens, tokens * 32, tokens * 32))
    runtime.merge_heads(s.context, s.merged16, windows, tokens, channels, heads,
                        epilogue=xmxres.EPI_E4M3, narrow=True)
    runtime.gemm(s.merged16, w.out, s.attended, windows * tokens, channels, channels)


def record_block(runtime, w, s, source=None, target=None, publish=0, source_half=False,
                 target_half=False):
    """A whole window block: feed-forward, attention, both residuals.

    `source` and `target` default to the scratch's own buffers; passing them lets one
    level's blocks chain into the next without a copy. `publish` is the epilogue the
    closing residual applies, which is how a block's output is published without a
    second pass over it.
    """
    source = source or s.value
    target = target or s.out
    pixels = s.height * s.width
    if getattr(w, "split", False):
        record_split_feed_forward(runtime, w, s, source, source_half)
    else:
        record_feed_forward(runtime, w, s, source, source_half)
    record_window_attention(runtime, w, s, s.ffn)
    # the window reverse is the residual's own gather, not a pass of its own
    runtime.residual(s.attended, s.ffn, w.attn_cos, target, pixels * w.channels,
                     w.channels, reverse=(s.height, s.width, 8, w.origin),
                     epilogue=publish, narrow=target_half)


def run_block(runtime, w, s, value):
    """Host convenience: one block, one submit, numpy in and numpy out."""
    xmxres.host_write(s.value, value)
    runtime.begin()
    record_block(runtime, w, s)
    passes = runtime.submit()
    return xmxres.host_view(s.out, shape=value.shape).copy(), passes


# --------------------------------------------------------------------------
# transitions between levels
# --------------------------------------------------------------------------


class Transition:
    """The weights a level change needs, uploaded once."""

    def __init__(self, runtime, weights, index, *, kind):
        self.kind = kind
        prefix = f"block{index}.layer0"
        if kind in ("down", "up"):
            self.weight0 = runtime.buffer_from(weights[f"{prefix}.weight0"], np.float16)
            self.out_channels = weights[f"{prefix}.weight0"].shape[1]
        if kind == "up":
            self.sine = runtime.buffer_from(weights[f"{prefix}.sin"])


def record_downsample(runtime, transition, scratch, source, target, height, width,
                      channels, *, pad_to=0, source_half=False, target_half=False):
    """Pool the block's unpublished output, publish it, then project.

    The fused `ds` kernels pool the half-precision output before its E4M3 publish and
    publish the pooled tensor again before the QMMA projection, so both are here.
    """
    if pad_to:
        padded_height = -(-height // pad_to) * pad_to
        padded_width = -(-width // pad_to) * pad_to
        runtime.pad_end(source, scratch.padded, height, width, padded_height,
                        padded_width, channels, a_half=source_half, narrow=source_half)
        source, height, width = scratch.padded, padded_height, padded_width
    half_height, half_width = height // 2, width // 2
    pixels = half_height * half_width
    runtime.pool2(source, scratch.pooled16, height, width, channels,
                  epilogue=xmxres.EPI_E4M3, narrow=True, a_half=source_half)
    runtime.gemm(scratch.pooled16, transition.weight0, target, pixels,
                 transition.out_channels, channels, epilogue=xmxres.EPI_E4M3,
                 narrow=target_half)
    return half_height, half_width


def record_upsample_merge(runtime, transition, scratch, source, skip, target,
                          source_height, source_width, height, width, channels,
                          out_channels, *, source_half=False, skip_half=False,
                          target_half=False):
    """Project, nearest-upsample onto the skip, add the scaled skip, publish.

    The fused `upsample` kernels read the merged tensor as E4M3, so the publish is
    part of the transition rather than of the block that follows.
    """
    source_pixels = source_height * source_width
    projected16 = source if source_half else scratch.projected16
    if not source_half:
        runtime.to_half(source, scratch.projected16, source_pixels * channels)
    runtime.gemm(projected16, transition.weight0, scratch.projected,
                 source_pixels, out_channels, channels)
    with runtime.independent():
        runtime.upsample2(scratch.projected, scratch.upsampled, source_width, height,
                          width, out_channels)
        runtime.scale_channel(skip, transition.sine, scratch.scaled,
                              height * width * out_channels, out_channels,
                              a_half=skip_half)
    runtime.add(scratch.upsampled, scratch.scaled, target, height * width * out_channels,
                epilogue=xmxres.EPI_E4M3, narrow=target_half)


class TransitionScratch:
    """Buffers a transition needs, sized for the largest level that uses it."""

    def __init__(self, runtime, elements, half_elements, arena=None):
        make = (arena.buffer if arena is not None else
                lambda name, count, dtype=np.float32: runtime.buffer(count, dtype))
        # `padded` holds whichever width its source has, so it is sized for float32
        self.padded = make("transition.padded", elements)
        self.pooled16 = make("transition.pooled16", elements, np.float16)
        self.projected = make("transition.projected", elements)
        self.projected16 = make("transition.projected16", elements, np.float16)
        self.upsampled = make("transition.upsampled", elements)
        self.scaled = make("transition.scaled", elements)

    def free(self):
        for name in dir(self):
            value = getattr(self, name)
            if isinstance(value, xmxres.Buffer):
                value.free()


def record_plain_downsample(runtime, edge, scratch, source, target, height, width,
                            channels, *, pad_to=0, source_half=False, target_half=False):
    """`downsample()`: pool then project, with no publish between.

    Block 30's bridge into the bottleneck, which unlike the encoder's `ds` kernels
    does not republish the pooled tensor before the projection.
    """
    if pad_to:
        padded_height = -(-height // pad_to) * pad_to
        padded_width = -(-width // pad_to) * pad_to
        runtime.pad_end(source, scratch.padded, height, width, padded_height,
                        padded_width, channels, a_half=source_half, narrow=source_half)
        source, height, width = scratch.padded, padded_height, padded_width
    pixels = (height // 2) * (width // 2)
    runtime.pool2(source, scratch.pooled16, height, width, channels,
                  epilogue=xmxres.EPI_HALF, narrow=True, a_half=source_half)
    runtime.gemm(scratch.pooled16, edge.weight0, target, pixels, edge.out_channels,
                 channels, epilogue=xmxres.EPI_E4M3, narrow=target_half)
