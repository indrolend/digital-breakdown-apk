package com.indrolend.digitalbreakdown;

public final class NativeBridge {
    private NativeBridge() {}

    public static native void onSurfaceCreated();
    public static native void onSurfaceChanged(int width, int height);
    public static native void onDrawFrame();
    public static native void onTouch(int action, float x, float y, int pointerCount);
    public static native void onTouchControls(
        float moveX,
        float moveZ,
        float lookDeltaX,
        float lookDeltaY,
        boolean vacuumHeld,
        boolean sprintHeld,
        boolean jumpPressed,
        boolean meleePressed,
        boolean shootPressed,
        boolean cameraTogglePressed
    );
    public static native void onKey(int keyCode, boolean down);
}
