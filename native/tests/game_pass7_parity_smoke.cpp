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
    game.setTouchControls(0, 1, 0, 0, false, false, true, false, false, false);
    step(game);
    game.setTouchControls(0, 1, 0, 0, false, false, true, false, false, false);
    step(game);
    ok &= expect(game.state().phonePose.actionState == 5 && game.state().phonePose.doubleJumpTimer > 0.0f,
        "double jump gives the browser phone flip pose priority");
    step(game, 8);
    ok &= expect(game.state().phonePose.doubleJumpFlip > 2.0f && game.state().phonePose.doubleJumpFlip < 4.3f,
        "double-jump phone rotation passes through the half-flip phase");

    game.reset();
    game.setTouchControls(0, 0, 0, 0, true, false, false, false, false, false);
    step(game, 20);
    ok &= expect(game.state().phonePose.screenForwardTurn > 0.75f && game.state().phonePose.actionState == 2,
        "vacuum eases the phone toward its screen-forward pose");
    game.setTouchControls(0, 0, 0, 0, false, false, false, false, false, false);
    step(game, 30);
    ok &= expect(game.state().phonePose.screenForwardTurn < 0.10f,
        "phone screen-forward pose returns toward neutral after vacuum release");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& enemy=setup.targets[0]; enemy=TargetState{}; enemy.alive=true; enemy.pos=setup.player.pos+Vec3{4,0,0};
        enemy.walkTarget=enemy.pos+Vec3{2,0,0}; enemy.armor=2.0f;
    }
    const float enemyStartX=game.state().targets[0].pos.x;
    step(game,30);
    ok &= expect(game.state().targets[0].pos.x < enemyStartX-0.15f && near(game.state().targets[0].pos.y,spawn.player.pos.y,0.001f),
        "enemy stays grounded and pursues the player inside notice range");
    {
        GameState& setup=const_cast<GameState&>(game.state()); setup.targets[0].pos=setup.player.pos+Vec3{0,0,-1.0f};
        setup.targets[0].attackCooldown=0; setup.targets[0].attackTimer=0;
    }
    const float batteryBeforeEnemyAttack=game.state().player.battery;
    step(game,24);
    ok &= expect(game.state().targets[0].attackHit && game.state().player.battery < batteryBeforeEnemyAttack,
        "enemy attack uses the Pass 7 timed single-hit window");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.targets[0]=TargetState{}; setup.targets[0].alive=true; setup.targets[0].pos=setup.player.pos+Vec3{0,0,-1.5f}; setup.targets[0].armor=4;
        setup.targets[1]=TargetState{}; setup.targets[1].alive=true; setup.targets[1].pos=setup.player.pos+Vec3{0,0,1.5f}; setup.targets[1].armor=4;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false); step(game);
    ok &= expect(game.state().targets[0].armor < 4.0f && near(game.state().targets[1].armor,4.0f,0.001f),
        "phone melee uses the browser directional hit volume instead of an omnidirectional radius");
    ok &= expect(game.state().meleeVisual.visualTimer > 0 && game.state().phonePose.actionState==4,
        "phone melee exposes shared attack pose and FX timing");

    game.reset();
    {
        GameState& setup = const_cast<GameState&>(game.state());
        TargetState& target = setup.targets[0];
        target.alive = true;
        target.slurpable = true;
        target.pos = setup.player.pos + Vec3{0.0f, 0.5f, -0.30f};
        target.health = 1.0f;
        target.ingestProgress = 0.0f;
        target.soulState = SoulState::Free;
        setup.camera.forward = {0.0f, 0.0f, -1.0f};
    }
    game.setTouchControls(0, 0, 0, 0, true, false, false, false, false, false);
    step(game, 20);
    const GameState vacuumState = game.state();
    ok &= expect(vacuumState.vacuum.power > 0.32f, "vacuum reaches Pass 7 attraction threshold");
    ok &= expect(vacuumState.targets[0].soulState == SoulState::Latched || vacuumState.targets[0].soulState == SoulState::Ingesting, "near slurpable target enters latched or ingesting state");
    ok &= expect(vacuumState.targets[0].vacuumPullAmount > 0.0f, "vacuum reaction amount is shared on target state");
    ok &= expect(vacuumState.targets[0].captureCollapseAmount >= 0.0f, "capture collapse amount is deterministic on target state");

    game.reset();
    {
        GameState& setup = const_cast<GameState&>(game.state());
        TargetState& target = setup.targets[0];
        target.alive = true;
        target.slurpable = true;
        target.soulMorph = 0.99f;
        target.soulCubeAmount = 0.0f;
    }
    step(game, 2);
    ok &= expect(game.state().targets[0].soulCubeAmount >= 0.995f, "soul cube visibility follows shared morph threshold");

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
