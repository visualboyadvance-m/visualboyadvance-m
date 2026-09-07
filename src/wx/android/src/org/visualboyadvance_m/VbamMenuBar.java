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
// Immersive-sticky is used for the system bars: a swipe from the edge reveals
// them temporarily and Android re-hides them by itself, with no callback
// bookkeeping needed here.
public class VbamMenuBar {

    // Safe to call from any thread; the window work is posted to the UI thread.
    public static void setHidden(final Activity activity, final boolean hidden) {
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

    private static void apply(Activity activity, boolean hidden) {
        ActionBar bar = activity.getActionBar();
        if (bar != null) {
            if (hidden) {
                bar.hide();
            } else {
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
        if (hidden) {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        } else {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
        }
    }
}
