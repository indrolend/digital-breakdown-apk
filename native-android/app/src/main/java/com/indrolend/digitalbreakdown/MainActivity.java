package com.indrolend.digitalbreakdown;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.Surface;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;

public final class MainActivity extends Activity {
    private GameView gameView;

    static {
        System.loadLibrary("digitalbreakdown");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        gameView = new GameView(this);
        ControlOverlayView controls = new ControlOverlayView(this, gameView);
        FrameLayout root = new FrameLayout(this);
        root.addView(gameView, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));
        root.addView(controls, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));
        setContentView(root);

        gameView.post(() -> requestSixtyHertz(gameView.getHolder().getSurface()));
    }

    private static void requestSixtyHertz(Surface surface) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && surface != null && surface.isValid()) {
            surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
        }
    }

    @Override
    protected void onPause() {
        if (gameView != null) gameView.pauseGameLoop();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (gameView != null) gameView.resumeGameLoop();
    }
}