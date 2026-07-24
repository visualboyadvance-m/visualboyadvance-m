package org.visualboyadvance_m;

import android.app.Activity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// Hosts an android.view.SurfaceView inside the (non-SDL) QtActivity so SDL can
// render video into its Surface. Created/managed from native code (panel.cpp)
// via JNI. Placed as a media-overlay layer above the app window surface, sized
// to the emulator panel; the wx on-screen controller keeps working over/around
// it. All view work runs on the UI thread; native callers block on a latch for
// the Surface to be created.
public class VbamVideoSurface implements SurfaceHolder.Callback {

    private static VbamVideoSurface sInstance;

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
                FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                        Math.max(1, w), Math.max(1, h));
                lp.leftMargin = x;
                lp.topMargin = y;
                ViewGroup content = (ViewGroup) activity.findViewById(android.R.id.content);
                content.addView(sv, lp);
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
                FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                        Math.max(1, w), Math.max(1, h));
                lp.leftMargin = x;
                lp.topMargin = y;
                self.mView.setLayoutParams(lp);
            }
        });
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
