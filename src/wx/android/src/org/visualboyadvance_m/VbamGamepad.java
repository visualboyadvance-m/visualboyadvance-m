package org.visualboyadvance_m;

import android.app.Activity;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

// Physical game controllers for the wxQt build.
//
// SDL's joystick backend needs the SDLActivity Java lifecycle, which does not
// exist under the QtActivity host, and Qt itself only forwards controller
// buttons as unknown key codes and drops joystick motion events outright. So
// the activity hands every event coming from a game controller to this class
// (see VbamActivity), and it is pushed over JNI into
// widgets::AndroidGamepad (src/wx/widgets/android-gamepad.cpp), which turns it
// into the same joystick UserInputEvents the SDL poller produces on desktop.
//
// Buttons and axes are reported with the SDL gamepad numbering
// (SDL_GamepadButton / SDL_GamepadAxis), so the built-in Joystick 1 bindings
// and any bindings made with a desktop build carry over:
//   buttons: 0 A(south) 1 B(east) 2 X(west) 3 Y(north) 4 Back 5 Guide 6 Start
//            7 LStick 8 RStick 9 L1 10 R1 11-14 D-pad up/down/left/right
//   axes:    0/1 left stick 2/3 right stick 4 L2 5 R2
// Android's KEYCODE_BUTTON_A is the south (bottom) face button, so it is
// SDL button 0, which the default bindings map to the GBA B button; the east
// face button is SDL button 1 = GBA A. That matches how a desktop SDL build
// maps the same controller.
//
// The native side registers the native methods and then calls install(); until
// then everything is passed back to Qt untouched, so a JNI mismatch can never
// take the activity down with an UnsatisfiedLinkError.
public final class VbamGamepad {

    // SDL_GamepadButton values.
    private static final int SDL_BUTTON_SOUTH = 0;
    private static final int SDL_BUTTON_EAST = 1;
    private static final int SDL_BUTTON_WEST = 2;
    private static final int SDL_BUTTON_NORTH = 3;
    private static final int SDL_BUTTON_BACK = 4;
    private static final int SDL_BUTTON_GUIDE = 5;
    private static final int SDL_BUTTON_START = 6;
    private static final int SDL_BUTTON_LEFT_STICK = 7;
    private static final int SDL_BUTTON_RIGHT_STICK = 8;
    private static final int SDL_BUTTON_LEFT_SHOULDER = 9;
    private static final int SDL_BUTTON_RIGHT_SHOULDER = 10;
    private static final int SDL_BUTTON_DPAD_UP = 11;
    private static final int SDL_BUTTON_DPAD_DOWN = 12;
    private static final int SDL_BUTTON_DPAD_LEFT = 13;
    private static final int SDL_BUTTON_DPAD_RIGHT = 14;
    // Buttons SDL has no gamepad name for (C, Z, the generic BUTTON_1..16)
    // are numbered from here, like SDL's own Android backend does for extras.
    private static final int SDL_BUTTON_MISC_BASE = 15;

    // SDL_GamepadAxis values.
    private static final int SDL_AXIS_LEFTX = 0;
    private static final int SDL_AXIS_LEFTY = 1;
    private static final int SDL_AXIS_RIGHTX = 2;
    private static final int SDL_AXIS_RIGHTY = 3;
    private static final int SDL_AXIS_LEFT_TRIGGER = 4;
    private static final int SDL_AXIS_RIGHT_TRIGGER = 5;

    // A hat axis (HAT_X / HAT_Y) counts as pressed past this deflection.
    private static final float HAT_THRESHOLD = 0.5f;

    private static volatile boolean sReady = false;
    private static volatile Activity sActivity = null;
    private static InputManager.InputDeviceListener sListener = null;

    private VbamGamepad() {}

    // --- Native callbacks (registered from widgets::AndroidGamepad) --------

    // A controller appeared (also called for the ones present at startup).
    private static native void nativeDeviceAdded(int deviceId, String name);

    // A controller went away; the native side releases anything it held.
    private static native void nativeDeviceRemoved(int deviceId);

