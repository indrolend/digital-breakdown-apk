#include "DesktopRenderer.hpp"
#include "Game.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace {
constexpr int KEY_W_ANDROID = 51;
constexpr int KEY_A_ANDROID = 29;
constexpr int KEY_S_ANDROID = 47;
constexpr int KEY_D_ANDROID = 32;
constexpr int KEY_Q_ANDROID = 45;
constexpr int KEY_C_ANDROID = 31;
constexpr int KEY_F_ANDROID = 34;
constexpr int KEY_SHIFT_LEFT_ANDROID = 59;
constexpr int KEY_SHIFT_RIGHT_ANDROID = 60;
constexpr int KEY_SPACE_ANDROID = 62;

struct HostState {
    Game game;
    DesktopRenderer renderer;
    double lookX = 0.0;
    double lookY = 0.0;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool haveMouse = false;
    bool mouseCaptured = true;
};

int androidKeyForGlfw(int key) {
    switch (key) {
        case GLFW_KEY_W: return KEY_W_ANDROID;
        case GLFW_KEY_A: return KEY_A_ANDROID;
        case GLFW_KEY_S: return KEY_S_ANDROID;
        case GLFW_KEY_D: return KEY_D_ANDROID;
        case GLFW_KEY_Q: return KEY_Q_ANDROID;
        case GLFW_KEY_C: return KEY_C_ANDROID;
        case GLFW_KEY_F: return KEY_F_ANDROID;
        case GLFW_KEY_LEFT_SHIFT: return KEY_SHIFT_LEFT_ANDROID;
        case GLFW_KEY_RIGHT_SHIFT: return KEY_SHIFT_RIGHT_ANDROID;
        case GLFW_KEY_SPACE: return KEY_SPACE_ANDROID;
        default: return -1;
    }
}

HostState* stateFor(GLFWwindow* window) {
    return static_cast<HostState*>(glfwGetWindowUserPointer(window));
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    HostState* host = stateFor(window);
    if (!host) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (host->mouseCaptured) {
            host->mouseCaptured = false;
            host->haveMouse = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        host->mouseCaptured = !host->mouseCaptured;
        host->haveMouse = false;
        glfwSetInputMode(window, GLFW_CURSOR, host->mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        return;
    }

    const int androidKey = androidKeyForGlfw(key);
    if (androidKey >= 0) {
        host->game.setKey(androidKey, action != GLFW_RELEASE);
    }
}

void cursorCallback(GLFWwindow* window, double x, double y) {
    HostState* host = stateFor(window);
    if (!host || !host->mouseCaptured) return;

    if (!host->haveMouse) {
        host->lastMouseX = x;
        host->lastMouseY = y;
        host->haveMouse = true;
        return;
    }

    host->lookX += x - host->lastMouseX;
    host->lookY += y - host->lastMouseY;
    host->lastMouseX = x;
    host->lastMouseY = y;
}

void framebufferCallback(GLFWwindow* window, int width, int height) {
    HostState* host = stateFor(window);
    if (host) host->renderer.resize(width, height);
}

void errorCallback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description ? description : "unknown");
}

bool hasArg(int argc, char** argv, const char* expected) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], expected) == 0) return true;
    }
    return false;
}
}

int main(int argc, char** argv) {
    const bool smokeTest = hasArg(argc, argv, "--smoke-test");

    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Digital Breakdown: GLFW initialization failed.\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_SAMPLES, 0);
    if (smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        smokeTest ? 64 : 1280,
        smokeTest ? 64 : 720,
        "Digital Breakdown - Native Desktop",
        nullptr,
        nullptr
    );
    if (!window) {
        glfwTerminate();
        std::fprintf(stderr, "Digital Breakdown: window creation failed.\n");
        return 1;
    }

    HostState host;
    host.game.reset();

    glfwSetWindowUserPointer(window, &host);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetFramebufferSizeCallback(window, framebufferCallback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(smokeTest ? 0 : 1);
    if (!smokeTest) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    int framebufferWidth = 1;
    int framebufferHeight = 1;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    host.renderer.resize(framebufferWidth, framebufferHeight);

    if (smokeTest) {
        for (int i = 0; i < 8; ++i) {
            glfwPollEvents();
            host.game.update(1.0f / 60.0f);
            host.renderer.draw(host.game.state());
            glfwSwapBuffers(window);
        }
        const GameState& state = host.game.state();
        std::printf(
            "SMOKE_TEST_OK frame=%d room=%d battery=%.2f targets=%d\n",
            state.frame,
            state.roomIndex,
            state.player.battery,
            TARGET_COUNT
        );
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

    std::printf("Digital Breakdown native desktop host running.\n");
    std::printf("WASD move | Shift sprint | Space jump | Mouse look | Left mouse vacuum | F melee | Q shoot | C camera | Tab release mouse | Esc quit\n");

    auto previous = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        const bool vacuumHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool sprintHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        host.game.setTouchControls(
            0.0f,
            0.0f,
            static_cast<float>(host.lookX),
            static_cast<float>(host.lookY),
            vacuumHeld,
            sprintHeld,
            false,
            false,
            false,
            false
        );
        host.lookX = 0.0;
        host.lookY = 0.0;

        host.game.update(std::min(dt, 0.033f));
        host.renderer.draw(host.game.state());
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
