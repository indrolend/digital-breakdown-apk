#include <cmath>
#include <iostream>

#include "Game.hpp"

namespace {
constexpr float kEps = 0.025f;
constexpr float kHalfPi = 1.5707963267948966f;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        return false;
    }
    std::cout << "PASS: " << message << "\n";
    return true;
}

void step(Game& game, int ticks = 1, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < ticks; ++i) game.update(dt);
}

float horizontalSpeed(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

bool near(float a, float b, float eps = kEps) {
    return std::fabs(a - b) <= eps;
}
}

int main() {
    bool ok = true;
    Game game;
    game.reset();
    const GameState spawn = game.state();
    ok &= expect(near(spawn.player.pos.y, PHONE_MODEL_HEIGHT * 0.5f, 0.0001f), "spawn support y equals half Pass 7 phone height");
    ok &= expect(near(PHONE_BODY_WIDTH, 0.08f, 0.0001f) && near(PHONE_BODY_HEIGHT, 0.16f, 0.0001f) && near(PHONE_BODY_DEPTH, 0.012f, 0.0001f), "phone body dimensions match Pass 7 fallback/model normalized size");
    ok &= expect(near(spawn.camera.pos.y - spawn.player.pos.y, 1.1f, 0.0001f), "third-person camera height is relative to corrected player support");

    game.setTouchControls(0, 1, 0, 0, false, false, false, false, false, false);
    step(game);
    const GameState forward0 = game.state();
    ok &= expect(forward0.player.vel.z < -0.01f && near(forward0.player.vel.x, 0.0f), "yaw 0 forward accelerates along Pass 7 -Z camera forward");
    ok &= expect(near(forward0.player.yaw, forward0.camera.yaw, 0.0001f), "visible yaw equals camera yaw during forward movement");

    game.reset();
    game.setTouchControls(0, 0, kHalfPi / 0.003f, 0, false, false, false, false, false, false);
    step(game);
    game.setTouchControls(0, 1, 0, 0, false, false, false, false, false, false);
    step(game);
    const GameState forward90 = game.state();
    ok &= expect(forward90.player.vel.x > 0.01f && near(forward90.player.vel.z, 0.0f), "camera yaw -90 degrees forward accelerates along +X");
    ok &= expect(near(forward90.player.yaw, forward90.camera.yaw, 0.0001f), "stationary camera rotation sets visible player yaw without velocity-facing lag");

    game.reset();
    game.setTouchControls(0, 1, 0, 0, false, false, false, false, false, false);
    step(game, 3);
    game.setTouchControls(0, 1, kHalfPi / 0.003f, 0, false, false, false, false, false, false);
    step(game);
    const GameState rotateWhileMoving = game.state();
    ok &= expect(rotateWhileMoving.player.vel.x > 0.01f, "same-frame camera rotation affects ongoing forward movement");
    ok &= expect(near(rotateWhileMoving.player.yaw, rotateWhileMoving.camera.yaw, 0.0001f), "visible yaw follows camera while already moving");

    game.reset();
    game.setTouchControls(-1, 0, 0, 0, false, false, false, false, false, false);
    step(game);
    const GameState strafeLeft0 = game.state();
    ok &= expect(strafeLeft0.player.vel.x < -0.01f && near(strafeLeft0.player.vel.z, 0.0f), "strafe left at yaw 0 accelerates along -X");
    ok &= expect(near(strafeLeft0.player.yaw, strafeLeft0.camera.yaw, 0.0001f), "visible yaw remains camera-tied during strafing");

    game.reset();
    game.setTouchControls(1, 0, kHalfPi / 0.003f, 0, false, false, false, false, false, false);
    step(game);
    game.setTouchControls(1, 0, 0, 0, false, false, false, false, false, false);
    step(game);
    const GameState strafeRight90 = game.state();
    ok &= expect(strafeRight90.player.vel.z > 0.01f && near(strafeRight90.player.vel.x, 0.0f), "strafe right at yaw -90 accelerates along +Z");

    game.reset();
    game.setTouchControls(1, 1, 0, 0, false, false, false, false, false, false);
    step(game);
    const GameState diagonal = game.state();
    const float diagSpeed = horizontalSpeed(diagonal.player.vel);
    ok &= expect(near(diagonal.player.vel.x / diagSpeed, 0.7071f, 0.02f) && near(diagonal.player.vel.z / diagSpeed, -0.7071f, 0.02f), "diagonal movement normalizes combined camera forward/right intent");

    game.reset();
    game.setTouchControls(0, 0, -0.75f / 0.003f, 0, false, false, false, false, false, true);
    step(game);
    const GameState firstPerson = game.state();
    ok &= expect(firstPerson.camera.firstPerson && near(firstPerson.player.yaw, firstPerson.camera.yaw, 0.0001f), "first-person heading remains tied to camera yaw");

    std::cout << "numeric forward0=(" << forward0.player.vel.x << "," << forward0.player.vel.z << ")"
              << " forward90=(" << forward90.player.vel.x << "," << forward90.player.vel.z << ")"
              << " rotateWhileMoving=(" << rotateWhileMoving.player.vel.x << "," << rotateWhileMoving.player.vel.z << ")"
              << " diagonalNorm=(" << diagonal.player.vel.x / diagSpeed << "," << diagonal.player.vel.z / diagSpeed << ")"
              << " firstPersonYaw=" << firstPerson.player.yaw << "\n";
    return ok ? 0 : 1;
}
