package dev.raveform.game;

import android.app.Activity;
import android.content.Context;
// M2 audio begin
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.view.HapticFeedbackConstants;
// M2 audio end
import android.media.AudioAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/**
 * RAVEFORM's activity. SDL runs the native SDL_main() from libmain.so on its own thread; this class
 * only adds what SDLActivity does not do: intent extras as arguments, immersive mode, keep-screen-on,
 * a surface hand-shake with the render thread, and a clean process exit when the game quits.
 */
public class RaveformActivity extends SDLActivity {
    private static final String TAG = "RAVEFORM";

    /** adb shell am start -n dev.raveform.game/.RaveformActivity --es args '--scenario bench_idle --seconds 60' */
    @Override
    protected String[] getArguments() {
        String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        if (args == null) {
            return new String[0];
        }
        args = args.trim();
        return args.isEmpty() ? new String[0] : args.split("\\s+");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }

    @Override
    protected void onDestroy() {
        boolean finishing = isFinishing();
        super.onDestroy();
        // SDL cannot start a second SDL_main() in a process whose first one returned, so end the
        // process when the game quits; the next launch then starts clean.
        if (finishing) {
            System.exit(0);
        }
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        return new RaveformSurface(context);
    }

    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            // Without this the window keeps fitting inside the (hidden) system bars and the display
            // cutout, and the game renders below the panel's native size (measured on the Moto:
            // 2174x1020 instead of 2400x1080).
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    /** Current panel refresh rate. Called from native code at start-up, on resume and after benchmarks. */
    static float getDisplayRefreshRate() {
        Activity activity = mSingleton;
        if (activity == null) {
            return 0.0f;
        }
        Display display = Build.VERSION.SDK_INT >= 30 ? activity.getDisplay()
                                                      : activity.getWindowManager().getDefaultDisplay();
        return display != null ? display.getRefreshRate() : 0.0f;
    }

    // M2 audio begin
    /**
     * Beat haptics for the band (native code, main thread): one tick per call, posted to the UI thread.
     * kind: 0 light (kick), 1 medium, 2 heavy (Drop); 3-5 the same with the STRONG setting (M3 stage C).
     * SOFT and STRONG differ by amplitude where the vibrator has amplitude control, else by length. The
     * effects are made on the first tick and the runnables once, so a tick allocates nothing. Without a
     * vibrator it falls back to performHapticFeedback (one strength).
     */
    private static final Runnable[] sHaptics = new Runnable[6];
    private static final int[] HAPTIC_MS = {12, 18, 40};             // with amplitude control
    private static final int[] HAPTIC_SOFT = {60, 90, 130};           // amplitudes 1..255
    private static final int[] HAPTIC_STRONG = {170, 220, 255};
    private static final int[][] HAPTIC_MS_ONOFF = {{8, 20}, {12, 30}, {30, 70}};  // without: SOFT, STRONG
    private static final int[] HAPTIC_CONSTANTS = {HapticFeedbackConstants.CLOCK_TICK, HapticFeedbackConstants.KEYBOARD_TAP,
                                                   HapticFeedbackConstants.LONG_PRESS};
    private static VibrationEffect[] sEffects;
    private static Vibrator sVibrator;
    private static AudioAttributes sHapticAttributes;
    static {
        for (int i = 0; i < 6; ++i) {
            final int kind = i;
            sHaptics[i] = new Runnable() {
                @Override
                public void run() {
                    playHaptic(kind);
                }
            };
        }
    }

    private static void playHaptic(int kind) {
        if (sEffects == null) {
            Activity activity = mSingleton;
            if (activity == null) {
                return;
            }
            sEffects = new VibrationEffect[6];
            Vibrator vibrator = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
            if (vibrator != null && vibrator.hasVibrator()) {
                boolean amplitude = vibrator.hasAmplitudeControl();
                for (int i = 0; i < 3; ++i) {
                    sEffects[i] = amplitude ? VibrationEffect.createOneShot(HAPTIC_MS[i], HAPTIC_SOFT[i])
                                            : VibrationEffect.createOneShot(HAPTIC_MS_ONOFF[i][0], VibrationEffect.DEFAULT_AMPLITUDE);
                    sEffects[i + 3] = amplitude ? VibrationEffect.createOneShot(HAPTIC_MS[i], HAPTIC_STRONG[i])
                                                : VibrationEffect.createOneShot(HAPTIC_MS_ONOFF[i][1], VibrationEffect.DEFAULT_AMPLITUDE);
                }
                sHapticAttributes = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build();
                sVibrator = vibrator;
                Log.i(TAG, "haptics: vibrator, amplitude control " + (amplitude ? "yes (SOFT and STRONG by amplitude)" : "no (by length)"));
            } else {
                Log.i(TAG, "haptics: no vibrator, performHapticFeedback");
            }
        }
        if (sVibrator != null) {
            sVibrator.vibrate(sEffects[kind], sHapticAttributes);
            return;
        }
        View view = mSurface;
        if (view != null) {
            view.performHapticFeedback(HAPTIC_CONSTANTS[kind % 3]);
        }
    }

    static void haptic(int kind) {
        View view = mSurface;
        if (view != null && kind >= 0 && kind < sHaptics.length) {
            view.post(sHaptics[kind]);
        }
    }

    /** AudioDeviceInfo type of output device `deviceId` (0 when unknown). After a stream (re)opens only. */
    static int audioDeviceType(int deviceId) {
        Activity activity = mSingleton;
        if (activity == null) {
            return 0;
        }
        AudioManager manager = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);
        if (manager == null) {
            return 0;
        }
        for (AudioDeviceInfo device : manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            if (device.getId() == deviceId) {
                return device.getType();
            }
        }
        return 0;
    }
    // M2 audio end

    /** Blocks until the render thread has destroyed its Vulkan surface (or the timeout passes). */
    static native boolean nativeWaitSurfaceReleased(int timeoutMs);

    /**
     * SurfaceHolder.Callback contract: once surfaceDestroyed() returns, nobody may touch the surface.
     * SDL queues the pause and drops its window reference; we then wait for the render thread to tear
     * down its swapchain and VkSurfaceKHR before letting Android destroy the surface.
     */
    static class RaveformSurface extends SDLSurface {
        RaveformSurface(Context context) {
            super(context);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            super.surfaceDestroyed(holder);
            long start = System.nanoTime();
            boolean released = nativeWaitSurfaceReleased(2000);
            long ms = (System.nanoTime() - start) / 1000000;
            if (released) {
                Log.i(TAG, "surfaceDestroyed: render thread released the surface in " + ms + " ms");
            } else {
                Log.e(TAG, "surfaceDestroyed: render thread did not release the surface within " + ms + " ms");
            }
        }
    }
}
