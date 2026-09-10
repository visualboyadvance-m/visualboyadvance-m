package org.visualboyadvance_m;

import android.app.ActionBar;
import android.app.Activity;
import android.view.View;

// Hides or shows the activity's top bar -- the action bar holding the overflow
// (three-dot) menu Qt maps the wx menu bar into -- together with the system
// bars, so the game takes the whole screen. Driven from native code via JNI
// (VbamSetAndroidMenuBarHidden) when the user toggles the Hide Menu Bar
// option; the on-screen controller's popup menu keeps a check item for it, so
// the bar can always be brought back while it is hidden.
//
// Qt does not know about this option and shows the action bar again on its
// own: QtActivityBase.onPrepareOptionsMenu() calls setActionBarVisibility(true)
// whenever the options menu is rebuilt, which Qt triggers (invalidateOptionsMenu
// via resetOptionsMenu) each time a menu item changes -- and loading a ROM
// enables dozens of them. The native side caches the last state it asked for,
// so it cannot notice. The requested state is therefore remembered here and
// re-applied by VbamActivity right after Qt's hooks run; see reapply().
//
// Immersive-sticky is used for the system bars: a swipe from the edge reveals
// them temporarily and Android re-hides them by itself, with no callback
// bookkeeping needed here.
public class VbamMenuBar {

    // The state last requested through setHidden(), or null before the first
    // request (then Qt's own behavior is left alone).
    private static volatile Boolean sHidden = null;

    // Safe to call from any thread; the window work is posted to the UI thread.
    public static void setHidden(final Activity activity, final boolean hidden) {
        sHidden = hidden;
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                apply(activity, hidden);
            }
        });
    }

    // Re-asserts the last requested state; a no-op until setHidden() has been
    // called. UI thread only (called from the activity's own callbacks).
    static void reapply(Activity activity) {
        Boolean hidden = sHidden;
        if (hidden == null || activity == null) {
            return;
        }
        apply(activity, hidden);
    }

    private static void apply(Activity activity, boolean hidden) {
        ActionBar bar = activity.getActionBar();
        if (bar != null) {
            // Only flip when needed: hide()/show() restart the bar's slide
            // animation even when it is already in the requested state.
            if (hidden && bar.isShowing()) {
                bar.hide();
            } else if (!hidden && !bar.isShowing()) {
                bar.show();
            }
        }

        if (activity.getWindow() == null) {
            return;
        }
        View decor = activity.getWindow().getDecorView();
        if (decor == null) {
            return;
        }
        final int flags = hidden
                ? (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION)
                : View.SYSTEM_UI_FLAG_VISIBLE;
        if (decor.getSystemUiVisibility() != flags) {
            decor.setSystemUiVisibility(flags);
        }
    }
}
