/* CPU image passes. Keep the NumPy operation order and every FP16 rounding point.
 * Built locally for the host CPU; no fast-math or fused multiply-add is allowed.
 * Allocation, shape validation and buffer lifetime belong to nr_image.py.
 *
 * Taken from the parallel ProjectsCodex tree, which wrote it and measured it
 * (`notes/phase57`): feature assembly 146 -> 24 ms, composition 53 -> 9, the two
 * resizes 60 -> 8. Extended here for the two passes this tree has and that one does
 * not — history in the feature channels, and the temporal composition with its floor.
 *
 * Every function is a transcription of the NumPy above it, not a reimplementation.
 * The outputs are required to be byte-identical, and `test_native_image.py` checks it.
 */
#include <stddef.h>
#include <stdint.h>

#include "nr_portable.h"

static float half(float value) { return nr_half_round(value); }

static float unit(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

void nr_decode8(const uint8_t *source, size_t pixels, int bgra, float *output)
{
    for (size_t p = 0; p < pixels; ++p) {
        output[p * 3] = (float)source[p * 4 + (bgra ? 2 : 0)] / 255.0f;
        output[p * 3 + 1] = (float)source[p * 4 + 1] / 255.0f;
        output[p * 3 + 2] = (float)source[p * 4 + (bgra ? 0 : 2)] / 255.0f;
    }
}

void nr_encode8(const float *image, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                 const uint8_t *raw, size_t height, size_t width, int bgra,
                 uint8_t *output)
{
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const float *rgb = image + (ptrdiff_t)y * sy + (ptrdiff_t)x * sx;
            size_t p = y * width + x;
            for (size_t c = 0; c < 3; ++c) {
                float value = rgb[(ptrdiff_t)c * sc];
                /* NumPy's byte cast maps NaN to zero; do not cast NaN in C. */
                value = value == value ? unit(value) : 0.0f;
                output[p * 4 + (bgra ? 2 - c : c)] = (uint8_t)(value * 255.0f + 0.5f);
            }
            output[p * 4 + 3] = raw[p * 4 + 3];
        }
    }
}

void nr_compose(const float *head, ptrdiff_t hy, ptrdiff_t hx, ptrdiff_t hc,
                const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                size_t height, size_t width, float intensity, float *output)
{
    /* Our intensity > 1 extrapolates; the vendor recipe clamps negative intensity.
     * Keep both subtract/add operations even at intensity 1: simplifying to the
     * prediction would remove an FP32 rounding and can change encoded pixels.
     */
    float blend = intensity > 1.0f ? intensity : unit(intensity);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const float *h = head + (ptrdiff_t)y * hy + (ptrdiff_t)x * hx;
            const float *rgb = colour + (ptrdiff_t)y * sy + (ptrdiff_t)x * sx;
            for (size_t c = 0; c < 3; ++c) {
                float source = rgb[(ptrdiff_t)c * sc];
                float predicted = unit(source + half(h[(ptrdiff_t)c * hc]) * 0.25f);
                output[(y * width + x) * 3 + c] = unit(source + blend * (predicted - source));
            }
        }
    }
}

/* `history` is this tree's addition: the previous output, at the same logical extent as
 * the colour and mirrored onto the network extent the same way, standing in channels 7-9
 * where the first-frame layout repeats the colour. Identity reprojection, because a
 * layer at vkQueuePresentKHR has no motion vectors (notes/phase54). NULL for a still
 * frame, which is then bit-identical to the vendor's own first-frame layout.
 */
void nr_features(const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                 const float *history, ptrdiff_t ty, ptrdiff_t tx, ptrdiff_t tc,
                 const int32_t *rows, const int32_t *columns,
                 size_t height, size_t width, const float *noise,
                 const float *controls, float *output)
{
    for (size_t y = 0; y < height; ++y) {
        const float *row = colour + rows[y] * sy;
        const float *old = history ? history + rows[y] * ty : 0;
        for (size_t x = 0; x < width; ++x) {
            size_t pixel = y * width + x;
            const float *rgb = row + columns[x] * sx;
            const float *was = old ? old + columns[x] * tx : 0;
            float *out = output + pixel * 16;
            for (size_t c = 0; c < 3; ++c) {
                float scaled = half(half(half(rgb[(ptrdiff_t)c * sc]) - 0.5f) * 0.125f);
                out[c] = noise[pixel * 3 + c];
                out[4 + c] = scaled;
                out[7 + c] = was
                    ? half(half(half(was[(ptrdiff_t)c * tc]) - 0.5f) * 0.125f)
                    : scaled;
            }
            out[3] = 1.0f;
            for (size_t c = 0; c < 5; ++c) out[10 + c] = controls[c];
            out[15] = 0.0f;
        }
    }
}

