package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.Surface;
import android.view.Choreographer;
import android.view.KeyEvent;
import android.view.MotionEvent;

public final class GameView extends GLSurfaceView implements Choreographer.FrameCallback {
    private static final long FRAME_INTERVAL_NANOS = 1_000_000_000L / 60L;
    private static final long ACTION_HOLD_MILLIS = 90L;
    private static final float ACTION_LOOK_SCALE = 0.24f;
    private static final float CONTROLLER_LOOK_DELTA_PER_FRAME = 12.0f;
    private float touchLookMultiplier = 1.0f;
    private float controllerLookMultiplier = 1.0f;
    private static final float TOUCH_MOVE_DEAD_ZONE = 0.10f;
    private static final float CONTROLLER_MOVE_DEAD_ZONE = 0.12f;
    private static final float CONTROLLER_LOOK_DEAD_ZONE = 0.10f;
    private static final float TRIGGER_PRESS_THRESHOLD = 0.28f;
    private final MainActivity activity;

    private final NativeRenderer renderer;
    private int viewWidth = 1;
    private int viewHeight = 1;
    private int surfaceBufferWidth = 0;
    private int surfaceBufferHeight = 0;

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
    private long menuInputBlockedUntil = 0L;

    // Controller values are stateful. They are merged with touch controls so a
    // Bluetooth pad can be connected or disconnected during a run without
    // leaving movement, vacuum, or sprint latched on.
    private float controllerMoveX = 0.0f;
    private float controllerMoveZ = 0.0f;
    private float controllerLookX = 0.0f;
    private float controllerLookY = 0.0f;
    private boolean controllerTriggerVacuumHeld = false;
    private boolean controllerBumperVacuumHeld = false;
    private boolean controllerLeftTriggerDown = false;
    private boolean controllerStickSprintHeld = false;
    private int controllerHatX = 0;
    private int controllerHatY = 0;
    private boolean controllerJumpPressed = false;
    private boolean controllerMeleePressed = false;
    private boolean controllerShootPressed = false;
    private boolean controllerCameraPressed = false;

    private boolean frameLoopRunning = false;
    private long nextRenderedFrameNanos = 0L;
    private long lastImmediateRenderMillis = 0L;

    public GameView(Context context) {
        super(context);
        activity = context instanceof MainActivity ? (MainActivity)context : null;
        NativeBridge.initializeAudio(context);
        NativeBridge.initializeProgression(context);
        NativeBridge.initializeModels(context);
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
        configureRenderBuffer();
    }

    private void configureRenderBuffer() {
        final int maxLong = 960;
        final int maxShort = 540;
        final boolean landscape = viewWidth >= viewHeight;
        final int longSide = Math.max(viewWidth, viewHeight);
        final int shortSide = Math.min(viewWidth, viewHeight);
        float scale = Math.min(1.0f, Math.min((float)maxLong / Math.max(1, longSide), (float)maxShort / Math.max(1, shortSide)));
        int bufferW = Math.max(1, Math.round(viewWidth * scale));
        int bufferH = Math.max(1, Math.round(viewHeight * scale));
        if (landscape && bufferW < bufferH) { final int tmp = bufferW; bufferW = bufferH; bufferH = tmp; }
        if (bufferW != surfaceBufferWidth || bufferH != surfaceBufferHeight) {
            surfaceBufferWidth = bufferW;
            surfaceBufferHeight = bufferH;
            getHolder().setFixedSize(bufferW, bufferH);
        }
    }

    void resumeGameLoop() {
        super.onResume();
        if (!frameLoopRunning) {
            frameLoopRunning = true;
            nextRenderedFrameNanos = 0L;
            Choreographer.getInstance().postFrameCallback(this);
        }
    }

    void pauseGameLoop() {
        frameLoopRunning = false;
        Choreographer.getInstance().removeFrameCallback(this);
        clearControllerState();
        clearTouchState();
        super.onPause();
    }

