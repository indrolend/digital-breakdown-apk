#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>

#include "../game/Game.hpp"
#include "../render/Renderer.hpp"

namespace {
Game gGame;
Renderer gRenderer;
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gContext = 0;
bool gReady = false;
constexpr float FIXED_STEP_SECONDS = 1.0f / 60.0f;
constexpr float MAX_FRAME_SECONDS = 0.25f;
constexpr int MAX_STEPS_PER_FRAME = 4;
float gAccumulatorSeconds = 0.0f;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int db_init(const char* canvasSelector) {
    if (gReady) return 1;

    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.alpha = EM_FALSE;
    attributes.depth = EM_TRUE;
    attributes.antialias = EM_TRUE;
    attributes.majorVersion = 1;
    attributes.minorVersion = 0;

    gContext = emscripten_webgl_create_context(canvasSelector, &attributes);
    if (gContext <= 0) return 0;
    if (emscripten_webgl_make_context_current(gContext) != EMSCRIPTEN_RESULT_SUCCESS) return 0;

    gGame.reset();
    gAccumulatorSeconds = 0.0f;
    gRenderer.surfaceCreated();
    gReady = true;
    return 1;
}

EMSCRIPTEN_KEEPALIVE void db_resize(int width, int height) {
    if (!gReady) return;
    gRenderer.surfaceChanged(width, height);
}

EMSCRIPTEN_KEEPALIVE void db_frame(float dt) {
    if (!gReady) return;
    gAccumulatorSeconds += std::min(std::max(dt, 0.0f), MAX_FRAME_SECONDS);
    int steps = 0;
    while (gAccumulatorSeconds >= FIXED_STEP_SECONDS && steps < MAX_STEPS_PER_FRAME) {
        gGame.update(FIXED_STEP_SECONDS);
        gAccumulatorSeconds -= FIXED_STEP_SECONDS;
        ++steps;
    }
    if (steps == MAX_STEPS_PER_FRAME && gAccumulatorSeconds >= FIXED_STEP_SECONDS) gAccumulatorSeconds = 0.0f;
    gRenderer.draw(gGame.state());
}

EMSCRIPTEN_KEEPALIVE void db_key(int keyCode, int down) {
    gGame.setKey(keyCode, down != 0);
}

EMSCRIPTEN_KEEPALIVE void db_touch_controls(
    float moveX,
    float moveZ,
    float lookDeltaX,
    float lookDeltaY,
    int vacuumHeld,
    int sprintHeld,
    int jumpPressed,
    int meleePressed,
    int shootPressed,
    int cameraTogglePressed
) {
    gGame.setTouchControls(
        moveX,
        moveZ,
        lookDeltaX,
        lookDeltaY,
        vacuumHeld != 0,
        sprintHeld != 0,
        jumpPressed != 0,
        meleePressed != 0,
        shootPressed != 0,
        cameraTogglePressed != 0
    );
}

EMSCRIPTEN_KEEPALIVE const char* db_source_commit() {
#ifdef DIGITAL_BREAKDOWN_SOURCE_COMMIT
    return DIGITAL_BREAKDOWN_SOURCE_COMMIT;
#else
    return "unknown";
#endif
}

}
