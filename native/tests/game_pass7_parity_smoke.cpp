#include <cmath>
#include <iostream>
#include <cstring>
#include <memory>

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
int countAudioCue(const GameState& state,AudioCue cue){int count=0;for(const auto& event:state.audio.events)if(event.serial>0&&event.cue==cue)++count;return count;}
}

int main() {
    bool ok = true;
    Game progressionFixture;progressionFixture.setPersistentProgression(17,2,3,9);progressionFixture.reset();
    ok &= expect(progressionFixture.state().progression.permanent.tokens==17&&progressionFixture.state().progression.permanent.levels[0]==2&&progressionFixture.state().progression.permanent.levels[1]==3&&progressionFixture.state().progression.permanent.levels[2]==5,
        "versioned permanent progression survives run reset and clamps upgrade tracks safely");
    ok &= expect(progressionFixture.state().progression.run.temporaryLevels==std::array<int,3>{},
        "run-only progression resets independently from permanent shop state");
    {GameState& settings=const_cast<GameState&>(progressionFixture.state());settings.localSettings.musicVolume=0.30f;settings.localSettings.sfxVolume=0.80f;settings.localSettings.shadows=false;settings.localSettings.particles=false;settings.localSettings.menuPage=LocalMenuPage::Graphics;}
    progressionFixture.reset();
    ok &= expect(near(progressionFixture.state().localSettings.musicVolume,0.30f,0.001f)&&near(progressionFixture.state().localSettings.sfxVolume,0.80f,0.001f)&&!progressionFixture.state().localSettings.shadows&&!progressionFixture.state().localSettings.particles&&progressionFixture.state().localSettings.menuPage==LocalMenuPage::Main,
        "local audio and graphics settings survive run reset while submenu navigation returns safely to main");
    Game precisionBuild;precisionBuild.setPersistentProgression(0,2,2,0);precisionBuild.reset();step(precisionBuild);
    ok &= expect(std::strstr(precisionBuild.state().hud.buildLabel.data(),"PINBALL SNIPER")!=nullptr,
        "paired shot and lunge levels unlock a visible precision-mobility build identity");
    Game relayBuild;relayBuild.setPersistentProgression(0,2,0,2);relayBuild.reset();
    {
        GameState& setup=const_cast<GameState&>(relayBuild.state());for(auto& target:setup.targets)target.alive=false;
        TargetState& target=setup.targets[0];target=TargetState{};target.alive=true;target.armor=2.0f;target.pos={setup.player.pos.x,0.08f,setup.player.pos.z-4.0f};target.walkTarget=target.pos;target.attackCooldown=999.0f;
        BulletState& bullet=setup.bullets[0];bullet=BulletState{};bullet.alive=true;bullet.life=1.0f;bullet.pos={target.pos.x,0.65f,target.pos.z+0.5f};bullet.vel={0,0,-25.0f};
    }
    step(relayBuild);
    ok &= expect(relayBuild.state().progression.run.relayPrimerStacks==1&&relayBuild.state().progression.run.relayPrimerTimer>3.0f,
        "a body shot in the shot-attack pairing primes the next melee contact instead of acting as an isolated stat bonus");
    Game cockroachBuild;cockroachBuild.setPersistentProgression(0,2,2,2);cockroachBuild.reset();
    {
        GameState& setup=const_cast<GameState&>(cockroachBuild.state());for(auto& target:setup.targets)target.alive=false;
        setup.player.battery=5.0f;TargetState& target=setup.targets[0];target=TargetState{};target.alive=true;target.armor=2.0f;target.pos=setup.player.pos+Vec3{0,0,-1.0f};target.walkTarget=target.pos;target.attackCooldown=0.0f;
    }
    step(cockroachBuild,90);
    ok &= expect(cockroachBuild.state().player.alive&&(cockroachBuild.state().player.grabbedByTarget>=0||cockroachBuild.state().progression.run.lastStandCooldown>0.0f),
        "the balanced low-damage survival circuit enters a recoverable grab or catches one lethal swing");
    Game wiggleGame;wiggleGame.reset();
    {GameState& setup=const_cast<GameState&>(wiggleGame.state());for(auto& target:setup.targets)target.alive=false;setup.targets[0]=TargetState{};setup.targets[0].alive=true;setup.targets[0].pos=setup.player.pos+Vec3{0,0,-0.5f};setup.targets[0].grabbedPlayerId=0;setup.player.grabbedByTarget=0;}
    for(int tap=0;tap<5;++tap){wiggleGame.setWiggle(0.0f);step(wiggleGame);}
    ok &= expect(wiggleGame.state().player.grabbedByTarget<0&&!wiggleGame.state().input.meleePressed&&!wiggleGame.state().input.shootPressed,
        "five simple phone taps synthesize alternating wiggles and release without leaking an action into the next menu");
    Game game;
    game.reset();
    const GameState spawn = game.state();
    ok &= expect(hasAudioCue(spawn,AudioCue::VcInvitation),"new native run queues the browser invitation cue");
    ok &= expect(near(spawn.player.pos.y, PHONE_MODEL_HEIGHT * 0.5f, 0.0001f), "spawn support y equals half Pass 7 phone height");
    Game ledgeGame;ledgeGame.reset();
    {GameState& ledge=const_cast<GameState&>(ledgeGame.state());const RoomCollider& platform=ledge.roomColliders[0];ledge.player.pos={platform.maxX+0.12f,platform.topY+PHONE_MODEL_HEIGHT*0.5f,platform.center.z};ledge.player.vel={};ledge.player.jumpVel=0.0f;ledge.player.grounded=true;}
    step(ledgeGame);
    ok &= expect(!ledgeGame.state().player.grounded,
        "phone leaves obstacle support shortly after its visible footprint crosses the ledge instead of standing on invisible air");
    auto ledgeHangGame=std::make_unique<Game>();ledgeHangGame->reset();
    {GameState& ledge=const_cast<GameState&>(ledgeHangGame->state());const RoomCollider& platform=ledge.roomColliders[0];ledge.player.pos={platform.maxX+0.34f,platform.topY-PHONE_MODEL_HEIGHT*0.5f,platform.center.z};ledge.player.vel={0,0,1.6f};ledge.player.jumpVel=-0.8f;ledge.player.grounded=false;}
    step(*ledgeHangGame);
    const float caughtTop=ledgeHangGame->state().player.pos.y+PHONE_MODEL_HEIGHT*0.5f;
    ok &= expect(ledgeHangGame->state().player.ledgeHanging&&near(caughtTop,ledgeHangGame->state().roomColliders[0].topY+0.008f,0.002f),
        "descending phone catches the obstacle lip with its visible top edge instead of its collision capsule");
    auto lungeLedgeGame=std::make_unique<Game>();lungeLedgeGame->reset();
    {
        GameState& ledge=const_cast<GameState&>(lungeLedgeGame->state());const RoomCollider& platform=ledge.roomColliders[0];
        ledge.player.pos={platform.maxX+0.34f,platform.topY-PHONE_MODEL_HEIGHT*0.5f,platform.center.z};
        ledge.player.vel={0,0,5.0f};ledge.player.jumpVel=-0.8f;ledge.player.grounded=false;
        ledge.meleeVisual.airLungeLandingPending=true;ledge.meleeVisual.locomotionLunge=true;
        ledge.meleeVisual.airLungeTimer=0.12f;ledge.meleeVisual.airLungeAngularVelocity=8.0f;
    }
    step(*lungeLedgeGame);
    ok &= expect(lungeLedgeGame->state().player.ledgeHanging&&
                 !lungeLedgeGame->state().meleeVisual.airLungeLandingPending&&
                 !lungeLedgeGame->state().meleeVisual.locomotionLunge&&
                 lungeLedgeGame->state().meleeVisual.landingRecovery<=0.0f&&
                 lungeLedgeGame->state().player.ledgeShimmySpeed>1.0f,
        "a descending locomotion lunge cancels into ledge grab and carries tangent speed into shimmy");
    const float shimmyStart=ledgeHangGame->state().player.pos.z;
    {GameState& ledge=const_cast<GameState&>(ledgeHangGame->state());ledge.camera.yaw=3.14159265f;}
    ledgeHangGame->setTouchControls(0,1,0,0,false,false,false,false,false,false);step(*ledgeHangGame,18);
    ok &= expect(ledgeHangGame->state().player.ledgeHanging&&ledgeHangGame->state().player.pos.z>shimmyStart+0.08f,
        "ledge shimmy accelerates laterally while retaining a bounded tactile momentum");
    {GameState& ledge=const_cast<GameState&>(ledgeHangGame->state());ledge.camera.yaw=kHalfPi;}
    ledgeHangGame->setTouchControls(0,1,0,0,false,false,true,false,false,false);step(*ledgeHangGame);
    ok &= expect(!ledgeHangGame->state().player.ledgeHanging&&ledgeHangGame->state().player.grounded&&ledgeHangGame->state().player.ledgeMantleTimer>0.0f&&ledgeHangGame->state().phonePose.actionState==9,
        "toward-plus-jump mantles onto the platform through the quick phone flip-up pose");
    auto ledgeVaultGame=std::make_unique<Game>();ledgeVaultGame->reset();
    {GameState& ledge=const_cast<GameState&>(ledgeVaultGame->state());const RoomCollider& platform=ledge.roomColliders[0];ledge.player.pos={platform.maxX+0.34f,platform.topY-PHONE_MODEL_HEIGHT*0.5f,platform.center.z};ledge.player.jumpVel=-0.8f;ledge.player.grounded=false;}
    step(*ledgeVaultGame);ledgeVaultGame->setTouchControls(0,0,0,0,false,false,true,false,false,false);step(*ledgeVaultGame);
    ok &= expect(!ledgeVaultGame->state().player.ledgeHanging&&!ledgeVaultGame->state().player.grounded&&ledgeVaultGame->state().player.jumpVel>3.5f&&ledgeVaultGame->state().player.ledgeGrabCooldown>0.0f,
        "neutral jump vaults outward with gravity-owned airtime and a short anti-loop regrab cooldown");
    auto ledgeAttackGame=std::make_unique<Game>();ledgeAttackGame->reset();
    {GameState& ledge=const_cast<GameState&>(ledgeAttackGame->state());const RoomCollider& platform=ledge.roomColliders[0];ledge.player.pos={platform.maxX+0.34f,platform.topY-PHONE_MODEL_HEIGHT*0.5f,platform.center.z};ledge.player.jumpVel=-0.8f;ledge.player.grounded=false;}
    step(*ledgeAttackGame);ledgeAttackGame->setTouchControls(0,0,0,0,false,false,false,true,false,false);step(*ledgeAttackGame);
    ok &= expect(!ledgeAttackGame->state().player.ledgeHanging&&ledgeAttackGame->state().meleeVisual.airLungeLandingPending&&ledgeAttackGame->state().player.jumpVel>0.0f,
        "attack from a ledge releases the phone into the existing gravity-owned locomotion lunge");
    ok &= expect(near(PHONE_BODY_WIDTH, 0.08f, 0.0001f) && near(PHONE_BODY_HEIGHT, 0.16f, 0.0001f) && near(PHONE_BODY_DEPTH, 0.012f, 0.0001f), "phone body dimensions match Pass 7 fallback/model normalized size");
    const float intactThinning=humanShellThinningAmount(2.0f,2.0f,false),criticalThinning=humanShellThinningAmount(0.1f,2.0f,false);
    int missingFixtureTriangles=0;for(std::size_t triangle=0;triangle<1000;++triangle)if(humanShellTriangleMissing(triangle,criticalThinning))++missingFixtureTriangles;
    ok &= expect(near(intactThinning,0.0f,0.0001f)&&criticalThinning>0.38f&&missingFixtureTriangles>350&&missingFixtureTriangles<500,
        "low armor deterministically exposes an obvious bounded share of shell triangles without affecting intact enemies");
    int missingNearCrit=0,missingAwayFromCrit=0;for(std::size_t triangle=0;triangle<1000;++triangle){if(humanShellTriangleMissingTowardCrit(triangle,criticalThinning,humanShellCritCenter()))++missingNearCrit;if(humanShellTriangleMissingTowardCrit(triangle,criticalThinning,{0.0f,0.05f,0.0f}))++missingAwayFromCrit;}
    ok &= expect(missingNearCrit>missingAwayFromCrit+250&&length(humanShellAbsorbTowardCrit({0.0f,0.05f,0.0f},7,criticalThinning)-humanShellCritCenter())<length(Vec3{0.0f,0.05f,0.0f}-humanShellCritCenter()),
        "damaged shell erosion and data channels converge visibly toward the collision-authoritative crit point");
    ok &= expect(near(Pass7Visual::CameraVerticalFovDegrees,75.0f,0.0001f) && near(Pass7Visual::CameraNearPlane,0.1f,0.0001f) && near(Pass7Visual::CameraFarPlane,1000.0f,0.0001f), "native projection matches the browser PerspectiveCamera contract");
    ok &= expect(near(spawn.camera.pos.y - spawn.player.pos.y, 1.1f, 0.0001f), "third-person camera height is relative to corrected player support");
    Game clearCameraGame; clearCameraGame.reset();
    { GameState& clear=const_cast<GameState&>(clearCameraGame.state());clear.debug.colliderCount=0;clear.player.pos={0.0f,PHONE_MODEL_HEIGHT*0.5f,0.0f};clear.camera.yaw=0;clear.camera.pitch=0; }
    step(clearCameraGame);const GameState clearCamera=clearCameraGame.state();
    ok &= expect(near(clearCamera.camera.pos.x-clearCamera.player.pos.x,0.0f,0.0001f) && near(clearCamera.camera.pos.z-clearCamera.player.pos.z,3.0f,0.0001f), "unobstructed third-person boom matches the browser three-unit aim-relative offset");
    ok &= expect(near(clearCamera.camera.lookTarget.y-clearCamera.player.pos.y,0.45f,0.0001f) && near(clearCamera.camera.lookTarget.z-clearCamera.player.pos.z,-10.0f,0.0001f), "third-person look target preserves browser aim direction and look lift");
    auto wallCameraGame=std::make_unique<Game>();wallCameraGame->reset();
    {GameState& fixture=const_cast<GameState&>(wallCameraGame->state());const RoomCollider& wall=fixture.roomColliders[0];fixture.debug.colliderCount=1;fixture.player.pos={wall.maxX+0.34f,PHONE_MODEL_HEIGHT*0.5f,wall.center.z};fixture.player.vel={};fixture.player.grounded=true;fixture.camera.yaw=kHalfPi;fixture.camera.pitch=0.0f;fixture.camera.firstPerson=false;}
    step(*wallCameraGame);
    ok &= expect(!wallCameraGame->state().camera.firstPerson&&horizontalSpeed(wallCameraGame->state().camera.pos-wallCameraGame->state().player.pos)>1.2f,
        "walking directly into a wall lets the third-person boom exit its initial padding instead of collapsing onto the phone");

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
    ok &= expect(game.state().cinematic.textInteraction>0.80f,
        "start interaction publishes a strong shared text wave-and-tracking impulse");
    step(game,70);
    ok &= expect(!game.state().cinematic.introActive && horizontalSpeed(game.state().camera.pos-game.state().player.pos)>2.45f,
        "product reveal hands off into the collision-safe gameplay chase camera before play");
    ok &= expect(length(game.state().player.pos-introLockedPosition)>0.001f,
        "direction held through the entrance becomes active on the first gameplay frame");
    ok &= expect(game.state().cinematic.textInteraction<0.01f,
        "floating text interaction expansion settles smoothly instead of remaining stretched");
    {GameState& pauseFixture=const_cast<GameState&>(game.state());pauseFixture.camera.firstPerson=true;}
    game.setUiPaused(true);const Vec3 pausedPosition=game.state().player.pos;const float pausedTime=game.state().time;const float pausedShotAge=game.state().environmentVisual.latestShotAge;game.setTouchControls(1,1,50,50,true,true,true,true,true,true);step(game,10);
    ok &= expect(game.state().uiPaused&&near(length(game.state().player.pos-pausedPosition),0.0f,0.0001f)&&!game.state().vacuum.active,
        "open native HUD pause freezes gameplay and releases held vacuum input");
    ok &= expect(near(game.state().time,pausedTime,0.0001f)&&near(game.state().environmentVisual.latestShotAge,pausedShotAge,0.0001f),
        "solo pause freezes the shared simulation and environment animation clocks");
    ok &= expect(game.state().camera.firstPerson&&game.state().phoneVisual.visible&&length(game.state().camera.pos-game.state().phoneTransform.position)<0.85f,
        "first-person pause preserves camera preference while framing a readable phone menu");
    Game onlineMenuGame;onlineMenuGame.reset();onlineMenuGame.configureNetworkHost();
    {GameState& online=const_cast<GameState&>(onlineMenuGame.state());online.targets[0].alive=true;online.targets[0].slurpable=false;online.targets[0].attackTimer=0.50f;online.enemyAttackOwner=0;}
    onlineMenuGame.setUiPaused(true);const float onlineAttackBefore=onlineMenuGame.state().targets[0].attackTimer;step(onlineMenuGame);
    ok &= expect(onlineMenuGame.state().uiPaused&&onlineMenuGame.state().targets[0].attackTimer<onlineAttackBefore,
        "connected menu releases local controls without freezing the authoritative match clock");
    game.setUiPaused(false);
    step(game);
    ok &= expect(game.state().camera.firstPerson,
        "resuming from the phone menu restores first-person gameplay without a camera-mode toggle");
    bool repeatedMenuTransitionsFinite=true;
    for(int cycle=0;cycle<3;++cycle){
        const Vec3 beforeEnter=game.state().phoneTransform.position;
        game.setUiPaused(true);step(game);
        repeatedMenuTransitionsFinite&=length(game.state().phoneTransform.position-beforeEnter)<0.65f;
        step(game,20);const Vec3 beforeExit=game.state().phoneTransform.position;
        game.setUiPaused(false);step(game);
        repeatedMenuTransitionsFinite&=length(game.state().phoneTransform.position-beforeExit)<0.65f;
        step(game,30);
    }
    ok &= expect(repeatedMenuTransitionsFinite&&game.state().phoneDisplay.presentationBlend<0.03f,
        "repeated pause transitions continue from the rendered phone pose and settle on one canonical gameplay presentation");
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
    ok &= expect(game.state().player.pos.y<=1.0f&&game.state().player.jumpVel<=0.0f,
        "holding jump into a wall no longer climbs, preserving ledges and lunges as navigation verbs");
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
    game.setPersistentProgression(0,0,0,0);
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
    auto lowAimLunge=std::make_unique<Game>(),highAimLunge=std::make_unique<Game>();lowAimLunge->reset();highAimLunge->reset();
    for(Game* fixture:{lowAimLunge.get(),highAimLunge.get()}){GameState& setup=const_cast<GameState&>(fixture->state());for(auto& target:setup.targets)target.alive=false;setup.player.pos={0,1.4f,0};setup.player.grounded=false;setup.player.jumpVel=-0.4f;setup.camera.yaw=0;}
    const_cast<GameState&>(lowAimLunge->state()).camera.pitch=0.0f;const_cast<GameState&>(highAimLunge->state()).camera.pitch=0.62f;
    lowAimLunge->setTouchControls(0,0,0,0,false,false,false,true,false,false);highAimLunge->setTouchControls(0,0,0,0,false,false,false,true,false,false);step(*lowAimLunge);step(*highAimLunge);
    ok &= expect(highAimLunge->state().player.jumpVel>lowAimLunge->state().player.jumpVel+2.4f&&highAimLunge->state().player.battery<lowAimLunge->state().player.battery-5.0f,
        "upward aim buys a higher physical lunge trajectory with a proportional battery surcharge");
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
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.player.battery=35.0f; setup.player.pos={0.0f,0.58f,0.0f}; setup.player.grounded=false;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true;
        target.pos={0.0f,0.08f,-0.20f}; target.armor=4.0f; target.scale=1.0f;
        setup.phoneTransform.position={target.pos.x,1.055f,target.pos.z};
    }
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false);step(game);
    ok &= expect(std::strstr(game.state().hud.energyTicker.data(),"HEADSHOT")!=nullptr &&
                 game.state().progression.run.headshotRechargeBoost>1.0f && hasAudioCue(game.state(),AudioCue::Headshot) && hasAudioCue(game.state(),AudioCue::RewardWoah),
        "an accurate airborne phone-body contact with the modeled head opens the precision recharge window");
    ok &= expect(!game.state().meleeVisual.airLungeLandingPending&&game.state().progression.run.lungeReboundTimer>1.0f,
        "a lunge headshot releases the committed arc and opens a bounded player-owned rebound window");
    const float reboundBattery=game.state().player.battery;
    {GameState& setup=const_cast<GameState&>(game.state());setup.targets[0].alive=false;}
    game.setKey(34,true);step(game);game.setKey(34,false);
    ok &= expect(game.state().meleeVisual.airLungeLandingPending&&game.state().progression.run.lungeReboundTimer<=0.0f&&reboundBattery-game.state().player.battery<3.0f&&!game.state().camera.firstPerson,
        "F retriggers the rebound lunge on demand and consumes the one-shot reduced battery cost");
    game.setKey(31,true);step(game);game.setKey(31,false);
    ok &= expect(game.state().camera.firstPerson,"C owns the remapped desktop first-person camera toggle");

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
    { GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=13.0f;setup.player.grounded=true;setup.player.jumpVel=0.0f;setup.player.vel={};setup.vacuum=VacuumState{};setup.progression.run.batteryRegenLock=0.0f; }
    step(game);
    { GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=99.4f;setup.player.grounded=true;setup.player.jumpVel=0.0f;setup.player.vel={};setup.progression.run.batteryRegenLock=0.0f; }
    step(game,2);
    ok &= expect(hasAudioCue(game.state(),AudioCue::ConnectPower),"recovery to full emits connect-power once after crossing the browser 14-percent arming threshold");

    game.reset();
    {GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=1.0f;setup.player.souls=0;setup.player.grounded=true;setup.player.jumpVel=0.0f;}
    game.setTouchControls(0,0,0,0,false,false,true,false,false,false);step(game);
    ok &= expect(game.state().dead&&!game.state().player.alive&&game.state().player.jumpVel==0.0f,
        "a ground jump that exhausts battery enters death without applying airborne movement");

    game.reset();
    {GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=1.0f;setup.player.souls=0;setup.player.grounded=false;setup.player.pos.y=1.0f;setup.player.jumpVel=0.0f;setup.player.airJumpsRemaining=1;}
    game.setTouchControls(0,0,0,0,false,false,true,false,false,false);step(game);
    ok &= expect(game.state().dead&&!game.state().player.alive&&game.state().player.jumpVel==0.0f&&game.state().player.airJumpsRemaining==1,
        "a double jump that exhausts battery cannot consume or apply its air impulse after death");

    game.reset();
    {GameState& setup=const_cast<GameState&>(game.state());setup.player.battery=1.0f;setup.player.souls=0;setup.player.grounded=false;setup.player.pos.y=1.0f;setup.player.jumpVel=0.0f;setup.meleeVisual.wallGripTimer=0.2f;setup.meleeVisual.wallNormal={1,0,0};}
    game.setTouchControls(0,0,0,0,false,false,true,false,false,false);step(game);
    ok &= expect(game.state().dead&&!game.state().player.alive&&game.state().player.jumpVel==0.0f&&horizontalSpeed(game.state().player.vel)==0.0f,
        "a wall jump that exhausts battery cannot apply its launch after death");

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
    ok &= expect(hasAudioCue(game.state(),AudioCue::ReceivedMessage)&&hasAudioCue(game.state(),AudioCue::RewardNice),
        "completed capture queues the received-message cue and lightweight engagement rehook");
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
    {
        GameState& setup=const_cast<GameState&>(game.state());setup.debug.colliderCount=0;
        setup.player.pos={0.0f,PHONE_MODEL_HEIGHT*0.5f,-20.90f};setup.player.vel={0,0,-14.0f};setup.player.grounded=true;
        for(auto& target:setup.targets)target.alive=false;
        TargetState& enemy=setup.targets[0];enemy=TargetState{};enemy.alive=true;enemy.pos={5.0f,PHONE_MODEL_HEIGHT*0.5f,0.0f};
        enemy.walkTarget=enemy.pos;enemy.armor=1.7f;enemy.attackCooldown=100.0f;
    }
    step(game);
    ok &= expect(game.state().topology.currentTileIndex==-1&&game.state().targets[0].alive&&
                 game.state().targets[0].pos.z<-21.0f&&near(game.state().targets[0].armor,1.7f,0.001f),
        "locked room looping re-anchors one canonical enemy simulation instead of spawning duplicate AI");
    ok &= expect(near(game.state().player.battery,84.0f,0.05f)&&game.state().progression.run.batteryRegenLock>1.20f,
        "locked room looping spends battery once and suppresses immediate regeneration");

    game.reset();
    game.setPersistentProgression(2,0,0,0);
    {
        GameState& setup=const_cast<GameState&>(game.state());setup.debug.colliderCount=0;
        setup.player.pos={0.0f,PHONE_MODEL_HEIGHT*0.5f,-20.90f};setup.player.vel={0,0,-14.0f};setup.player.grounded=true;setup.player.battery=20.0f;
    }
    step(game);
    ok &= expect(game.state().progression.permanent.tokens==1&&game.state().player.alive,
        "a low-battery locked loop spends exactly one emergency token");

    game.reset();
    game.setPersistentProgression(0,0,0,0);
    {
        GameState& setup=const_cast<GameState&>(game.state());setup.debug.colliderCount=0;
        setup.player.pos={0.0f,PHONE_MODEL_HEIGHT*0.5f,-20.90f};setup.player.vel={0,0,-14.0f};setup.player.grounded=true;setup.player.battery=20.0f;
    }
    step(game);
    ok &= expect(game.state().dead&&!game.state().player.alive,
        "a low-battery locked loop without an emergency token exhausts the run");

    game.reset();
    game.setPersistentProgression(0,0,0,0);
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
        ok &= expect(state.progression.permanent.tokens==firstRoomRequired,
            "each newly filled goal awards exactly one persistent token");
        ok &= expect(hasAudioCue(state,AudioCue::PaymentSuccess),
            "final room deposit queues the authoritative payment-success cue");
    }

    {
        Game secretWake;secretWake.reset();
        GameState& setup=const_cast<GameState&>(secretWake.state());
        setup.roomIndex=10;setup.requiredSouls=1;setup.roomClear=false;setup.captures[0].filled=true;setup.player.pos={0.0f,PHONE_MODEL_HEIGHT*0.5f,12.0f};
        step(secretWake);
        ok &= expect(secretWake.state().roomClear&&secretWake.state().secretTv.knockCueTimer>5.0f&&std::strstr(secretWake.state().hud.energyTicker.data(),"KNOCK")!=nullptr,
            "filling every level-ten capture point wakes the secret TV entrance instead of silently only opening the exit");
        step(secretWake);
        ok &= expect(secretWake.state().secretTv.available&&secretWake.state().secretTv.knockVolume>0.0f,
            "the awakened level-ten TV entrance becomes audible and discoverable on the following desktop frame");
        {GameState& enter=const_cast<GameState&>(secretWake.state());enter.player.pos=enter.secretTv.entrancePos;enter.player.vel={};enter.player.grounded=true;}
        step(secretWake);
        ok &= expect(secretWake.state().player.inSecretRoom,
            "a grounded level-ten player can enter the secret TV room through the awakened membrane");
        ok &= expect(secretWake.state().player.secretVisitTimer>100.0f&&secretWake.state().camera.pos.x>36.8f,
            "secret TV entry gives the player a readable visit window and keeps the camera in the off-map room");
    }

    {
        GameState& setup=const_cast<GameState&>(game.state());
        const int previousRoom=setup.roomIndex;
        const int previousSeed=setup.roomSeed;
        const Vec3 previousFirstObstacle=setup.roomColliders[0].center;
        setup.player.pos={0,PHONE_MODEL_HEIGHT*0.5f,-20.8f};
        setup.player.vel={0,0,-20.0f};
        step(game,2);
        const GameState& state=game.state();
        int filled=0; for(const auto& capture:state.captures) if(capture.filled) ++filled;
        ok &= expect(state.roomIndex==previousRoom+1 && !state.roomClear && filled==0,
            "crossing the opened doorway advances the room and resets its goal inventory");
        ok &= expect(state.roomSeed!=previousSeed&&length(state.roomColliders[0].center-previousFirstObstacle)>0.01f,
            "open-door advancement generates the next seeded obstacle layout only at ownership transfer");
        const int ruleStacks=state.runRules.requiredSlotStacks+state.runRules.crowdedRoomStacks+state.runRules.fasterSlurpStacks;
        ok &= expect(ruleStacks==1 && state.runRules.lastAdded>=0,
            "room advancement deterministically adds one eligible browser run rule");
        ok &= expect(state.requiredSouls==5+state.runRules.requiredSlotStacks,
            "required-slot run rules rebuild the following room goal inventory");
        ok &= expect(state.doorTransition.active && state.doorTransition.progress>0.75f,
            "open-door crossing starts the browser distance-owned datamosh transition state");
        ok &= expect(state.upgradeMenu.active&&state.uiPaused,
            "room advancement enters one bounded upgrade choice beat before the next round");
    }
    const auto tokensBeforeShop=game.state().progression.permanent.tokens;
    ok &= expect(game.purchasePermanentUpgrade(static_cast<int>(UpgradeTrack::Shot))&&game.state().progression.permanent.tokens==tokensBeforeShop-1&&game.state().progression.permanent.levels[0]==1,
        "the inter-round permanent shop converts exactly one earned goal token into one level");
    ok &= expect(game.chooseTemporaryUpgrade(static_cast<int>(UpgradeTrack::Lunge))&&!game.state().upgradeMenu.active&&!game.state().uiPaused&&game.state().progression.run.temporaryLevels[1]==1,
        "choosing one free run upgrade smoothly resumes the next room");
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
        setup.roomIndex=256;setup.progression.run.roomHeat=1.0f;
        for(auto& target:setup.targets)target=TargetState{};
        TargetState& enemy=setup.targets[0];enemy.alive=true;enemy.pos={10.0f,0.08f,8.0f};enemy.walkTarget=enemy.pos;
        enemy.armor=1.0f;enemy.armorRegenDelay=0.0f;enemy.attackCooldown=100.0f;
    }
    const float armorBeforeInfiniteRegen=game.state().targets[0].armor;
    step(game,60);
    ok &= expect(game.state().targets[0].armor>armorBeforeInfiniteRegen&&game.state().targets[0].armor<=2.0f,
        "deep-room enemy regeneration scales through bounded arithmetic without exceeding shell armor");
    {
        GameState& setup=const_cast<GameState&>(game.state());
        setup.targets[0].armor=1.0f;setup.targets[0].armorRegenDelay=1.0f;
    }
    step(game,30);
    ok &= expect(near(game.state().targets[0].armor,1.0f,0.001f),
        "recent shell damage pauses regeneration so rhythmic headshot guarantees remain stable");

    game.reset();
    game.setPersistentProgression(0,0,0,0);
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
        near(game.state().targets[1].armor,4.0f,0.001f) && game.state().bullets[0].alive &&
        game.state().bullets[0].contactCooldown>0.0f &&
        std::strstr(game.state().hud.energyTicker.data(),"HEADSHOT")==nullptr,
        "normal fired cube damages only the first living shell on its swept path and remains live");

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
    ok &= expect(near(game.state().targets[0].armor,2.35f,0.001f) && game.state().bullets[0].alive,
        "brute fired cube uses the wider 0.95 shell radius and remains live after 1.65 damage");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());
        for(auto& target:setup.targets) target.alive=false;
        setup.player.battery=35.0f;
        TargetState& target=setup.targets[0]; target=TargetState{}; target.alive=true;
        target.armor=1.0f; target.pos={setup.player.pos.x,0.08f,setup.player.pos.z-4.0f}; target.walkTarget=target.pos;
        BulletState& bullet=setup.bullets[0]; bullet=BulletState{}; bullet.alive=true; bullet.life=1.0f;
        bullet.pos={target.pos.x,1.055f,target.pos.z+0.50f}; bullet.vel={0,0,-25.0f};
    }
    step(game);
    ok &= expect(std::strstr(game.state().hud.energyTicker.data(),"HEADSHOT")!=nullptr &&
                 game.state().progression.run.headshotRechargeBoost>1.0f && game.state().bullets[0].alive &&
                 hasAudioCue(game.state(),AudioCue::HeadshotCritical) && hasAudioCue(game.state(),AudioCue::RewardWoah),
        "a final-hit-band soul headshot opens recharge and selects the two-step critical jingle");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());for(auto& target:setup.targets)target.alive=false;
        setup.requiredSouls=3;TargetState& target=setup.targets[0];target=TargetState{};target.alive=true;
        target.armor=2.0f;target.pos={setup.player.pos.x,0.08f,setup.player.pos.z-4.0f};target.walkTarget=target.pos;target.attackCooldown=999.0f;
    }
    bool exactHeadshotFractions=true;
    for(int shot=1;shot<=3;++shot){
        GameState& setup=const_cast<GameState&>(game.state());TargetState& target=setup.targets[0];target.pos={setup.player.pos.x,0.08f,setup.player.pos.z-4.0f};target.walkTarget=target.pos;
        BulletState& bullet=setup.bullets[0];bullet=BulletState{};bullet.alive=true;bullet.life=1.0f;bullet.pos={target.pos.x,1.055f,target.pos.z+0.50f};bullet.vel={0,0,-25.0f};
        step(game);
        if(shot<3)exactHeadshotFractions=exactHeadshotFractions&&!game.state().targets[0].slurpable&&near(game.state().targets[0].armor,2.0f*(3-shot)/3.0f,0.001f);
    }
    const int headshotJingles=countAudioCue(game.state(),AudioCue::Headshot)+countAudioCue(game.state(),AudioCue::HeadshotCritical);
    ok &= expect(exactHeadshotFractions&&game.state().targets[0].slurpable&&headshotJingles==3,
        "a three-slot room takes exactly three headshots to break a fresh shell and plays all three jingles");
    ok &= expect(game.state().progression.run.accuracyStacks==3&&near(game.state().progression.run.accuracyMultiplier,1.24f,0.001f)&&game.state().hud.headshotPulse>0.8f,
        "consecutive accurate headshots build the bounded percentage chain and shared tactile pulse");
    ok &= expect(game.state().progression.run.headshotRegenTax>0.34f&&game.state().progression.run.headshotRegenTax<=0.65f,
        "rapid headshots trade their immediate battery reward for a bounded passive-regeneration tax");
    step(game,200);
    ok &= expect(game.state().progression.run.accuracyStacks==0&&near(game.state().progression.run.accuracyMultiplier,1.0f,0.001f)&&near(game.state().progression.run.headshotRegenTax,0.0f,0.001f),
        "an idle accuracy chain and regeneration tax cool off cleanly instead of scaling without bound");

    game.reset();
    {
        GameState& setup=const_cast<GameState&>(game.state());for(auto& target:setup.targets)target.alive=false;
        setup.requiredSouls=4;TargetState& target=setup.targets[0];target=TargetState{};target.alive=true;target.brute=true;target.scale=1.7f;
        target.armor=4.0f;target.pos={setup.player.pos.x,0.08f,setup.player.pos.z-4.0f};target.walkTarget=target.pos;target.attackCooldown=999.0f;
    }
    for(int shot=0;shot<4;++shot){GameState& setup=const_cast<GameState&>(game.state());TargetState& target=setup.targets[0];target.pos={setup.player.pos.x,0.08f,setup.player.pos.z-4.0f};target.walkTarget=target.pos;BulletState& bullet=setup.bullets[0];bullet=BulletState{};bullet.alive=true;bullet.life=1.0f;bullet.pos={target.pos.x,1.7935f,target.pos.z+0.50f};bullet.vel={0,0,-25.0f};step(game);}
    ok &= expect(game.state().targets[0].slurpable,
        "the same slot-count headshot guarantee applies to the brute shell's larger armor pool");

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
    ok &= expect(near(game.state().energy.supplementalValue,85.0f-7.0f,0.0002f),
        "stored souls no longer discount native supplemental discharge cost");

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
    int shellFragments=0;for(const auto& particle:game.state().particles)if(particle.kind==1&&particle.life>0.0f)++shellFragments;
    ok &= expect(game.state().targets[0].slurpable && droppedFlowers==1 && dropDistance>=1.05f && shellFragments==48,
        "brute shell conversion performs one drop roll and separates the flower from its source");
    ok &= expect(game.state().targets[0].soulMorph<0.10f,
        "shell shatter begins while the exposed soul still follows its independent emergence timing");
    game.setTouchControls(0,0,0,0,false,false,false,false,false,false); step(game,20);
    game.setTouchControls(0,0,0,0,false,false,false,true,false,false); step(game);
    int repeatedDrops=0; for(const auto& flower:game.state().flowers) if(flower.active) ++repeatedDrops;
    ok &= expect(repeatedDrops==1,
        "subsequent hits on an already slurpable brute do not roll duplicate flowers");
    step(game,70);int lingeringShellFragments=0;for(const auto& particle:game.state().particles)if(particle.kind==1&&particle.life>0.0f)++lingeringShellFragments;
    ok &= expect(lingeringShellFragments==0,
        "shattered shell fragments settle, shrink, and are fully reabsorbed into the floor");

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

    game.reset();
    step(game);
    {
        GameState& setup=const_cast<GameState&>(game.state());
        FlowerPowerupState& flower=setup.flowers[0];flower.active=true;flower.baseY=setup.phoneTransform.vacuumPullPoint.y;
        const Vec3 offered=setup.phoneTransform.vacuumPullPoint+setup.camera.forward*5.0f;
        flower.pos=offered;
    }
    game.setTouchControls(0,0,0,0,true,false,false,false,false,false);
    step(game,12);
    const float heavyFlowerSpeed=length(game.state().flowers[0].vacuumVelocity);
    ok &= expect(game.state().flowers[0].active&&game.state().flowers[0].vacuumAttracted&&heavyFlowerSpeed>0.05f&&heavyFlowerSpeed<=3.6f,
        "flower enters the shared vacuum offer cone with a bounded heavy-body response");
    step(game,180);
    ok &= expect(!game.state().flowers[0].active&&game.state().energy.flowerStacks==1,
        "continued vacuum attraction captures the heavy flower without changing its powerup semantics");

    std::cout << "numeric forward0=(" << forward0.player.vel.x << "," << forward0.player.vel.z << ")"
              << " forward90=(" << forward90.player.vel.x << "," << forward90.player.vel.z << ")"
              << " rotateWhileMoving=(" << rotateWhileMoving.player.vel.x << "," << rotateWhileMoving.player.vel.z << ")"
              << " diagonalNorm=(" << diagonal.player.vel.x / diagSpeed << "," << diagonal.player.vel.z / diagSpeed << ")"
              << " firstPersonYaw=" << firstPerson.player.yaw << "\n";
    return ok ? 0 : 1;
}
