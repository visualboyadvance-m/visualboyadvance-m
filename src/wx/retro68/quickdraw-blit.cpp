#include "wx/retro68/quickdraw-blit.h"

#include <Quickdraw.h>

namespace qdblit {

namespace {

GWorldPtr AsGWorld(void* p) {
    return static_cast<GWorldPtr>(p);
}

}  // namespace

Surface::~Surface() {
    if (gworld_)
        DisposeGWorld(AsGWorld(gworld_));
}

bool Surface::Resize(int width, int height) {
    if (gworld_ && width == width_ && height == height_)
        return true;

    if (gworld_) {
        DisposeGWorld(AsGWorld(gworld_));
        gworld_ = nullptr;
        width_ = height_ = 0;
    }

    if (width <= 0 || height <= 0)
        return false;

    Rect bounds;
    SetRect(&bounds, 0, 0, width, height);

    // 32 bits, so that every frame format converts to it without loss and
    // CopyBits() does any reduction to the screen's depth. In the application
    // heap: the frame is drawn every tick, temporary memory would only add
    // the risk of it being taken back.
    GWorldPtr gw = nullptr;
    if (NewGWorld(&gw, 32, &bounds, nullptr, nullptr, 0) != noErr || !gw)
        return false;

    NoPurgePixels(GetGWorldPixMap(gw));
    gworld_ = gw;
    width_ = width;
    height_ = height;
    return true;
}

uint32_t* Surface::Lock(int* row_stride) {
    if (!gworld_)
        return nullptr;

    PixMapHandle pm = GetGWorldPixMap(AsGWorld(gworld_));
    if (!LockPixels(pm))
        return nullptr;

    // The high bits of rowBytes are flags (a PixMap, not a BitMap).
    *row_stride = ((**pm).rowBytes & 0x3FFF) / 4;
    return reinterpret_cast<uint32_t*>(GetPixBaseAddr(pm));
}

void Surface::Unlock() {
    if (gworld_)
        UnlockPixels(GetGWorldPixMap(AsGWorld(gworld_)));
}

void Surface::Blit(GrafPort* dst, int left, int top, int right, int bottom) {
    if (!gworld_ || !dst)
        return;

    PixMapHandle pm = GetGWorldPixMap(AsGWorld(gworld_));
    if (!LockPixels(pm))
        return;

    Rect src_rect;
    SetRect(&src_rect, 0, 0, width_, height_);
    Rect dst_rect;
    SetRect(&dst_rect, left, top, right, bottom);

    // Black on white, or CopyBits() colourizes the copy.
    ForeColor(blackColor);
    BackColor(whiteColor);

    // The portBits of a colour port (a GWorld or a colour window) stand for
    // its PixMap, which is how CopyBits() expects to be given them; both port
    // records are nonrelocatable, so nothing moves under the pointers.
    CopyBits(&reinterpret_cast<GrafPtr>(AsGWorld(gworld_))->portBits,
             &reinterpret_cast<GrafPtr>(dst)->portBits,
             &src_rect, &dst_rect, srcCopy, nullptr);

    UnlockPixels(pm);
}

}  // namespace qdblit
