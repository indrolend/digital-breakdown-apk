package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.SystemClock;
import android.view.Choreographer;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.util.SparseIntArray;

public final class GameView extends GLSurfaceView implements Choreographer.FrameCallback {
    private static final long FRAME_INTERVAL_NANOS = 1_000_000_000L / 60L;
    private static final long ACTION_HOLD_MILLIS = 180L;
    private static final float ACTION_LOOK_SCALE = 0.24f;

    private final NativeRenderer renderer;
    private int viewWidth = 1;
    private int viewHeight = 1;
<<<<<<< Updated upstream
=======
    private int activeLookPointerId = -1;
    private float lastLookX = 0.0f;
    private float lastLookY = 0.0f;
    private int activeMovePointerId = -1;
    private float moveAnchorX = 0.0f;
    private float moveAnchorY = 0.0f;
    private final SparseIntArray pointerRoles = new SparseIntArray();

    private static final int ROLE_NONE = 0;
    private static final int ROLE_MOVE = 1;
    private static final int ROLE_LOOK = 2;
    private static final int ROLE_JUMP = 3;
    private static final int ROLE_MELEE = 4;
    private static final int ROLE_SHOOT = 5;
    private static final int ROLE_CAMERA = 6;
    private static final int ROLE_VACUUM = 7;
>>>>>>> Stashed changes

    private int movePointerId = -1;
    private int actionPointerId = -1;
    private float moveX = 0.0f;
    private float moveZ = 0.0f;
    private boolean sprintHeld = false;
    private boolean vacuumHeld = false;
    private float actionStartX = 0.0f;
    private float actionLastX = 0.0f;
    private long actionDownMillis = 0L;
    private float actionTravel = 0.0f;

    private boolean frameLoopRunning = false;
    private long lastRenderedFrameNanos = 0L;

    public GameView(Context context) {
        super(context);
<<<<<<< Updated upstream
=======
        NativeBridge.initializeAudio(context);
        NativeBridge.initializeModels(context);

>>>>>>> Stashed changes
        setEGLContextClientVersion(2);
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();

        renderer = new NativeRenderer();
        setRenderer(renderer);
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        viewWidth = Math.max(1, width);
        viewHeight = Math.max(1, height);
    }

    void resumeGameLoop() {
        super.onResume();
        if (!frameLoopRunning) {
            frameLoopRunning = true;
            lastRenderedFrameNanos = 0L;
            Choreographer.getInstance().postFrameCallback(this);
        }
    }

    void pauseGameLoop() {
        frameLoopRunning = false;
        Choreographer.getInstance().removeFrameCallback(this);
        clearTouchState();
        super.onPause();
    }

    @Override
    public void doFrame(long frameTimeNanos) {
        if (!frameLoopRunning) return;
        if (lastRenderedFrameNanos == 0L || frameTimeNanos - lastRenderedFrameNanos >= FRAME_INTERVAL_NANOS) {
            lastRenderedFrameNanos = frameTimeNanos;
            requestRender();
        }
        Choreographer.getInstance().postFrameCallback(this);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return processTouchEvent(event);
    }

    boolean processTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final int actionIndex = event.getActionIndex();
        final int pointerId = event.getPointerId(actionIndex);
<<<<<<< Updated upstream
        final float x = event.getX(actionIndex);
        final float y = event.getY(actionIndex);

        boolean jumpPressed = false;
        boolean meleePressed = false;
        boolean shootPressed = false;
        boolean cameraPressed = false;
        float lookDx = 0.0f;

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            if (inJumpButton(x, y)) {
                jumpPressed = true;
            } else if (inShootButton(x, y)) {
                shootPressed = true;
            } else if (inCameraButton(x, y)) {
                cameraPressed = true;
            } else if (isMoveZone(x, y) && movePointerId < 0) {
                movePointerId = pointerId;
                updateMove(x, y);
            } else if (isActionZone(x, y) && actionPointerId < 0) {
                actionPointerId = pointerId;
                actionStartX = x;
                actionLastX = x;
                actionTravel = 0.0f;
                actionDownMillis = SystemClock.uptimeMillis();
                postDelayed(actionHoldRunnable, ACTION_HOLD_MILLIS);
=======
        final float actionX = event.getX(actionIndex);
        final float actionY = event.getY(actionIndex);
        NativeBridge.onTouch(action, actionX, actionY, event.getPointerCount());

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int role = chooseRole(actionX, actionY);
            pointerRoles.put(pointerId, role);
            if (role == ROLE_MOVE && activeMovePointerId < 0) {
                activeMovePointerId = pointerId;
                final float minSide = Math.max(1.0f, Math.min(viewWidth, viewHeight));
                moveAnchorX = minSide * 0.18f;
                moveAnchorY = viewHeight - minSide * 0.20f;
            } else if (role == ROLE_LOOK && activeLookPointerId < 0) {
                activeLookPointerId = pointerId;
                lastLookX = actionX;
                lastLookY = actionY;
>>>>>>> Stashed changes
            }
        }

