package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.KeyEvent;
import android.view.MotionEvent;

public final class GameView extends GLSurfaceView {
    private final NativeRenderer renderer;
    private float lastX = 0.0f;
    private float lastY = 0.0f;

    public GameView(Context context) {
        super(context);

        setEGLContextClientVersion(2);
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();

        renderer = new NativeRenderer();
        setRenderer(renderer);
        setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final float x = event.getX();
        final float y = event.getY();

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            lastX = x;
            lastY = y;
        } else if (action == MotionEvent.ACTION_MOVE) {
            lastX = x;
            lastY = y;
        }

        NativeBridge.onTouch(action, x, y, event.getPointerCount());
        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        NativeBridge.onKey(keyCode, true);
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        NativeBridge.onKey(keyCode, false);
        return true;
    }

    private static final class NativeRenderer implements GLSurfaceView.Renderer {
        @Override
        public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl, javax.microedition.khronos.egl.EGLConfig config) {
            NativeBridge.onSurfaceCreated();
        }

        @Override
        public void onSurfaceChanged(javax.microedition.khronos.opengles.GL10 gl, int width, int height) {
            NativeBridge.onSurfaceChanged(width, height);
        }

        @Override
        public void onDrawFrame(javax.microedition.khronos.opengles.GL10 gl) {
            NativeBridge.onDrawFrame();
        }
    }
}