    // A button (SDL gamepad numbering) went down or up.
    private static native void nativeButton(int deviceId, int button, boolean pressed);

    // An axis (SDL gamepad numbering) moved; value is -1..1 for sticks and
    // 0..1 for triggers.
    private static native void nativeAxis(int deviceId, int axis, float value);

    // --- Setup (called from native once the natives are registered) --------

    // Enables event forwarding and starts watching for controllers. Safe to
    // call from any thread; the InputManager listener has to be registered
    // from a thread with a Looper, so that part is posted to the UI thread.
    // Takes Qt's application context, which is the activity while one runs.
    public static void install(Object context) {
        sReady = true;
        if (!(context instanceof Activity)) {
            return;
        }
        final Activity activity = (Activity) context;
        sActivity = activity;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                installOnUiThread(activity);
            }
        });
    }

    private static void installOnUiThread(Activity activity) {
        InputManager manager =
                (InputManager) activity.getSystemService(Context.INPUT_SERVICE);
        if (manager == null) {
            return;
        }
        if (sListener == null) {
            sListener = new InputManager.InputDeviceListener() {
                @Override
                public void onInputDeviceAdded(int deviceId) {
                    announce(InputDevice.getDevice(deviceId));
                }

                @Override
                public void onInputDeviceRemoved(int deviceId) {
                    // The device object is already gone, so this cannot check
                    // whether it was a controller; the native side ignores ids
                    // it never saw.
                    if (sReady) {
                        nativeDeviceRemoved(deviceId);
                    }
                }

                @Override
                public void onInputDeviceChanged(int deviceId) {
                    // Nothing cached here depends on the device's capabilities.
                }
            };
            // A null handler means callbacks on this (the UI) thread's Looper.
            manager.registerInputDeviceListener(sListener, null);
        }

        for (int id : InputDevice.getDeviceIds()) {
            announce(InputDevice.getDevice(id));
        }
    }

    private static void announce(InputDevice device) {
        if (device != null && sReady && isGameController(device)) {
            nativeDeviceAdded(device.getId(), device.getName());
        }
    }

    // --- Event entry points (called from VbamActivity, UI thread) ----------

    // Returns true when the event came from a game controller and has been
    // consumed; false leaves it for Qt (keyboard, volume keys, BACK, ...).
    public static boolean onKeyEvent(KeyEvent event) {
        if (!sReady || event == null) {
            return false;
        }
        InputDevice device = event.getDevice();
        if (device == null || !isGameController(device)) {
            return false;
        }

        final int keyCode = event.getKeyCode();
        final int action = event.getAction();
        if (action != KeyEvent.ACTION_DOWN && action != KeyEvent.ACTION_UP) {
            return false;
        }
        final boolean pressed = action == KeyEvent.ACTION_DOWN;

        // Some pads report the analog triggers as L2/R2 buttons only.
        if (keyCode == KeyEvent.KEYCODE_BUTTON_L2) {
            if (event.getRepeatCount() == 0) {
                nativeAxis(device.getId(), SDL_AXIS_LEFT_TRIGGER, pressed ? 1.0f : 0.0f);
            }
            return true;
        }
        if (keyCode == KeyEvent.KEYCODE_BUTTON_R2) {
            if (event.getRepeatCount() == 0) {
                nativeAxis(device.getId(), SDL_AXIS_RIGHT_TRIGGER, pressed ? 1.0f : 0.0f);
            }
            return true;
        }

        final int button = sdlButtonForKeyCode(keyCode);
        if (button < 0) {
            // Not a controller button (a BACK key on a TV remote, say): let the
            // system have it.
            return false;
        }
        // Android auto-repeats held buttons like keyboard keys; the emulator
        // tracks held state itself, so only edges are forwarded.
        if (event.getRepeatCount() == 0) {
            nativeButton(device.getId(), button, pressed);
        }
        return true;
    }

    // Returns true when the event was joystick motion from a game controller
    // and has been consumed.
    public static boolean onGenericMotionEvent(MotionEvent event) {
        if (!sReady || event == null) {
            return false;
        }
        if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK
                || event.getAction() != MotionEvent.ACTION_MOVE) {
            return false;
        }
        InputDevice device = event.getDevice();
        if (device == null || !isGameController(device)) {
            return false;
        }
        final int id = device.getId();

        // Only the newest sample of a batched event matters: the native side
        // thresholds axes into pressed/released, so intermediate positions
        // would collapse into the same transitions anyway.
        sendAxis(event, device, MotionEvent.AXIS_X, SDL_AXIS_LEFTX);
        sendAxis(event, device, MotionEvent.AXIS_Y, SDL_AXIS_LEFTY);
        sendAxis(event, device, MotionEvent.AXIS_Z, SDL_AXIS_RIGHTX);
        sendAxis(event, device, MotionEvent.AXIS_RZ, SDL_AXIS_RIGHTY);

        // Triggers: LTRIGGER/RTRIGGER on most pads, BRAKE/GAS on the ones
        // using the automotive names. Take whichever the device exposes.
        if (hasAxis(device, MotionEvent.AXIS_LTRIGGER)) {
            sendAxis(event, device, MotionEvent.AXIS_LTRIGGER, SDL_AXIS_LEFT_TRIGGER);
        } else if (hasAxis(device, MotionEvent.AXIS_BRAKE)) {
            sendAxis(event, device, MotionEvent.AXIS_BRAKE, SDL_AXIS_LEFT_TRIGGER);
        }
        if (hasAxis(device, MotionEvent.AXIS_RTRIGGER)) {
            sendAxis(event, device, MotionEvent.AXIS_RTRIGGER, SDL_AXIS_RIGHT_TRIGGER);
        } else if (hasAxis(device, MotionEvent.AXIS_GAS)) {
            sendAxis(event, device, MotionEvent.AXIS_GAS, SDL_AXIS_RIGHT_TRIGGER);
        }

        // The D-pad is a pair of hat axes on most controllers. Report it as the
        // four SDL D-pad buttons, the way SDL's gamepad layer does; the native
        // side dedupes against pads that also send KEYCODE_DPAD_* keys.
        if (hasAxis(device, MotionEvent.AXIS_HAT_X)) {
            final float x = event.getAxisValue(MotionEvent.AXIS_HAT_X);
            nativeButton(id, SDL_BUTTON_DPAD_LEFT, x < -HAT_THRESHOLD);
            nativeButton(id, SDL_BUTTON_DPAD_RIGHT, x > HAT_THRESHOLD);
        }
        if (hasAxis(device, MotionEvent.AXIS_HAT_Y)) {
            final float y = event.getAxisValue(MotionEvent.AXIS_HAT_Y);
            nativeButton(id, SDL_BUTTON_DPAD_UP, y < -HAT_THRESHOLD);
            nativeButton(id, SDL_BUTTON_DPAD_DOWN, y > HAT_THRESHOLD);
        }
        return true;
    }

    // Whether the device has the axis under any source; some pads file their
    // hat or trigger ranges under SOURCE_GAMEPAD rather than SOURCE_JOYSTICK.
    private static boolean hasAxis(InputDevice device, int axis) {
        return device.getMotionRange(axis) != null;
    }

    private static void sendAxis(MotionEvent event, InputDevice device, int androidAxis,
            int sdlAxis) {
        InputDevice.MotionRange range = device.getMotionRange(androidAxis);
        if (range == null) {
            return;
        }
        float value = event.getAxisValue(androidAxis);
        // Values inside the device's flat (dead) zone are noise around center.
        if (Math.abs(value) < range.getFlat()) {
            value = 0.0f;
        }
        nativeAxis(device.getId(), sdlAxis, value);
    }

    // --- Rumble (called from native, wx thread) ----------------------------

    // Starts or stops a continuous vibration on the given controller. Pads
    // without a vibrator are silently ignored.
    public static void setRumble(int deviceId, boolean on) {
        InputDevice device = InputDevice.getDevice(deviceId);
        if (device == null) {
            return;
        }
        Vibrator vibrator = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager manager = device.getVibratorManager();
            if (manager != null) {
                vibrator = manager.getDefaultVibrator();
            }
        } else {
            vibrator = device.getVibrator();
        }
        if (vibrator == null || !vibrator.hasVibrator()) {
            return;
        }
        try {
            if (on) {
                // A one-second full-strength segment repeated from index 0
                // runs until cancel(), so the emulator's periodic rumble
                // refreshes only need to flip it on and off.
                vibrator.vibrate(VibrationEffect.createWaveform(
                        new long[] {1000}, new int[] {VibrationEffect.DEFAULT_AMPLITUDE}, 0));
            } else {
                vibrator.cancel();
            }
        } catch (RuntimeException e) {
            // Some vendor vibrator services throw on unsupported effects;
            // rumble is best-effort.
        }
    }

    // --- Helpers -----------------------------------------------------------

    // A game controller is anything exposing gamepad buttons or joystick axes.
    // Keyboards (including ones that also claim SOURCE_DPAD for their arrow
    // keys) and touch screens are left to Qt.
    public static boolean isGameController(InputDevice device) {
        if (device == null || device.isVirtual()) {
            return false;
        }
        final int sources = device.getSources();
        return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    // Maps an Android key code to an SDL gamepad button, or -1 when the key is
    // not a controller button.
    private static int sdlButtonForKeyCode(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A:
                return SDL_BUTTON_SOUTH;
            case KeyEvent.KEYCODE_BUTTON_B:
                return SDL_BUTTON_EAST;
            case KeyEvent.KEYCODE_BUTTON_X:
                return SDL_BUTTON_WEST;
            case KeyEvent.KEYCODE_BUTTON_Y:
                return SDL_BUTTON_NORTH;
            // Older controllers send BACK for their Select/View button and
            // MENU for Start; SDL's Android backend treats them the same way.
            // Only reached for events from a game controller device, so a TV
            // remote's or the navigation bar's BACK still goes to the system.
            case KeyEvent.KEYCODE_BUTTON_SELECT:
            case KeyEvent.KEYCODE_BACK:
                return SDL_BUTTON_BACK;
            case KeyEvent.KEYCODE_BUTTON_MODE:
                return SDL_BUTTON_GUIDE;
            case KeyEvent.KEYCODE_BUTTON_START:
            case KeyEvent.KEYCODE_MENU:
                return SDL_BUTTON_START;
            case KeyEvent.KEYCODE_BUTTON_THUMBL:
                return SDL_BUTTON_LEFT_STICK;
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
                return SDL_BUTTON_RIGHT_STICK;
            case KeyEvent.KEYCODE_BUTTON_L1:
                return SDL_BUTTON_LEFT_SHOULDER;
            case KeyEvent.KEYCODE_BUTTON_R1:
                return SDL_BUTTON_RIGHT_SHOULDER;
            case KeyEvent.KEYCODE_DPAD_UP:
                return SDL_BUTTON_DPAD_UP;
            case KeyEvent.KEYCODE_DPAD_DOWN:
                return SDL_BUTTON_DPAD_DOWN;
            case KeyEvent.KEYCODE_DPAD_LEFT:
                return SDL_BUTTON_DPAD_LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                return SDL_BUTTON_DPAD_RIGHT;
            case KeyEvent.KEYCODE_BUTTON_C:
                return SDL_BUTTON_MISC_BASE;
            case KeyEvent.KEYCODE_BUTTON_Z:
                return SDL_BUTTON_MISC_BASE + 1;
            default:
                break;
        }
        if (keyCode >= KeyEvent.KEYCODE_BUTTON_1 && keyCode <= KeyEvent.KEYCODE_BUTTON_16) {
            return SDL_BUTTON_MISC_BASE + 2 + (keyCode - KeyEvent.KEYCODE_BUTTON_1);
        }
        return -1;
    }
}
