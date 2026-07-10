package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.KeyEvent;
import android.view.MotionEvent;

public final class GameView extends GLSurfaceView {
    private final NativeRenderer renderer;

    private int viewWidth = 1;
    private int viewHeight = 1;
    private int activeLookPointerId = -1;
    private float lastLookX = 0.0f;
    private float lastLookY = 0.0f;

    private boolean jumpHeld = false;
    private boolean meleeHeld = false;
    private boolean shootHeld = false;
    private boolean cameraHeld = false;

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
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        viewWidth = Math.max(1, width);
        viewHeight = Math.max(1, height);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final int actionIndex = event.getActionIndex();
        final int pointerId = event.getPointerId(actionIndex);
        final float actionX = event.getX(actionIndex);
        final float actionY = event.getY(actionIndex);

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            if (isLookZone(actionX, actionY) && activeLookPointerId < 0) {
                activeLookPointerId = pointerId;
                lastLookX = actionX;
                lastLookY = actionY;
            }
        }

        float lookDx = 0.0f;
        float lookDy = 0.0f;
        if (action == MotionEvent.ACTION_MOVE && activeLookPointerId >= 0) {
            final int lookIndex = event.findPointerIndex(activeLookPointerId);
            if (lookIndex >= 0) {
                final float x = event.getX(lookIndex);
                final float y = event.getY(lookIndex);
                lookDx = x - lastLookX;
                lookDy = y - lastLookY;
                lastLookX = x;
                lastLookY = y;
            }
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_CANCEL) {
            if (pointerId == activeLookPointerId || action == MotionEvent.ACTION_CANCEL) {
                activeLookPointerId = -1;
            }
        }

        TouchControls controls = readTouchControls(event);
        controls.lookDx = lookDx;
        controls.lookDy = lookDy;

        final boolean jumpPressed = controls.jumpHeld && !jumpHeld;
        final boolean meleePressed = controls.meleeHeld && !meleeHeld;
        final boolean shootPressed = controls.shootHeld && !shootHeld;
        final boolean cameraPressed = controls.cameraHeld && !cameraHeld;

        jumpHeld = controls.jumpHeld;
        meleeHeld = controls.meleeHeld;
        shootHeld = controls.shootHeld;
        cameraHeld = controls.cameraHeld;

        if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP) {
            if (event.getPointerCount() <= 1) {
                jumpHeld = false;
                meleeHeld = false;
                shootHeld = false;
                cameraHeld = false;
            }
        }

        NativeBridge.onTouchControls(
            controls.moveX,
            controls.moveZ,
            controls.lookDx,
            controls.lookDy,
            controls.vacuumHeld,
            controls.sprintHeld,
            jumpPressed,
            meleePressed,
            shootPressed,
            cameraPressed
        );

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

    private TouchControls readTouchControls(MotionEvent event) {
        TouchControls controls = new TouchControls();
        final float minSide = Math.max(1.0f, Math.min(viewWidth, viewHeight));
        final float moveCx = minSide * 0.18f;
        final float moveCy = viewHeight - minSide * 0.20f;
        final float moveRadius = minSide * 0.18f;

        for (int i = 0; i < event.getPointerCount(); ++i) {
            final float x = event.getX(i);
            final float y = event.getY(i);

            if (x < viewWidth * 0.50f && y > viewHeight * 0.32f) {
                final float dx = clamp((x - moveCx) / moveRadius, -1.0f, 1.0f);
                final float dz = clamp((moveCy - y) / moveRadius, -1.0f, 1.0f);
                if ((dx * dx + dz * dz) > (controls.moveX * controls.moveX + controls.moveZ * controls.moveZ)) {
                    controls.moveX = dx;
                    controls.moveZ = dz;
                    controls.sprintHeld = Math.sqrt(dx * dx + dz * dz) > 0.82f;
                }
            }

            if (inJumpButton(x, y)) controls.jumpHeld = true;
            else if (inMeleeButton(x, y)) controls.meleeHeld = true;
            else if (inShootButton(x, y)) controls.shootHeld = true;
            else if (inCameraButton(x, y)) controls.cameraHeld = true;
            else if (inVacuumButton(x, y)) controls.vacuumHeld = true;
        }

        return controls;
    }

    private boolean isLookZone(float x, float y) {
        return x >= viewWidth * 0.45f
            && !inJumpButton(x, y)
            && !inMeleeButton(x, y)
            && !inShootButton(x, y)
            && !inCameraButton(x, y)
            && !inVacuumButton(x, y);
    }

    private boolean inJumpButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 1.45f, viewHeight - r * 1.45f, r);
    }

    private boolean inMeleeButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 3.35f, viewHeight - r * 1.45f, r);
    }

    private boolean inShootButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 1.45f, viewHeight - r * 3.35f, r);
    }

    private boolean inCameraButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 3.35f, viewHeight - r * 3.35f, r);
    }

    private boolean inVacuumButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 5.25f, viewHeight - r * 1.45f, r);
    }

    private float buttonRadius() {
        return Math.max(44.0f, Math.min(viewWidth, viewHeight) * 0.070f);
    }

    private static boolean inside(float x, float y, float cx, float cy, float r) {
        final float dx = x - cx;
        final float dy = y - cy;
        return dx * dx + dy * dy <= r * r;
    }

    private static float clamp(float v, float lo, float hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    private static final class TouchControls {
        float moveX = 0.0f;
        float moveZ = 0.0f;
        float lookDx = 0.0f;
        float lookDy = 0.0f;
        boolean vacuumHeld = false;
        boolean sprintHeld = false;
        boolean jumpHeld = false;
        boolean meleeHeld = false;
        boolean shootHeld = false;
        boolean cameraHeld = false;
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