    @Override
    public void doFrame(long frameTimeNanos) {
        if (!frameLoopRunning) return;
        if (nextRenderedFrameNanos == 0L) nextRenderedFrameNanos = frameTimeNanos;
        if (frameTimeNanos >= nextRenderedFrameNanos) {
            sendControls(
                controllerLookX * CONTROLLER_LOOK_DELTA_PER_FRAME * controllerLookMultiplier,
                controllerLookY * CONTROLLER_LOOK_DELTA_PER_FRAME * controllerLookMultiplier,
                false, false, false, false
            );
            requestRender();
            do { nextRenderedFrameNanos += FRAME_INTERVAL_NANOS; }
            while (nextRenderedFrameNanos <= frameTimeNanos);
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
        final float x = event.getX(actionIndex);
        final float y = event.getY(actionIndex);

        if (!NativeBridge.isStarted()) {
            clearTouchState();
            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
                restartRun();
            }
            return true;
        }

        if (NativeBridge.isGrabbed()) {
            clearTouchState();
            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
                NativeBridge.onWiggle(0.0f);
                menuInputBlockedUntil = SystemClock.uptimeMillis() + 350L;
            }
            return true;
        }

        final int menuMode = NativeBridge.getMenuMode();
        if (menuMode == 2) {
            clearTouchState();
            if (action == MotionEvent.ACTION_DOWN) {
                if (activity != null) activity.resumeFromPauseMenu();
                else NativeBridge.setPaused(false);
            }
            return true;
        }

        if (menuMode == 1) {
            clearTouchState();
            if (action == MotionEvent.ACTION_DOWN && SystemClock.uptimeMillis() >= menuInputBlockedUntil) {
                final float panelWidth = Math.min(680.0f, viewWidth - 24.0f);
                final float panelX = (viewWidth - panelWidth) * 0.5f;
                final int track = Math.max(0, Math.min(2, (int)((x - panelX) / (panelWidth / 3.0f))));
                final float panelY = (viewHeight - 300.0f) * 0.5f;
                NativeBridge.chooseUpgrade(track, y >= panelY + 176.0f);
            }
            return true;
        }

        boolean jumpPressed = false;
        boolean meleePressed = false;
        boolean shootPressed = false;
        boolean cameraPressed = false;
        float lookDx = 0.0f;

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int commSignal = commSignalAt(x, y);
            if (inOptionsButton(x, y)) {
                clearTouchState();
                showPauseMenu();
                return true;
            } else if (commSignal > 0) {
                sendCommSignal(commSignal);
            } else if (inJumpButton(x, y)) {
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
                    lookDx = clamp(delta * ACTION_LOOK_SCALE * touchLookMultiplier, -8.0f, 8.0f);
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
        }

        if (action == MotionEvent.ACTION_CANCEL) {
            clearTouchState();
        }

