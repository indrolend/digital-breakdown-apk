#include <jni.h>
#include <android/log.h>
#include <algorithm>
#include <chrono>

#include "game/Game.hpp"
#include "render/Renderer.hpp"

#ifndef DIGITAL_BREAKDOWN_SOURCE_COMMIT
#define DIGITAL_BREAKDOWN_SOURCE_COMMIT "unknown"
#endif

namespace {
constexpr double FIXED_STEP_SECONDS = 1.0 / 60.0;
constexpr double MAX_FRAME_SECONDS = 0.25;
constexpr int MAX_STEPS_PER_FRAME = 4;

Game gGame;
Renderer gRenderer;
auto gLastFrame = std::chrono::steady_clock::now();
double gAccumulatorSeconds = 0.0;

void resetFrameClock() {
    gLastFrame = std::chrono::steady_clock::now();
    gAccumulatorSeconds = 0.0;
}

void advanceSimulation() {
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - gLastFrame;
    gLastFrame = now;
    gAccumulatorSeconds += std::min(elapsed.count(), MAX_FRAME_SECONDS);

    int steps = 0;
    while (gAccumulatorSeconds >= FIXED_STEP_SECONDS && steps < MAX_STEPS_PER_FRAME) {
        gGame.update(static_cast<float>(FIXED_STEP_SECONDS));
        gAccumulatorSeconds -= FIXED_STEP_SECONDS;
        ++steps;
    }

    if (steps == MAX_STEPS_PER_FRAME && gAccumulatorSeconds >= FIXED_STEP_SECONDS) {
        gAccumulatorSeconds = 0.0;
    }
}
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onSurfaceCreated(JNIEnv*, jclass) {
    __android_log_print(
        ANDROID_LOG_INFO,
        "DBNATIVE",
        "surface created native_source=%s",
        DIGITAL_BREAKDOWN_SOURCE_COMMIT
    );
    gGame.reset();
    gRenderer.surfaceCreated();
    resetFrameClock();
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onSurfaceChanged(JNIEnv*, jclass, jint width, jint height) {
    __android_log_print(ANDROID_LOG_INFO, "DBNATIVE", "surface changed %d x %d", width, height);
    gRenderer.surfaceChanged(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onDrawFrame(JNIEnv*, jclass) {
    advanceSimulation();
    gRenderer.draw(gGame.state());

    const GameState& s = gGame.state();
    if ((s.frame % 180) == 0) {
        __android_log_print(
            ANDROID_LOG_INFO,
            "DBNATIVE",
            "frame=%d room=%d pos=%.2f,%.2f,%.2f souls=%d battery=%.0f target=%d",
            s.frame,
            s.roomIndex,
            s.player.pos.x,
            s.player.pos.y,
            s.player.pos.z,
            s.player.souls,
            s.player.battery,
            s.vacuum.target
        );
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onTouch(
    JNIEnv*,
    jclass,
    jint action,
    jfloat x,
    jfloat y,
    jint pointerCount
) {
    gGame.setTouch(action, x, y, pointerCount);
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onTouchControls(
    JNIEnv*,
    jclass,
    jfloat moveX,
    jfloat moveZ,
    jfloat lookDeltaX,
    jfloat lookDeltaY,
    jboolean vacuumHeld,
    jboolean sprintHeld,
    jboolean jumpPressed,
    jboolean meleePressed,
    jboolean shootPressed,
    jboolean cameraTogglePressed
) {
    gGame.setTouchControls(
        moveX,
        moveZ,
        lookDeltaX,
        lookDeltaY,
        vacuumHeld == JNI_TRUE,
        sprintHeld == JNI_TRUE,
        jumpPressed == JNI_TRUE,
        meleePressed == JNI_TRUE,
        shootPressed == JNI_TRUE,
        cameraTogglePressed == JNI_TRUE
    );
}

extern "C" JNIEXPORT void JNICALL
Java_com_indrolend_digitalbreakdown_NativeBridge_onKey(JNIEnv*, jclass, jint keyCode, jboolean down) {
    gGame.setKey(keyCode, down == JNI_TRUE);
}