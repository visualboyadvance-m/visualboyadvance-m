#ifndef VBAM_WX_RETRO68_QUICKDRAW_BLIT_H_
#define VBAM_WX_RETRO68_QUICKDRAW_BLIT_H_

// The QuickDraw side of the QuickDraw renderer (classic Mac OS, Retro68): an
// offscreen 32-bit GWorld the frame is written into, and CopyBits() from it to
// a window port. Kept apart from the panel so that the Toolbox headers, whose
// Rect, Point, Ptr and the like collide with names elsewhere in the frontend,
// are only seen in quickdraw-blit.cpp.

#include <cstdint>

struct GrafPort;

namespace qdblit {

// An offscreen 32-bit GWorld, pixels xRGB: 0x00RRGGBB in a big-endian long.
class Surface {
public:
    Surface() = default;
    ~Surface();
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    // Makes the GWorld width x height, keeping it when it already is; false
    // when QuickDraw cannot make one (no 32-bit QuickDraw, or no memory).
    bool Resize(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    // Locks the pixels for writing and returns the first row; `row_stride` is
    // the distance between rows in pixels. nullptr when the pixels are gone
    // (purged), which they never should be as they are not purgeable.
    uint32_t* Lock(int* row_stride);
    void Unlock();

    // Copies the whole surface to `dst` (a window port), stretched to the
    // rectangle in port coordinates, through its current clipping region.
    // `dst` must be the current port (wxRetro68DCImpl::SelectPort()).
    void Blit(GrafPort* dst, int left, int top, int right, int bottom);

private:
    void* gworld_ = nullptr;  // GWorldPtr
    int width_ = 0;
    int height_ = 0;
};

}  // namespace qdblit

#endif  // VBAM_WX_RETRO68_QUICKDRAW_BLIT_H_
