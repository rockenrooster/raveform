package dev.raveform.game;

import android.app.Activity;
import android.content.Context;
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
