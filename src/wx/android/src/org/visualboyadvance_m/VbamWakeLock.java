package org.visualboyadvance_m;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.Window;
import android.view.WindowManager;

// Keeps the device awake while a game is running, driven from native code
// (GameArea::SuspendScreenSaver / UnsuspendScreenSaver) via JNI.
//
// Two mechanisms, because they cover different things:
//
//  * FLAG_KEEP_SCREEN_ON on the activity window stops the display from dimming
//    and locking. This is the only supported way to hold the screen on -- the
//    PowerManager screen wake locks were deprecated in API 17 and are ignored
//    on modern releases -- and it needs no permission. Android drops it by
//    itself whenever the window stops being visible, so backgrounding the app
//    can never leak it.
//
//  * A PARTIAL_WAKE_LOCK keeps the CPU running so the emulator thread and its
//    audio are not suspended if the screen does go off anyway (the user hits
//    the power button, or a device policy blanks the display). Requires the
//    WAKE_LOCK permission; if it is not granted we skip it and keep the window
//    flag. Unlike the window flag this one is ours to manage, so it is dropped
//    on activity pause and retaken on resume rather than held in the background.
//
// All window work is posted to the UI thread. Native callers may be on the
// emulator thread, so the acquire/release bookkeeping is synchronized.
public class VbamWakeLock {

    private static final String TAG = "VBAM:WakeLock";

    private static PowerManager.WakeLock sWakeLock;

    // Last state requested by native code, so repeat calls are cheap no-ops and
    // the activity lifecycle knows whether to retake the CPU lock on resume.
    private static boolean sEnabled;

    private static boolean sLifecycleRegistered;

    // Turns the wake lock on or off. Safe to call repeatedly with the same
    // value, from any thread, and with a null activity (does nothing).
    public static synchronized void setEnabled(final Activity activity, final boolean enable) {
        if (activity == null || enable == sEnabled) {
            return;
        }
        sEnabled = enable;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                final Window window = activity.getWindow();
                if (window == null) {
                    return;
                }
                if (enable) {
                    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                } else {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            }
        });

        if (enable) {
            registerLifecycle(activity);
            acquirePartial(activity);
        } else {
            releasePartial();
        }

        android.util.Log.i(TAG, "wake lock " + (enable ? "acquired" : "released")
                + " (cpu=" + (sWakeLock != null && sWakeLock.isHeld()) + ")");
    }

    // Releases everything, for activity teardown. A later setEnabled(true)
    // re-acquires from scratch.
    public static synchronized void release(final Activity activity) {
        setEnabled(activity, false);
    }

    // Drops the CPU lock while the app is not in the foreground, and retakes it
    // when the game comes back. Registered once, on the first acquire.
    private static void registerLifecycle(final Activity activity) {
        if (sLifecycleRegistered) {
            return;
        }
        final Application app = activity.getApplication();
        if (app == null) {
            return;
        }
        sLifecycleRegistered = true;
        app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            @Override
            public void onActivityPaused(Activity a) {
                synchronized (VbamWakeLock.class) {
                    releasePartial();
                }
            }

            @Override
            public void onActivityResumed(Activity a) {
                synchronized (VbamWakeLock.class) {
                    if (sEnabled) {
                        acquirePartial(a);
                    }
                }
            }

            @Override
            public void onActivityCreated(Activity a, Bundle b) {}

            @Override
            public void onActivityStarted(Activity a) {}

            @Override
            public void onActivityStopped(Activity a) {}

            @Override
            public void onActivitySaveInstanceState(Activity a, Bundle b) {}

            @Override
            public void onActivityDestroyed(Activity a) {}
        });
    }

    private static void acquirePartial(final Activity activity) {
        if (activity.checkCallingOrSelfPermission(android.Manifest.permission.WAKE_LOCK)
                != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        try {
            if (sWakeLock == null) {
                PowerManager pm = (PowerManager) activity.getSystemService(Context.POWER_SERVICE);
                if (pm == null) {
                    return;
                }
                sWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "vbam:emulation");
                sWakeLock.setReferenceCounted(false);
            }
            if (!sWakeLock.isHeld()) {
                sWakeLock.acquire();
            }
        } catch (Exception e) {
            // A denied or unavailable PowerManager must not take the game down;
            // the window flag above is what actually keeps the screen alive.
            android.util.Log.w(TAG, "cpu wake lock unavailable: " + e);
            sWakeLock = null;
        }
    }

    private static void releasePartial() {
        if (sWakeLock != null && sWakeLock.isHeld()) {
            try {
                sWakeLock.release();
            } catch (Exception e) {
                android.util.Log.w(TAG, "wake lock release failed: " + e);
            }
        }
    }
}