        flushControlsNow(lookDx, 0.0f, jumpPressed, meleePressed, shootPressed, cameraPressed);
        return true;
    }

    private final Runnable actionHoldRunnable = () -> {
        if (actionPointerId >= 0 && !vacuumHeld) {
            vacuumHeld = true;
            flushControlsNow(0.0f, 0.0f, false, false, false, false);
        }
    };

    private void updateMove(float x, float y) {
        final float radius = joystickRadius();
        float dx = (x - joystickCenterX()) / radius;
        float dz = (joystickCenterY() - y) / radius;
        final float[] stick = radialStick(dx, dz, TOUCH_MOVE_DEAD_ZONE, false);
        moveX = stick[0];
        moveZ = stick[1];
        sprintHeld = stick[2] > 0.86f;
    }

    private void sendControls(float lookDx, float lookDy, boolean jumpPressed, boolean meleePressed, boolean shootPressed, boolean cameraPressed) {
        float combinedMoveX = moveX + controllerMoveX;
        float combinedMoveZ = moveZ + controllerMoveZ;
        final float length = (float)Math.sqrt(combinedMoveX * combinedMoveX + combinedMoveZ * combinedMoveZ);
        if (length > 1.0f) { combinedMoveX /= length; combinedMoveZ /= length; }
        final boolean nextJump = jumpPressed || controllerJumpPressed;
        final boolean nextMelee = meleePressed || controllerMeleePressed;
        final boolean nextShoot = shootPressed || controllerShootPressed;
        final boolean nextCamera = cameraPressed || controllerCameraPressed;
        controllerJumpPressed = controllerMeleePressed = controllerShootPressed = controllerCameraPressed = false;
        final float nativeMoveX = combinedMoveX;
        final float nativeMoveZ = combinedMoveZ;
        final float nativeLookDx = lookDx;
        final float nativeLookDy = lookDy;
        final boolean nativeVacuum = vacuumHeld || controllerTriggerVacuumHeld || controllerBumperVacuumHeld;
        final boolean nativeSprint = sprintHeld || controllerStickSprintHeld;
        queueEvent(() -> NativeBridge.onTouchControls(
            nativeMoveX,
            nativeMoveZ,
            nativeLookDx,
            nativeLookDy,
            nativeVacuum,
            nativeSprint,
            nextJump,
            nextMelee,
            nextShoot,
            nextCamera
        ));
    }

    private void flushControlsNow(float lookDx, float lookDy, boolean jumpPressed, boolean meleePressed, boolean shootPressed, boolean cameraPressed) {
        sendControls(lookDx, lookDy, jumpPressed, meleePressed, shootPressed, cameraPressed);
        requestLowLatencyFrame();
    }

    private void requestLowLatencyFrame() {
        if (!frameLoopRunning) return;
        final long now = SystemClock.uptimeMillis();
        if (now == lastImmediateRenderMillis) return;
        lastImmediateRenderMillis = now;
        requestRender();
    }

    private void clearTouchState() {
        removeCallbacks(actionHoldRunnable);
        movePointerId = -1;
        actionPointerId = -1;
        moveX = 0.0f;
        moveZ = 0.0f;
        sprintHeld = false;
        vacuumHeld = false;
        flushControlsNow(0.0f, 0.0f, false, false, false, false);
    }

    void setLookSensitivity(float touch, float controller) {
        touchLookMultiplier = clamp(touch, 0.5f, 1.75f);
        controllerLookMultiplier = clamp(controller, 0.5f, 1.75f);
    }

    private void clearControllerState() {
        controllerMoveX = 0.0f;
        controllerMoveZ = 0.0f;
        controllerLookX = 0.0f;
        controllerLookY = 0.0f;
        controllerTriggerVacuumHeld = false;
        controllerBumperVacuumHeld = false;
        controllerLeftTriggerDown = false;
        controllerStickSprintHeld = false;
        controllerHatX = 0;
        controllerHatY = 0;
        controllerJumpPressed = false;
        controllerMeleePressed = false;
        controllerShootPressed = false;
        controllerCameraPressed = false;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (event.getAction() != MotionEvent.ACTION_MOVE || !isController(event)) return super.onGenericMotionEvent(event);
        final float[] moveStick = radialStick(
            centeredAxis(event, MotionEvent.AXIS_X),
            -centeredAxis(event, MotionEvent.AXIS_Y),
            CONTROLLER_MOVE_DEAD_ZONE,
            false
        );
        controllerMoveX = moveStick[0];
        controllerMoveZ = moveStick[1];
        final float[] lookStick = radialStick(
            rawAxis(event, MotionEvent.AXIS_Z, MotionEvent.AXIS_RX),
            rawAxis(event, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RY),
            CONTROLLER_LOOK_DEAD_ZONE,
            true
        );
        controllerLookX = lookStick[0];
        controllerLookY = lookStick[1];
        controllerTriggerVacuumHeld = trigger(event, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS);
        final boolean nextLeftTriggerDown = trigger(event, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE);
        if (nextLeftTriggerDown && !controllerLeftTriggerDown) controllerMeleePressed = true;
        controllerLeftTriggerDown = nextLeftTriggerDown;
        final int nextHatX = hatDirection(event.getAxisValue(MotionEvent.AXIS_HAT_X));
        final int nextHatY = hatDirection(event.getAxisValue(MotionEvent.AXIS_HAT_Y));
        if (nextHatY < 0 && controllerHatY >= 0) sendCommSignal(1);
        else if (nextHatX > 0 && controllerHatX <= 0) sendCommSignal(2);
        else if (nextHatY > 0 && controllerHatY <= 0) sendCommSignal(3);
        else if (nextHatX < 0 && controllerHatX >= 0) sendCommSignal(4);
        controllerHatX = nextHatX;
        controllerHatY = nextHatY;
        flushControlsNow(0.0f, 0.0f, false, false, false, false);
        return true;
    }

    private boolean isController(MotionEvent event) {
        final int source = event.getSource();
        return (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
            || (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD;
    }

    private float rawAxis(MotionEvent event, int preferredAxis, int fallbackAxis) {
        float value = centeredAxis(event, preferredAxis);
        if (Math.abs(value) < CONTROLLER_LOOK_DEAD_ZONE) value = centeredAxis(event, fallbackAxis);
        return clamp(value, -1.0f, 1.0f);
    }

    private float centeredAxis(MotionEvent event, int axis) {
        final InputDevice device = event.getDevice();
        final InputDevice.MotionRange range = device == null ? null : device.getMotionRange(axis, event.getSource());
        final float value = event.getAxisValue(axis);
        return range != null && Math.abs(value) <= range.getFlat() ? 0.0f : value;
    }

    private boolean trigger(MotionEvent event, int preferredAxis, int fallbackAxis) {
        return Math.max(event.getAxisValue(preferredAxis), event.getAxisValue(fallbackAxis)) > TRIGGER_PRESS_THRESHOLD;
    }

    private float[] radialStick(float x, float y, float deadZone, boolean lookStick) {
        final float rawMagnitude = (float)Math.sqrt(x * x + y * y);
        if (rawMagnitude <= deadZone) return new float[]{0.0f, 0.0f, 0.0f};
        final float clampedMagnitude = Math.min(1.0f, rawMagnitude);
        float magnitude = (clampedMagnitude - deadZone) / (1.0f - deadZone);
        magnitude = clamp(magnitude, 0.0f, 1.0f);
        if (lookStick) {
            magnitude = (float)Math.pow(magnitude, 1.35f);
        }
        final float scale = magnitude / rawMagnitude;
        return new float[]{clamp(x * scale, -1.0f, 1.0f), clamp(y * scale, -1.0f, 1.0f), magnitude};
    }

    private int hatDirection(float value) {
        return value > 0.5f ? 1 : value < -0.5f ? -1 : 0;
    }

    float joystickCenterX() { return Math.min(viewWidth, viewHeight) * 0.17f; }
    float joystickCenterY() { return viewHeight - Math.min(viewWidth, viewHeight) * 0.18f; }
    float joystickRadius() { return Math.min(viewWidth, viewHeight) * 0.128f; }
    float buttonRadius() { return Math.max(40.0f, Math.min(60.0f, Math.min(viewWidth, viewHeight) * 0.055f)); }
    float getMoveX() { return moveX; }
    float getMoveZ() { return moveZ; }
    boolean isVacuumHeld() { return vacuumHeld; }
    boolean isMoveActive() { return movePointerId >= 0; }
    boolean isActionActive() { return actionPointerId >= 0 || vacuumHeld; }

    float actionCenterX() { return viewWidth - buttonRadius() * 1.62f; }
    float actionCenterY() { return viewHeight - buttonRadius() * 1.52f; }
    float jumpCenterX() { return viewWidth - buttonRadius() * 1.18f; }
    float jumpCenterY() { return viewHeight - buttonRadius() * 3.78f; }
    float shootCenterX() { return viewWidth - buttonRadius() * 3.62f; }
    float shootCenterY() { return viewHeight - buttonRadius() * 2.78f; }
    float cameraCenterX() { return viewWidth - buttonRadius() * 5.16f; }
    float cameraCenterY() { return viewHeight - buttonRadius() * 1.10f; }
    float optionsCenterX() { return viewWidth - buttonRadius() * 0.86f; }
    float optionsCenterY() { return buttonRadius() * 0.92f; }
    float optionsRadius() { return buttonRadius() * 0.46f; }
    float commCenterX(int signal) { return viewWidth * 0.5f + (signal - 2.5f) * buttonRadius() * 1.18f; }
    float commCenterY() { return buttonRadius() * 0.88f; }
    float commRadius() { return buttonRadius() * 0.46f; }
    String commLabel(int signal) {
        switch (signal) {
            case 1: return "H";
            case 2: return "P";
            case 3: return "G";
            case 4: return "OK";
            default: return "";
        }
    }

    private boolean isMoveZone(float x, float y) {
        return x < viewWidth * 0.46f && y > viewHeight * 0.35f;
    }

    private boolean isActionZone(float x, float y) {
        return x >= viewWidth * 0.46f
            && y > viewHeight * 0.28f
            && !inJumpButton(x, y)
            && !inShootButton(x, y)
            && !inCameraButton(x, y)
            && commSignalAt(x, y) == 0;
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

    private boolean inOptionsButton(float x, float y) {
        return inside(x, y, optionsCenterX(), optionsCenterY(), optionsRadius());
    }

    private int commSignalAt(float x, float y) {
        final float r = commRadius();
        for (int signal = 1; signal <= 4; ++signal) {
            if (inside(x, y, commCenterX(signal), commCenterY(), r)) return signal;
        }
        return 0;
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
        if (isController(event)) {
            if (!NativeBridge.isStarted()) {
                if (event.getRepeatCount() == 0 && isControllerStartKey(keyCode)) restartRun();
                return true;
            }
            if (NativeBridge.getMenuMode() == 2) {
                if (keyCode == KeyEvent.KEYCODE_BUTTON_A || keyCode == KeyEvent.KEYCODE_BUTTON_B || keyCode == KeyEvent.KEYCODE_BUTTON_START) {
                    if (activity != null) activity.resumeFromPauseMenu();
                    else NativeBridge.setPaused(false);
                    return true;
                }
            }
            if (event.getRepeatCount() > 0 && isControllerTapKey(keyCode)) return true;
            if (setControllerKey(keyCode, true)) { flushControlsNow(0.0f, 0.0f, false, false, false, false); return true; }
        }
        sendNativeKey(keyCode, true);
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (isController(event)) {
            if (setControllerKey(keyCode, false)) { flushControlsNow(0.0f, 0.0f, false, false, false, false); return true; }
        }
        sendNativeKey(keyCode, false);
        return true;
    }

    private void requestSixtyHertzSurface() {
        final Surface surface = getHolder().getSurface();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && surface != null && surface.isValid()) {
            surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
        }
    }

    private boolean isController(KeyEvent event) {
        final int source = event.getSource();
        return (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    private boolean isControllerTapKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_BUTTON_X:
            case KeyEvent.KEYCODE_BUTTON_Y:
            case KeyEvent.KEYCODE_BUTTON_L1:
            case KeyEvent.KEYCODE_BUTTON_R1:
            case KeyEvent.KEYCODE_BUTTON_L2:
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
                return true;
            default:
                return false;
        }
    }

    private boolean isControllerStartKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_BUTTON_B:
            case KeyEvent.KEYCODE_BUTTON_X:
            case KeyEvent.KEYCODE_BUTTON_Y:
            case KeyEvent.KEYCODE_BUTTON_START:
                return true;
            default:
                return false;
        }
    }

    private boolean setControllerKey(int keyCode, boolean down) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A: if (down) controllerJumpPressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_B: controllerBumperVacuumHeld = down; return true;
            case KeyEvent.KEYCODE_BUTTON_X: if (down) controllerMeleePressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_Y: if (down) controllerShootPressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_THUMBR: if (down) controllerCameraPressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_THUMBL: controllerStickSprintHeld = down; return true;
            case KeyEvent.KEYCODE_BUTTON_START: if (down) { if (NativeBridge.getMenuMode() == 2) { if (activity != null) activity.resumeFromPauseMenu(); else NativeBridge.setPaused(false); } else showPauseMenu(); } return true;
            case KeyEvent.KEYCODE_BUTTON_R1: if (down) controllerShootPressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_L1: if (down) controllerJumpPressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_L2: if (down) controllerMeleePressed = true; return true;
            case KeyEvent.KEYCODE_BUTTON_R2: controllerTriggerVacuumHeld = down; return true;
            case KeyEvent.KEYCODE_DPAD_UP: if (down) sendCommSignal(1); return true;
            case KeyEvent.KEYCODE_DPAD_RIGHT: if (down) sendCommSignal(2); return true;
            case KeyEvent.KEYCODE_DPAD_DOWN: if (down) sendCommSignal(3); return true;
            case KeyEvent.KEYCODE_DPAD_LEFT: if (down) sendCommSignal(4); return true;
            default: return false;
        }
    }

    private void sendCommSignal(int signal) {
        queueEvent(() -> NativeBridge.onCommSignal(signal));
        requestLowLatencyFrame();
    }

    private void sendNativeKey(int keyCode, boolean down) {
        queueEvent(() -> NativeBridge.onKey(keyCode, down));
        requestLowLatencyFrame();
    }

    private void restartRun() {
        queueEvent(NativeBridge::restart);
        requestLowLatencyFrame();
    }

    private void showPauseMenu() {
        NativeBridge.setPaused(true);
        if (activity != null) activity.showPauseMenu();
    }

    private final class NativeRenderer implements GLSurfaceView.Renderer {
        @Override
        public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl, javax.microedition.khronos.egl.EGLConfig config) {
            NativeBridge.onSurfaceCreated();
            post(GameView.this::requestSixtyHertzSurface);
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
