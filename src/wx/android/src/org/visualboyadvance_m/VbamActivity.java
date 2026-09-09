package org.visualboyadvance_m;

import android.view.KeyEvent;
import android.view.Menu;
import android.view.MotionEvent;

import org.qtproject.qt.android.bindings.QtActivity;

// The application's activity: Qt's stock QtActivity plus the hooks needed to
// keep the Hide Menu Bar option in force, and the entry points for physical
// game controllers.
//
// Menu bar: Qt re-shows the action bar from its onPrepareOptionsMenu()
// whenever the options menu is rebuilt (which it does each time a wx menu item
// is enabled, checked, ...), and Android may restore the system bars when the
// window regains focus after a dialog or the file picker. Both hooks run here
// right after Qt's, so the bar is put back the way the user asked before the
// frame is drawn. VbamMenuBar remembers the state.
//
// Controllers: key and joystick-motion events are dispatched to the activity
// before any view sees them, so this is the one place every controller event
// passes through. VbamGamepad takes the ones from game controllers and hands
// them to the emulator over JNI; everything else goes on to Qt as usual.
public class VbamActivity extends QtActivity {

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        boolean result = super.onPrepareOptionsMenu(menu);
        VbamMenuBar.reapply(this);
        return result;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            VbamMenuBar.reapply(this);
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (VbamGamepad.onKeyEvent(event)) {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (VbamGamepad.onGenericMotionEvent(event)) {
            return true;
        }
        return super.dispatchGenericMotionEvent(event);
    }
}
