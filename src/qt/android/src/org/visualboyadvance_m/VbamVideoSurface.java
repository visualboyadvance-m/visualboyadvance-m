package org.visualboyadvance_m;

import android.app.Activity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// Hosts an android.view.SurfaceView inside the (non-SDL) QtActivity so the SDL
// and Vulkan renderers can render video into its Surface. Created/managed from
// native code (panel.cpp) via JNI. Placed as a media-overlay layer above the app
// window surface and sized to the emulator panel, so Qt content underneath it --
// including the wx on-screen controller -- is hidden; the renderer composites
// that overlay into the frame itself. Touches still reach the widgets below,
// because a SurfaceView is neither clickable nor focusable. All view work runs on
// the UI thread; native callers block on a latch for the Surface to be created.
public class VbamVideoSurface implements SurfaceHolder.Callback {

    private static VbamVideoSurface sInstance;

    // Last known size of the activity's content view, in physical pixels. This is
    // the area actually visible below the action bar, which Qt's window can
    // overhang; native code sizes the panel overlays against it. Written on the
    // UI thread whenever the overlay is laid out, read from the emulator thread.
    private static volatile int sContentWidth;
    private static volatile int sContentHeight;

    public static int contentWidthPx() {
        return sContentWidth;
    }

    public static int contentHeightPx() {
        return sContentHeight;
    }

    // True once a layout listener is watching the content view (UI thread only).
    private static boolean sWatchingContent;

    // Starts (once) tracking the activity content view's size, so the numbers
    // above are available even when no overlay SurfaceView exists. The overlay
    // path fills them in as a side effect of laying the overlay out, but the
    // in-tree GLES2 renderer draws inside Qt's own window and creates no
    // overlay, while still needing to know how much of that window is on screen.
    // Safe to call from any thread; the measurement is posted to the UI thread,
    // so the first call after startup may return before any value is available.
    public static void watchContentSize(final Activity activity) {
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                final View content = activity.findViewById(android.R.id.content);
                if (content == null) {
                    return;
                }
                measureContent(content);
                if (sWatchingContent) {
                    return;
                }
                sWatchingContent = true;
                content.getViewTreeObserver().addOnGlobalLayoutListener(
                        new ViewTreeObserver.OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                measureContent(content);
                            }
                        });
            }
        });
    }

    private static void measureContent(final View content) {
        final int w = content.getWidth();
        final int h = content.getHeight();
        if (w > 0 && h > 0) {
            sContentWidth = w;
            sContentHeight = h;
        }
    }

    private SurfaceView mView;
    private Surface mSurface;
    private CountDownLatch mLatch;

    // Create the overlay SurfaceView and return its Surface (or null on timeout).
    public static synchronized Surface create(final Activity activity,
                                              final int x, final int y,
                                              final int w, final int h) {
        if (sInstance != null) {
            return sInstance.mSurface;
        }
        final VbamVideoSurface self = new VbamVideoSurface();
        self.mLatch = new CountDownLatch(1);
        sInstance = self;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                SurfaceView sv = new SurfaceView(activity);
                sv.setZOrderMediaOverlay(true);
                sv.getHolder().addCallback(self);
                ViewGroup content = (ViewGroup) activity.findViewById(android.R.id.content);
                content.addView(sv, layoutParamsFor(content, x, y, w, h));
                self.mView = sv;
            }
        });

        try {
            self.mLatch.await(3, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            // fall through; mSurface may still be null
        }
        return self.mSurface;
    }

    // Reposition/resize the overlay to match the panel (UI thread).
    public static synchronized void setGeometry(final Activity activity,
                                                final int x, final int y,
                                                final int w, final int h) {
        final VbamVideoSurface self = sInstance;
        if (self == null || self.mView == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ViewGroup parent = (ViewGroup) self.mView.getParent();
                self.mView.setLayoutParams(layoutParamsFor(parent, x, y, w, h));
            }
        });
    }

    // Builds layout params for the overlay. Native passes the panel's rect in
    // physical pixels relative to the Qt drawing area, which is this parent's
    // origin. The size is clamped to what is left of the parent: Qt's window can
    // be taller than the content view below the action bar, and an overlay sized
    // from it would draw the picture partly off the bottom of the screen.
    private static FrameLayout.LayoutParams layoutParamsFor(final ViewGroup parent,
                                                            final int x, final int y,
                                                            final int w, final int h) {
        final int left = Math.max(0, x);
        final int top = Math.max(0, y);
        int width = Math.max(1, w);
        int height = Math.max(1, h);
        if (parent != null && parent.getWidth() > 0 && parent.getHeight() > 0) {
            sContentWidth = parent.getWidth();
            sContentHeight = parent.getHeight();
            width = Math.max(1, Math.min(width, parent.getWidth() - left));
            height = Math.max(1, Math.min(height, parent.getHeight() - top));
            android.util.Log.i("VBAM", "VbamVideoSurface layout " + width + "x" + height
                    + " at " + left + "," + top
                    + " parent " + parent.getWidth() + "x" + parent.getHeight());
        }
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(width, height);
        lp.leftMargin = left;
        lp.topMargin = top;
        return lp;
    }

    public static synchronized void destroy(final Activity activity) {
        final VbamVideoSurface self = sInstance;
        sInstance = null;
        if (self == null || self.mView == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ViewGroup parent = (ViewGroup) self.mView.getParent();
                if (parent != null) {
                    parent.removeView(self.mView);
                }
                self.mView = null;
                self.mSurface = null;
            }
        });
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mSurface = holder.getSurface();
        if (mLatch != null) {
            mLatch.countDown();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mSurface = holder.getSurface();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mSurface = null;
    }
}