/* One axis at a time: the intermediate is deliberately rounded to FP32 before
 * the second axis. Coordinates/weights come from the unchanged NumPy formula.
 * Signed strides permit padded crops and reversed views without another copy.
 */
void nr_resize_axis(const float *source, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                    size_t height, size_t width, size_t channels, int axis,
                    const int32_t *low, const int32_t *high,
                    const float *weight, float *output)
{
    for (size_t y = 0; y < height; ++y) {
        if (axis == 0 && sx == (ptrdiff_t)channels && sc == 1) {
            const float *a = source + low[y] * sy;
            const float *b = source + high[y] * sy;
            float w = weight[y], other = 1.0f - w;
            for (size_t i = 0; i < width * channels; ++i)
                output[y * width * channels + i] = a[i] * other + b[i] * w;
        } else {
            for (size_t x = 0; x < width; ++x) {
                size_t index = axis == 0 ? y : x;
                const float *a = axis == 0 ? source + low[y] * sy + (ptrdiff_t)x * sx
                                          : source + (ptrdiff_t)y * sy + low[x] * sx;
                const float *b = axis == 0 ? source + high[y] * sy + (ptrdiff_t)x * sx
                                          : source + (ptrdiff_t)y * sy + high[x] * sx;
                float w = weight[index], other = 1.0f - w;
                for (size_t c = 0; c < channels; ++c)
                    output[(y * width + x) * channels + c] =
                        a[(ptrdiff_t)c * sc] * other + b[(ptrdiff_t)c * sc] * w;
            }
        }
    }
}


/* The temporal composition, which this tree has and the other does not.
 *
 * `gate` is the model's own history weight, already through its sigmoid in NumPy and
 * already multiplied by the confidence knob. It arrives rather than being computed here
 * because `expf` and NumPy's float32 exponential do not agree in the last bit, and the
 * contract for all of this is byte-identical output, not nearly.
 *
 * `previous` is the game's own frame from the present before, or NULL. Where it is
 * unchanged the history is right for that pixel by construction, so the gate gets a
 * floor: full at no change, gone by four levels of 255, never above `scale`
 * (notes/phase54). `mask` is the interface control mask's red channel, or NULL.
 */
void nr_compose_temporal(const float *head, ptrdiff_t hy, ptrdiff_t hx, ptrdiff_t hc,
                         const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                         const float *history, ptrdiff_t ry, ptrdiff_t rx, ptrdiff_t rc,
                         const float *previous, ptrdiff_t py, ptrdiff_t px, ptrdiff_t pc,
                         const float *gate, ptrdiff_t gy, ptrdiff_t gx,
                         const float *mask, ptrdiff_t my, ptrdiff_t mx,
                         size_t height, size_t width, float intensity,
                         float scale, float hold, float slope, float *output)
{
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const float *h = head + (ptrdiff_t)y * hy + (ptrdiff_t)x * hx;
            const float *rgb = colour + (ptrdiff_t)y * sy + (ptrdiff_t)x * sx;
            const float *was = history + (ptrdiff_t)y * ry + (ptrdiff_t)x * rx;
            float alpha = gate[(ptrdiff_t)y * gy + (ptrdiff_t)x * gx];
            if (previous) {
                const float *before = previous + (ptrdiff_t)y * py + (ptrdiff_t)x * px;
                float moved = 0.0f;
                for (size_t c = 0; c < 3; ++c) {
                    float step = rgb[(ptrdiff_t)c * sc] - before[(ptrdiff_t)c * pc];
                    if (step < 0.0f) step = -step;
                    if (step > moved) moved = step;
                }
                /* `moved * slope + hold`, clamped to [0, hold], and `slope` arrives
                 * already folded: NumPy multiplies by one constant and adds another,
                 * and `clip(1 - moved * 255 / ramp, 0, 1) * hold` is the same value by
                 * algebra and a different one in float32. */
                float floored = moved * slope + hold;
                if (floored < 0.0f) floored = 0.0f;
                if (floored > hold) floored = hold;
                floored *= scale;
                if (floored > alpha) alpha = floored;
            }
            /* No clamp on the blend: the NumPy this transcribes does not clamp it
             * either, and the vendor's clamp is the one thing our composition
             * deliberately drops so that intensity above 1 can extrapolate. */
            float blend = intensity;
            if (mask) blend *= mask[(ptrdiff_t)y * my + (ptrdiff_t)x * mx];
            for (size_t c = 0; c < 3; ++c) {
                float source = rgb[(ptrdiff_t)c * sc];
                float predicted = unit(source + half(h[(ptrdiff_t)c * hc]) * 0.25f);
                predicted += alpha * (was[(ptrdiff_t)c * rc] - predicted);
                output[(y * width + x) * 3 + c] = unit(source + blend * (predicted - source));
            }
        }
    }
}
