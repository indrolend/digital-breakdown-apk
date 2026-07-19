#include <cmath>
#include <iostream>
#include <cstring>

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

bool hasAudioCue(const GameState& state, AudioCue cue) {
    for(const auto& event:state.audio.events) if(event.serial>0 && event.cue==cue) return true;
    return false;
}
bool hasAudioCueAfter(const GameState& state, AudioCue cue, unsigned int serial) {
    for(const auto& event:state.audio.events) if(event.serial>serial && event.cue==cue) return true;
    return false;
}
}

int main() {
    bool ok = true;
    Game game;
    game.reset();
    const GameState spawn = game.state();
    ok &= expect(hasAudioCue(spawn,AudioCue::VcInvitation),"new native run queues the browser invitation cue");
    ok &= expect(near(spawn.player.pos.y, PHONE_MODEL_HEIGHT * 0.5f, 0.0001f), "spawn support y equals half Pass 7 phone height");
    ok &= expect(near(PHONE_BODY_WIDTH, 0.08f, 0.0001f) && near(PHONE_BODY_HEIGHT, 0.16f, 0.0001f) && near(PHONE_BODY_DEPTH, 0.012f, 0.0001f), "phone body dimensions match Pass 7 fallback/model normalized size");
    ok &= expect(near(Pass7Visual::CameraVerticalFovDegrees,75.0f,0.0001f) && near(Pass7Visual::CameraNearPlane,0.1f,0.0001f) && near(Pass7Visual::CameraFarPlane,1000.0f,0.0001f), "native projection matches the browser PerspectiveCamera contract");
    ok &= expect(near(spawn.camera.pos.y - spawn.player.pos.y, 1.1f, 0.0001f), "third-person camera height is relative to corrected player support");
    Game clearCameraGame; clearCameraGame.reset();
    { GameState& clear=const_cast<GameState&>(clearCameraGame.state());clear.debug.colliderCount=0;clear.player.pos={0.0f,PHONE_MODEL_HEIGHT*0.5f,0.0f};clear.camera.yaw=0;clear.camera.pitch=0; }
    step(clearCameraGame);const GameState clearCamera=clearCameraGame.state();
    ok &= expect(near(clearCamera.camera.pos.x-clearCamera.player.pos.x,0.0f,0.0001f) && near(clearCamera.camera.pos.z-clearCamera.player.pos.z,3.0f,0.0001f), "unobstructed third-person boom matches the browser three-unit aim-relative offset");
    ok &= expect(near(clearCamera.camera.lookTarget.y-clearCamera.player.pos.y,0.45f,0.0001f) && near(clearCamera.camera.lookTarget.z-clearCamera.player.pos.z,-10.0f,0.0001f), "third-person look target preserves browser aim direction and look lift");

    game.prepareStartScreen();
    const Vec3 preStartPosition=game.state().player.pos;
    game.setTouchControls(1,1,50,50,true,true,true,true,true,true);
    step(game,10);
    ok &= expect(!game.state().started&&!game.state().dead&&near(length(game.state().player.pos-preStartPosition),0.0f,0.0001f)&&game.state().audio.nextSerial==1,
        "browser start overlay freezes native gameplay and defers invitation audio until START");
    game.restart();
    ok &= expect(game.state().started&&hasAudioCue(game.state(),AudioCue::VcInvitation),
        "START begins a fresh native run and queues the browser invitation cue");
    const Vec3 introLockedPosition=game.state().player.pos;
    game.setTouchControls(1,1,80,40,true,true,true,true,true,true);
    step(game);
    ok &= expect(game.state().cinematic.introActive && length(game.state().camera.pos-game.state().phoneTransform.position)<0.8f &&
        near(length(game.state().player.pos-introLockedPosition),0.0f,0.0001f),
        "fresh START locks control for a close product-style phone reveal");
    step(game,70);
    ok &= expect(!game.state().cinematic.introActive && horizontalSpeed(game.state().camera.pos-game.state().player.pos)>2.45f,
        "product reveal hands off into the collision-safe gameplay chase camera before play");
    ok &= expect(length(game.state().player.pos-introLockedPosition)>0.001f,
        "direction held through the entrance becomes active on the first gameplay frame");
    game.setUiPaused(true);const Vec3 pausedPosition=game.state().player.pos;game.setTouchControls(1,1,50,50,true,true,true,true,true,true);step(game,10);
    ok &= expect(game.state().uiPaused&&near(length(game.state().player.pos-pausedPosition),0.0f,0.0001f)&&!game.state().vacuum.active,
        "open native HUD pause freezes gameplay and releases held vacuum input");
    game.setUiPaused(false);
    game.reset();

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
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);step(game,2);
    game.setTouchControls(0,0,0,0,true,false,false,true,false,false);step(game);
    ok &= expect(game.state().vacuum.active&&game.state().meleeVisual.visualTimer<=0.0f,
        "held grounded vacuum owns its action beat instead of overlapping a melee attack");
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.pos.y=1.4f;setup.player.grounded=false;setup.player.jumpVel=0.0f;
    }
    game.setTouchControls(0,0,0,0,true,false,false,true,false,false);step(game);
    const int lungeAirJumps=game.state().player.airJumpsRemaining;
    ok &= expect(game.state().meleeVisual.airLungeLandingPending&&!game.state().vacuum.active,
        "air lunge cleanly cancels vacuum and owns the phone until physical landing");
    game.setTouchControls(0,0,0,0,true,false,true,false,false,false);step(game,8);
    ok &= expect(!game.state().vacuum.active&&game.state().player.airJumpsRemaining==lungeAirJumps,
        "held vacuum and jump input cannot overlap or interrupt a committed lunge");
    ok &= expect(game.state().hud.crosshairOpacity<0.30f,
        "crosshair fades away while the committed lunge owns every phone action");
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false);step(game,60);
    ok &= expect(!game.state().meleeVisual.airLungeLandingPending&&game.state().hud.crosshairOpacity<0.05f,
        "crosshair stays hidden after landing until a reticle-driven action begins");
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);step(game,12);
    ok &= expect(game.state().vacuum.active&&game.state().hud.crosshairOpacity>0.80f,
        "crosshair fades in when vacuuming begins");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets)target.alive=false;
        setup.player.pos={-13.9f,1.0f,0.0f};setup.player.vel={};setup.player.jumpVel=0.0f;setup.player.grounded=false;
        setup.camera.yaw=0.0f;
    }
    game.setKey(62,true);
    game.setTouchControls(-1,0,0,0,false,false,false,false,false,false);
    step(game,60);
    ok &= expect(game.state().meleeVisual.wallClimbRemaining<=0.0f&&game.state().player.jumpVel<0.0f,
        "wall climb spends a finite airborne grip budget and returns control to gravity");
    game.setKey(62,false);

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
    step(game,42);
    ok &= expect(game.state().targets[0].attackHit && game.state().player.battery < batteryBeforeEnemyAttack,
        "enemy committed lateral sweep crosses the player once after its readable windup");

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
    ok &= expect(hasAudioCue(game.state(),AudioCue::PhoneAttack),
        "phone-attack audio is emitted by confirmed melee contact");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.player.pos={0.0f,1.4f,0.0f}; setup.player.vel={2.0f,0.0f,0.0f};
        setup.player.jumpVel=-0.4f; setup.player.grounded=false; setup.camera.yaw=0.0f;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false); step(game);
    const GameState airLungeForward=game.state();
    ok &= expect(airLungeForward.player.vel.z < -7.0f && std::abs(airLungeForward.player.vel.x) < 1.2f,
        "airborne action becomes a strong camera-forward locomotion impulse");
    ok &= expect(airLungeForward.player.jumpVel > 2.0f && !airLungeForward.player.grounded,
        "airborne locomotion begins a forward physical arc instead of a flat attack dash");
    ok &= expect(airLungeForward.meleeVisual.dashTimer <= 0.0f && airLungeForward.meleeVisual.airLungeTimer > 0.0f && !airLungeForward.meleeVisual.airLungePending,
        "airborne locomotion consumes one impulse instead of stacking the grounded positional dash");
    ok &= expect(airLungeForward.meleeVisual.locomotionLunge && airLungeForward.phonePose.actionState==6,
        "airborne locomotion uses the full-phone arch pose rather than the grounded swing pose");
    ok &= expect(airLungeForward.meleeVisual.airLungeRotation>0.0f && airLungeForward.meleeVisual.airLungeAngularVelocity>0.0f,
        "phone body arch integrates angular velocity instead of sampling a canned attack curve");
    ok &= expect(horizontalSpeed(airLungeForward.camera.pos-airLungeForward.player.pos)>3.05f,
        "lunge camera retains inertia so physical travel remains visible on screen");
    step(game,20);
    const float uninterruptedLungeTimer=game.state().meleeVisual.airLungeTimer;
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false);step(game);
    ok &= expect(game.state().meleeVisual.airLungeTimer<uninterruptedLungeTimer,
        "repeated attack input cannot renew an unfinished airborne trajectory");
    step(game,23);
    ok &= expect(game.state().player.grounded && !game.state().meleeVisual.airLungeLandingPending && game.state().player.pos.z<-4.0f,
        "ballistic lunge closes at a forward ground contact instead of recovering in midair");
    ok &= expect(game.state().meleeVisual.airLungeCameraLag>0.0f,
        "camera follow remains eased across physical landing instead of switching modes mid-arc");
    const float lowDropRecovery=game.state().meleeVisual.landingRecoveryDuration;
    ok &= expect(lowDropRecovery>=0.06f && game.state().player.vel.z<0.0f,
        "lunge landing retains forward momentum and enters a short recovery blend");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.player.pos={0.0f,4.5f,0.0f};setup.player.vel={0,0,0};
        setup.player.jumpVel=-0.5f;setup.player.grounded=false;setup.camera.yaw=0.0f;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false);step(game);
    step(game,44);
    ok &= expect(!game.state().player.grounded && game.state().meleeVisual.airLungeLandingPending && game.state().meleeVisual.airLungeTimer<=0.0f,
        "a high-drop lunge keeps falling after its powered phase instead of snapping to a timed landing");
    step(game,35);
    ok &= expect(game.state().player.grounded && !game.state().meleeVisual.airLungeLandingPending &&
                 game.state().meleeVisual.landingRecoveryDuration>lowDropRecovery,
        "higher-impact lunge landings receive a proportionally longer recovery blend");
    game.setTouchControls(0,0,0,0,false,false,true,false,false,false);step(game);
    ok &= expect(!game.state().player.grounded && game.state().meleeVisual.landingRecovery<=0.0f,
        "jump input cancels lunge landing recovery directly into continued locomotion");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.player.pos={0.0f,1.4f,0.0f}; setup.player.grounded=false; setup.camera.yaw=-DB_PI*0.5f;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false); step(game);
    ok &= expect(game.state().player.vel.x > 7.0f && std::abs(game.state().player.vel.z) < 0.25f,
        "airborne locomotion lunge follows camera heading on touch and desktop input paths");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.player.pos={0.0f,0.8f,0.0f};setup.player.grounded=false;setup.player.jumpVel=0.0f;setup.camera.yaw=0.0f;
        setup.targets[0]=TargetState{};setup.targets[0].alive=true;setup.targets[0].pos={0.0f,0.08f,-2.25f};setup.targets[0].armor=4.0f;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false);step(game);
    ok &= expect(near(game.state().targets[0].armor,4.0f,0.001f),
        "airborne locomotion does not project the grounded melee hit volume ahead of the phone");
    step(game,24);
    ok &= expect(game.state().targets[0].armor<4.0f && game.state().player.vel.z<0.0f,
        "airborne locomotion deals secondary damage on body contact without cancelling travel");

    game.reset();
    { GameState& setup=const_cast<GameState&>(game.state()); for(auto& target:setup.targets)target.alive=false; }
    const unsigned int missedMeleeSerial=game.state().audio.nextSerial-1;
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false); step(game);
    ok &= expect(!hasAudioCueAfter(game.state(),AudioCue::PhoneAttack,missedMeleeSerial),
        "missed melee does not play the phone-attack contact cue");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state()); for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.player.pos+Vec3{0,0.5f,-3.0f}; target.health=1; target.soulState=SoulState::Free;
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false); step(game,45);
    ok &= expect(game.state().vacuum.target==0 && game.state().targets[0].pos.z > game.state().player.pos.z-3.0f,
        "vacuum still acquires and moves a valid soul after melee changes");

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
    {
        const TargetState& soul=vacuumState.targets[0];
        float greatestDisplacement=0.0f;
        for(int n=0;n<SOUL_LATTICE_NODE_COUNT;++n){
            const int x=n%3,y=(n/3)%3,z=n/9;
            const Vec3 rest{(x-1)*0.23f,(y-1)*0.23f,(z-1)*0.23f};
            greatestDisplacement=std::max(greatestDisplacement,length(soul.latticePos[n]-rest));
        }
        ok &= expect(greatestDisplacement>0.01f,
            "active vacuum deforms the browser-equivalent 27-node soul lattice");
        ok &= expect(soul.tetherVisible && soul.tetherWidth>=0.12f &&
            length(soul.tetherDestination-vacuumState.phoneTransform.vacuumPullPoint)<0.0001f,
            "active soul tether narrows toward the live phone pull point");
    }
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false);
    step(game);
    {
        const TargetState& soul=game.state().targets[0];
        bool resetExactly=true;
        for(int n=0;n<SOUL_LATTICE_NODE_COUNT;++n){
            const int x=n%3,y=(n/3)%3,z=n/9;
            const Vec3 rest{(x-1)*0.23f,(y-1)*0.23f,(z-1)*0.23f};
            resetExactly=resetExactly&&length(soul.latticePos[n]-rest)<0.00001f&&length(soul.latticeVel[n])<0.00001f;
        }
        ok &= expect(resetExactly && !soul.tetherVisible,
            "vacuum release immediately resets the lattice before rigid recoil");
    }

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
    {
        GameState& setup=const_cast<GameState&>(game.state()); TargetState& target=setup.targets[0];
        target.alive=true; target.slurpable=true; target.soulMorph=0; target.hitFlash=1.35f;
    }
    step(game,6);
    ok &= expect(game.state().targets[0].soulCubeAmount>0.001f && game.state().targets[0].soulCubeAmount<0.995f,
        "soul cube instantiates during the human morph instead of popping in at the end");
    ok &= expect(near(game.state().targets[0].soulVisual.scale.x,game.state().targets[0].soulVisual.scale.y,0.0001f),
        "idle soul pulse remains uniformly cubic");

    game.reset();
    game.setTouchControls(0, 0, -0.75f / 0.003f, 0, false, false, false, false, false, true);
    step(game);
    const GameState firstPerson = game.state();
    ok &= expect(firstPerson.camera.firstPerson && near(firstPerson.player.yaw, firstPerson.camera.yaw, 0.0001f), "first-person heading remains tied to camera yaw");

    game.reset();
    step(game);
    {
        const GameState& state = game.state();
        ok &= expect(near(length(state.phoneTransform.screenRight), 1.0f, 0.0001f) &&
            near(length(state.phoneTransform.screenUp), 1.0f, 0.0001f) &&
            near(length(state.phoneTransform.screenNormal), 1.0f, 0.0001f),
            "shared phone transform exposes a normalized screen basis");
        ok &= expect(std::abs(state.phoneTransform.screenNormal.x*state.phoneTransform.screenRight.x +
            state.phoneTransform.screenNormal.y*state.phoneTransform.screenRight.y +
            state.phoneTransform.screenNormal.z*state.phoneTransform.screenRight.z) < 0.0001f,
            "shared screen normal remains perpendicular to screen right");
    }

    for(float pitch : {-1.2f,-0.75f,-0.35f,0.0f,0.35f,0.75f,1.2f}) {
        game.reset();
        game.setTouchControls(0,0,0,-pitch/0.003f,false,false,false,false,false,false);
        step(game);
        const GameState& state=game.state();
        const Vec3 renderRay=normalized(state.camera.lookTarget-state.camera.pos);
        ok &= expect(length(renderRay-state.camera.forward)<0.00001f,
            "vacuum offer ray matches the rendered crosshair center ray across browser pitch fixtures");
    }

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.player.pos+Vec3{0,0.5f,3.0f};
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);
    step(game,30);
    ok &= expect(game.state().vacuum.target==-1 && game.state().targets[0].soulState==SoulState::Free,
        "vacuum attraction cone rejects a soul behind the camera");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.player.pos+Vec3{0,0.5f,-4.0f};
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);
    step(game,30);
    ok &= expect(game.state().vacuum.target==0, "vacuum acquires a soul before the aim-away regression path");
    game.setTouchControls(0,0,-3.14159265f/0.003f,0,true,false,false,false,false,false);
    step(game,2);
    ok &= expect(game.state().vacuum.active && game.state().vacuum.power>0.95f && game.state().vacuum.fieldStrength>0.90f,
        "pointing away does not stop or discharge the active vacuum");
    ok &= expect(game.state().vacuum.lockStrength<1.0f && game.state().vacuum.coneTightness<1.0f,
        "aim loss relaxes target lock separately from the active vacuum field");
    game.setTouchControls(0,0,3.14159265f/0.003f,0,true,false,false,false,false,false);
    step(game,3);
    ok &= expect(game.state().vacuum.target==0,
        "returning aim reacquires the offered soul without restarting vacuum charge");

    game.reset();
    step(game);
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.camera.pos+setup.camera.forward*12.0f;
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);
    for(int i=0;i<60 && game.state().targets[0].soulState!=SoulState::Attracted;++i) step(game);
    ok &= expect(game.state().targets[0].soulState==SoulState::Attracted && game.state().vacuum.target==0,
        "oracle pre-latch fixture reaches attracted state");
    const float spinBeforeAimLoss=game.state().hud.crosshairRotationDegrees;
    game.setTouchControls(0,0,-(DB_PI*0.75f)/0.003f,0,true,false,false,false,false,false);
    step(game);
    ok &= expect(game.state().targets[0].soulState==SoulState::Attracted && game.state().vacuum.target==0,
        "browser ordering retains the prior vacuum target for the first rendered aim-loss frame");
    ok &= expect(game.state().hud.crosshairRotationDegrees!=spinBeforeAimLoss && game.state().hud.crosshairSpreadPixels>15.0f,
        "vacuum crosshair rotor and spread continue independently through aim loss");
    step(game);
    ok &= expect(game.state().targets[0].soulState==SoulState::Free && game.state().vacuum.target==-1 && game.state().vacuum.active,
        "the following vacuum frame releases attracted to free without deactivating vacuum");
    game.setTouchControls(0,0,(DB_PI*0.75f)/0.003f,0,true,false,false,false,false,false);
    step(game,3);
    ok &= expect(game.state().targets[0].soulState==SoulState::Attracted && game.state().vacuum.target==0,
        "released pre-latch soul is reacquired when the crosshair returns");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);
    step(game,20);
    {
        GameState& setup=const_cast<GameState&>(game.state());
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.phoneTransform.screenCenter+setup.phoneTransform.screenRight*1.0f
            +setup.phoneTransform.screenUp*1.0f+setup.phoneTransform.screenNormal*0.40f;
        target.soulState=SoulState::Latched;
    }
    step(game);
    {
        const GameState& state=game.state();
        const Vec3 local=inverseRotate(state.phoneTransform.orientation,state.targets[0].latchPoint-state.phoneTransform.screenCenter);
        ok &= expect(std::abs(local.x)<=PHONE_SCREEN_WIDTH*0.5f*0.92f+0.0001f&&
                     std::abs(local.y)<=PHONE_SCREEN_HEIGHT*0.5f*0.92f+0.0001f,
            "screen-local latch clamps sideways and above-phone drift to the authoritative aperture");
    }
    {
        GameState& setup=const_cast<GameState&>(game.state());
        TargetState& target=setup.targets[0];target.soulState=SoulState::Attracted;target.latchedToScreen=false;
        target.pos=setup.phoneTransform.position+setup.phoneTransform.screenUp*0.10f;
    }
    step(game);
    {
        const GameState& state=game.state();
        const Vec3 local=inverseRotate(state.phoneTransform.orientation,state.targets[0].pos-state.phoneTransform.position);
        ok &= expect(local.z>=PHONE_BODY_DEPTH*0.5f+0.33f-0.001f,
            "attracted soul core is moved to the screen-facing side before it can cross through or over the phone");
    }

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        BulletState& bullet=setup.bullets[0];bullet=BulletState{};bullet.alive=true;bullet.life=1.0f;
        const float wallZ=setup.captures[0].pos.z;
        bullet.pos={4.0f,setup.captures[0].pos.y,wallZ+0.45f};bullet.vel={0,0,-60.0f};
    }
    step(game);
    ok &= expect(hasAudioCue(game.state(),AudioCue::PaymentFailure),
        "narrow capture-wall miss emits the browser payment-failure cue once");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=24.05f;
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);step(game,12);
    ok &= expect(hasAudioCue(game.state(),AudioCue::LowPower),"crossing below 24 percent emits low-power audio");
    { GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=99.4f; }
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false);step(game);
    ok &= expect(!hasAudioCue(game.state(),AudioCue::ConnectPower),"ordinary near-full recovery does not emit connect-power audio");
    { GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=13.0f; }
    step(game);
    { GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=99.4f; }
    step(game);
    ok &= expect(hasAudioCue(game.state(),AudioCue::ConnectPower),"recovery to full emits connect-power once after crossing the browser 14-percent arming threshold");

    game.reset();
    step(game);
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.phoneTransform.screenCenter+setup.phoneTransform.screenNormal*0.38f;
        target.soulState=SoulState::Ingesting; target.ingestProgress=0.60f; target.latchedToScreen=true;
    }
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false);
    step(game);
    ok &= expect(game.state().targets[0].soulState==SoulState::Recoiling &&
        near(game.state().targets[0].ingestProgress,0.0f,0.0001f) && game.state().targets[0].recoilTime>0.0f,
        "interrupting ingestion immediately resets progress and enters rigid recoil");
    ok &= expect(hasAudioCue(game.state(),AudioCue::EndCallTone),
        "releasing a partially ingested soul queues the browser end-call tone");

    game.reset();
    step(game);
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.slurpable=true;
        target.pos=setup.phoneTransform.screenCenter+setup.phoneTransform.screenNormal*0.38f;
        target.soulState=SoulState::Ingesting; target.ingestProgress=0.92f;
    }
    const int soulsBeforeCommit=game.state().player.souls;
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false);
    step(game);
    ok &= expect(!game.state().targets[0].alive && game.state().player.souls==soulsBeforeCommit+1,
        "release at the commit threshold completes through the capture queue");
    ok &= expect(hasAudioCue(game.state(),AudioCue::ReceivedMessage),
        "completed capture queues the authoritative received-message cue");
    int captureParticles=0; for(const auto& particle:game.state().particles) if(particle.life>0.0f) ++captureParticles;
    ok &= expect(captureParticles==22,
        "completed capture emits the browser's 22-cube particle burst from shared state");
    step(game,150);
    ok &= expect(game.state().targets[0].alive,
        "captured living-shell slot returns through the browser delayed room-population queue");

    game.reset();
    step(game);
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.souls=1;
        setup.player.storedSoulBrute[0]=true;
    }
    const int dischargeStartFrame=game.state().frame;
    game.setTouchControls(0,0,0,0,false,false,false,false,true,false);
    step(game);
    {
        bool pending=false; for(const auto& shot:game.state().pendingShots) if(shot.active) pending=true;
        bool launched=false; for(const auto& bullet:game.state().bullets) if(bullet.alive) launched=true;
        ok &= expect(pending && !launched,
            "shooting queues the stored cube while the browser discharge pose advances");
    }
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false);
    step(game,5);
    {
        const GameState& state=game.state();
        const BulletState* fired=nullptr;
        for(const auto& bullet:state.bullets) if(bullet.alive){fired=&bullet;break;}
        ok &= expect(state.player.souls==0 && fired!=nullptr && fired->brute,
            "shooting consumes the stored cube and launches that cube's type");
        ok &= expect(hasAudioCue(state,AudioCue::SentMessage),
            "released projectile queues the authoritative sent-message cue");
        ok &= expect(fired && horizontalSpeed(fired->vel)<23.0f,
            "stored brute soul uses the browser's heavier launch speed");
        ok &= expect(state.hud.shootJoinTimer>0.0f && state.hud.crosshairSpreadPixels<15.0f,
            "successful discharge starts the browser crosshair join cue");
        ok &= expect(state.hud.crosshairOpacity>0.65f,
            "soul discharge fades in the action-specific crosshair");
        ok &= expect(state.frame-dischargeStartFrame==6 && fired!=nullptr,
            "fresh discharge releases on the oracle sixth frame after its 0.09-second hold");
        ok &= expect(std::strstr(state.hud.energyTicker.data(),"SHOOT")!=nullptr && state.time<state.hud.energyTickerUntil,
            "discrete discharge publishes the browser sequential energy ticker contract");
    }

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());setup.debug.colliderCount=0;
        setup.player.pos={2.0f,PHONE_MODEL_HEIGHT*0.5f,-20.90f};setup.player.vel={0,0,-14.0f};setup.player.grounded=true;
    }
    step(game);
    ok &= expect(game.state().player.pos.z>=-21.0f+0.34f-0.001f&&game.state().topology.currentTileIndex==0,
        "swept doorway collision blocks crossing through the solid side wall");
    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());setup.debug.colliderCount=0;
        setup.player.pos={1.70f,PHONE_MODEL_HEIGHT*0.5f,-20.80f};setup.player.vel={8.0f,0,0};setup.player.grounded=true;
    }
    step(game);
    ok &= expect(game.state().player.pos.x<=2.1f-0.34f+0.001f,
        "doorway jamb stops lateral capsule motion while the phone occupies the opening");

    game.reset();
    const int firstRoomRequired=game.state().requiredSouls;
    for(int shot=0;shot<firstRoomRequired;++shot){
        GameState& setup=const_cast<GameState&>(game.state());
        BulletState& bullet=setup.bullets[0]; bullet=BulletState{}; bullet.alive=true; bullet.life=1.0f;
        bullet.pos=setup.captures[shot].pos+Vec3{0,0,1.9f}; bullet.vel={0,0,-25.0f};
        step(game);
    }
    {
        const GameState& state=game.state();
        int filled=0; for(const auto& capture:state.captures) if(capture.filled) ++filled;
        ok &= expect(filled==firstRoomRequired && state.roomClear,
            "fired soul cubes fill the wall goals sequentially and open the room");
        ok &= expect(hasAudioCue(state,AudioCue::PaymentSuccess),
            "final room deposit queues the authoritative payment-success cue");
    }

    {
        GameState& setup=const_cast<GameState&>(game.state());
        const int previousRoom=setup.roomIndex;
        setup.player.pos={0,PHONE_MODEL_HEIGHT*0.5f,-20.8f};
        setup.player.vel={0,0,-20.0f};
        step(game,2);
        const GameState& state=game.state();
        int filled=0; for(const auto& capture:state.captures) if(capture.filled) ++filled;
        ok &= expect(state.roomIndex==previousRoom+1 && !state.roomClear && filled==0,
            "crossing the opened doorway advances the room and resets its goal inventory");
        const int ruleStacks=state.runRules.requiredSlotStacks+state.runRules.crowdedRoomStacks+state.runRules.fasterSlurpStacks;
        ok &= expect(ruleStacks==1 && state.runRules.lastAdded>=0,
            "room advancement deterministically adds one eligible browser run rule");
        ok &= expect(state.requiredSouls==5+state.runRules.requiredSlotStacks,
            "required-slot run rules rebuild the following room goal inventory");
        ok &= expect(state.doorTransition.active && state.doorTransition.progress>0.75f,
            "open-door crossing starts the browser distance-owned datamosh transition state");
    }
    {
        const_cast<GameState&>(game.state()).player.vel={};
        const float heldProgress=game.state().doorTransition.progress;
        game.setTouchControls(0,0,0,0,false,false,false,false,false,false);step(game,30);
        ok &= expect(near(game.state().doorTransition.progress,heldProgress,0.0001f),
            "door datamosh remains frozen while the player stops moving");
        GameState& setup=const_cast<GameState&>(game.state());setup.player.pos.x+=3.1f;step(game);
        ok &= expect(!game.state().doorTransition.active&&near(game.state().doorTransition.progress,0.0f,0.0001f),
            "three meters of traversal resolves the browser doorway prediction-frame effect");
    }

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        const float hitZ=setup.player.pos.z-4.0f;
        for(int i=0;i<2;++i) {
            TargetState& target=setup.targets[i]; target=TargetState{}; target.alive=true;
            target.armor=4.0f; target.pos={setup.player.pos.x,0.08f,hitZ}; target.walkTarget=target.pos;
        }
        BulletState& bullet=setup.bullets[0]; bullet=BulletState{}; bullet.alive=true; bullet.life=1.0f;
        bullet.pos={setup.player.pos.x,0.65f,hitZ+0.50f}; bullet.vel={0,0,-25.0f};
    }
    step(game);
    ok &= expect(near(game.state().targets[0].armor,3.1f,0.001f) &&
        near(game.state().targets[1].armor,4.0f,0.001f) && !game.state().bullets[0].alive,
        "normal fired cube damages only the first living shell on its swept path and is consumed");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true; target.brute=true;
        target.armor=4.0f; target.pos={setup.player.pos.x+0.85f,0.08f,setup.player.pos.z-4.0f}; target.walkTarget=target.pos;
        BulletState& bullet=setup.bullets[0]; bullet=BulletState{}; bullet.alive=true; bullet.life=1.0f; bullet.brute=true;
        bullet.pos={setup.player.pos.x,0.65f,target.pos.z+0.50f}; bullet.vel={0,0,-20.0f};
    }
    step(game);
    ok &= expect(near(game.state().targets[0].armor,2.35f,0.001f) && !game.state().bullets[0].alive,
        "brute fired cube uses the browser's wider 0.95 shell radius and 1.65 damage");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.battery=50.0f;
    }
    step(game,60);
    ok &= expect(near(game.state().player.battery,72.0f,0.001f),
        "idle native battery reproduces the browser 22-per-second regeneration");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.battery=1.0f;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false);
    step(game);
    const Vec3 deadPosition=game.state().player.pos;
    const Vec3 deathStartCamera=game.state().camera.pos;
    const float deadTargetPhase=game.state().targets[0].visualWalkPhase;
    ok &= expect(game.state().dead && !game.state().started && !game.state().player.alive &&
        game.state().hud.gameOver && near(game.state().player.battery,0.0f,0.0001f),
        "battery exhaustion enters the browser run-death lifecycle and exposes game-over HUD state");
    ok &= expect(hasAudioCue(game.state(),AudioCue::VcEnded),
        "run death queues the authoritative call-ended cue");
    game.setTouchControls(1,1,20,20,true,true,true,true,true,true);
    step(game,10);
    ok &= expect(near(game.state().player.pos.x,deadPosition.x,0.0001f) && near(game.state().player.pos.z,deadPosition.z,0.0001f) &&
        near(game.state().targets[0].visualWalkPhase,deadTargetPhase,0.0001f),
        "dead lifecycle freezes gameplay simulation despite held movement and actions");
    ok &= expect(game.state().cinematic.deathActive && length(game.state().camera.pos-deathStartCamera)>0.05f,
        "death presentation keeps the world frozen while the camera begins its slow pullback");
    game.restart();
    ok &= expect(!game.state().dead && game.state().started && game.state().player.alive &&
        near(game.state().player.battery,100.0f,0.0001f) && game.state().roomIndex==1,
        "restart creates a clean browser run with reset progression and battery");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.battery=50.0f;
        setup.energy.flowerStacks=2;
        setup.energy.supplementalActive=true;
        setup.energy.supplementalValue=92.0f;
        setup.energy.supplementalMax=117.0f;
        for(auto& target:setup.targets) target.alive=false;
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);
    step(game,60);
    ok &= expect(near(game.state().player.battery,50.0f,0.0001f) &&
        near(game.state().energy.supplementalValue,90.812f,0.0002f),
        "native supplemental power matches the oracle one-second 60 FPS vacuum trace");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.battery=50.0f;
        setup.player.souls=5;
        setup.energy.flowerStacks=1;
        setup.energy.supplementalActive=true;
        setup.energy.supplementalValue=85.0f;
        setup.energy.supplementalMax=85.0f;
    }
    game.setTouchControls(0,0,0,0,false,false,false,false,true,false);
    step(game);
    ok &= expect(near(game.state().energy.supplementalValue,85.0f-7.0f/1.8f,0.0002f),
        "stored-soul efficiency is applied before native supplemental discharge cost");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.player.battery=50.0f;
        setup.energy.flowerStacks=1;
        setup.energy.supplementalActive=true;
        setup.energy.supplementalValue=2.0f;
        setup.energy.supplementalMax=85.0f;
    }
    game.setTouchControls(0,0,0,0,false,false,true,false,false,false);
    step(game);
    ok &= expect(game.state().energy.flowerStacks==0 && !game.state().energy.supplementalActive &&
        near(game.state().energy.supplementalValue,0.0f,0.0001f) && near(game.state().energy.supplementalMax,85.0f,0.0001f),
        "supplemental depletion clears native flower stacks and restores the 85-point base maximum");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        TargetState& brute=setup.targets[0]; brute=TargetState{}; brute.alive=true; brute.brute=true;
        brute.armor=0.1f; brute.pos=setup.player.pos+Vec3{0,0,-1.5f};
        setup.flowerRandomState=1u;
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false);
    step(game);
    int droppedFlowers=0; float dropDistance=0.0f;
    for(const auto& flower:game.state().flowers) if(flower.active){
        ++droppedFlowers;
        const Vec3 d=flower.pos-game.state().targets[0].pos;
        dropDistance=std::sqrt(d.x*d.x+d.z*d.z);
    }
    ok &= expect(game.state().targets[0].slurpable && droppedFlowers==1 && dropDistance>=1.05f,
        "brute shell conversion performs one drop roll and separates the flower from its source");
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false); step(game,20);
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false); step(game);
    int repeatedDrops=0; for(const auto& flower:game.state().flowers) if(flower.active) ++repeatedDrops;
    ok &= expect(repeatedDrops==1,
        "subsequent hits on an already slurpable brute do not roll duplicate flowers");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        FlowerPowerupState& flower=setup.flowers[0]; flower.active=true; flower.baseY=0.38f;
        flower.pos=setup.player.pos+Vec3{0.5f,0.30f,0.0f};
    }
    step(game);
    ok &= expect(!game.state().flowers[0].active && game.state().energy.flowerStacks==1 &&
        near(game.state().energy.supplementalValue,46.0f,0.0001f) && near(game.state().energy.supplementalMax,85.0f,0.0001f),
        "three-dimensional flower pickup activates the first 46-of-85 supplemental stack");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        FlowerPowerupState& flower=setup.flowers[0]; flower.active=true; flower.baseY=0.50f;
        flower.pos={8.0f,0.50f,0.0f}; flower.rotationY=0.25f;
    }
    step(game,30);
    ok &= expect(near(game.state().flowers[0].age,0.5f,0.0001f) &&
        near(game.state().flowers[0].pos.y,0.50f+std::sin(1.6f)*0.16f,0.0001f) &&
        near(game.state().flowers[0].rotationY,0.25f+0.5f*1.35f,0.0001f),
        "flower bob and rotation reproduce the browser 60 FPS motion equations");

    std::cout << "numeric forward0=(" << forward0.player.vel.x << "," << forward0.player.vel.z << ")"
              << " forward90=(" << forward90.player.vel.x << "," << forward90.player.vel.z << ")"
              << " rotateWhileMoving=(" << rotateWhileMoving.player.vel.x << "," << rotateWhileMoving.player.vel.z << ")"
              << " diagonalNorm=(" << diagonal.player.vel.x / diagSpeed << "," << diagonal.player.vel.z / diagSpeed << ")"
              << " firstPersonYaw=" << firstPerson.player.yaw << "\n";
    return ok ? 0 : 1;
}