        if (action == MotionEvent.ACTION_MOVE) {
            if (movePointerId >= 0) {
                final int moveIndex = event.findPointerIndex(movePointerId);
                if (moveIndex >= 0) updateMove(event.getX(moveIndex), event.getY(moveIndex));
            }
            if (actionPointerId >= 0) {
                final int actionMoveIndex = event.findPointerIndex(actionPointerId);
                if (actionMoveIndex >= 0) {
                    final float nextX = event.getX(actionMoveIndex);
                    final float delta = nextX - actionLastX;
                    actionTravel += Math.abs(delta);
                    actionLastX = nextX;
                    lookDx = clamp(delta * ACTION_LOOK_SCALE, -8.0f, 8.0f);
                    if (!vacuumHeld && SystemClock.uptimeMillis() - actionDownMillis >= ACTION_HOLD_MILLIS) {
                        vacuumHeld = true;
                    }
                }
            }
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            if (pointerId == movePointerId) {
                movePointerId = -1;
                moveX = 0.0f;
                moveZ = 0.0f;
                sprintHeld = false;
            }
            if (pointerId == actionPointerId) {
                removeCallbacks(actionHoldRunnable);
                final long heldMillis = SystemClock.uptimeMillis() - actionDownMillis;
                if (!vacuumHeld && heldMillis < ACTION_HOLD_MILLIS && actionTravel < buttonRadius() * 0.65f) {
                    meleePressed = true;
                }
                actionPointerId = -1;
                vacuumHeld = false;
            }
            if (pointerId == activeMovePointerId || action == MotionEvent.ACTION_CANCEL) {
                activeMovePointerId = -1;
            }
            if (action == MotionEvent.ACTION_CANCEL) pointerRoles.clear();
            else pointerRoles.delete(pointerId);
        }

        if (action == MotionEvent.ACTION_CANCEL) {
            clearTouchState();
        }

