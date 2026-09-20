/* The CPU image passes in nr_image.c, for callers in C. nr_image.py binds the same
 * functions for NumPy; the contract there — byte-identical to the NumPy they replace —
 * is the contract here. */
#ifndef NR_IMAGE_H
#define NR_IMAGE_H
#include <stddef.h>
#include <stdint.h>

void nr_decode8(const uint8_t *source, size_t pixels, int bgra, float *output);
void nr_encode8(const float *image, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                const uint8_t *raw, size_t height, size_t width, int bgra, uint8_t *output);
void nr_compose(const float *head, ptrdiff_t hy, ptrdiff_t hx, ptrdiff_t hc,
                const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                size_t height, size_t width, float intensity, float *output);
void nr_features(const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                 const float *history, ptrdiff_t ty, ptrdiff_t tx, ptrdiff_t tc,
                 const int32_t *rows, const int32_t *columns,
                 size_t height, size_t width, const float *noise,
                 const float *controls, float *output);
void nr_resize_axis(const float *source, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                    size_t height, size_t width, size_t channels, int axis,
                    const int32_t *low, const int32_t *high,
                    const float *weight, float *output);
void nr_compose_temporal(const float *head, ptrdiff_t hy, ptrdiff_t hx, ptrdiff_t hc,
                         const float *colour, ptrdiff_t sy, ptrdiff_t sx, ptrdiff_t sc,
                         const float *history, ptrdiff_t ry, ptrdiff_t rx, ptrdiff_t rc,
                         const float *previous, ptrdiff_t py, ptrdiff_t px, ptrdiff_t pc,
                         const float *gate, ptrdiff_t gy, ptrdiff_t gx,
                         const float *mask, ptrdiff_t my, ptrdiff_t mx,
                         size_t height, size_t width, float intensity,
                         float scale, float hold, float slope, float *output);
#endif
