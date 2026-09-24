# The eight global blocks survive int8, and the bigger the frame the better

`notes/phase23-integer-weights.md` closed the integer question in 2026-09-09 and closed it
correctly, on two grounds. Cooperative-matrix config 4 is `sint8 x sint8 -> sint32`: both
operands integer, no mixed mode, so integer weights force integer activations. And the
published activations carry up to **180 000x** of dynamic range, where a per-tensor int8
grid rounds 21.5 % of the non-zero values on level `l1` away entirely.

Both facts are still true. What was never measured is that they are facts about the
**whole** graph, and the graph is not uniform.

## Why the bottleneck is a different proposition

`src/bench/int_activations.py`, re-run unchanged, prints a gradient rather than a verdict:

| level | int8 rounds to zero | relative error | against E4M3's 6.25 % step |
|---|---|---|---|
| l1 | 21.5 % | 31.95 % | five times worse |
| l2 | 4.2 % | 10.10 % | worse |
| l3 | 2.1 % | 5.96 % | level |
| l4 | 0.9 % | 3.15 % | twice as fine |
| l5 | **0.4 %** | **1.59 %** | **four times as fine** |

The graph is already built around a 6.25 % relative step, taken at roughly a hundred E4M3
publishes between input and output. At `l5` an int8 grid is *finer than that*. `phase23`
measured `l1` and stopped, which was the right thing to do for an all-or-nothing question
and the wrong thing to conclude about a part.

Blocks 31-38, the ViT-1D bottleneck, sit at that end. Counted from the logical shapes
rather than quoted: **100 663 296 of 145 755 123 parameters, 69.1 % of the model**. They are
also the small-M, large-matrix GEMMs where config 4's doubled K removes the most
instructions — 1.30x to 1.87x on those exact shapes, measured on this machine.

So the three things line up: the blocks that hold most of the weights are the blocks whose
activations tolerate int8 and the blocks where int8 pays best.

## What it costs, on real frames

`src/bench/int8_bottleneck.py`. Every 2-D weight of blocks 31-38 is rounded onto an int8
grid per output channel and back; the 1-D tensors — gates, `attn_scale`, the transition
scalars — are not GEMM operands and are untouched. Judged the way this project judges, as a
share of the effect the network exists to produce, not as a share of the head's sd:

| frame | extent | the effect | int8 damage | share | worst |
|---|---|---|---|---|---|
| Cyberpunk 2077 | 1920x1080 | 6.03 L | **0.29 L** | **4.8 %** | 14.3 L |
| Cyberpunk 2077 | 1280x720 | 6.48 L | 0.45 L | 6.9 % | 15.3 L |
| a test image | 256x512 | 5.32 L | 0.80 L | 15.0 % | 8.9 L |

Levels of 255. Each row carries a control: the same configuration rendered twice differs by
**0.000 L**, so the damage column is the quantiser and not the machine.

And the damage is not a haze over the picture. At 1080p:

| | int8 moves | the network itself moves |
|---|---|---|
| above 1 level | 11.2 % of pixels | 99.6 % |
| above 4 levels | **0.157 %** | 49.8 % |
| above 8 levels | **0.003 %** | 22.8 % |

The network changes half the frame by more than four levels. int8 changes one and a half
pixels in a thousand by that much, and 62 pixels out of two million by more than eight.

## The trend is the useful part

15.0 % at 256x512, 6.9 % at 720p, 4.8 % at 1080p. **The cost falls as the frame grows.**
The bottleneck's output is raised back through five upsamples and blended with skip
connections carrying the full-resolution detail; the more fine detail the skips carry, the
more they dilute an error made at 1/32 of each axis. A first reading of this taken at
256x512 gave 15 % and was quoted as the answer for a day. It was the answer for a frame
nobody renders.

## What this does not say

- **Weights only.** A real config-4 kernel quantises the activations too. That half was
  measured on the CPU reference at 320x320 and at 256x512, where it added 0.4 points
  (15.3 % -> 15.7 %) — near enough to free. It has **not** been measured at 1080p and
  cannot be until the kernel exists.
- **No speed was measured here.** The prize is inferred: GEMM is 216 ms of 488 at 720p, the
  bottleneck is part of it, and the integer path is 1.3-1.87x on those shapes. Call it
  under a tenth of the frame on this iGPU. `phase23`'s "it would buy nothing anyway" rested
  on traffic arithmetic from before FFN batching and is not re-checked here.
- **The discrete case is an expectation, not a result.** On a card with five times this
  machine's bandwidth the passes that `phase45` measured at the memory ceiling shrink and
  GEMM becomes a larger share of the frame, so a GEMM-only optimisation should pay more
  there. There is no discrete GPU here to show it.

## Two silent no-ops, and what caught them

Both attempts at this experiment first produced a perfect result from doing nothing. The
CPU run reported identical heads because `progress` was never passed to `forward`, so the
hook never learned which block it was in. The resident run reported zero damage because the
weight filter tested `dtype == float16` and `load_logical` returns float32.

In both cases the thing that caught it was a counter printed beside the result — *how many
matrices were quantised* — and in both cases the result without it was a confident 0.0 %.
`notes/HANDOFF.md` already carries the general form: a test that exercises the mechanism
around the thing under test proves nothing about it. The specific form is worth adding:
**an experiment that can do nothing must report how much it did.**
