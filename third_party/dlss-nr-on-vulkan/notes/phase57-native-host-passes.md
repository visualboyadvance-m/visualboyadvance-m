# Phase 57 — the host passes in C, taken from the other tree

2026-09-12. `ProjectsCodex` built what this tree's own notes had been calling the live
lever for two days: the full-frame passes *around* the network, written in C instead of
NumPy. Their phase36. This is that library, imported, extended for the two passes this
tree has and that one does not, and measured here.

**Host passes 74 ms -> 28 ms per frame, byte-identical output.** The frame goes 259 -> 214
ms at 1024x768 / scale 0.55, because the graph is 185 ms of it and unchanged.

## What was taken, and what was added

`src/ref/nr_image.c` is theirs: feature assembly with the mirror onto the network extent,
separable bilinear resizing, still composition, and the 8-bit codecs — each a
transcription of the NumPy beside it, with every FP16 rounding point kept,
`-ffp-contract=off -fno-fast-math` so no multiply and add fuse, and signed strides so a
reversed view or a padded crop needs no copy first. That last detail is exactly what this
tree hands it: the decode is a reversed slice of the wire buffer and the letterbox crop is
a padded one.

Two functions are added here:

- **`nr_features` takes a history.** Channels 7-9 carry the previous output (`phase54`),
  and folding it into the same pass means the mirror, the three FP16 roundings of
  `scaled_color` and the history all happen while a pixel is in a register. In NumPy it
  was `make_features` and then `apply_history`, a full-frame gather each.
- **`nr_compose_temporal`** is the composition with the history, the floor under the gate
  and the interface control mask.

**The gate stays in NumPy.** `expf` and NumPy's float32 exponential disagree in the last
bit, and the contract here is byte-identical output rather than nearly, so the sigmoid is
computed in NumPy and the result handed to C. Same reason the floor's folded constant is
passed in rather than recomputed: NumPy evaluates `moved * slope + hold`, and
`clip(1 - moved * 255 / ramp, 0, 1) * hold` is the same value by algebra and a different
one in float32.

## Measured

Four consecutive DoA5 frames, 1024x768 with a 1024x576 active region, scale 0.55, on
**power-saver with the cores at 1.2 GHz** — the absolute numbers are not comparable with
earlier sessions, the pair is.

| pass | NumPy | native | |
| --- | ---: | ---: | ---: |
| feature assembly | 14.5 | **4.4** | 3.3x |
| composition | 12.6 | **7.0** | 1.8x |
| head upscale | 9.0 | **1.9** | 4.7x |
| downscale | 4.3 | **1.3** | 3.3x |
| encode | 4.3 | **2.1** | 2.0x |
| decode | 2.9 | **1.0** | 2.9x |
| letterbox scan | 21.3 | **0.2** | cached, below |
| **host total** | **68.9** | **17.9** | **3.8x** |
| the graph | 184.6 | 184.6 | untouched |

`src/ref/test_native_image.py` runs both implementations over reversed views, padded
crops, a mirrored network extent and values sitting on an FP16 boundary, and requires
`array_equal`. Through the socket, the replayed sequence returns the same flicker
statistics to the last digit — 0.87 levels over byte-identical pixels either way.

## Their 40 % and our 17 %

Their phase36 reports 354 -> 213 ms at 720p / scale .55. Ours moves less because **our
NumPy was already much faster**: their feature assembly went 146 -> 24 ms, ours went
14.5 -> 4.4. Most of their headline was recovering ground `phase47`, `phase48` and
`phase51` had already covered here — the separable resample, the memoised noise, the
in-place codec. What is genuinely new is the remaining factor of three, and it is real.

This is worth stating plainly because the obvious reading of their table is that this tree
was leaving 40 % on the floor. It was leaving 20 %.

## The pass that was left, and then was the largest

With everything else native, **`active_region` was the most expensive host pass in the
frame**: 21 ms, more than the feature assembly, the composition and both resizes together.
It reduces the whole frame twice looking for black bars — and a letterbox does not move,
because the game chose it when it opened the swapchain.

`Letterbox` finds them once and afterwards checks **eight lines**: each bar still black,
each first active line still not. A bar that stopped being black, or an active line that
started being black, runs the full scan again. Either mistake is a slower frame, never a
wrong crop. 21.3 -> 0.2 ms in the steady state.

Writing the test for it found a real hole: `_holds` trusted the caller's key and never
checked that the remembered bounds fit the frame in front of it. A mismatched key would
have read other rows and cropped to them.

## What was not taken

- **Their present-semaphore fix** (`phase35`) does not apply. They consumed the
  application's binary present waits for their capture and then forwarded presentation
  with none, which stalls. This layer never consumes them: it submits its transfer with no
  wait semaphores, waits on the queue, and forwards `VkPresentInfoKHR` untouched. Checked
  in `src/layer/nr_layer.c`. The remaining question here is a different one — a capture on
  the present queue is ordered against earlier submissions on *that* queue by the barrier
  in our own command buffer, but not against a separate graphics queue. DXVK presents on
  the graphics queue, which is why this has never shown.
- **Paired cosine rounding** (`their phase37`) was measured and rejected: 514 -> 518 ms.
- Their review is worth reading on our own claims. On `hold` they are right that the ramp
  blends some history for small nonzero changes rather than only for identical pixels;
  that is deliberate and is what the four-level ramp means, but the note should not be
  read as "only byte-identical pixels are held".