        sendControls(lookDx, jumpPressed, meleePressed, shootPressed, cameraPressed);
        return true;
    }

    private final Runnable actionHoldRunnable = () -> {
        if (actionPointerId >= 0 && !vacuumHeld) {
            vacuumHeld = true;
            sendControls(0.0f, false, false, false, false);
        }
    };

    private void updateMove(float x, float y) {
        final float radius = joystickRadius();
        float dx = (x - joystickCenterX()) / radius;
        float dz = (joystickCenterY() - y) / radius;
        final float length = (float)Math.sqrt(dx * dx + dz * dz);
        if (length > 1.0f) {
            dx /= length;
            dz /= length;
        }
        if (length < 0.10f) {
            dx = 0.0f;
            dz = 0.0f;
        }
        moveX = dx;
        moveZ = dz;
        sprintHeld = length > 0.86f;
    }

    private void sendControls(float lookDx, boolean jumpPressed, boolean meleePressed, boolean shootPressed, boolean cameraPressed) {
        NativeBridge.onTouchControls(
            moveX,
            moveZ,
            lookDx,
            0.0f,
            vacuumHeld,
            sprintHeld,
            jumpPressed,
            meleePressed,
            shootPressed,
            cameraPressed
        );
    }

    private void clearTouchState() {
        removeCallbacks(actionHoldRunnable);
        movePointerId = -1;
        actionPointerId = -1;
        moveX = 0.0f;
        moveZ = 0.0f;
        sprintHeld = false;
        vacuumHeld = false;
        sendControls(0.0f, false, false, false, false);
    }

    float joystickCenterX() { return Math.min(viewWidth, viewHeight) * 0.18f; }
    float joystickCenterY() { return viewHeight - Math.min(viewWidth, viewHeight) * 0.20f; }
    float joystickRadius() { return Math.min(viewWidth, viewHeight) * 0.145f; }
    float buttonRadius() { return Math.max(42.0f, Math.min(viewWidth, viewHeight) * 0.062f); }
    float getMoveX() { return moveX; }
    float getMoveZ() { return moveZ; }
    boolean isVacuumHeld() { return vacuumHeld; }

    float actionCenterX() { return viewWidth - buttonRadius() * 2.2f; }
    float actionCenterY() { return viewHeight - buttonRadius() * 1.55f; }
    float jumpCenterX() { return viewWidth - buttonRadius() * 1.35f; }
    float jumpCenterY() { return viewHeight - buttonRadius() * 3.35f; }
    float shootCenterX() { return viewWidth - buttonRadius() * 3.25f; }
    float shootCenterY() { return viewHeight - buttonRadius() * 3.35f; }
    float cameraCenterX() { return viewWidth - buttonRadius() * 5.15f; }
    float cameraCenterY() { return viewHeight - buttonRadius() * 3.35f; }

    private boolean isMoveZone(float x, float y) {
        return x < viewWidth * 0.46f && y > viewHeight * 0.35f;
    }

    private boolean isActionZone(float x, float y) {
        return x >= viewWidth * 0.46f
            && y > viewHeight * 0.28f
            && !inJumpButton(x, y)
            && !inShootButton(x, y)
            && !inCameraButton(x, y);
    }

    private boolean inJumpButton(float x, float y) {
        return inside(x, y, jumpCenterX(), jumpCenterY(), buttonRadius());
    }

    private boolean inShootButton(float x, float y) {
        return inside(x, y, shootCenterX(), shootCenterY(), buttonRadius());
    }

    private boolean inCameraButton(float x, float y) {
        return inside(x, y, cameraCenterX(), cameraCenterY(), buttonRadius());
    }

    private static boolean inside(float x, float y, float cx, float cy, float r) {
        final float dx = x - cx;
        final float dy = y - cy;
        return dx * dx + dy * dy <= r * r;
    }

    private static float clamp(float value, float min, float max) {
        return Math.max(min, Math.min(max, value));
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

<<<<<<< Updated upstream
=======
    private TouchControls readTouchControls(MotionEvent event) {
        TouchControls controls = new TouchControls();
        final float minSide = Math.max(1.0f, Math.min(viewWidth, viewHeight));
        final float moveRadius = minSide * 0.18f;

        for (int i = 0; i < event.getPointerCount(); ++i) {
            final float x = event.getX(i);
            final float y = event.getY(i);

            switch (pointerRoles.get(event.getPointerId(i), ROLE_NONE)) {
                case ROLE_MOVE:
                    if (event.getPointerId(i) == activeMovePointerId) {
                        float dx = (x - moveAnchorX) / moveRadius;
                        float dz = (moveAnchorY - y) / moveRadius;
                        final float magnitude = (float)Math.sqrt(dx * dx + dz * dz);
                        if (magnitude < 0.10f) { dx = 0.0f; dz = 0.0f; }
                        else if (magnitude > 1.0f) { dx /= magnitude; dz /= magnitude; }
                        controls.moveX = dx; controls.moveZ = dz;
                        controls.sprintHeld = magnitude > 0.82f;
                    }
                    break;
                case ROLE_JUMP: controls.jumpHeld = true; break;
                case ROLE_MELEE: controls.meleeHeld = true; break;
                case ROLE_SHOOT: controls.shootHeld = true; break;
                case ROLE_CAMERA: controls.cameraHeld = true; break;
                case ROLE_VACUUM: controls.vacuumHeld = true; break;
                default: break;
            }
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

    private int chooseRole(float x, float y) {
        if (inJumpButton(x, y)) return ROLE_JUMP;
        if (inMeleeButton(x, y)) return ROLE_MELEE;
        if (inShootButton(x, y)) return ROLE_SHOOT;
        if (inCameraButton(x, y)) return ROLE_CAMERA;
        if (inVacuumButton(x, y)) return ROLE_VACUUM;
        if (x < viewWidth * 0.48f && y > viewHeight * 0.28f && activeMovePointerId < 0) return ROLE_MOVE;
        if (isLookZone(x, y) && activeLookPointerId < 0) return ROLE_LOOK;
        return ROLE_NONE;
    }

    private boolean inJumpButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 1.40f, viewHeight - r * 3.70f, r);
    }

    private boolean inMeleeButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 3.70f, viewHeight - r * 1.40f, r);
    }

    private boolean inShootButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 3.70f, viewHeight - r * 3.70f, r);
    }

    private boolean inCameraButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 6.00f, viewHeight - r * 1.40f, r * 0.82f);
    }

    private boolean inVacuumButton(float x, float y) {
        final float r = buttonRadius();
        return inside(x, y, viewWidth - r * 1.40f, viewHeight - r * 1.40f, r * 1.12f);
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

>>>>>>> Stashed changes
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