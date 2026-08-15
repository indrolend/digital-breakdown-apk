#include "Game.hpp"
#include "gameplay/PhoneBody.hpp"
#include "gameplay/SoulMotion.hpp"
#include "gameplay/TargetRoles.hpp"
#include "gameplay/TraversalCapabilities.hpp"
#include "EarlyBrowserVisuals.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

namespace {
constexpr float ROOM_WIDTH = 30.0f;
constexpr float ROOM_DEPTH = 42.0f;
constexpr float ROOM_WALL_HEIGHT = 7.2f;
constexpr float GROUND_Y = 0.08f;
constexpr float ROOM_EXIT_Z = -ROOM_DEPTH * 0.5f + 1.15f;
constexpr float ROOM_START_Z = ROOM_DEPTH * 0.5f - 5.5f;
constexpr float ROOM_GRID_Z = ROOM_EXIT_Z + 0.42f;
constexpr float ROOM_MIN_SPAWN_Z = -ROOM_DEPTH * 0.5f + 9.0f;
constexpr float ROOM_MAX_SPAWN_Z = ROOM_DEPTH * 0.5f - 7.0f;

constexpr float WALK_ACCEL = 16.0f;
constexpr float RUN_ACCEL = 42.0f;
constexpr float WALK_MAX_SPEED = 18.0f;
constexpr float RUN_MAX_SPEED = 42.0f;
constexpr float GROUND_FRICTION = 0.88f;
constexpr float AIR_FRICTION = 0.985f;
constexpr float AIR_ACCEL_MULT = 0.62f;
constexpr float AIR_MAX_SPEED_MULT = 1.08f;
constexpr float WALL_SLIDE_RETENTION = 0.94f;
constexpr float GRAVITY = gameplay::TRAVERSAL_CAPABILITIES.gravity;
constexpr float JUMP_SPEED = gameplay::TRAVERSAL_CAPABILITIES.groundJumpSpeed;
constexpr float AIR_JUMP_SPEED = gameplay::TRAVERSAL_CAPABILITIES.airJumpSpeed;
constexpr float COYOTE_TIME = 0.12f;
constexpr float JUMP_BUFFER = 0.12f;
constexpr float LANDING_MOMENTUM_BOOST = 1.04f;
constexpr float CEILING_CLEARANCE = 0.42f;
constexpr float PLAYER_CEILING_BODY_CLEARANCE = gameplay::PHONE_BODY.ceilingClearance;
constexpr float PLAYER_COLLISION_RADIUS = gameplay::PHONE_BODY.collisionRadius;
constexpr float PLAYER_WALL_MARGIN = gameplay::PHONE_BODY.wallMargin;
// Side collision remains generous and game-feeling; floor support follows the
// phone's visible footprint so ledges do not grow an invisible shelf.
constexpr float PLAYER_SUPPORT_RADIUS = gameplay::PHONE_BODY.supportRadius;
constexpr float LEDGE_GRAB_VERTICAL_BELOW = gameplay::PHONE_BODY.ledgeGrabVerticalBelow;
constexpr float LEDGE_GRAB_VERTICAL_ABOVE = gameplay::PHONE_BODY.ledgeGrabVerticalAbove;
constexpr float LEDGE_GRAB_REACH = gameplay::PHONE_BODY.ledgeGrabReach;
constexpr float LEDGE_PHONE_FACE_GAP = gameplay::PHONE_BODY.ledgeFaceGap;
constexpr float LEDGE_CORNER_INSET = gameplay::PHONE_BODY.ledgeCornerInset;
constexpr float LEDGE_SHIMMY_ACCEL = 14.0f;
constexpr float LEDGE_SHIMMY_MAX_SPEED = 2.2f;
constexpr float LEDGE_SHIMMY_DAMPING = 0.82f;
constexpr float LEDGE_REGRAB_COOLDOWN = 0.28f;
constexpr float LEDGE_VAULT_UP_SPEED = 4.2f;
constexpr float LEDGE_VAULT_OUT_SPEED = 2.4f;
constexpr float LEDGE_MANTLE_DURATION = 0.26f;
constexpr float CAMERA_COLLISION_RADIUS = gameplay::PHONE_BODY.cameraCollisionRadius;
constexpr float CAMERA_COLLISION_BACKOFF = gameplay::PHONE_BODY.cameraCollisionBackoff;
constexpr float INTRO_CAMERA_DURATION = 1.15f;
constexpr float ATTRACT_EXIT_DURATION = 0.62f;
constexpr float MENU_ENTER_FADE_DURATION = 0.48f;
constexpr float DEATH_CAMERA_DURATION = 1.35f;
constexpr float MENU_EXIT_CAMERA_DURATION = 0.42f;
constexpr float MENU_CAMERA_VERTICAL_FOV = 42.0f;
constexpr float MENU_PHONE_VIEWPORT_HEIGHT = 0.80f;
constexpr float DEATH_PRESENTATION_SCALE = 0.18f;


constexpr float VACUUM_MOVE_MULT = 0.35f;
constexpr float VACUUM_CHARGE_SPEED = 3.5f;
constexpr float VACUUM_DECAY_SPEED = 6.0f;
constexpr float SOUL_ATTRACTION_RANGE = 15.5f;
constexpr float SOUL_ATTRACTION_CONE_RADIUS = 2.35f;
constexpr float SOUL_CAPTURE_CYLINDER_RADIUS = 1.75f;
constexpr float SOUL_CAPTURE_CYLINDER_HEIGHT = 2.25f;
constexpr float SOUL_LATCH_DISTANCE = 0.48f;
constexpr float SOUL_SEAL_DISTANCE = 0.14f;
constexpr float SOUL_RECOIL_DURATION = 0.55f;
constexpr float SOUL_CAPTURE_COMMIT_PHASE = 0.92f;
constexpr float SOUL_CAPTURE_DECAY = 0.75f;
constexpr float SOUL_MORPH_DURATION = 0.72f;
constexpr float SOUL_ARMOR_NORMAL = 2.0f;
constexpr float SOUL_ARMOR_BRUTE = 4.0f;
constexpr float SCREEN_HALF_WIDTH = PHONE_SCREEN_WIDTH * 0.5f;
constexpr float SCREEN_HALF_HEIGHT = PHONE_SCREEN_HEIGHT * 0.5f;
constexpr float SCREEN_FRONT_OFFSET = 0.018f;
constexpr float SOUL_SEAL_BODY_OFFSET = 0.36f;
constexpr float PHONE_SOLID_HALF_X = PHONE_BODY_WIDTH * 0.5f;
constexpr float PHONE_SOLID_HALF_Y = PHONE_BODY_HEIGHT * 0.5f;
constexpr float PHONE_SOLID_HALF_Z = PHONE_BODY_DEPTH * 0.5f;
constexpr float SOUL_CORE_SOLID_RADIUS = 0.33f;
constexpr float HUMAN_SCALE_BRUTE = 1.7f;
constexpr float HUMAN_WALK_PHASE_PER_METER = 7.5f;
constexpr float HUMAN_WALK_SPEED = 0.72f;
constexpr float HUMAN_WALK_TARGET_RADIUS = 0.55f;
constexpr float HUMAN_WALK_RANGE = 5.5f;
constexpr float HUMAN_ATTACK_NOTICE_RANGE = 5.6f;
constexpr float HUMAN_ATTACK_START_RANGE = 1.55f;
constexpr float HUMAN_ATTACK_HIT_RANGE = 1.85f;
constexpr float HUMAN_ATTACK_DURATION = HUMAN_SWING_ATTACK_DURATION;
constexpr float HUMAN_ATTACK_COOLDOWN = 1.15f;
constexpr float HUMAN_ATTACK_KNOCKBACK = 3.0f;
constexpr float HUMAN_ATTACK_BATTERY_COST = 26.0f;
constexpr float BATTERY_IDLE_REGEN = 22.0f;
constexpr float BATTERY_WALK_DRAIN = 0.45f;
constexpr float BATTERY_SPRINT_DRAIN = 3.0f;
constexpr float BATTERY_AIR_DRAIN = 0.9f;
constexpr float BATTERY_VACUUM_DRAIN = 1.35f;
constexpr float BATTERY_JUMP_COST = 3.0f;
constexpr float BATTERY_DOUBLE_JUMP_COST = 6.0f;
constexpr float BATTERY_SHOOT_COST = 7.0f;
constexpr float BATTERY_CAPTURE_GAIN = 10.0f;
constexpr float BATTERY_SOUL_EFFICIENCY = 0.16f;
constexpr float BATTERY_MELEE_HIT_GAIN = 2.0f;
constexpr float BATTERY_COMBO_GROWTH = 1.22f;
constexpr float BATTERY_COMBO_TIMEOUT = 1.8f;
constexpr float MULTI_HIT_BONUS_GROWTH = 1.12f;
constexpr float MULTI_HIT_BATTERY_BONUS = 4.5f;
constexpr float FLOWER_ATTACK_FEED = 2.8f;
constexpr float FLOWER_SLURP_FEED = 9.0f;
constexpr float FLOWER_PICKUP_VALUE = 46.0f;
constexpr float FLOWER_DROP_CHANCE = 0.26f;
constexpr float FLOWER_PICKUP_RADIUS = 1.05f;

bool localPhoneMenuPresentation(const GameState& state) {
    return (((!state.started && !state.dead) || state.cinematic.introActive || state.uiPaused) &&
        !state.multiplayer.enabled && !state.upgradeMenu.active);
}

constexpr float FLOWER_BOB_HEIGHT = 0.16f;
constexpr float FLOWER_POWERUP_GROUND_Y = 0.38f;
constexpr float FLOWER_DROP_SEPARATION_RADIUS = 1.15f;
constexpr float FLOWER_DROP_MIN_SPACING = 1.05f;
constexpr float POWERUP_STOCK_BASE_MAX = 85.0f;
constexpr float POWERUP_STOCK_PER_STACK = 32.0f;
constexpr float TARGET_HITFLASH_DECAY_PER_FRAME = 0.045f;
constexpr float VACUUM_DAMAGE = 0.28f;

constexpr float MELEE_COMBO_WINDOW = 0.720f;
constexpr float AIR_MELEE_LATERAL_RETENTION = 0.52f;
constexpr float AIR_MELEE_LOCOMOTION_DURATION = 0.68f;
constexpr float AIR_MELEE_LOCOMOTION_DISTANCE = gameplay::TRAVERSAL_CAPABILITIES.airLungeDistance;
constexpr float AIR_MELEE_PHONE_RADIUS = gameplay::PHONE_BODY.airMeleeRadius;
constexpr float AIR_MELEE_BODY_FORGIVENESS = gameplay::PHONE_BODY.airMeleeBodyForgiveness;
constexpr float HEADSHOT_BATTERY_GAIN = 3.0f;
constexpr float PASSIVE_RECHARGE_DELAY = 0.80f;
constexpr float HEADSHOT_RECHARGE_BOOST_SECONDS = 1.35f;
// The rendered head moves around the normalized bind-pose center as the human
// walks, recoils, and swings.  Keep the precision target centered on the face,
// but give each input method enough margin to agree with that moving silhouette.
constexpr float LUNGE_HEAD_CONTACT_RADIUS = 0.36f;
constexpr float BULLET_HEAD_CONTACT_RADIUS = 0.25f;
constexpr float HEAD_CONTACT_NECK_FRACTION = 0.62f;
constexpr float HEADSHOT_CRITICAL_ARMOR_FRACTION = 0.60f;
constexpr float HEADSHOT_PARRY_EARLY_PHASE = 0.22f;
constexpr float HEADSHOT_PARRY_LATE_PHASE = 0.46f;
constexpr float ACCURACY_STACK_BONUS = 0.08f;
constexpr int ACCURACY_STACK_CAP = 15;
constexpr float ACCURACY_CHAIN_TIMEOUT = 3.2f;
constexpr int SYNERGY_LEVEL_STEP = 2;
constexpr int RELAY_PRIMER_STACK_CAP = 5;
constexpr float RELAY_PRIMER_SECONDS = 3.2f;
constexpr float RELAY_PRIMER_DAMAGE_PER_STACK = 0.12f;
constexpr float IMPACT_GUARD_SECONDS = 0.42f;
constexpr float LAST_STAND_COOLDOWN = 18.0f;
constexpr float AIR_MELEE_ANGULAR_VELOCITY = 4.4f;
constexpr float AIR_MELEE_ANGULAR_DAMPING = 2.2f;
constexpr float AIR_MELEE_CAMERA_RESPONSE = 13.5f;
constexpr float AIR_MELEE_CAMERA_RECOVERY_RESPONSE = 20.0f;
constexpr float AIR_MELEE_CAMERA_LAG_DECAY = 4.2f;
constexpr float AIR_MELEE_CAMERA_MAX_ERROR = 1.35f;
constexpr float AIR_MELEE_LANDING_RETENTION = 0.82f;
constexpr float AIR_MELEE_WALL_GRIP_TIME = 0.10f;
constexpr float LUNGE_HEADSHOT_REBOUND_WINDOW = 1.10f;
constexpr float LUNGE_HEADSHOT_REBOUND_COST_MULT = 0.55f;
struct MeleeCombo { int variant; float range, damage, hitRadius, visual, dash, dashSpeed, cooldown, recoilDistance, recoilSpeed, lunge, cost; };
constexpr MeleeCombo MELEE_COMBOS[] = {
    {0,2.35f,0.82f,0.78f,0.20f,0.13f,12.5f,0.22f,0.08f,1.25f,0.15f,2.8f},
    {1,2.85f,1.08f,0.90f,0.25f,0.18f,14.0f,0.27f,0.12f,1.75f,0.22f,3.6f},
    {2,3.18f,1.48f,1.02f,0.31f,0.23f,15.2f,0.38f,0.15f,2.10f,0.29f,5.0f},
    {3,3.00f,1.22f,0.96f,0.29f,0.20f,13.8f,0.34f,0.12f,1.80f,0.25f,4.2f}
};
constexpr float MELEE_VARIANT_SIDE[] = {1,-1,1,-1};
constexpr float MELEE_VARIANT_ROLL[] = {-0.72f,0.72f,-0.42f,0.42f};
constexpr float MELEE_VARIANT_YAW[] = {0.62f,-0.62f,0.42f,-0.42f};
constexpr float MELEE_VARIANT_PITCH[] = {-0.32f,-0.32f,0.42f,0.42f};
constexpr float MELEE_VARIANT_LIFT[] = {0.012f,0.012f,-0.006f,-0.006f};
constexpr float BULLET_SPEED = 25.0f;
constexpr float BULLET_BRUTE_SPEED = 20.0f;
constexpr float BULLET_GRAVITY = 11.5f;
constexpr float BULLET_LIFE = 3.25f;
constexpr float BULLET_VERTICAL_LIFT = 1.15f;
constexpr float BULLET_AIR_DRAG_PER_SECOND = 0.72f;
constexpr float BULLET_MAX_UP_AIM = 0.30f;
constexpr float BULLET_MAX_DOWN_AIM = -0.10f;
constexpr float ROOM_DEPOSIT_HIT_RADIUS = 1.65f;
constexpr int MAX_STORED_SOULS = 30;
constexpr int ACTIVE_HUMAN_TARGET = 5;
constexpr int ACTIVE_HUMAN_TARGET_CAP = 20;
constexpr float HUMAN_RESPAWN_DELAY_MIN = 1.45f;
constexpr float HUMAN_RESPAWN_DELAY_MAX = 2.35f;
constexpr float ENEMY_ARMOR_REGEN_DELAY = 2.40f;
constexpr float ROOM_HEAT_SECONDS = 90.0f;
constexpr float DOOR_DATAMOSH_DISTANCE = 3.0f;
constexpr float DOOR_DATAMOSH_MIN_STRENGTH = 0.018f;

constexpr float PHONE_GAIT_WALK_PITCH = 0.34f;
constexpr float PHONE_GAIT_RUN_PITCH = 0.88f;
constexpr float PHONE_GAIT_WALK_ROLL = 0.24f;
constexpr float PHONE_GAIT_RUN_ROLL = 0.66f;
constexpr float PHONE_GAIT_WALK_YAW = 0.18f;
constexpr float PHONE_GAIT_RUN_YAW = 0.52f;
constexpr float PHONE_GAIT_FORWARD_OFFSET = 0.052f;
constexpr float PHONE_GAIT_SIDE_OFFSET = 0.062f;
constexpr float PHONE_GAIT_LIFT = 0.065f;
constexpr float PHONE_GAIT_OBLIQUE_SWEEP = 0.58f;
constexpr float PHONE_GAIT_CONE_TWIST = 0.34f;
constexpr float PHONE_GAIT_OLOID_MEANDER = 0.045f;
constexpr float PHONE_GAIT_CYLINDER_RADIUS = 0.36f;

constexpr int KEY_W = 51;
constexpr int KEY_A = 29;
constexpr int KEY_S = 47;
constexpr int KEY_D = 32;
constexpr int KEY_Q = 45;
constexpr int KEY_C = 31;
constexpr int KEY_V = 50;
constexpr int KEY_F = 34;
constexpr int KEY_SHIFT_LEFT = 59;
constexpr int KEY_SHIFT_RIGHT = 60;
constexpr int KEY_SPACE = 62;
constexpr int KEY_ENTER = 66;
constexpr int KEY_R = 46;
constexpr int KEY_1 = 8;
constexpr int KEY_4 = 11;
constexpr int TOUCH_DOWN = 0;
constexpr int TOUCH_UP = 1;
constexpr int TOUCH_MOVE = 2;
constexpr int TOUCH_CANCEL = 3;

float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float dotXZ(const Vec3& a, const Vec3& b) { return a.x * b.x + a.z * b.z; }
float batteryPower(const PlayerState& p) { return clampf(p.battery / 18.0f, 0.35f, 1.0f); }
float smooth01(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
void syncTargetReactionVisual(TargetState& target) {
    target.visualReaction = makeHumanReactionVisual(
        target.visualWalkPhase,
        target.locomotionAmount,
        target.hitFlash,
        target.hitDirectionLocal,
        target.vacuumPullAmount,
        target.captureCollapseAmount,
        target.soulMorph,
        target.visibility > 0.5f,
        target.attackTimer,
        target.attackVariant
    );
}
float smoothRange(float value,float edge0,float edge1){return smooth01((value-edge0)/std::max(0.0001f,edge1-edge0));}
float secretDoorKnockPulse(float time) {
    constexpr float phraseSeconds = 11.8f;
    constexpr float audibleSeconds = 5.4f;
    constexpr float hits[] = {0.18f,0.54f,1.10f,2.05f,2.31f,3.18f,4.42f};
    float phrase = std::fmod(std::max(0.0f, time), phraseSeconds);
    if (phrase > audibleSeconds) return 0.0f;
    float pulse = 0.0f;
    for (float hit : hits) {
        const float d = std::abs(phrase - hit);
        pulse = std::max(pulse, std::exp(-d * d * 120.0f));
    }
    return clampf(pulse, 0.0f, 1.0f);
}
bool secretDoorKnockPhraseActive(float time) {
    constexpr float phraseSeconds = 11.8f;
    constexpr float audibleSeconds = 5.4f;
    return std::fmod(std::max(0.0f, time), phraseSeconds) <= audibleSeconds;
}
Vec3 latticeRest(int index){const int z=index/9,y=(index-z*9)/3,x=index%3;return{static_cast<float>(x)*0.23f-0.23f,static_cast<float>(y)*0.23f-0.23f,static_cast<float>(z)*0.23f-0.23f};}
float latticeSurface(int index){const int z=index/9,y=(index-z*9)/3,x=index%3;return(x==0||x==2||y==0||y==2||z==0||z==2)?1.0f:0.0f;}
float latticeCorner(int index){const int z=index/9,y=(index-z*9)/3,x=index%3;return static_cast<float>((x==0||x==2)+(y==0||y==2)+(z==0||z==2))/3.0f;}
void springScalar(float value,float velocity,float target,float frequency,float damping,float dt,float& newValue,float& newVelocity){const float omega=frequency*DB_PI*2.0f,f=1+2*dt*damping*omega,oo=omega*omega,hoo=dt*oo,hhoo=dt*hoo,detInv=1/(f+hhoo);newValue=(f*value+dt*velocity+hhoo*target)*detInv;newVelocity=(velocity+hoo*(target-value))*detInv;}
float pointSegmentDistanceSq(const Vec3& point, const Vec3& a, const Vec3& b) {
    const Vec3 ab=b-a, ap=point-a;
    const float denom=std::max(lengthSq(ab),0.0001f);
    const float t=clampf((ap.x*ab.x+ap.y*ab.y+ap.z*ab.z)/denom,0.0f,1.0f);
    return lengthSq(point-(a+ab*t));
}

void syncSoulVisual(TargetState& target, float time) {
    target.soulVisual = makeSoulVisualState(
        static_cast<int>(target.soulState), target.vacuumPullAmount, target.ingestProgress,
        target.hitFlash, time, target.phase, target.alive && target.slurpable,
        target.soulMorph, target.floatOffset, target.spinSpeed);
}

PhoneDisplayMode phoneDisplayModeForState(const GameState& state) {
    if (state.dead) return PhoneDisplayMode::Off;
    if (state.upgradeMenu.active) return PhoneDisplayMode::Upgrade;
    if (state.cinematic.introActive) return PhoneDisplayMode::Boot;
    if (state.started && state.uiPaused) return PhoneDisplayMode::Pause;
    if (!state.started) {
        switch (state.localSettings.menuPage) {
            case LocalMenuPage::Main: return PhoneDisplayMode::MainMenu;
            case LocalMenuPage::Online: return PhoneDisplayMode::Online;
            case LocalMenuPage::JoinCode: return PhoneDisplayMode::JoinCode;
            case LocalMenuPage::Settings: return PhoneDisplayMode::Settings;
            case LocalMenuPage::Controls: return PhoneDisplayMode::Controls;
            case LocalMenuPage::Audio: return PhoneDisplayMode::Audio;
            case LocalMenuPage::Graphics: return PhoneDisplayMode::Graphics;
        }
    }
    return state.hud.lowBattery ? PhoneDisplayMode::Warning : PhoneDisplayMode::Gameplay;
}

float finiteClamped(float value, float lo, float hi) {
    if (!std::isfinite(value)) return lo;
    return clampf(value, lo, hi);
}

Vec3 mix3(const Vec3& a, const Vec3& b, float t) {
    return a * (1.0f - t) + b * t;
}
}

void Game::reset() {
    const PermanentProgressionState permanent=state_.progression.permanent;
    const LocalSettingsState localSettings=state_.localSettings;
    auto freshState=std::make_unique<GameState>();
    state_ = std::move(*freshState);
    state_.progression.permanent=permanent;
    state_.localSettings=localSettings;
    state_.localSettings.menuPage=LocalMenuPage::Main;
    state_.localSettings.menuScroll=0.0f;
    state_.localSettings.menuHistoryDepth=0;
    state_.localSettings.rebindingAction=-1;
    state_.secretTv.tolerance=2+(std::abs(state_.roomSeed)%3);
    resetRoom();
    state_.started=true;
    state_.dead=false;
    state_.uiPaused=false;
    emitAudio(AudioCue::VcInvitation,0.58f);
    updatePhoneDisplay(0.0f);
}

void Game::restart() {
    const bool networkGuest=state_.multiplayer.enabled&&
        !state_.multiplayer.authoritativeHost;
    if(networkGuest)return;
    const bool networkHost=state_.multiplayer.enabled&&
        state_.multiplayer.authoritativeHost;
    std::array<bool,NETWORK_PLAYER_COUNT> activePeers{};
    if(networkHost)for(int id=1;id<NETWORK_PLAYER_COUNT;++id)
        activePeers[id]=state_.multiplayer.peers[id].active;
    reset();
    if(networkHost){
        configureNetworkHost();
        for(int id=1;id<NETWORK_PLAYER_COUNT;++id)
            if(activePeers[id])setNetworkPeerActive(id,true);
    }
    state_.cinematic.introActive = true;
    state_.cinematic.introElapsed = 0.0f;
    state_.cinematic.baseYaw = state_.camera.yaw;
    state_.cinematic.textInteraction = 1.0f;
    state_.cinematic.restartAwaken = 1.0f;
    updatePhoneDisplay(0.0f);
}

void Game::debugStartSecretTvTest(bool enterRoom) {
    reset();
    state_.roomIndex = 10;
    state_.roomSeed = 565010;
    resetRoom();
    state_.started = true;
    state_.dead = false;
    state_.uiPaused = false;
    state_.roomClear = true;
    state_.depositedSouls = state_.requiredSouls;
    for (int i = 0; i < state_.requiredSouls; ++i) state_.captures[i].filled = true;
    state_.secretTv.available = true;
    state_.secretTv.knockCueTimer = 5.4f;
    state_.player.souls = std::min(PHONE_CAPACITY, 12);
    state_.hud.storedSouls = state_.player.souls;
    if (enterRoom) {
        state_.player.inSecretRoom = true;
        state_.player.secretVisitRoom = state_.roomIndex;
        state_.player.secretVisitTimer = 120.0f;
        state_.player.pos = {38.95f, GROUND_Y + PHONE_BODY_HEIGHT * 0.5f, 0.0f};
    } else {
        state_.player.pos = state_.secretTv.entrancePos + state_.secretTv.entranceNormal * 5.4f;
        state_.player.pos.y = GROUND_Y + PHONE_BODY_HEIGHT * 0.5f;
    }
    state_.player.vel = {};
    state_.player.grounded = true;
    state_.camera.yaw = enterRoom ? -DB_PI * 0.5f : std::atan2(-state_.secretTv.entranceNormal.x, -state_.secretTv.entranceNormal.z);
    state_.camera.pitch = 0.0f;
    setEnergyTicker(enterRoom ? "TV ROOM" : "KNOCK KNOCK", 2);
    updatePhoneTransform();
    updateCamera(0.0f);
}

void Game::debugStartTraversalLab() {
    reset();
    state_.traversalLab=true;
    state_.requiredSouls=0;state_.depositedSouls=0;state_.roomClear=true;
    state_.upgradeMenu.active=false;state_.cinematic.introActive=false;
    for(auto& target:state_.targets)target=TargetState{};
    for(auto& capture:state_.captures)capture=CapturePointState{};
    for(auto& collider:state_.roomColliders)collider=RoomCollider{};
    const auto box=[&](int index,float x,float z,float width,float depth,float height){
        RoomCollider& c=state_.roomColliders[index];
        c.minX=x-width*0.5f;c.maxX=x+width*0.5f;c.minZ=z-depth*0.5f;c.maxZ=z+depth*0.5f;
        c.bottomY=0;c.topY=height;c.width=width;c.depth=depth;c.height=height;c.center={x,height*0.5f,z};
    };
    box(0,0,15,5,4,0.45f);box(1,0,9.5f,5,4,0.45f);box(2,0,3.5f,5,4,0.45f);box(3,0,-3,5,4,0.45f);
    box(4,7,14.5f,3.4f,3.4f,0.25f);box(5,7,9.8f,3.4f,3.4f,0.55f);box(6,7,4.8f,3.4f,3.4f,0.90f);box(7,7,-0.6f,3.4f,3.4f,1.30f);
    box(8,-7,13.5f,3.8f,3,0.55f);box(9,-7,8.5f,3.8f,3,1.05f);box(10,-7,3.5f,3.8f,3,1.55f);box(11,-7,-1.5f,3.8f,3,2.05f);
    state_.debug.colliderCount=12;
    state_.player.pos={0,0.53f,16};state_.player.vel={};state_.player.jumpVel=0;state_.player.grounded=true;
    state_.player.airJumpsRemaining=1;state_.player.battery=100;
    state_.camera.yaw=0;state_.camera.pitch=-0.08f;state_.camera.firstPerson=false;
    updatePhoneDisplay(0.0f);
}

void Game::debugStartRoomInspector(){
    reset();state_.roomInspector=true;state_.roomInspectorPreset=0;state_.roomInspectorEnemies=false;
    debugStepRoomInspector(0,false);
}

void Game::debugStepRoomInspector(int delta,bool newSeed){
    if(!state_.roomInspector)return;
    constexpr int presetCount=8,inspectionRoom=12;
    state_.roomInspectorPreset=(state_.roomInspectorPreset+delta%presetCount+presetCount)%presetCount;
    int candidate=newSeed?state_.roomSeed+1:1;
    for(int attempt=0;attempt<250000;++attempt,++candidate){if(early_browser_visuals::matchesInspectorPreset(early_browser_visuals::roomPlan(candidate,inspectionRoom),state_.roomInspectorPreset))break;}
    state_.roomIndex=inspectionRoom;state_.roomSeed=candidate;resetRoom();
    state_.roomInspector=true;state_.started=true;state_.dead=false;state_.uiPaused=false;state_.cinematic=CinematicState{};state_.upgradeMenu.active=false;
    state_.requiredSouls=0;state_.depositedSouls=0;state_.roomClear=true;for(auto& capture:state_.captures)capture=CapturePointState{};
    if(!state_.roomInspectorEnemies)for(auto& target:state_.targets)target=TargetState{};
    state_.player.battery=100.0f;updatePhoneDisplay(0.0f);updatePhoneTransform();updateCamera(0.0f);
}

void Game::debugToggleRoomInspectorEnemies(){
    if(!state_.roomInspector)return;state_.roomInspectorEnemies=!state_.roomInspectorEnemies;debugStepRoomInspector(0,false);
}

void Game::setPersistentProgression(std::int64_t tokens,int shotLevel,int lungeLevel,int attackLevel){
    auto& permanent=state_.progression.permanent;
    permanent.tokens=std::max<std::int64_t>(0,tokens);
    permanent.levels[static_cast<int>(UpgradeTrack::Shot)]=std::max(0,std::min(5,shotLevel));
    permanent.levels[static_cast<int>(UpgradeTrack::Lunge)]=std::max(0,std::min(5,lungeLevel));
    permanent.levels[static_cast<int>(UpgradeTrack::Attack)]=std::max(0,std::min(5,attackLevel));
    permanent.revision=1;
}

void Game::prepareStartScreen(){
    reset();
    state_.started=false;
    state_.dead=false;
    state_.uiPaused=false;
    state_.hud.gameOver=false;
    clearInputState();
    state_.audio=AudioState{};
    updatePhoneDisplay(0.0f);
}

void Game::prepareAttractScreen(){
    reset();
    const auto clockSeed=static_cast<std::uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto addressSeed=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(this));
    const std::uint32_t showcaseSeed=(clockSeed^(clockSeed>>16)^addressSeed)*0x9e3779b9u;
    state_.roomSeed=static_cast<int>(showcaseSeed&0x7fffffffu);
    state_.flowerRandomState=showcaseSeed^0xa511e9b3u;
    resetRoom();
    state_.attractMode=true;
    state_.started=true;
    state_.dead=false;
    state_.uiPaused=false;
    state_.cinematic=CinematicState{};
    state_.player.battery=100.0f;
    state_.player.pos.x=(seededRoomValue(3101.0f)-0.5f)*5.0f;
    state_.camera.yaw=(seededRoomValue(3102.0f)-0.5f)*1.2f;
    clearInputState();
    updatePhoneDisplay(0.0f);
}

void Game::dismissAttractMode(){
    if(state_.attractMode&&!state_.cinematic.attractExitActive){
        state_.cinematic.attractExitActive=true;
        state_.cinematic.attractExitElapsed=0.0f;
        clearInputState();
    }
}

void Game::setUiPaused(bool paused){
    if(!state_.started||state_.dead)return;
    if(state_.upgradeMenu.active&&!paused)return;
    const bool wasPaused=state_.uiPaused;
    state_.uiPaused=paused;
    if(paused&&!state_.multiplayer.enabled){
        state_.cinematic.textInteraction=1.0f;
        state_.cinematic.menuExitActive=false;
    }else if(wasPaused&&!paused&&!state_.multiplayer.enabled){
        state_.cinematic.menuExitActive=true;
        state_.cinematic.menuExitElapsed=0.0f;
        state_.cinematic.menuExitCameraPos=state_.camera.pos;
        state_.cinematic.menuExitLookTarget=state_.camera.lookTarget;
    }
    clearInputState();
}

bool Game::chooseTemporaryUpgrade(int track){
    if(!state_.upgradeMenu.active||track<0||track>=static_cast<int>(UpgradeTrack::Count))return false;
    if(state_.multiplayer.enabled&&!state_.multiplayer.authoritativeHost)return false;
    auto& level=state_.progression.run.temporaryLevels[track];
    state_.cinematic.textInteraction=1.0f;
    level=std::min(12,level+1);
    state_.upgradeMenu.active=false;
    state_.uiPaused=false;
    clearInputState();
    return true;
}

bool Game::purchasePermanentUpgrade(int track){
    if(!state_.upgradeMenu.active||track<0||track>=static_cast<int>(UpgradeTrack::Count))return false;
    if(state_.multiplayer.enabled&&!state_.multiplayer.authoritativeHost)return false;
    auto& permanent=state_.progression.permanent;
    if(permanent.tokens<=0||permanent.levels[track]>=5)return false;
    state_.cinematic.textInteraction=1.0f;
    --permanent.tokens;++permanent.levels[track];++permanent.revision;
    return true;
}

float Game::batteryDrainMultiplier() const {
    return 1.0f / (1.0f + static_cast<float>(state_.player.souls) * BATTERY_SOUL_EFFICIENCY);
}

int Game::upgradeLevel(UpgradeTrack track) const {
    const int i=static_cast<int>(track);
    const int permanent=(state_.multiplayer.enabled&&!state_.multiplayer.authoritativeHost)
        ? state_.progression.run.networkSharedPermanentLevels[i]
        : state_.progression.permanent.levels[i];
    return state_.progression.run.temporaryLevels[i]+permanent;
}

int Game::pairSynergyTier(UpgradeTrack a,UpgradeTrack b) const {
    return std::min(5,std::min(upgradeLevel(a),upgradeLevel(b))/SYNERGY_LEVEL_STEP);
}

int Game::survivalSynergyTier() const {
    const int shot=upgradeLevel(UpgradeTrack::Shot),lunge=upgradeLevel(UpgradeTrack::Lunge),attack=upgradeLevel(UpgradeTrack::Attack);
    return std::min(5,std::min(shot,std::min(lunge,attack))/SYNERGY_LEVEL_STEP);
}

float Game::outgoingDamageMultiplier() const {
    const int survival=survivalSynergyTier();
    if(survival<=0||state_.player.battery>=24.0f)return 1.0f;
    // The balanced "cockroach" circuit exchanges lethality for staying power
    // only while critically discharged; it never penalizes normal play.
    return std::max(0.72f,1.0f-0.055f*static_cast<float>(survival));
}

void Game::updateBuildLabel(){
    const int precision=pairSynergyTier(UpgradeTrack::Shot,UpgradeTrack::Lunge);
    const int relay=pairSynergyTier(UpgradeTrack::Shot,UpgradeTrack::Attack);
    const int momentum=pairSynergyTier(UpgradeTrack::Lunge,UpgradeTrack::Attack);
    const int survival=survivalSynergyTier();
    const char* label="OPEN CIRCUIT";
    if(survival>0&&state_.player.battery<24.0f)label="COCKROACH CIRCUIT";
    else if(precision>=relay&&precision>=momentum&&precision>0)label="PINBALL SNIPER";
    else if(relay>=momentum&&relay>0)label="RELAY BRAWLER";
    else if(momentum>0)label="MOMENTUM BRUISER";
    std::snprintf(state_.hud.buildLabel.data(),state_.hud.buildLabel.size(),"%s",label);
}

void Game::clearActivePowerups() {
    state_.energy.flowerStacks = 0;
    state_.energy.supplementalActive = false;
    state_.energy.supplementalValue = 0.0f;
    state_.energy.supplementalMax = POWERUP_STOCK_BASE_MAX;
}

void Game::addFlowerPowerupStack(float amount) {
    EnergyState& energy = state_.energy;
    energy.flowerStacks += 1;
    energy.supplementalMax = POWERUP_STOCK_BASE_MAX + static_cast<float>(std::max(0, energy.flowerStacks - 1)) * POWERUP_STOCK_PER_STACK;
    energy.supplementalValue = clampf(energy.supplementalValue + std::max(0.0f, amount), 0.0f, energy.supplementalMax);
    energy.supplementalActive = energy.supplementalValue > 0.0f;
}

float Game::nextFlowerRandom() {
    unsigned int& value = state_.flowerRandomState;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return static_cast<float>(value & 0x00ffffffu) / 16777216.0f;
}

void Game::spawnFlowerPowerup(float x, float y, float z) {
    if (!state_.player.alive) return;
    FlowerPowerupState* slot = nullptr;
    for (auto& flower : state_.flowers) if (!flower.active) { slot = &flower; break; }
    if (!slot) return;
    const float sourceZ = wrapZ(z);
    const float offsets[] = {0.0f, 0.72f, -0.72f, 1.45f, -1.45f, DB_PI};
    float dropX = x + FLOWER_DROP_SEPARATION_RADIUS;
    float dropZ = sourceZ;
    const float playerLocalZ = wrapZ(state_.player.pos.z);
    const float baseAngle = std::atan2(sourceZ - playerLocalZ, x - state_.player.pos.x);
    for (int i = 0; i < 6; ++i) {
        const float angle = baseAngle + offsets[i];
        const float radius = FLOWER_DROP_SEPARATION_RADIUS + static_cast<float>(i) * 0.12f;
        const float candidateX = x + std::cos(angle) * radius;
        const float candidateZ = sourceZ + std::sin(angle) * radius;
        const float sourceDx = candidateX - x;
        const float sourceDz = candidateZ - sourceZ;
        bool clear = sourceDx*sourceDx + sourceDz*sourceDz >= FLOWER_DROP_MIN_SPACING*FLOWER_DROP_MIN_SPACING;
        for (const auto& flower : state_.flowers) if (clear && flower.active) {
            const float dx = candidateX - flower.pos.x;
            const float dz = candidateZ - flower.pos.z;
            if (dx*dx + dz*dz < FLOWER_DROP_MIN_SPACING*FLOWER_DROP_MIN_SPACING) clear = false;
        }
        if (clear) { dropX = candidateX; dropZ = candidateZ; break; }
    }
    *slot = FlowerPowerupState{};
    slot->active = true;
    slot->baseY = std::max(FLOWER_POWERUP_GROUND_Y, y);
    slot->pos = {dropX, slot->baseY, dropZ};
    slot->rotationY = nextFlowerRandom() * DB_PI * 2.0f;
}

void Game::updateFlowerPowerups(float dt) {
    if (!state_.player.alive) return;
    const float playerLocalZ = wrapZ(state_.player.pos.z);
    for (auto& flower : state_.flowers) {
        if (!flower.active) continue;
        flower.age += dt;
        flower.pos.y = flower.baseY + std::sin(flower.age * 3.2f) * FLOWER_BOB_HEIGHT;
        flower.rotationY += dt * 1.35f;
        const float dx = flower.pos.x - state_.player.pos.x;
        const float dz = flower.pos.z - playerLocalZ;
        const float dy = flower.pos.y - state_.player.pos.y;
        if (dx*dx + dy*dy + dz*dz <= FLOWER_PICKUP_RADIUS*FLOWER_PICKUP_RADIUS) {
            addFlowerPowerupStack(FLOWER_PICKUP_VALUE);
            flower = FlowerPowerupState{};
        }
    }
}

void Game::spawnParticleBurst(const Vec3& position) {
    for(int n=0;n<22;++n) {
        ParticleState& particle=state_.particles[state_.nextParticle];
        state_.nextParticle=(state_.nextParticle+1)%PARTICLE_COUNT;
        const float life=0.55f+nextFlowerRandom()*0.35f;
        particle=ParticleState{}; particle.pos=position;
        particle.vel={(nextFlowerRandom()-0.5f)*5.0f,nextFlowerRandom()*4.0f,(nextFlowerRandom()-0.5f)*5.0f};
        particle.life=life; particle.maxLife=life;
    }
}

void Game::spawnFlameBurst(const Vec3& position,float strength) {
    const int count=static_cast<int>(36.0f*clampf(strength,0.7f,2.2f));
    for(int n=0;n<count;++n) {
        ParticleState& particle=state_.particles[state_.nextParticle]; state_.nextParticle=(state_.nextParticle+1)%PARTICLE_COUNT;
        const float angle=nextFlowerRandom()*DB_PI*2.0f,radial=1.2f+nextFlowerRandom()*5.4f*strength,life=0.42f+nextFlowerRandom()*0.32f;
        particle=ParticleState{}; particle.pos=position+Vec3{0,0.06f,0};
        particle.vel={std::cos(angle)*radial,2.4f+nextFlowerRandom()*5.0f*strength,std::sin(angle)*radial}; particle.life=life; particle.maxLife=life;
    }
}

void Game::spawnShellShatter(const TargetState& target) {
    const int count=target.brute?48:32;
    const float bodyHeight=PASS7_HUMAN_VISUAL_SPEC.totalHeight*target.scale;
    unsigned int visualSeed=0x91E10DA5u^static_cast<unsigned int>((target.pos.x+32.0f)*997.0f)^static_cast<unsigned int>((target.pos.z+64.0f)*619.0f);
    const auto visualRandom=[&](){visualSeed=visualSeed*1664525u+1013904223u;return static_cast<float>((visualSeed>>8)&0x00ffffffu)/16777216.0f;};
    for(int n=0;n<count;++n){
        ParticleState& particle=state_.particles[state_.nextParticle];state_.nextParticle=(state_.nextParticle+1)%PARTICLE_COUNT;
        const float angle=visualRandom()*DB_PI*2.0f;
        const float height=0.08f+visualRandom()*bodyHeight*0.90f;
        const float bodyWidth=(height>bodyHeight*0.68f?0.16f:0.25f)*target.scale;
        const float radial=0.45f+visualRandom()*1.55f;
        const float life=0.72f+visualRandom()*0.38f;
        particle=ParticleState{};
        particle.kind=1;
        particle.size=(0.055f+visualRandom()*0.075f)*target.scale;
        particle.pos=target.pos+Vec3{std::cos(angle)*bodyWidth*visualRandom(),height,std::sin(angle)*bodyWidth*visualRandom()};
        particle.vel={std::cos(angle)*radial,0.45f+visualRandom()*2.25f,std::sin(angle)*radial};
        particle.life=life;particle.maxLife=life;
    }
}

void Game::updateParticles(float dt) {
    for(auto& particle:state_.particles) {
        if(particle.life<=0.0f) continue;
        particle.vel.y-=(particle.kind==1?10.5f:8.0f)*dt;
        particle.pos+=particle.vel*dt;
        if(particle.kind==1&&particle.pos.y<=0.025f){
            particle.pos.y=0.025f;particle.vel.y=0.0f;
            const float settle=std::exp(-16.0f*dt);particle.vel.x*=settle;particle.vel.z*=settle;
            particle.life=std::max(0.0f,particle.life-dt*1.25f);
        }
        particle.life=std::max(0.0f,particle.life-dt);
    }
}

float Game::consumeSupplementalBattery(float cost) {
    EnergyState& energy = state_.energy;
    if (!energy.supplementalActive || energy.supplementalValue <= 0.0f || cost <= 0.0f) return cost;
    const float absorbed = std::min(cost, energy.supplementalValue);
    energy.supplementalValue = std::max(0.0f, energy.supplementalValue - absorbed);
    if (energy.supplementalValue <= 0.001f) clearActivePowerups();
    return std::max(0.0f, cost - absorbed);
}

void Game::setEnergyTicker(const char* text,int type){
    state_.hud.energyTicker.fill(0);if(text)std::snprintf(state_.hud.energyTicker.data(),state_.hud.energyTicker.size(),"%s",text);state_.hud.energyTickerType=type;state_.hud.energyTickerUntil=state_.time+1.05f;
}

bool Game::spendBattery(float amount,BatteryReason reason) {
    PlayerState& player = state_.player;
    if (!player.alive) return false;
    const float before=player.battery;
    if(reason==BatteryReason::Hit){
        const int survival=survivalSynergyTier();
        const float guard=state_.progression.run.impactGuardTimer>0.0f?0.42f:1.0f;
        amount*=guard*std::max(0.35f,1.0f-0.11f*static_cast<float>(survival));
    }
    const float remaining = consumeSupplementalBattery(std::max(0.0f, amount) * batteryDrainMultiplier());
    player.battery = clampf(player.battery - remaining, 0.0f, 100.0f);
    if(reason!=BatteryReason::Continuous){
        const char* label=reason==BatteryReason::Melee?"MELEE":reason==BatteryReason::Shoot?"SHOOT":reason==BatteryReason::Hit?"HIT":reason==BatteryReason::Climb?"CLIMB":reason==BatteryReason::Loop?"LOOP":"JUMP";char message[48]{};
        const float spent=before-player.battery;if(spent>0.001f)std::snprintf(message,sizeof(message),"-%.1F %s",spent,label);else std::snprintf(message,sizeof(message),"FLOWER %s",label);setEnergyTicker(message,spent>0.001f?1:0);
    }
    updateBatteryAudio(before);
    if (player.battery <= 0.0f && reason==BatteryReason::Hit && survivalSynergyTier()>0 && state_.progression.run.lastStandCooldown<=0.0f) {
        player.battery=1.0f;
        state_.progression.run.lastStandCooldown=LAST_STAND_COOLDOWN;
        setEnergyTicker("LAST SIGNAL",2);
        return true;
    }
    if (player.battery <= 0.0f) {
        player.battery = 0.0f;
        if(reason==BatteryReason::Hit&&state_.multiplayer.enabled){player.downed=true;player.bleedoutTimer=15.0f;player.reviveCharge=0.0f;player.vel={};player.jumpVel=0.0f;clearPlayerLifecycleActions();setEnergyTicker("SIGNAL DOWN",1);return false;}
        if(reason==BatteryReason::Hit&&!state_.multiplayer.enabled&&player.souls>0&&!player.soloSoulRebootUsed){--player.souls;player.battery=15.0f;player.soloSoulRebootUsed=true;state_.progression.run.batteryRegenLock=PASSIVE_RECHARGE_DELAY;setEnergyTicker("SOUL REBOOT",2);return true;}
        triggerRunDeath();
        return false;
    }
    return true;
}

void Game::gainBattery(float amount,BatteryReason reason) {
    if (!state_.player.alive) return;
    const float before=state_.player.battery;
    state_.player.battery = clampf(state_.player.battery + std::max(0.0f, amount), 0.0f, 100.0f);
    if(reason!=BatteryReason::Continuous){const char* label=reason==BatteryReason::Ingest?"INGEST":reason==BatteryReason::NextRoom?"ROOM":reason==BatteryReason::Chain?"CHAIN":reason==BatteryReason::Headshot?"HEADSHOT":"COMBO";char message[48]{};std::snprintf(message,sizeof(message),"+%.1F %s",state_.player.battery-before,label);setEnergyTicker(message,reason==BatteryReason::Combo||reason==BatteryReason::Chain||reason==BatteryReason::Headshot?2:0);}
    updateBatteryAudio(before);
}

void Game::emitAudio(AudioCue cue,float volume) {
    // The title demonstration is visual ambience, not a second audible match.
    // Keeping its event stream empty also prevents queued combat cues from
    // spilling into the menu when the player dismisses the title.
    if(state_.attractMode)return;
    AudioState& audio=state_.audio;
    const unsigned int serial=audio.nextSerial++;
    AudioEventState& event=audio.events[(serial-1u)%AUDIO_EVENT_COUNT];
    event.cue=cue; event.serial=serial; event.volume=volume;
}

void Game::updateBatteryAudio(float beforeValue) {
    if(simulationPlayerId_!=0) return;
    if(state_.dead) return;
    AudioState& audio=state_.audio;
    const float before=clampf(beforeValue/100.0f,0.0f,1.0f),now=clampf(state_.player.battery/100.0f,0.0f,1.0f);
    if(before>0.24f && now<=0.24f && audio.lowPowerArmed){emitAudio(AudioCue::LowPower,0.48f);audio.lowPowerArmed=false;}
    // Pass 7 only arms the recovered/full cue after a genuinely critical
    // discharge. Small expenditures near full must not repeatedly chime.
    if(now<=0.14f) audio.connectPowerArmed=true;
    if(audio.connectPowerArmed && before<1.0f && now>=0.995f){emitAudio(AudioCue::ConnectPower,0.52f);audio.connectPowerArmed=false;audio.lowPowerArmed=true;}
    if(now>0.32f) audio.lowPowerArmed=true;
}

void Game::updateSlurpAudio() {
    bool active=false;
    if(!state_.dead && state_.started && state_.player.souls<MAX_STORED_SOULS) for(const auto& target:state_.targets) {
        if(target.alive && (target.soulState==SoulState::Latched || target.soulState==SoulState::Ingesting)){active=true;break;}
    }
    if(active==state_.audio.slurpPlaying) return;
    state_.audio.slurpPlaying=active;
    emitAudio(active?AudioCue::SlurpRingtoneStart:AudioCue::SlurpRingtoneStop,0.13f);
}

bool Game::feedSupplementalBattery(float amount) {
    EnergyState& energy = state_.energy;
    if (!energy.supplementalActive || energy.supplementalMax <= 0.0f) return false;
    const float before = energy.supplementalValue;
    energy.supplementalValue = clampf(before + std::max(0.0f, amount), 0.0f, energy.supplementalMax);
    energy.supplementalActive = energy.supplementalValue > 0.0f;
    return energy.supplementalValue > before + 0.001f;
}

void Game::registerMeleeBatteryHit(int hitCount) {
    const int hits = std::max(1, hitCount);
    EnergyState& energy = state_.energy;
    const float multiBonus = hits > 1 ? static_cast<float>(hits - 1) * MULTI_HIT_BATTERY_BONUS : 0.0f;
    const float multiplierBefore=energy.comboMultiplier;
    gainBattery((BATTERY_MELEE_HIT_GAIN + multiBonus) * multiplierBefore);
    {char message[48]{};std::snprintf(message,sizeof(message),"+%.1F X%.2F",(BATTERY_MELEE_HIT_GAIN+multiBonus)*multiplierBefore,multiplierBefore);setEnergyTicker(message,2);}
    energy.comboHits += hits;
    energy.lastComboHitTime = state_.time;
    energy.comboMultiplier *= BATTERY_COMBO_GROWTH * std::pow(MULTI_HIT_BONUS_GROWTH, static_cast<float>(std::max(0, hits - 1)));
}

void Game::updateBattery(float dt) {
    if (!state_.player.alive) return;
    if(state_.player.downed){state_.player.bleedoutTimer=std::max(0.0f,state_.player.bleedoutTimer-dt);if(state_.player.bleedoutTimer<=0.0f){state_.player.downed=false;if(simulationPlayerId_!=0)state_.player.alive=false;else {bool teammate=false;for(const auto& peer:state_.multiplayer.peers)if(peer.active&&peer.player.alive&&!peer.player.downed){teammate=true;break;}if(teammate)state_.player.alive=false;else triggerRunDeath();}}return;}
    if(simulationPlayerId_==0) updateRunProgressionTimers(dt);
    EnergyState& energy = state_.energy;
    if (energy.comboHits > 0 && state_.time - energy.lastComboHitTime > BATTERY_COMBO_TIMEOUT) {
        energy.comboHits = 0;
        energy.comboMultiplier = 1.0f;
        energy.lastComboHitTime = -9999.0f;
    }
    energy.dischargeTimer = std::max(0.0f, energy.dischargeTimer - dt);
    const InputState& input = state_.input;
    const float forward = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    const float strafe = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    const bool moving = std::abs(forward) + std::abs(strafe) > 0.0f;
    const bool running = moving && (input.sprint || input.touchSprint);
    float drain = 0.0f;
    bool active = false;
    if (moving) { drain += running ? BATTERY_SPRINT_DRAIN : BATTERY_WALK_DRAIN; active = true; }
    if (!state_.player.grounded) { drain += BATTERY_AIR_DRAIN; active = true; }
    if (state_.vacuum.active) { drain += BATTERY_VACUUM_DRAIN * std::max(0.35f, state_.vacuum.power); active = true; }
    if (state_.meleeVisual.visualTimer > 0.0f || energy.dischargeTimer > 0.0f) active = true;
    if(active)state_.progression.run.batteryRegenLock=std::max(state_.progression.run.batteryRegenLock,PASSIVE_RECHARGE_DELAY);
    if (drain > 0.0f) spendBattery(drain * dt);
    else if(state_.progression.run.batteryRegenLock<=0.0f) {
        const float survivalRegen=(survivalSynergyTier()>0&&state_.player.battery<24.0f)?1.0f+0.10f*survivalSynergyTier():1.0f;
        const float precisionWindow=state_.progression.run.headshotRechargeBoost>0.0f?1.35f:1.0f;
        const float tvSignal=1.0f+std::min(0.45f,0.06f*std::sqrt(static_cast<float>(std::max(0,state_.secretTv.signal))));
        gainBattery(BATTERY_IDLE_REGEN * tvSignal * precisionWindow * survivalRegen * (1.0f-state_.progression.run.headshotRegenTax) * dt);
    }
}

void Game::updateRunProgressionTimers(float dt) {
    state_.progression.run.batteryRegenLock = std::max(0.0f, state_.progression.run.batteryRegenLock - dt);
    state_.progression.run.headshotRegenTax = std::max(0.0f, state_.progression.run.headshotRegenTax - dt * 0.11f);
    state_.progression.run.relayPrimerTimer=std::max(0.0f,state_.progression.run.relayPrimerTimer-dt);
    if(state_.progression.run.relayPrimerTimer<=0.0f)state_.progression.run.relayPrimerStacks=0;
    state_.progression.run.impactGuardTimer=std::max(0.0f,state_.progression.run.impactGuardTimer-dt);
    state_.progression.run.lastStandCooldown=std::max(0.0f,state_.progression.run.lastStandCooldown-dt);
    state_.progression.run.lungeReboundTimer=std::max(0.0f,state_.progression.run.lungeReboundTimer-dt);
    state_.progression.run.headshotRechargeBoost=std::max(0.0f,state_.progression.run.headshotRechargeBoost-dt);
}

void Game::triggerRunDeath() {
    if(simulationPlayerId_!=0){state_.player.alive=false;state_.player.battery=0;clearPlayerLifecycleActions();return;}
    if(state_.dead) return;
    state_.dead=true; state_.started=false; state_.uiPaused=false;
    state_.cinematic.introActive=false;
    state_.cinematic.deathActive=true;
    state_.cinematic.deathElapsed=0.0f;
    state_.cinematic.overlayFade=0.0f;
    state_.cinematic.deathChoice=0;
    state_.cinematic.textInteraction=1.0f;
    state_.cinematic.baseYaw=state_.camera.yaw;
    state_.cinematic.startCameraPos=state_.camera.pos;
    state_.camera.firstPerson=false;
    state_.hud.menuSelection=0;
    state_.player.alive=false; state_.player.battery=0.0f; state_.player.vel={}; state_.player.jumpVel=0.0f;
    clearPlayerLifecycleActions();
    clearActivePowerups();
    for(auto& flower:state_.flowers) flower=FlowerPowerupState{};
    state_.energy.comboHits=0; state_.energy.comboMultiplier=1.0f; state_.energy.lastComboHitTime=-9999.0f;
    if(state_.audio.slurpPlaying){state_.audio.slurpPlaying=false;emitAudio(AudioCue::SlurpRingtoneStop,0.13f);}
    emitAudio(AudioCue::VcEnded,0.64f);
    clearInputState();
    state_.hud.batteryFill=0.0f; state_.hud.lowBattery=true; state_.hud.gameOver=true;
}

void Game::clearPlayerLifecycleActions(){
    state_.vacuum=VacuumState{};
    state_.meleeVisual=MeleeVisualState{};
    state_.meleeComboWindow=0.0f;
    state_.energy.dischargeTimer=0.0f;
    state_.energy.dischargePositionAmount=0.0f;
    for(auto& pending:state_.pendingShots)pending=PendingShotState{};
    state_.player.ledgeHanging=false;
    state_.player.ledgeCollider=-1;
    state_.player.ledgeMantleTimer=0.0f;
    state_.player.grabbedByTarget=-1;
    state_.player.grabEscape=0.0f;
    clearInputState();
}

float Game::seededRoomValue(float offset) const {
    const float x = std::sin((static_cast<float>(state_.roomSeed) + offset) * 12.9898f) * 43758.5453f;
    return x - std::floor(x);
}

int Game::getRoomTileIndex(float z) const {
    return static_cast<int>(std::floor((z + ROOM_DEPTH * 0.5f) / ROOM_DEPTH));
}

float Game::getRoomTileOriginZ(int tileIndex) const {
    return static_cast<float>(tileIndex) * ROOM_DEPTH;
}

float Game::wrapZ(float z) const {
    return z - getRoomTileOriginZ(getRoomTileIndex(z));
}

void Game::buildRoomColliders() {
    const auto plan=early_browser_visuals::roomPlan(state_.roomSeed,state_.roomIndex);
    // Completeness is never randomized. If a future grammar combination fails
    // its conservative required-route contract, retain the room shell and
    // objectives but reject its optional obstacle geometry.
    const bool routeValid=early_browser_visuals::requiredRouteIsTraversable(plan,state_.roomSeed,state_.roomIndex);
    state_.debug.colliderCount=routeValid?std::min(ROOM_COLLIDER_COUNT,plan.obstacleCount):0;
    for (auto& c : state_.roomColliders) c = RoomCollider{};
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const auto spec=early_browser_visuals::obstacle(plan,state_.roomSeed,state_.roomIndex,i);
        const float px=spec.center.x,pz=spec.center.z,w=spec.size.x,h=spec.size.y,d=spec.size.z;
        RoomCollider& c = state_.roomColliders[i];
        c.minX = px - w * 0.5f; c.maxX = px + w * 0.5f;
        c.minZ = pz - d * 0.5f; c.maxZ = pz + d * 0.5f;
        c.bottomY = 0.0f; c.topY = h;
        c.width = w; c.depth = d; c.height = h; c.center = {px, h * 0.5f, pz};
    }
    const bool propsValid=routeValid&&early_browser_visuals::environmentPropsValid(plan,state_.roomSeed,state_.roomIndex);
    if(propsValid)for(int i=0;i<early_browser_visuals::environmentPropCount(plan)&&state_.debug.colliderCount<ROOM_COLLIDER_COUNT;++i){
        const auto prop=early_browser_visuals::environmentProp(plan,state_.roomSeed,state_.roomIndex,i);if(!early_browser_visuals::environmentPropSolid(prop))continue;
        const auto spec=early_browser_visuals::environmentPropCollider(prop);RoomCollider& c=state_.roomColliders[state_.debug.colliderCount++];
        c.minX=spec.center.x-spec.size.x*0.5f;c.maxX=spec.center.x+spec.size.x*0.5f;c.minZ=spec.center.z-spec.size.z*0.5f;c.maxZ=spec.center.z+spec.size.z*0.5f;
        c.bottomY=0;c.topY=spec.size.y;c.width=spec.size.x;c.depth=spec.size.z;c.height=spec.size.y;c.center=spec.center;
    }
}

void Game::chooseSecretTvEntrance() {
    SecretTvState& tv = state_.secretTv;
    const bool rightWall = seededRoomValue(2210.0f + static_cast<float>(state_.roomIndex) * 17.0f) >= 0.5f;
    const float x = rightWall ? ROOM_WIDTH * 0.5f - 1.10f : -ROOM_WIDTH * 0.5f + 1.10f;
    const Vec3 normal = rightWall ? Vec3{-1.0f, 0.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};

    float bestZ = 4.8f;
    float bestClearance = -1.0f;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const float localZ = -12.0f + seededRoomValue(2230.0f + static_cast<float>(attempt) * 19.0f) * 24.0f;
        float clearance = 99.0f;
        for (int i = 0; i < state_.debug.colliderCount; ++i) {
            const RoomCollider& c = state_.roomColliders[i];
            const float dx = std::max(std::max(c.minX - x, 0.0f), x - c.maxX);
            const float dz = std::max(std::max(c.minZ - localZ, 0.0f), localZ - c.maxZ);
            clearance = std::min(clearance, std::sqrt(dx * dx + dz * dz));
        }
        const float goalClearance = std::abs(localZ - ROOM_GRID_Z) * 0.35f;
        const float startClearance = std::abs(localZ - ROOM_START_Z) * 0.15f;
        clearance += std::min(goalClearance, 3.0f) + std::min(startClearance, 2.5f);
        if (clearance > bestClearance) {
            bestClearance = clearance;
            bestZ = localZ;
        }
    }
    tv.entrancePos = {x, GROUND_Y + PHONE_BODY_HEIGHT * 0.5f, bestZ};
    tv.entranceNormal = normal;
}

void Game::resetRoom() {
    const int roomIndex = state_.roomIndex;
    const int roomSeed = state_.roomSeed;
    state_.roomClear = false;
    state_.player = PlayerState{};
    state_.player.pos = {0.0f, GROUND_Y, ROOM_START_Z};
    state_.player.grounded = true;
    state_.player.jumpVel = 0.0f;
    state_.player.airJumpsRemaining = 1;
    state_.player.coyoteTimer = 0.0f;
    state_.player.jumpBufferTimer = 0.0f;
    state_.camera = CameraState{};
    state_.vacuum = VacuumState{};
    state_.phonePose = PhonePoseState{};
    state_.meleeVisual = MeleeVisualState{};
    state_.meleeComboWindow = 0.0f;
    state_.topology = RoomTopologyState{};
    state_.roomIndex = roomIndex;
    state_.roomSeed = roomSeed;
    state_.captureSoundSlots={{0,1,2,3,4}};
    for(int i=4;i>0;--i){const int j=static_cast<int>(nextFlowerRandom()*static_cast<float>(i+1))%(i+1);std::swap(state_.captureSoundSlots[i],state_.captureSoundSlots[j]);}
    buildRoomColliders();
    chooseSecretTvEntrance();

    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& target = state_.targets[i];
        target = TargetState{};
        target.pos = {-8.0f + static_cast<float>(i % 5) * 4.0f, GROUND_Y, -12.0f + static_cast<float>(i / 5) * 4.5f};
        target.alive = i < activeHumanTarget();
        target.brute = seededRoomValue(420 + i) < 0.18f;
        target.armor = target.brute ? SOUL_ARMOR_BRUTE : SOUL_ARMOR_NORMAL;
        target.health = 1.0f;
        target.scale = target.brute ? HUMAN_SCALE_BRUTE : 1.0f;
        target.phase = static_cast<float>(i) * 0.77f;
        target.floatOffset = seededRoomValue(430 + i) * DB_PI * 2.0f;
        target.spinSpeed = 0.4f + seededRoomValue(435 + i) * 0.8f;
        target.visualWalkPhase = target.phase;
        target.visualYaw = seededRoomValue(440 + i) * DB_PI * 2.0f;
        target.attackCooldown = seededRoomValue(460 + i) * 0.5f;
        target.attackVariant = static_cast<int>(seededRoomValue(480 + i) * 4.0f) % 4;
        resetSoulLattice(target);
        chooseHumanWalkTarget(i);
        syncTargetReactionVisual(target);
    }

    state_.requiredSouls=static_cast<int>(clampf(5.0f+static_cast<float>(state_.runRules.requiredSlotStacks),1.0f,9.0f));
    state_.depositedSouls=0;
    const float startX=-((static_cast<float>(state_.requiredSouls)-1.0f)*0.82f)*0.5f;
    for (int i = 0; i < CAPTURE_COUNT; ++i) {
        state_.captures[i] = CapturePointState{};
        state_.captures[i].pos = {startX+static_cast<float>(i)*0.82f,3.05f,ROOM_GRID_Z};
    }
    for (auto& bullet : state_.bullets) bullet = BulletState{};
}

void Game::setKey(int keyCode, bool down) {
    InputState& input = state_.input;
    if (down && !state_.started && (keyCode == KEY_SPACE || keyCode == KEY_ENTER || keyCode == KEY_R || keyCode == KEY_F)) {
        restart();
        return;
    }
    if (keyCode == KEY_W) input.forward = down;
    if (keyCode == KEY_S) input.back = down;
    if (keyCode == KEY_A) input.left = down;
    if (keyCode == KEY_D) input.right = down;
    if (keyCode == KEY_SHIFT_LEFT || keyCode == KEY_SHIFT_RIGHT) input.sprint = down;
    if (keyCode == KEY_SPACE) {
        if (down && !input.jumpHeld) input.jumpPressed = true;
        input.jumpHeld = down;
    }
    if (keyCode == KEY_F && down) input.meleePressed = true;
    if (keyCode == KEY_Q && down) input.shootPressed = true;
    if (keyCode == KEY_C && down) input.meleePressed = true;
    if (keyCode == KEY_V && down) input.cameraTogglePressed = true;
    if (down && keyCode >= KEY_1 && keyCode <= KEY_4) setCommSignal(keyCode - KEY_1 + 1);
}

void Game::clearInputState() {
    InputState& input = state_.input;
    input.forward = false;
    input.back = false;
    input.left = false;
    input.right = false;
    input.sprint = false;
    input.jumpHeld = false;
    input.primaryHeld = false;
    input.touchPrimaryHeld = false;
    input.touchSprint = false;
    input.touchMoveX = 0.0f;
    input.touchMoveZ = 0.0f;
    input.lookDeltaX = 0.0f;
    input.lookDeltaY = 0.0f;
    input.wiggleAxis = 0.0f;
    input.commSignalPressed = 0;
    input.jumpPressed = false;
    input.meleePressed = false;
    input.shootPressed = false;
    input.cameraTogglePressed = false;
}

void Game::setTouch(int action, float x, float y, int pointerCount) {
    if(state_.attractMode&&action==TOUCH_DOWN){dismissAttractMode();return;}
    (void)x;
    (void)y;
    (void)pointerCount;
    // Android's role-based controls own gameplay input. The raw channel remains
    // only as the platform gesture used to dismiss the title showcase.
}

void Game::setTouchControls(float moveX, float moveZ, float lookDeltaX, float lookDeltaY,
    bool vacuumHeld, bool sprintHeld, bool jumpPressed, bool meleePressed,
    bool shootPressed, bool cameraTogglePressed) {
    InputState& input = state_.input;
    input.touchMoveX = clampf(moveX, -1.0f, 1.0f);
    input.touchMoveZ = clampf(moveZ, -1.0f, 1.0f);
    input.lookDeltaX += lookDeltaX; input.lookDeltaY += lookDeltaY;
    input.touchPrimaryHeld = vacuumHeld; input.touchSprint = sprintHeld;
    if (jumpPressed) input.jumpPressed = true;
    if (meleePressed) input.meleePressed = true;
    if (shootPressed) input.shootPressed = true;
    if (cameraTogglePressed) input.cameraTogglePressed = true;
}

void Game::update(float dt) {
    dt = clampf(dt, 0.0f, 0.033f);
    state_.time += dt; state_.frame += 1;
    state_.environmentVisual.latestShotAge=std::min(9999.0f,state_.environmentVisual.latestShotAge+dt);
    state_.cinematic.textInteraction*=std::exp(-7.0f*dt);
    state_.cinematic.overlayFade += ((state_.dead ? 1.0f : 0.0f) - state_.cinematic.overlayFade) * std::min(1.0f, dt * 4.0f);
    state_.cinematic.restartAwaken = std::max(0.0f, state_.cinematic.restartAwaken - dt * 1.8f);
    if(state_.cinematic.menuEnterActive){
        state_.cinematic.menuEnterElapsed=std::min(MENU_ENTER_FADE_DURATION,state_.cinematic.menuEnterElapsed+dt);
        if(state_.cinematic.menuEnterElapsed>=MENU_ENTER_FADE_DURATION)state_.cinematic.menuEnterActive=false;
    }
    updatePhoneDisplay(dt);
    if(state_.attractMode&&state_.cinematic.attractExitActive){
        state_.cinematic.attractExitElapsed=std::min(ATTRACT_EXIT_DURATION,state_.cinematic.attractExitElapsed+dt);
        if(state_.cinematic.attractExitElapsed>=ATTRACT_EXIT_DURATION){
            prepareStartScreen();
            state_.cinematic.menuEnterActive=true;
            state_.cinematic.menuEnterElapsed=0.0f;
            return;
        }
    }
    if(state_.attractMode&&state_.dead&&!state_.cinematic.attractExitActive){prepareAttractScreen();return;}
    if(state_.dead) {
        state_.hud.crosshairOpacity+=(0.0f-state_.hud.crosshairOpacity)*std::min(1.0f,dt*14.0f);
        state_.phoneVisual=makePhoneVisualState(0.0f,0.0f,0.0f,state_.time,false);
        updatePhoneTransform();
        updateDeathCamera(dt);
        updateParticles(dt*DEATH_PRESENTATION_SCALE);
        state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
        state_.hud.lowBattery=state_.player.battery<24.0f;
        state_.hud.gameOver=true;
        return;
    }
    if(!state_.started) {
        state_.hud.crosshairOpacity+=(0.0f-state_.hud.crosshairOpacity)*std::min(1.0f,dt*14.0f);
        state_.phoneVisual=makePhoneVisualState(0.0f,0.0f,0.0f,state_.time,false);
        updatePhoneTransform();
        updateCamera(dt);
        state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
        state_.hud.lowBattery=state_.player.battery<24.0f;
        state_.hud.gameOver=false;
        return;
    }
    if(state_.cinematic.introActive) {
        state_.hud.crosshairOpacity+=(0.0f-state_.hud.crosshairOpacity)*std::min(1.0f,dt*14.0f);
        // Freeze simulation without deleting held locomotion. Touch sticks,
        // keyboard keys, and controller axes held through the reveal should
        // become live on the first gameplay frame. Only transient actions and
        // accumulated look deltas are discarded during the locked camera shot.
        state_.input.lookDeltaX=0.0f;
        state_.input.lookDeltaY=0.0f;
        state_.input.primaryHeld=false;
        state_.input.touchPrimaryHeld=false;
        state_.input.jumpHeld=false;
        state_.input.jumpPressed=false;
        state_.input.meleePressed=false;
        state_.input.shootPressed=false;
        state_.input.cameraTogglePressed=false;
        state_.phoneVisual=makePhoneVisualState(0.0f,0.0f,0.0f,state_.time,false);
        updatePhoneTransform();
        updateIntroCamera(dt);
        state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
        state_.hud.lowBattery=state_.player.battery<24.0f;
        state_.hud.gameOver=false;
        return;
    }
    // A local menu owns the solo simulation clock. In a connected match it
    // only owns this player's controls; the authoritative room keeps moving.
    if(state_.uiPaused&&!state_.multiplayer.enabled){
        state_.phoneVisual=makePhoneVisualState(0.0f,0.0f,0.0f,state_.time,false);
        updatePhoneTransform();
        updateCamera(dt);
        updateSoulLattices();
        updateCrosshair(dt);
        return;
    }
    state_.hud.headshotPulse=std::max(0.0f,state_.hud.headshotPulse-dt*5.5f);
    state_.hud.criticalHitPulse=std::max(0.0f,state_.hud.criticalHitPulse-dt*4.8f);
    state_.hud.perfectPulse=std::max(0.0f,state_.hud.perfectPulse-dt*3.8f);
    state_.hud.headshotKillCharge=std::max(0.0f,state_.hud.headshotKillCharge-dt*1.7f);
    updateBuildLabel();
    auto& runProgression=state_.progression.run;
    if(runProgression.accuracyStacks>0){
        runProgression.accuracyDecayTimer=std::max(0.0f,runProgression.accuracyDecayTimer-dt);
        if(runProgression.accuracyDecayTimer<=0.0f){runProgression.accuracyStacks=0;runProgression.accuracyMultiplier=1.0f;}
    }
    if(state_.multiplayer.enabled&&!state_.multiplayer.authoritativeHost){updateNetworkGuest(dt);return;}
    if(state_.attractMode)updateAttractInput(dt);
    updateSecretTv(dt);
    updateInputActions(dt);
    updatePlayer(dt);
    updateNetworkPeers(dt);
    updateTeamRevival(dt);
    state_.phoneVisual = makePhoneVisualState(state_.vacuum.pose, state_.vacuum.power, 0.0f, state_.time, state_.camera.firstPerson);
    const float phoneActionAmount=std::max(state_.vacuum.pose,state_.energy.dischargePositionAmount);
    state_.phoneVisual.actionLift=phoneActionAmount*0.65f;
    state_.phoneVisual.actionForward=phoneActionAmount*0.25f;
    updatePhoneTransform();
    if(state_.meleeVisual.airLungeLandingPending)applyMeleeHits();
    processPendingShots(dt);
    updateTargets(dt);
    updateVacuum(dt);
    updateSlurpAudio();
    updateBattery(dt);
    if(state_.dead) {
        state_.hud.batteryFill=0.0f; state_.hud.lowBattery=true; state_.hud.gameOver=true;
        updatePhoneDisplay(dt);
        return;
    }
    float contact = 0.0f;
    for (auto& target : state_.targets) {
        syncSoulVisual(target, state_.time);
        if (target.soulState == SoulState::Latched || target.soulState == SoulState::Ingesting) {
            contact = std::max(contact, target.ingestProgress > 0.0f ? target.ingestProgress : 0.25f);
        }
    }
    state_.phoneVisual = makePhoneVisualState(state_.vacuum.pose, state_.vacuum.power, contact, state_.time, state_.camera.firstPerson);
    state_.phoneVisual.actionLift=phoneActionAmount*0.65f;
    state_.phoneVisual.actionForward=phoneActionAmount*0.25f;
    processQueuedSoulCaptures();
    updateFlowerPowerups(dt);
    updateBullets(dt);
    updateCaptures(dt);
    updateRoomPopulation(dt);
    updateDoorTransition();
    updateParticles(dt);
    // Pass 7 evaluates vacuum against the prior rendered camera transform, then
    // advances the camera and crosshair for the frame that is about to render.
    updateCamera(dt);
    updateIntroCamera(dt);
    updateSoulLattices();
    updateCrosshair(dt);
    state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
    state_.hud.storedSouls=state_.player.souls;
    state_.hud.filledGoals=0;
    for(const auto& capture:state_.captures) if(capture.filled) ++state_.hud.filledGoals;
    state_.hud.requiredGoals=state_.requiredSouls;
    state_.hud.vacuumField=state_.vacuum.fieldStrength;
    state_.hud.lockStrength=state_.vacuum.lockStrength;
    state_.hud.hasTarget=state_.vacuum.target!=-1;
    state_.hud.lowBattery=state_.player.battery<24.0f;
    state_.hud.gameOver=state_.dead;
    state_.hud.supplementalFill=state_.energy.supplementalMax>0.0f
        ? clampf(state_.energy.supplementalValue/state_.energy.supplementalMax,0.0f,1.0f) : 0.0f;
    state_.hud.flowerStacks=state_.energy.flowerStacks;
    updatePhoneDisplay(dt);
}

void Game::configureNetworkHost(){state_.multiplayer=MultiplayerRuntimeState{};state_.multiplayer.enabled=true;state_.multiplayer.authoritativeHost=true;state_.multiplayer.connected=true;state_.multiplayer.localPlayerId=0;std::snprintf(state_.multiplayer.status.data(),state_.multiplayer.status.size(),"HOSTING");}
void Game::configureNetworkGuest(int localPlayerId){state_.multiplayer=MultiplayerRuntimeState{};state_.multiplayer.enabled=true;state_.multiplayer.authoritativeHost=false;state_.multiplayer.connected=true;state_.multiplayer.localPlayerId=std::max(1,std::min(NETWORK_PLAYER_COUNT-1,localPlayerId));std::snprintf(state_.multiplayer.status.data(),state_.multiplayer.status.size(),"CONNECTED");}
void Game::disableNetwork(){state_.multiplayer=MultiplayerRuntimeState{};}
void Game::setNetworkRoom(const char* code,const char* status,bool connected){const bool changed=(code&&std::strncmp(state_.multiplayer.roomCode.data(),code,6)!=0)||(status&&std::strncmp(state_.multiplayer.status.data(),status,63)!=0)||state_.multiplayer.connected!=connected;if(code)std::snprintf(state_.multiplayer.roomCode.data(),state_.multiplayer.roomCode.size(),"%.6s",code);if(status)std::snprintf(state_.multiplayer.status.data(),state_.multiplayer.status.size(),"%.63s",status);state_.multiplayer.connected=connected;if(changed)state_.cinematic.textInteraction=0.42f;}
void Game::setNetworkPeerActive(int playerId,bool active){if(playerId<0||playerId>=NETWORK_PLAYER_COUNT||playerId==state_.multiplayer.localPlayerId)return;auto& peer=state_.multiplayer.peers[playerId];if(active&&!peer.active){peer=NetworkPeerState{};peer.active=true;peer.playerId=playerId;peer.player.pos=state_.player.pos+Vec3{static_cast<float>(playerId)*0.75f,0,0};peer.camera=state_.camera;peer.energy=state_.energy;}else if(!active)peer=NetworkPeerState{};}
PlayerCommand Game::capturePlayerCommand(unsigned int sequence,unsigned int localTick) const {
    const InputState& input=state_.input;
    PlayerCommand command;
    command.sequence=sequence;command.localTick=localTick;
    command.moveX=clampf((input.right?1.0f:0.0f)-(input.left?1.0f:0.0f)+input.touchMoveX,-1.0f,1.0f);
    command.moveZ=clampf((input.forward?1.0f:0.0f)-(input.back?1.0f:0.0f)+input.touchMoveZ,-1.0f,1.0f);
    command.yaw=state_.camera.yaw;command.pitch=state_.camera.pitch;
    if(input.forward)command.buttons|=CommandForward;if(input.back)command.buttons|=CommandBack;
    if(input.left)command.buttons|=CommandLeft;if(input.right)command.buttons|=CommandRight;
    if(input.sprint||input.touchSprint)command.buttons|=CommandSprint;
    if(input.jumpPressed)command.buttons|=CommandJump;
    if(input.primaryHeld||input.touchPrimaryHeld)command.buttons|=CommandVacuum;
    if(input.meleePressed)command.buttons|=CommandMelee;if(input.shootPressed)command.buttons|=CommandShoot;
    if(input.cameraTogglePressed)command.buttons|=CommandCameraToggle;
    if(input.wiggleAxis<0)command.buttons|=CommandWiggleLeft;else if(input.wiggleAxis>0)command.buttons|=CommandWiggleRight;
    if(input.commSignalPressed==1)command.buttons|=CommandCommHelp;
    else if(input.commSignalPressed==2)command.buttons|=CommandCommPing;
    else if(input.commSignalPressed==3)command.buttons|=CommandCommGroup;
    else if(input.commSignalPressed==4)command.buttons|=CommandCommOk;
    return command;
}
void Game::setNetworkPeerCommand(int playerId,const PlayerCommand& command){
    setNetworkPeerInput(playerId,command.sequence,command.moveX,command.moveZ,command.yaw,command.pitch,command.buttons);
}
void Game::setNetworkPeerInput(int playerId,unsigned int sequence,float moveX,float moveZ,float yaw,float pitch,unsigned short buttons){if(playerId<=0||playerId>=NETWORK_PLAYER_COUNT||!state_.multiplayer.authoritativeHost)return;setNetworkPeerActive(playerId,true);auto& peer=state_.multiplayer.peers[playerId];if(sequence<=peer.lastInputSequence)return;const unsigned short previous=peer.inputButtons;peer.lastInputSequence=sequence;peer.inputButtons=buttons;peer.input.touchMoveX=clampf(moveX,-1,1);peer.input.touchMoveZ=clampf(moveZ,-1,1);peer.input.touchSprint=(buttons&(1u<<4))!=0;peer.input.touchPrimaryHeld=(buttons&(1u<<6))!=0;peer.input.jumpPressed=(buttons&(1u<<5))!=0&&(previous&(1u<<5))==0;peer.input.meleePressed=(buttons&(1u<<7))!=0&&(previous&(1u<<7))==0;peer.input.shootPressed=(buttons&(1u<<8))!=0&&(previous&(1u<<8))==0;peer.input.cameraTogglePressed=(buttons&(1u<<9))!=0&&(previous&(1u<<9))==0;if((buttons&(1u<<10))!=0)peer.input.wiggleAxis=-1.0f;else if((buttons&(1u<<11))!=0)peer.input.wiggleAxis=1.0f;for(int signal=1;signal<=4;++signal){const unsigned short bit=static_cast<unsigned short>(1u<<(11+signal));if((buttons&bit)!=0&&(previous&bit)==0)peer.input.commSignalPressed=signal;}peer.camera.yaw=yaw;peer.camera.pitch=clampf(pitch,-DB_PI*0.48f,DB_PI*0.48f);}
void Game::applyNetworkPeerSnapshot(int playerId,const PlayerState& player,float pitch,float vacuumPower,float vacuumPose,int vacuumTarget,float meleeTimer,float dischargeAmount){if(playerId<0||playerId>=NETWORK_PLAYER_COUNT)return;if(playerId==state_.multiplayer.localPlayerId){state_.player=player;state_.camera.pitch=pitch;state_.vacuum.power=vacuumPower;state_.vacuum.pose=vacuumPose;state_.vacuum.target=vacuumTarget;state_.meleeVisual.visualTimer=meleeTimer;state_.energy.dischargePositionAmount=dischargeAmount;updatePhoneTransform();return;}setNetworkPeerActive(playerId,true);auto& peer=state_.multiplayer.peers[playerId];peer.player=player;peer.camera.pitch=pitch;peer.vacuum.power=vacuumPower;peer.vacuum.pose=vacuumPose;peer.vacuum.target=vacuumTarget;peer.meleeVisual.visualTimer=meleeTimer;peer.energy.dischargePositionAmount=dischargeAmount;peer.phoneVisual=makePhoneVisualState(vacuumPose,vacuumPower,0,state_.time,false);peer.phoneTransform.position=player.pos+Vec3{0,0.54f,0};peer.phoneTransform.orientation=quatAxisAngle({0,1,0},player.yaw);peer.phoneTransform.screenRight=rotate(peer.phoneTransform.orientation,{1,0,0});peer.phoneTransform.screenUp=rotate(peer.phoneTransform.orientation,{0,1,0});peer.phoneTransform.screenNormal=rotate(peer.phoneTransform.orientation,{0,0,1});peer.phoneTransform.screenCenter=peer.phoneTransform.position+peer.phoneTransform.screenNormal*PHONE_SCREEN_Z_OFFSET;peer.phoneTransform.vacuumPullPoint=peer.phoneTransform.screenCenter+peer.phoneTransform.screenNormal*0.24f;}

void Game::savePlayerContext(NetworkPeerState& c) const{c.input=state_.input;c.player=state_.player;c.energy=state_.energy;c.camera=state_.camera;c.vacuum=state_.vacuum;c.pendingShots=state_.pendingShots;c.phonePose=state_.phonePose;c.phoneTransform=state_.phoneTransform;c.phoneVisual=state_.phoneVisual;c.hud=state_.hud;c.meleeVisual=state_.meleeVisual;c.meleeCooldown=state_.meleeCooldown;c.meleePose=state_.meleePose;c.meleeComboWindow=state_.meleeComboWindow;}
void Game::loadPlayerContext(const NetworkPeerState& c){state_.input=c.input;state_.player=c.player;state_.energy=c.energy;state_.camera=c.camera;state_.vacuum=c.vacuum;state_.pendingShots=c.pendingShots;state_.phonePose=c.phonePose;state_.phoneTransform=c.phoneTransform;state_.phoneVisual=c.phoneVisual;state_.hud=c.hud;state_.meleeVisual=c.meleeVisual;state_.meleeCooldown=c.meleeCooldown;state_.meleePose=c.meleePose;state_.meleeComboWindow=c.meleeComboWindow;}
void Game::updateNetworkPeers(float dt){if(!state_.multiplayer.enabled||!state_.multiplayer.authoritativeHost)return;NetworkPeerState local;savePlayerContext(local);for(int id=1;id<NETWORK_PLAYER_COUNT;++id){auto& peer=state_.multiplayer.peers[id];if(!peer.active)continue;loadPlayerContext(peer);simulationPlayerId_=id;updateInputActions(dt);updatePlayer(dt);state_.phoneVisual=makePhoneVisualState(state_.vacuum.pose,state_.vacuum.power,0,state_.time,state_.camera.firstPerson);updatePhoneTransform();if(state_.meleeVisual.airLungeLandingPending)applyMeleeHits();processPendingShots(dt);updateVacuum(dt);processQueuedSoulCaptures();updateBattery(dt);updateCamera(dt);savePlayerContext(peer);}simulationPlayerId_=0;loadPlayerContext(local);}
void Game::updateTeamRevival(float dt){if(!state_.multiplayer.enabled||!state_.multiplayer.authoritativeHost)return;PlayerState* players[NETWORK_PLAYER_COUNT]={&state_.player};InputState* inputs[NETWORK_PLAYER_COUNT]={&state_.input};bool active[NETWORK_PLAYER_COUNT]={true};for(int id=1;id<NETWORK_PLAYER_COUNT;++id){players[id]=&state_.multiplayer.peers[id].player;inputs[id]=&state_.multiplayer.peers[id].input;active[id]=state_.multiplayer.peers[id].active;}for(int donor=0;donor<NETWORK_PLAYER_COUNT;++donor){PlayerState& source=*players[donor];if(!active[donor]||!source.alive||source.downed||source.battery<=10.0f||!(inputs[donor]->primaryHeld||inputs[donor]->touchPrimaryHeld))continue;int recipient=-1;float best=2.2f;const Vec3 forward{-std::sin(source.yaw),0,-std::cos(source.yaw)};for(int id=0;id<NETWORK_PLAYER_COUNT;++id){if(id==donor||!active[id]||!players[id]->alive||!players[id]->downed)continue;const Vec3 delta{players[id]->pos.x-source.pos.x,0,players[id]->pos.z-source.pos.z};const float distance=length(delta);if(distance<best&&distance>0.001f&&dot3(delta*(1.0f/distance),forward)>0.72f){best=distance;recipient=id;}}if(recipient<0)continue;const float spend=std::min(source.battery-10.0f,10.0f*dt);source.battery-=spend;PlayerState& target=*players[recipient];target.reviveCharge+=spend*0.8f;target.battery=target.reviveCharge;if(target.reviveCharge>=18.0f){target.battery=target.reviveCharge;target.downed=false;target.bleedoutTimer=0.0f;target.reviveCharge=0.0f;target.alive=true;target.pos.y=GROUND_Y+PHONE_BODY_HEIGHT*0.5f;target.grounded=true;if(recipient==0){setEnergyTicker("SIGNAL RESTORED",2);emitAudio(AudioCue::RewardWoah,0.42f);}}}}
void Game::updateSecretTv(float dt) {
    auto& tv = state_.secretTv;
    tv.available = state_.roomClear && state_.roomIndex == 10;
    tv.donationCooldown = std::max(0.0f, tv.donationCooldown - dt);
    tv.knockCueTimer = std::max(0.0f, tv.knockCueTimer - dt);
    tv.knockVolume = 0.0f;
    tv.knockPulse = 0.0f;
    tv.knockPan = 0.0f;

    const bool localCanHear = tv.available && state_.player.alive && !state_.player.downed &&
        !state_.player.inSecretRoom && state_.player.secretVisitRoom != state_.roomIndex;
    if (localCanHear) {
        const Vec3 entrance{tv.entrancePos.x, state_.player.pos.y, tv.entrancePos.z};
        const Vec3 delta = state_.player.pos - entrance;
        const float distance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
        const float nearFade = smoothRange(distance, 2.35f, 7.5f);
        const float farFade = 1.0f - smoothRange(distance, 26.0f, 36.0f);
        const float seededTime = state_.time + static_cast<float>(std::abs(state_.roomSeed % 17)) * 0.071f;
        const bool cueActive = tv.knockCueTimer > 0.0f;
        const float knockTime = cueActive ? 5.4f - tv.knockCueTimer : seededTime;
        const float pulse = secretDoorKnockPulse(knockTime);
        const bool phraseActive = cueActive || secretDoorKnockPhraseActive(seededTime);
        const float presence = nearFade * farFade;
        tv.knockPulse = presence * pulse;
        tv.knockVolume = phraseActive ? presence * (0.62f + 0.16f * pulse) : 0.0f;
        const Vec3 cameraRight{std::cos(state_.camera.yaw), 0.0f, -std::sin(state_.camera.yaw)};
        tv.knockPan = clampf(dot3(entrance - state_.camera.pos, cameraRight) / 12.0f, -0.55f, 0.55f);
    }

    PlayerState* players[NETWORK_PLAYER_COUNT] = {&state_.player};
    InputState* inputs[NETWORK_PLAYER_COUNT] = {&state_.input};
    bool active[NETWORK_PLAYER_COUNT] = {true};
    for (int id = 1; id < NETWORK_PLAYER_COUNT; ++id) {
        players[id] = &state_.multiplayer.peers[id].player;
        inputs[id] = &state_.multiplayer.peers[id].input;
        active[id] = state_.multiplayer.peers[id].active;
    }
    for (int id = 0; id < NETWORK_PLAYER_COUNT; ++id) {
        PlayerState& player = *players[id];
        if (!active[id] || !player.alive || player.downed) continue;
        if (!player.inSecretRoom && tv.available && player.secretVisitRoom != state_.roomIndex) {
            const Vec3 delta = player.pos - tv.entrancePos;
            const Vec3 tangent{-tv.entranceNormal.z, 0.0f, tv.entranceNormal.x};
            const float across = dot3(delta, tangent);
            const float depth = dot3(delta, tv.entranceNormal);
            if (std::abs(across) < 1.15f && depth > -0.35f && depth < 1.65f) {
                player.inSecretRoom = true;
                player.secretVisitRoom = state_.roomIndex;
                player.secretVisitTimer = 120.0f;
                player.pos = {38.95f, GROUND_Y + PHONE_BODY_HEIGHT * 0.5f, 0};
                player.vel = {};
                if (id == 0) {
                    state_.camera.yaw = -DB_PI * 0.5f;
                    state_.camera.pitch = 0.0f;
                    updatePhoneTransform();
                    updateCamera(0.0f);
                } else if (id > 0 && id < NETWORK_PLAYER_COUNT) {
                    state_.multiplayer.peers[id].camera.yaw = -DB_PI * 0.5f;
                    state_.multiplayer.peers[id].camera.pitch = 0.0f;
                }
            }
        }
        if (!player.inSecretRoom) continue;
        player.secretVisitTimer = std::max(0.0f, player.secretVisitTimer - dt);
        if (inputs[id]->shootPressed && player.souls > 0 && !tv.broken && tv.donationCooldown <= 0.0f) {
            inputs[id]->shootPressed = false;
            --player.souls;
            ++tv.signal;
            tv.donationCooldown = 0.70f;
            const int denominator = tv.signal < 6 ? 12 : tv.signal < 12 ? 9 : tv.signal < 18 ? 7 : tv.signal < 24 ? 5 : 4;
            const float roll = seededRoomValue(static_cast<float>(state_.roomSeed) * 0.17f + static_cast<float>(tv.signal) * 13.71f + static_cast<float>(tv.damage) * 31.3f);
            if (roll < 1.0f / static_cast<float>(denominator)) {
                ++tv.damage;
                if (tv.damage >= tv.tolerance) tv.broken = true;
            }
            if (id == 0) {
                setEnergyTicker(tv.broken ? "NO SIGNAL" : "SIGNAL +1", tv.broken ? 1 : 2);
                if (!tv.broken) emitAudio(AudioCue::RewardNice, 0.30f);
            }
        }
        if (player.secretVisitTimer <= 0.0f || player.pos.x >= 43.45f) {
            player.inSecretRoom = false;
            player.pos = tv.entrancePos + tv.entranceNormal * 0.72f;
            player.pos.y = GROUND_Y + PHONE_BODY_HEIGHT * 0.5f;
            player.vel = {};
        }
    }
}

void Game::setWiggle(float axis) {
    InputState& input=state_.input;
    if(state_.player.grabbedByTarget<0)return;
    if(std::abs(axis)<0.001f)axis=state_.player.grabLastDirection>=0?-1.0f:1.0f;
    input.wiggleAxis=axis>0.0f?1.0f:-1.0f;
}
void Game::setCommSignal(int signal) {
    if(signal<1||signal>4)return;
    state_.input.commSignalPressed=signal;
}
void Game::updateNetworkGuest(float dt){
    simulationPlayerId_=state_.multiplayer.localPlayerId;
    const bool melee=state_.input.meleePressed,shoot=state_.input.shootPressed;
    if(melee&&!state_.meleeVisual.airLungeLandingPending&&!state_.vacuum.active)
        triggerMelee(false);
    state_.input.meleePressed=false;state_.input.shootPressed=false;
    updateInputActions(dt);
    state_.input.meleePressed=melee;state_.input.shootPressed=shoot;
    updatePlayer(dt);
    const Vec3 correctionStep=state_.multiplayer.localPredictionCorrection*
        std::min(1.0f,dt*NETWORK_LOCAL_CORRECTION_SMOOTHING_RATE);
    state_.player.pos+=correctionStep;
    state_.multiplayer.localPredictionCorrection-=correctionStep;
    for(auto& target:state_.targets)syncSoulVisual(target,state_.time);
    const bool vacuumBlocked=state_.player.ledgeHanging||
        state_.player.ledgeMantleTimer>0.0f||
        state_.meleeVisual.airLungeLandingPending||
        state_.meleeVisual.visualTimer>0.0f||
        state_.phonePose.doubleJumpVacuumPause>0.0f;
    state_.vacuum.active=(state_.input.primaryHeld||state_.input.touchPrimaryHeld)&&
        state_.player.alive&&state_.player.battery>1&&!vacuumBlocked;
    state_.vacuum.power=clampf(state_.vacuum.power+
        (state_.vacuum.active?2.4f:-3.2f)*dt,0,1);
    state_.vacuum.pose+=((state_.vacuum.active?1.0f:0.0f)-state_.vacuum.pose)*
        std::min(1.0f,dt*10.0f);
    state_.vacuum.fieldStrength+=((state_.vacuum.active?1.0f:0.0f)-
        state_.vacuum.fieldStrength)*std::min(1.0f,dt*5.0f);
    if(shoot&&state_.player.souls>0&&!state_.vacuum.active)
        state_.energy.dischargeTimer=std::max(state_.energy.dischargeTimer,0.34f);
    else
        state_.energy.dischargeTimer=std::max(0.0f,state_.energy.dischargeTimer-dt);
    state_.energy.dischargePositionAmount+=
        ((state_.energy.dischargeTimer>0.0f?1.0f:0.0f)-
         state_.energy.dischargePositionAmount)*std::min(1.0f,dt*12.0f);
    state_.phoneVisual=makePhoneVisualState(state_.vacuum.pose,state_.vacuum.power,
        0,state_.time,state_.camera.firstPerson);
    const float phoneActionAmount=std::max(state_.vacuum.pose,
        state_.energy.dischargePositionAmount);
    state_.phoneVisual.actionLift=phoneActionAmount*0.65f;
    state_.phoneVisual.actionForward=phoneActionAmount*0.25f;
    updatePhoneTransform();
    // Prediction may show contact, but only the host mutates enemy outcomes.
    updateCamera(dt);updateSoulLattices();updateCrosshair(dt);
    state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0,1);
    state_.hud.storedSouls=state_.player.souls;
    state_.input.meleePressed=false;state_.input.shootPressed=false;
    simulationPlayerId_=0;
}

void Game::updateInputActions(float dt) {
    InputState& input = state_.input;
    state_.player.commSignalTimer=std::max(0.0f,state_.player.commSignalTimer-dt);
    if(state_.player.commSignalTimer<=0.0f)state_.player.commSignal=0;
    if(input.commSignalPressed>=1&&input.commSignalPressed<=4){
        state_.player.commSignal=input.commSignalPressed;
        state_.player.commSignalTimer=2.4f;
        input.commSignalPressed=0;
    }
    if(state_.player.grabbedByTarget>=0){input.jumpPressed=false;input.meleePressed=false;input.shootPressed=false;input.cameraTogglePressed=false;input.primaryHeld=false;input.touchPrimaryHeld=false;input.lookDeltaX=input.lookDeltaY=0.0f;return;}
    if (input.cameraTogglePressed) state_.camera.firstPerson = !state_.camera.firstPerson;
    state_.camera.yaw -= input.lookDeltaX * 0.003f;
    state_.camera.pitch = clampf(state_.camera.pitch - input.lookDeltaY * 0.003f, -DB_PI * 0.48f, DB_PI * 0.48f);
    input.lookDeltaX = input.lookDeltaY = 0.0f;
    MeleeVisualState& action=state_.meleeVisual;
    const bool lungeOwned=action.airLungeLandingPending;
    if(input.jumpPressed&&state_.player.ledgeHanging){
        float forwardAxis=(input.forward?1.0f:0.0f)-(input.back?1.0f:0.0f)+input.touchMoveZ;
        float strafeAxis=(input.right?1.0f:0.0f)-(input.left?1.0f:0.0f)+input.touchMoveX;
        Vec3 move=cameraForwardFlat()*forwardAxis+cameraRightFlat()*strafeAxis;
        if(lengthSq(move)>0.0001f)move=normalized(move);
        releaseLedgeHang(dotXZ(move,state_.player.ledgeNormal*-1.0f)>0.25f);
    }else if(input.jumpPressed&&action.wallGripTimer>0.0f&&spendBattery(BATTERY_DOUBLE_JUMP_COST,BatteryReason::DoubleJump)){
        state_.player.vel+=action.wallNormal*6.0f;
        state_.player.jumpVel=AIR_JUMP_SPEED;
        state_.player.grounded=false;
        action.wallGripTimer=0.0f;
        action.airLungeLandingPending=false;
        action.locomotionLunge=false;
        state_.vacuum.active=false;
    }else if(input.jumpPressed&&!lungeOwned){
        state_.player.ledgeMantleTimer=0.0f;
        state_.vacuum.active=false;
        state_.player.jumpBufferTimer=JUMP_BUFFER;
        tryJump();
    }
    // Air melee is an evasive locomotion cancel. Ground melee waits for a held
    // vacuum/slurp beat to finish instead of producing two actions at once.
    if(input.meleePressed&&state_.player.ledgeHanging){
        PlayerState& p=state_.player;
        const Vec3 launch=p.ledgeNormal*1.8f+p.ledgeTangent*p.ledgeShimmySpeed;
        p.pos+=p.ledgeNormal*(PLAYER_SUPPORT_RADIUS+0.04f);
        p.ledgeHanging=false; p.ledgeCollider=-1; p.ledgeGrabCooldown=LEDGE_REGRAB_COOLDOWN;
        p.grounded=false; p.jumpVel=std::max(p.jumpVel,2.3f); p.vel=launch;
    }
    if(input.meleePressed&&!lungeOwned){
        const bool airborne=!state_.player.grounded;
        if(airborne||!state_.vacuum.active){
            if(airborne)state_.vacuum.active=false;
            triggerMelee();
        }
    }
    const bool committedAfterInput=action.airLungeLandingPending;
    if(input.shootPressed&&!committedAfterInput&&!state_.vacuum.active)shootStoredSoul();
    input.cameraTogglePressed = input.jumpPressed = input.meleePressed = input.shootPressed = false;
    state_.meleeCooldown = std::max(0.0f, state_.meleeCooldown - dt);
    state_.meleeComboWindow = std::max(0.0f, state_.meleeComboWindow - dt);
    state_.meleeVisual.visualTimer = std::max(0.0f, state_.meleeVisual.visualTimer - dt);
    state_.meleePose = std::max(0.0f, state_.meleePose - dt * 5.5f);
    const bool attackOwned=action.visualTimer>0.0f;
    const bool jumpVacuumBlocked=state_.phonePose.doubleJumpVacuumPause>0.0f;
    state_.vacuum.active=(input.primaryHeld||input.touchPrimaryHeld)&&state_.player.alive&&state_.player.battery>1.0f
        && !action.airLungeLandingPending && !attackOwned && !jumpVacuumBlocked && !state_.player.ledgeHanging && state_.player.ledgeMantleTimer<=0.0f;
    if (state_.player.souls >= MAX_STORED_SOULS) state_.vacuum.active = false;
}

Vec3 Game::cameraForwardFlat() const {
    return normalized({-std::sin(state_.camera.yaw), 0.0f, -std::cos(state_.camera.yaw)});
}
Vec3 Game::cameraRightFlat() const {
    return normalized({std::cos(state_.camera.yaw), 0.0f, -std::sin(state_.camera.yaw)});
}

float Game::getPlayerCeilingLimit() const {
    return ROOM_WALL_HEIGHT - GROUND_Y - CEILING_CLEARANCE - PLAYER_CEILING_BODY_CLEARANCE;
}

float Game::getPlayerSupportY(float x, float z) const {
    float supportY = GROUND_Y;
    const float radius = PLAYER_SUPPORT_RADIUS;
    const float localZ = wrapZ(z);
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const RoomCollider& c = state_.roomColliders[i];
        if (x > c.minX - radius && x < c.maxX + radius && localZ > c.minZ - radius && localZ < c.maxZ + radius)
            supportY = std::max(supportY, c.topY + GROUND_Y);
    }
    return std::min(supportY, getPlayerCeilingLimit());
}

void Game::resolvePlayerObstacleCollisions() {
    PlayerState& player = state_.player;
    const float radius = PLAYER_COLLISION_RADIUS;
    const float tileOriginZ = getRoomTileOriginZ(getRoomTileIndex(player.pos.z));
    float localPlayerZ = player.pos.z - tileOriginZ;
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const RoomCollider& c = state_.roomColliders[i];
        const bool onTop = player.pos.y >= c.topY + GROUND_Y - 0.08f;
        if (onTop) continue;
        if (player.pos.y < c.bottomY - 0.4f || player.pos.y > c.topY + GROUND_Y + 0.4f) continue;
        if (player.pos.x > c.minX - radius && player.pos.x < c.maxX + radius && localPlayerZ > c.minZ - radius && localPlayerZ < c.maxZ + radius) {
            const Vec3 incoming=player.vel;
            const float pushes[4] = {
                std::abs(player.pos.x - (c.minX - radius)), std::abs((c.maxX + radius) - player.pos.x),
                std::abs(localPlayerZ - (c.minZ - radius)), std::abs((c.maxZ + radius) - localPlayerZ)
            };
            int axis = 0; for (int p = 1; p < 4; ++p) if (pushes[p] < pushes[axis]) axis = p;
            Vec3 normal;
            if (axis == 0) { normal={-1,0,0};player.pos.x = c.minX - radius; if (player.vel.x > 0) player.vel.x = 0; }
            else if (axis == 1) { normal={1,0,0};player.pos.x = c.maxX + radius; if (player.vel.x < 0) player.vel.x = 0; }
            else if (axis == 2) { normal={0,0,-1};localPlayerZ = c.minZ - radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z > 0) player.vel.z = 0; }
            else { normal={0,0,1};localPlayerZ = c.maxZ + radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z < 0) player.vel.z = 0; }
            if(state_.meleeVisual.airLungeLandingPending){
                const float headOn=std::abs(dotXZ(normalized(incoming),normal));
                const Vec3 tangent=incoming-normal*dotXZ(incoming,normal);
                player.vel=tangent*(headOn>0.72f?0.62f:0.88f);
                state_.meleeVisual.wallNormal=normal;
                if(headOn>0.72f)state_.meleeVisual.wallGripTimer=AIR_MELEE_WALL_GRIP_TIME;
            }
        }
    }
}

void Game::resolveDoorwayCollisions(float previousX,float previousZ){
    PlayerState& player=state_.player;
    const float radius=PLAYER_COLLISION_RADIUS;
    const float safeHalfWidth=2.1f-radius;
    const int previousTile=getRoomTileIndex(previousZ);
    const int currentTile=getRoomTileIndex(player.pos.z);
    const bool apertureHeight=player.pos.y>=GROUND_Y-0.12f&&player.pos.y<=3.72f+0.22f;
    if(previousTile!=currentTile&&(!apertureHeight||std::abs(player.pos.x)>safeHalfWidth)){
        const float seam=previousTile<currentTile
            ? getRoomTileOriginZ(previousTile)+ROOM_DEPTH*0.5f
            : getRoomTileOriginZ(previousTile)-ROOM_DEPTH*0.5f;
        player.pos.z=seam+(previousTile<currentTile?-radius:radius);
        if((previousTile<currentTile&&player.vel.z>0.0f)||(previousTile>currentTile&&player.vel.z<0.0f))player.vel.z=0.0f;
        return;
    }
    const float localZ=wrapZ(player.pos.z);
    const float seamDistance=ROOM_DEPTH*0.5f-std::abs(localZ);
    if(seamDistance>=radius)return;
    if(!apertureHeight){
        const float seamSign=localZ<0.0f?-1.0f:1.0f;
        player.pos.z=getRoomTileOriginZ(getRoomTileIndex(player.pos.z))+seamSign*(ROOM_DEPTH*0.5f-radius);
        if((seamSign<0.0f&&player.vel.z<0.0f)||(seamSign>0.0f&&player.vel.z>0.0f))player.vel.z=0.0f;
        return;
    }
    if(std::abs(previousX)<=safeHalfWidth&&std::abs(player.pos.x)>safeHalfWidth){
        player.pos.x=(player.pos.x<0.0f?-1.0f:1.0f)*safeHalfWidth;
        if((player.pos.x<0.0f&&player.vel.x<0.0f)||(player.pos.x>0.0f&&player.vel.x>0.0f))player.vel.x=0.0f;
    }else if(std::abs(player.pos.x)>safeHalfWidth){
        const float seamSign=localZ<0.0f?-1.0f:1.0f;
        player.pos.z=getRoomTileOriginZ(getRoomTileIndex(player.pos.z))+seamSign*(ROOM_DEPTH*0.5f-radius);
        if((seamSign<0.0f&&player.vel.z<0.0f)||(seamSign>0.0f&&player.vel.z>0.0f))player.vel.z=0.0f;
    }
}

void Game::applyWallClimb(float dt) {
    (void)dt;
}
bool Game::isInsideDoorAperture(const Vec3& position, float pad) const {
    return std::abs(position.x) <= 2.1f + pad && position.y >= GROUND_Y - 0.12f && position.y <= 3.72f + 0.22f;
}

void Game::updateRoomTopology(float previousZ, float currentZ) {
    const int previousTile = getRoomTileIndex(previousZ);
    const int currentTile = getRoomTileIndex(currentZ);
    state_.topology.previousTileIndex = previousTile;
    state_.topology.currentTileIndex = currentTile;
    state_.topology.advancing = false;
    if (previousTile == currentTile) return;
    Vec3 local = state_.player.pos; local.z = wrapZ(local.z);
    if (!isInsideDoorAperture(local, 0.04f)) return;
    if (state_.roomClear && currentTile < previousTile) {
        state_.doorTransition.active=true; state_.doorTransition.progress=1.0f;
        state_.doorTransition.distanceTravelled=0.0f; state_.doorTransition.lastPlayerPos=state_.player.pos;
        if(state_.roomIndex<std::numeric_limits<int>::max())++state_.roomIndex;
        const std::uint32_t seedStep=9973u+static_cast<std::uint32_t>(seededRoomValue(991.0f)*1000000.0f);
        state_.roomSeed=static_cast<int>(static_cast<std::uint32_t>(state_.roomSeed)+seedStep); state_.topology.advancing = true;
        advanceRunRulesForRoom();
        state_.roomClear=false;
        gainBattery(18.0f,BatteryReason::NextRoom);
        state_.player.soloSoulRebootUsed=false;
        state_.player.souls=0;
        state_.player.storedSoulBrute.fill(false);
        state_.requiredSouls=std::min(9,5+state_.runRules.requiredSlotStacks);
        state_.depositedSouls=0;
        state_.progression.run.roomHeat=0.0f;
        state_.progression.run.roomElapsed=0.0f;
        state_.progression.run.roomCaptures=0;
        state_.vacuum=VacuumState{};
        state_.meleeVisual=MeleeVisualState{};
        state_.meleeComboWindow=0.0f;
        state_.energy.dischargeTimer=0.0f;
        state_.energy.dischargePositionAmount=0.0f;
        const float startX=-((static_cast<float>(state_.requiredSouls)-1.0f)*0.82f)*0.5f;
        for(int i=0;i<CAPTURE_COUNT;++i){state_.captures[i]=CapturePointState{}; state_.captures[i].pos={startX+static_cast<float>(i)*0.82f,3.05f,ROOM_GRID_Z};}
        for(auto& bullet:state_.bullets) bullet=BulletState{};
        for(auto& pending:state_.pendingShots) pending=PendingShotState{};
        for(auto& flower:state_.flowers) flower=FlowerPowerupState{};
        buildRoomColliders();
        for(auto& request:state_.respawnQueue) request=HumanRespawnRequest{};
        for(int i=0;i<TARGET_COUNT;++i){if(i<activeHumanTarget()) respawnTarget(i); else state_.targets[i]=TargetState{};}
        state_.upgradeMenu.active=true;
        state_.uiPaused=true;
        clearInputState();
    } else if(!state_.roomClear) {
        chargeClosedDoorLoop();
    }
}

void Game::chargeClosedDoorLoop() {
    constexpr float LOW_BATTERY_THRESHOLD=24.0f;
    constexpr float LOOP_BATTERY_COST=16.0f;
    constexpr float LOOP_REGEN_LOCK=1.25f;
    auto& permanent=state_.progression.permanent;
    if(state_.player.battery>LOW_BATTERY_THRESHOLD){
        state_.progression.run.batteryRegenLock=std::max(state_.progression.run.batteryRegenLock,LOOP_REGEN_LOCK);
        spendBattery(LOOP_BATTERY_COST,BatteryReason::Loop);
        return;
    }
    if(permanent.tokens>0){
        --permanent.tokens;
        ++permanent.revision;
        setEnergyTicker("EMERGENCY LOOP -1 TOKEN",1);
        return;
    }
    setEnergyTicker("LOOP FAILURE / BATTERY EMPTY",1);
    spendBattery(state_.player.battery,BatteryReason::Loop);
}

void Game::awardGoalToken(CapturePointState& capture) {
    if(capture.tokenAwarded)return;
    capture.tokenAwarded=true;
    ++state_.progression.permanent.tokens;
    ++state_.progression.permanent.revision;
    setEnergyTicker("GOAL +1 TOKEN",2);
}

int Game::activeHumanTarget() const {
    int activePlayers=1;
    if(state_.multiplayer.authoritativeHost)for(int id=1;id<NETWORK_PLAYER_COUNT;++id)if(state_.multiplayer.peers[id].active)++activePlayers;
    const int roomExtra=std::min(ACTIVE_HUMAN_TARGET_CAP,std::max(0,state_.roomIndex-1));
    const int environmentAdjustment=early_browser_visuals::roomPlan(state_.roomSeed,state_.roomIndex).enemyAdjustment;
    return std::min(TARGET_COUNT,std::min(ACTIVE_HUMAN_TARGET_CAP,std::max(1,
        ACTIVE_HUMAN_TARGET+roomExtra+state_.runRules.crowdedRoomStacks+(activePlayers-1)*2+environmentAdjustment)));
}

void Game::advanceRunRulesForRoom() {
    int candidates[3]; int count=0;
    if(state_.runRules.requiredSlotStacks<3) candidates[count++]=0;
    if(state_.runRules.crowdedRoomStacks<4) candidates[count++]=1;
    if(state_.runRules.fasterSlurpStacks<4) candidates[count++]=2;
    if(count==0){state_.runRules.lastAdded=-1; return;}
    const float roll=seededRoomValue(static_cast<float>(state_.runRules.nextId)*19.13f+static_cast<float>(state_.roomIndex)*7.91f);
    const int chosen=candidates[static_cast<int>(roll*static_cast<float>(count))%count];
    if(chosen==0) ++state_.runRules.requiredSlotStacks;
    else if(chosen==1) ++state_.runRules.crowdedRoomStacks;
    else ++state_.runRules.fasterSlurpStacks;
    state_.runRules.lastAdded=chosen; ++state_.runRules.nextId;
}

void Game::updateDoorTransition() {
    DoorTransitionState& transition=state_.doorTransition;
    if(!transition.active) return;
    const Vec3 motion=state_.player.pos-transition.lastPlayerPos;
    transition.frameMotion=motion;
    transition.distanceTravelled+=length(motion); transition.lastPlayerPos=state_.player.pos;
    const float t=clampf(transition.distanceTravelled/DOOR_DATAMOSH_DISTANCE,0.0f,1.0f);
    transition.progress=1.0f-smooth01(t);
    if(transition.progress<=DOOR_DATAMOSH_MIN_STRENGTH){transition.active=false; transition.progress=0.0f;}
}

bool Game::tryBeginLedgeHang() {
    PlayerState& p=state_.player;
    if(p.ledgeHanging||p.grounded||p.ledgeGrabCooldown>0.0f||p.jumpVel>0.35f||!p.alive)return false;
    const float phoneTop=p.pos.y+PHONE_BODY_HEIGHT*0.5f;
    const float tileOriginZ=getRoomTileOriginZ(getRoomTileIndex(p.pos.z));
    const float localZ=p.pos.z-tileOriginZ;
    int best=-1; float bestDistance=LEDGE_GRAB_REACH;
    Vec3 bestNormal; Vec3 bestTangent;
    for(int i=0;i<state_.debug.colliderCount;++i){
        const RoomCollider& c=state_.roomColliders[i];
        if(phoneTop<c.topY-LEDGE_GRAB_VERTICAL_BELOW||phoneTop>c.topY+LEDGE_GRAB_VERTICAL_ABOVE)continue;
        auto candidate=[&](float distance,const Vec3& normal,const Vec3& tangent,bool within){
            if(within&&distance>=-0.02f&&distance<bestDistance){best=i;bestDistance=distance;bestNormal=normal;bestTangent=tangent;}
        };
        candidate(c.minX-p.pos.x,{-1,0,0},{0,0,1},localZ>c.minZ+LEDGE_CORNER_INSET&&localZ<c.maxZ-LEDGE_CORNER_INSET);
        candidate(p.pos.x-c.maxX,{1,0,0},{0,0,1},localZ>c.minZ+LEDGE_CORNER_INSET&&localZ<c.maxZ-LEDGE_CORNER_INSET);
        candidate(c.minZ-localZ,{0,0,-1},{1,0,0},p.pos.x>c.minX+LEDGE_CORNER_INSET&&p.pos.x<c.maxX-LEDGE_CORNER_INSET);
        candidate(localZ-c.maxZ,{0,0,1},{1,0,0},p.pos.x>c.minX+LEDGE_CORNER_INSET&&p.pos.x<c.maxX-LEDGE_CORNER_INSET);
    }
    if(best<0)return false;
    const RoomCollider& c=state_.roomColliders[best];
    p.ledgeHanging=true; p.ledgeCollider=best; p.ledgeNormal=bestNormal; p.ledgeTangent=bestTangent;
    p.ledgeShimmySpeed=dotXZ(p.vel,bestTangent)*0.55f; p.ledgeHangTime=0.0f;
    const float faceOffset=PHONE_BODY_DEPTH*0.5f+LEDGE_PHONE_FACE_GAP;
    if(bestNormal.x<0)p.pos.x=c.minX-faceOffset;
    else if(bestNormal.x>0)p.pos.x=c.maxX+faceOffset;
    else if(bestNormal.z<0)p.pos.z=tileOriginZ+c.minZ-faceOffset;
    else p.pos.z=tileOriginZ+c.maxZ+faceOffset;
    p.pos.y=c.topY-PHONE_BODY_HEIGHT*0.5f+0.008f;
    p.vel={0,0,0}; p.jumpVel=0.0f; p.grounded=false;
    p.yaw=p.targetYaw=std::atan2(bestNormal.x,bestNormal.z);
    // A valid descending lunge may route directly into the existing ledge
    // verbs. The catch cancels lunge ownership without manufacturing a ground
    // impact or its recovery, while the incoming tangent speed is already
    // preserved above as shimmy momentum.
    if(state_.meleeVisual.airLungeLandingPending){
        MeleeVisualState& lunge=state_.meleeVisual;
        lunge.airLungePending=false;
        lunge.airLungeLandingPending=false;
        lunge.locomotionLunge=false;
        lunge.airLungeTimer=0.0f;
        lunge.visualTimer=0.0f;
        lunge.airLungeAngularVelocity=0.0f;
        lunge.wallGripTimer=0.0f;
        lunge.contactPositionValid=false;
        lunge.landingRecovery=0.0f;
        lunge.landingRecoveryDuration=0.0f;
    }
    state_.vacuum.active=false;
    return true;
}

void Game::releaseLedgeHang(bool mantle) {
    PlayerState& p=state_.player;
    if(!p.ledgeHanging)return;
    const int collider=p.ledgeCollider; const Vec3 normal=p.ledgeNormal; const Vec3 tangent=p.ledgeTangent;
    const float shimmy=p.ledgeShimmySpeed;
    p.ledgeHanging=false; p.ledgeCollider=-1; p.ledgeGrabCooldown=LEDGE_REGRAB_COOLDOWN;
    if(mantle&&collider>=0&&collider<state_.debug.colliderCount){
        const RoomCollider& c=state_.roomColliders[collider];
        p.pos-=normal*(PLAYER_SUPPORT_RADIUS+PHONE_BODY_DEPTH*0.5f+0.08f);
        p.pos.y=c.topY+GROUND_Y; p.grounded=true; p.jumpVel=0.0f;
        p.vel=tangent*(shimmy*0.70f)-normal*1.35f;
        p.ledgeMantleTimer=LEDGE_MANTLE_DURATION;
        p.coyoteTimer=COYOTE_TIME; p.airJumpsRemaining=1;
    }else{
        p.pos+=normal*(PLAYER_SUPPORT_RADIUS+0.04f);
        p.grounded=false; p.jumpVel=LEDGE_VAULT_UP_SPEED;
        p.vel=normal*LEDGE_VAULT_OUT_SPEED+tangent*(shimmy*1.10f);
    }
}

bool Game::updateLedgeHang(float dt,float forwardAxis,float strafeAxis) {
    PlayerState& p=state_.player;
    if(!p.ledgeHanging)return false;
    if(p.ledgeCollider<0||p.ledgeCollider>=state_.debug.colliderCount){p.ledgeHanging=false;return false;}
    p.ledgeHangTime+=dt;
    Vec3 move=cameraForwardFlat()*forwardAxis+cameraRightFlat()*strafeAxis;
    if(lengthSq(move)>1.0f)move=normalized(move);
    const float away=dotXZ(move,p.ledgeNormal);
    if(-away>0.72f&&p.ledgeHangTime>0.12f){releaseLedgeHang(true);return false;}
    if(away>0.62f&&p.ledgeHangTime>0.08f){
        const Vec3 normal=p.ledgeNormal,tangent=p.ledgeTangent; const float shimmy=p.ledgeShimmySpeed;
        p.pos+=normal*(PLAYER_SUPPORT_RADIUS+0.04f);
        p.ledgeHanging=false;p.ledgeCollider=-1;p.ledgeGrabCooldown=LEDGE_REGRAB_COOLDOWN;
        p.grounded=false;p.jumpVel=-0.7f;p.vel=normal*1.1f+tangent*shimmy;
        return false;
    }
    const float intent=dotXZ(move,p.ledgeTangent);
    p.ledgeShimmySpeed+=intent*LEDGE_SHIMMY_ACCEL*dt;
    p.ledgeShimmySpeed=clampf(p.ledgeShimmySpeed,-LEDGE_SHIMMY_MAX_SPEED,LEDGE_SHIMMY_MAX_SPEED);
    p.ledgeShimmySpeed*=std::pow(LEDGE_SHIMMY_DAMPING,dt*60.0f);
    p.pos+=p.ledgeTangent*(p.ledgeShimmySpeed*dt);
    const RoomCollider& c=state_.roomColliders[p.ledgeCollider];
    const float tileOriginZ=getRoomTileOriginZ(getRoomTileIndex(p.pos.z));
    if(std::abs(p.ledgeTangent.x)>0.5f)p.pos.x=clampf(p.pos.x,c.minX+LEDGE_CORNER_INSET,c.maxX-LEDGE_CORNER_INSET);
    else p.pos.z=tileOriginZ+clampf(p.pos.z-tileOriginZ,c.minZ+LEDGE_CORNER_INSET,c.maxZ-LEDGE_CORNER_INSET);
    p.pos.y=c.topY-PHONE_BODY_HEIGHT*0.5f+0.008f;
    p.vel={0,0,0};p.jumpVel=0.0f;p.grounded=false;
    p.yaw=p.targetYaw=std::atan2(p.ledgeNormal.x,p.ledgeNormal.z);
    return true;
}

void Game::updatePlayer(float dt) {
    PlayerState& p = state_.player;
    InputState& input = state_.input;
    if(!p.alive){p.vel={};p.jumpVel=0.0f;return;}
    if(p.downed){p.vel={};p.jumpVel=0.0f;p.grounded=true;p.pos.y=GROUND_Y+PHONE_BODY_DEPTH*0.5f;state_.vacuum=VacuumState{};state_.meleeVisual=MeleeVisualState{};return;}
    if(p.inSecretRoom){const float forwardAxis=(input.forward?1.0f:0.0f)-(input.back?1.0f:0.0f)+input.touchMoveZ,strafeAxis=(input.right?1.0f:0.0f)-(input.left?1.0f:0.0f)+input.touchMoveX;Vec3 motion=cameraForwardFlat()*forwardAxis+cameraRightFlat()*strafeAxis;if(lengthSq(motion)>1)motion=normalized(motion);p.pos+=motion*(2.85f*dt);p.pos.x=clampf(p.pos.x,37.35f,43.55f);p.pos.z=clampf(p.pos.z,-2.45f,2.45f);p.pos.y=GROUND_Y+PHONE_BODY_HEIGHT*0.5f;p.vel=motion*2.85f;p.jumpVel=0;p.grounded=true;p.targetYaw=state_.camera.yaw;p.yaw=state_.camera.yaw;updatePhoneGait(dt,false);updatePhoneActionPose(dt,false,forwardAxis,strafeAxis);state_.debug.supportY=p.pos.y;state_.debug.localZ=wrapZ(p.pos.z);state_.debug.horizontalSpeed=horizontalLength(p.vel);state_.debug.cameraYaw=state_.camera.yaw;state_.debug.cameraPitch=state_.camera.pitch;state_.debug.cameraMode=state_.camera.firstPerson?1:0;state_.debug.phoneYaw=state_.phonePose.yaw;state_.debug.phonePitch=state_.phonePose.pitch;state_.debug.phoneRoll=state_.phonePose.roll;state_.debug.phoneLift=state_.phonePose.lift;state_.debug.phoneForward=state_.phonePose.forward;state_.debug.phoneSide=state_.phonePose.side;return;}
    const float previousX = p.pos.x;
    const float previousZ = p.pos.z;
    if (p.jumpBufferTimer > 0) p.jumpBufferTimer = std::max(0.0f, p.jumpBufferTimer - dt);
    p.ledgeGrabCooldown=std::max(0.0f,p.ledgeGrabCooldown-dt);
    p.ledgeMantleTimer=std::max(0.0f,p.ledgeMantleTimer-dt);
    if (p.grounded) p.coyoteTimer = COYOTE_TIME; else p.coyoteTimer = std::max(0.0f, p.coyoteTimer - dt);
    MeleeVisualState& lunge=state_.meleeVisual;
    lunge.landingRecovery=std::max(0.0f,lunge.landingRecovery-dt);
    lunge.wallGripTimer=std::max(0.0f,lunge.wallGripTimer-dt);
    const bool running = input.sprint || input.touchSprint;
    const float power = batteryPower(p);
    const bool committedLunge=state_.meleeVisual.locomotionLunge&&state_.meleeVisual.airLungeTimer>0.0f;
    const float lungeInfluence=std::min(0.42f,0.025f*static_cast<float>(upgradeLevel(UpgradeTrack::Lunge)));
    const float airControl = p.grounded ? 1.0f : AIR_ACCEL_MULT*(committedLunge?0.18f+lungeInfluence:1.0f);
    const float airSpeed = p.grounded ? 1.0f : AIR_MAX_SPEED_MULT;
    const float accel = (running ? RUN_ACCEL : WALK_ACCEL) * power * airControl;
    const float maxSpeed = (running ? RUN_MAX_SPEED : WALK_MAX_SPEED) * power * airSpeed;
    const float vacuumSlow = 1.0f - state_.vacuum.pose * (1.0f - VACUUM_MOVE_MULT);
    float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    if(updateLedgeHang(dt,forwardAxis,strafeAxis)){
        updatePhoneGait(dt,false);updatePhoneActionPose(dt,false,forwardAxis,strafeAxis);
        state_.debug.supportY=p.pos.y;state_.debug.localZ=wrapZ(p.pos.z);state_.debug.horizontalSpeed=std::abs(p.ledgeShimmySpeed);
        state_.debug.cameraYaw=state_.camera.yaw;state_.debug.cameraPitch=state_.camera.pitch;state_.debug.cameraMode=state_.camera.firstPerson?1:0;
        state_.debug.phoneYaw=state_.phonePose.yaw;state_.debug.phonePitch=state_.phonePose.pitch;state_.debug.phoneRoll=state_.phonePose.roll;
        state_.debug.phoneLift=state_.phonePose.lift;state_.debug.phoneForward=state_.phonePose.forward;state_.debug.phoneSide=state_.phonePose.side;
        return;
    }
    updateMeleeDash(dt);
    Vec3 move = cameraForwardFlat() * forwardAxis + cameraRightFlat() * strafeAxis;
    if (lengthSq(move) > 0.0f) { move = normalized(move); p.vel += move * (accel * vacuumSlow * dt); }
    limitHorizontal(p.vel, maxSpeed * vacuumSlow);
    // Ground melee retains the browser's short positional dash. Air melee is
    // a one-shot physical impulse, after ordinary air-speed limiting, so its
    // arc is subsequently governed by gravity, drag and collision response.
    if (state_.meleeVisual.airLungePending) {
        MeleeVisualState& melee = state_.meleeVisual;
        const Vec3 direction = normalized(Vec3{melee.direction.x, 0.0f, melee.direction.z});
        const float forwardSpeed = dotXZ(p.vel, direction);
        const Vec3 lateral = p.vel - direction * forwardSpeed;
        p.vel = direction * std::max(forwardSpeed, melee.airLungeSpeed) + lateral * AIR_MELEE_LATERAL_RETENTION;
        // This is a physical kick, not a trajectory fitted to an animation
        // duration. Gravity and the actual support below decide when it lands.
        p.jumpVel=std::max(p.jumpVel,melee.airLungeVerticalKick);
        melee.airLungePending = false;
    }
    if (!p.grounded) {
        p.jumpVel -= GRAVITY * dt;
        applyWallClimb(dt);
        p.pos.y += p.jumpVel * dt;
        if (p.pos.y > getPlayerCeilingLimit()) { p.pos.y = getPlayerCeilingLimit(); if (p.jumpVel > 0) p.jumpVel = 0; }
        const float support = getPlayerSupportY(p.pos.x, p.pos.z);
        if (p.pos.y <= support) {
            const float impactSpeed=std::max(0.0f,-p.jumpVel);
            p.pos.y = support; p.jumpVel = 0; p.grounded = true; p.coyoteTimer = COYOTE_TIME; p.airJumpsRemaining = 1;
            state_.phonePose.doubleJumpTimer = 0.0f;
            if(lunge.airLungeLandingPending)finishAirLungeLanding(impactSpeed);
            else if (horizontalLength(p.vel) > 1.2f) p.vel *= LANDING_MOMENTUM_BOOST;
        }
    }
    if (p.grounded && p.jumpBufferTimer > 0) startGroundJump();
    p.pos += p.vel * dt;
    resolvePlayerObstacleCollisions();
    resolveDoorwayCollisions(previousX,previousZ);
    if(simulationPlayerId_==0) updateRoomTopology(previousZ, p.pos.z);
    if (p.pos.y > getPlayerCeilingLimit()) { p.pos.y = getPlayerCeilingLimit(); if (p.jumpVel > 0) p.jumpVel = 0; }
    const bool caughtLedge=tryBeginLedgeHang();
    const float supportAfter = getPlayerSupportY(p.pos.x, p.pos.z);
    if (!caughtLedge&&p.grounded) {
        if (p.pos.y > supportAfter + 0.12f) p.grounded = false; else p.pos.y = supportAfter;
    } else if (!caughtLedge&&p.jumpVel <= 0 && p.pos.y <= supportAfter) {
        const float impactSpeed=std::max(0.0f,-p.jumpVel);
        p.pos.y = supportAfter; p.jumpVel = 0; p.grounded = true; p.coyoteTimer = COYOTE_TIME; p.airJumpsRemaining = 1;
        state_.phonePose.doubleJumpTimer = 0.0f;
        if(lunge.airLungeLandingPending)finishAirLungeLanding(impactSpeed);
    }
    const float minX = -ROOM_WIDTH * 0.5f + 1.1f, maxX = ROOM_WIDTH * 0.5f - 1.1f;
    if (p.pos.x < minX) { p.pos.x = minX; if (p.vel.x < 0) p.vel.x = 0; p.vel.z *= WALL_SLIDE_RETENTION; }
    else if (p.pos.x > maxX) { p.pos.x = maxX; if (p.vel.x > 0) p.vel.x = 0; p.vel.z *= WALL_SLIDE_RETENTION; }
    const float friction = p.grounded ? GROUND_FRICTION : AIR_FRICTION;
    p.vel *= std::pow(friction, dt * 60.0f);
    if(!p.ledgeHanging){p.targetYaw = state_.camera.yaw;p.yaw = state_.camera.yaw;}
    updatePhoneGait(dt, running);
    updatePhoneActionPose(dt, running, forwardAxis, strafeAxis);
    state_.debug.supportY = supportAfter;
    state_.debug.localZ = wrapZ(p.pos.z);
    state_.debug.horizontalSpeed = horizontalLength(p.vel);
    state_.debug.cameraYaw = state_.camera.yaw;
    state_.debug.cameraPitch = state_.camera.pitch;
    state_.debug.cameraMode = state_.camera.firstPerson ? 1 : 0;
    state_.debug.phoneYaw = state_.phonePose.yaw;
    state_.debug.phonePitch = state_.phonePose.pitch;
    state_.debug.phoneRoll = state_.phonePose.roll;
    state_.debug.phoneLift = state_.phonePose.lift;
    state_.debug.phoneForward = state_.phonePose.forward;
    state_.debug.phoneSide = state_.phonePose.side;
}

void Game::updatePhoneTransform() {
    PhoneTransformState& transform = state_.phoneTransform;
    if (localPhoneMenuPresentation(state_)) {
        transform.orientation = quatAxisAngle({0,1,0}, state_.player.yaw);
        transform.position = state_.player.pos;
        transform.screenRight = normalized(rotate(transform.orientation, {1,0,0}));
        transform.screenUp = normalized(rotate(transform.orientation, {0,1,0}));
        transform.screenNormal = normalized(rotate(transform.orientation, {0,0,1}));
        transform.screenCenter = transform.position + transform.screenNormal * PHONE_SCREEN_Z_OFFSET;
        transform.vacuumPullPoint = transform.screenCenter;
        return;
    }
    const Vec3 forward{-std::sin(state_.player.yaw), 0.0f, -std::cos(state_.player.yaw)};
    const Vec3 right{std::cos(state_.player.yaw), 0.0f, -std::sin(state_.player.yaw)};
    transform.orientation = state_.player.downed?quatNormalized(quatAxisAngle({0,1,0},state_.player.yaw)*quatAxisAngle({1,0,0},DB_PI*0.5f)):quatNormalized(
        state_.phonePose.orientation *
        quatAxisAngle({1,0,0}, state_.phoneVisual.pitch) *
        quatAxisAngle({0,0,1}, state_.phoneVisual.roll));
    transform.position = state_.player.pos + Vec3{0, state_.phonePose.lift + state_.phoneVisual.actionLift, 0}
        + forward * (state_.phonePose.forward - state_.phoneVisual.actionForward)
        + right * state_.phonePose.side;
    transform.screenRight = normalized(rotate(transform.orientation, {1,0,0}));
    transform.screenUp = normalized(rotate(transform.orientation, {0,1,0}));
    transform.screenNormal = normalized(rotate(transform.orientation, {0,0,1}));
    transform.screenCenter = transform.position + transform.screenNormal * (PHONE_SCREEN_Z_OFFSET + state_.phoneVisual.screenOffset);
    transform.vacuumPullPoint = state_.camera.firstPerson
        ? state_.camera.pos - state_.camera.forward * 0.85f
        : transform.screenCenter;
}

void Game::updatePhoneDisplay(float dt) {
    PhoneDisplayState& display = state_.phoneDisplay;
    const PhoneDisplayMode nextMode = phoneDisplayModeForState(state_);
    if (display.mode != nextMode) {
        display.previousMode = display.mode;
        display.mode = nextMode;
        display.transitionProgress = 0.0f;
    } else {
        display.transitionProgress = finiteClamped(display.transitionProgress + dt * 5.5f, 0.0f, 1.0f);
    }

    const float decay = std::exp(-dt * 5.5f);
    display.damagePulse *= decay;
    display.capturePulse *= decay;
    display.powerPulse *= decay;
    display.warningPulse *= decay;
    const float lowBatteryTarget = state_.hud.lowBattery ? (0.45f + 0.35f * std::sin(state_.time * 4.1f)) : 0.0f;
    display.lowBatteryPulse += (lowBatteryTarget - display.lowBatteryPulse) * (1.0f - std::exp(-dt * 8.0f));
    display.screenNoisePhase = finiteClamped(display.screenNoisePhase + dt * (0.08f + state_.vacuum.power * 0.10f), 0.0f, 1000000.0f);

    const bool menuMode = display.mode == PhoneDisplayMode::Boot || display.mode == PhoneDisplayMode::MainMenu ||
        display.mode == PhoneDisplayMode::Online || display.mode == PhoneDisplayMode::JoinCode ||
        display.mode == PhoneDisplayMode::Settings || display.mode == PhoneDisplayMode::Controls ||
        display.mode == PhoneDisplayMode::Audio || display.mode == PhoneDisplayMode::Graphics ||
        display.mode == PhoneDisplayMode::Pause;
    const bool displayOff = display.mode == PhoneDisplayMode::Off || display.mode == PhoneDisplayMode::Death;
    const float vacuum = finiteClamped(state_.vacuum.power, 0.0f, 1.0f);
    const float discharge = finiteClamped(state_.energy.dischargePositionAmount, 0.0f, 1.0f);
    const float battery = finiteClamped(state_.player.battery / 100.0f, 0.0f, 1.0f);
    const float modeBase = menuMode ? 0.66f : (displayOff ? 0.08f : 0.34f);
    display.brightness = finiteClamped(modeBase + vacuum * 0.18f + discharge * 0.26f + battery * 0.08f, 0.0f, 1.0f);
    display.contentOpacity = finiteClamped(menuMode ? 1.0f : 0.38f + vacuum * 0.24f, 0.0f, 1.0f);

    const Vec3 baseCyan{0.07f, 0.19f, 0.29f};
    const Vec3 activeCyan{0.18f, 0.76f, 0.92f};
    const Vec3 copper{0.70f, 0.34f, 0.18f};
    const Vec3 white{0.90f, 0.98f, 1.0f};
    Vec3 color = mix3(baseCyan, activeCyan, display.brightness);
    color = mix3(color, copper, finiteClamped(display.lowBatteryPulse, 0.0f, 1.0f) * 0.42f);
    color = mix3(color, white, finiteClamped(discharge + display.capturePulse, 0.0f, 1.0f) * 0.22f);
    const Vec3 magenta{Pass7Visual::ElectricMagenta.r, Pass7Visual::ElectricMagenta.g, Pass7Visual::ElectricMagenta.b};
    color = mix3(color, magenta, finiteClamped(state_.hud.criticalHitPulse, 0.0f, 1.0f) * 0.30f);
    display.screenTint = color;
    display.emissionColor = color;
    display.emissionStrength = finiteClamped(display.brightness * 0.95f + vacuum * 0.22f + discharge * 0.55f + state_.hud.criticalHitPulse * 0.42f, 0.0f, 2.4f);
    display.localLightIntensity = finiteClamped(display.emissionStrength * (menuMode ? 0.38f : 0.22f), 0.0f, 1.15f);
    display.localLightRadius = finiteClamped(0.16f + display.emissionStrength * 0.045f, 0.12f, 0.32f);
    display.glassResponse = finiteClamped(0.12f + display.brightness * 0.10f, 0.06f, 0.30f);
    display.blackLevel = finiteClamped(1.0f - display.brightness * 0.72f, 0.08f, 1.0f);
    display.interactive = menuMode || display.mode == PhoneDisplayMode::Upgrade;

    display.material.displayBrightness = display.brightness;
    display.material.emissionColor = display.emissionColor;
    display.material.emissionIntensity = display.emissionStrength;
    display.material.backgroundEmission = finiteClamped(display.emissionStrength, 0.0f, 2.4f);
    display.material.glassEmission = finiteClamped(display.emissionStrength * 0.09f, 0.0f, 0.25f);
    display.material.rimEmission = finiteClamped(display.emissionStrength * 0.18f, 0.0f, 0.45f);
    display.material.glassOpacity = display.glassResponse;
    display.material.glassRoughness = finiteClamped(0.46f - display.brightness * 0.12f, 0.28f, 0.62f);
    display.material.blackLevel = display.blackLevel;

    display.lighting.color = display.emissionColor;
    display.lighting.intensity = display.localLightIntensity;
    display.lighting.radius = display.localLightRadius;
    display.lighting.forwardOffset = 0.024f;
    display.lighting.pulse = finiteClamped(display.damagePulse + display.capturePulse + display.powerPulse + display.lowBatteryPulse + state_.hud.criticalHitPulse, 0.0f, 1.0f);
}

void Game::updatePhoneActionPose(float dt, bool running, float forwardAxis, float strafeAxis) {
    PhonePoseState& pose = state_.phonePose;
    pose.doubleJumpVacuumPause = std::max(0.0f, pose.doubleJumpVacuumPause - dt);
    pose.doubleJumpTimer = std::max(0.0f, pose.doubleJumpTimer - dt);
    if(state_.player.ledgeMantleTimer>0.0f){
        const float phase=1.0f-clampf(state_.player.ledgeMantleTimer/LEDGE_MANTLE_DURATION,0.0f,1.0f);
        const float settle=smooth01(phase);
        const float arc=std::sin(phase*DB_PI);
        pose.lift+=-0.10f*(1.0f-settle)+0.075f*arc;
        pose.forward+=0.08f*(1.0f-settle)-0.05f*arc;
        pose.orientation=quatNormalized(
            quatAxisAngle({0,1,0},state_.player.yaw)*
            quatAxisAngle({1,0,0},-DB_PI*2.0f*settle));
        pose.actionState=9;
        pose.screenForwardTurn=0.0f;
        return;
    }
    if(state_.player.ledgeHanging){
        const float effort=clampf(std::abs(state_.player.ledgeShimmySpeed)/LEDGE_SHIMMY_MAX_SPEED,0.0f,1.0f);
        const float dangle=std::sin(state_.player.ledgeHangTime*5.2f)*0.055f;
        const float travel=std::sin(state_.player.ledgeHangTime*10.4f)*effort;
        pose.side+=travel*0.018f;
        pose.lift+=dangle*0.10f;
        pose.orientation=quatNormalized(
            quatAxisAngle({0,1,0},state_.player.yaw)*
            quatAxisAngle({1,0,0},0.20f+dangle)*
            quatAxisAngle({0,0,1},-travel*0.16f));
        pose.actionState=8;
        pose.screenForwardTurn=0.0f;
        return;
    }
    const bool locomotionLunge = state_.meleeVisual.locomotionLunge && state_.meleeVisual.airLungeLandingPending;
    const bool jumpFlip = pose.doubleJumpTimer > 0.0f && !locomotionLunge;
    const bool dischargeFacing = state_.energy.dischargeTimer > 0.0f;
    state_.energy.dischargePositionAmount += ((dischargeFacing ? 1.0f : 0.0f) - state_.energy.dischargePositionAmount) * std::min(1.0f, dt * 12.0f);
    const bool vacuumFacing = (state_.vacuum.active && pose.doubleJumpVacuumPause <= 0.0f) || dischargeFacing;
    const float targetTurn = vacuumFacing ? 1.0f : 0.0f;
    pose.screenForwardTurn += (targetTurn - pose.screenForwardTurn) * std::min(1.0f, dt * 4.5f);
    const float easedTurn = pose.screenForwardTurn*pose.screenForwardTurn*(3.0f-2.0f*pose.screenForwardTurn);

    const float inputMag = std::max(1.0f, std::sqrt(forwardAxis*forwardAxis + strafeAxis*strafeAxis));
    const float lean = running ? 0.5f : 0.35f;
    const float bodyYaw = state_.player.inSecretRoom ? state_.player.yaw : state_.camera.yaw;
    Quat base = quatAxisAngle({0,1,0}, bodyYaw);
    base = base * quatAxisAngle({1,0,0}, -(forwardAxis/inputMag)*lean);
    base = base * quatAxisAngle({0,0,1}, -(strafeAxis/inputMag)*lean*0.82f);

    // THREE.Object3D.lookAt points the phone's local +Z axis along the camera ray.
    const Vec3 cameraRay = state_.camera.forward;
    const float aimYaw = std::atan2(cameraRay.x, cameraRay.z);
    const float aimPitch = -std::asin(clampf(cameraRay.y, -1.0f, 1.0f));
    Quat aim = quatAxisAngle({0,1,0}, aimYaw) * quatAxisAngle({1,0,0}, aimPitch);
    Quat q = quatSlerp(base, aim, easedTurn);
    const float ritualFlip = std::sin(easedTurn * DB_PI) * 0.75f;
    const float wobble = std::sin(state_.time * 18.0f) * 0.035f * state_.vacuum.pose;

    if (jumpFlip) {
        const float phase = 1.0f - clampf(pose.doubleJumpTimer / 0.30f, 0.0f, 1.0f);
        const float ease = phase*phase*(3.0f-2.0f*phase);
        pose.doubleJumpFlip = ease * DB_PI * 2.0f;
        q = quatAxisAngle({0,1,0}, pose.doubleJumpFlipYaw) * quatAxisAngle({1,0,0}, -pose.doubleJumpFlip);
        pose.actionState = 5;
    } else {
        pose.doubleJumpFlip = 0.0f;
        q = q * quatAxisAngle({1,0,0}, -ritualFlip + pose.pitch);
        q = q * quatAxisAngle({0,1,0}, pose.yaw);
        q = q * quatAxisAngle({0,0,1}, wobble + pose.roll);
        pose.actionState = dischargeFacing ? 3 : (vacuumFacing ? 2 : (pose.energy > 0.01f ? 1 : 0));
        const MeleeVisualState& melee = state_.meleeVisual;
        if (melee.visualTimer > 0.0f || locomotionLunge) {
            const float attackT = 1.0f - clampf(melee.visualTimer / std::max(0.001f, melee.visualDuration), 0.0f, 1.0f);
            const float snap = std::sin(attackT * DB_PI);
            if (melee.locomotionLunge) {
                // The airborne action is the phone itself travelling through a
                // compact forward arc. It is locomotion with contact damage,
                // not a handheld-style swing layered over ordinary movement.
                const float horizontal=std::max(0.01f,horizontalLength(state_.player.vel));
                const float trajectoryAngle=std::atan2(state_.player.jumpVel,horizontal);
                pose.forward += 0.10f+0.10f*snap;
                pose.lift += 0.06f+0.08f*snap;
                q = quatAxisAngle({0,1,0}, bodyYaw)
                    * quatAxisAngle({1,0,0},-0.12f-melee.airLungeRotation-trajectoryAngle*0.32f);
                pose.actionState = 6;
                pose.orientation = quatNormalized(q);
                return;
            }
            const float recover = std::sin(std::min(1.0f, attackT * 1.45f) * DB_PI);
            const int variant = std::max(0, std::min(3, melee.variant));
            const float hitWeight = melee.visualHit ? 1.18f : 0.82f;
            pose.forward += melee.lunge * snap;
            pose.side += MELEE_VARIANT_SIDE[variant] * 0.035f * snap;
            pose.lift += MELEE_VARIANT_LIFT[variant] * snap;
            q = q * quatAxisAngle({1,0,0}, MELEE_VARIANT_PITCH[variant] * snap * hitWeight);
            q = q * quatAxisAngle({0,0,1}, MELEE_VARIANT_ROLL[variant] * recover * hitWeight);
            q = q * quatAxisAngle({0,1,0}, MELEE_VARIANT_YAW[variant] * snap * hitWeight);
            pose.actionState = 4;
        }
        if(!locomotionLunge&&melee.landingRecovery>0.0f){
            const float recovery=clampf(melee.landingRecovery/std::max(0.001f,melee.landingRecoveryDuration),0.0f,1.0f);
            const float compression=recovery*recovery;
            pose.lift-=0.055f*compression;
            pose.forward+=0.045f*compression;
            q=quatAxisAngle({0,1,0},bodyYaw)*quatAxisAngle({1,0,0},-melee.landingPosePitch*recovery-0.22f*compression);
            pose.actionState=7;
        }
    }
    pose.orientation = quatNormalized(q);
}

void Game::updatePhoneGait(float dt, bool running) {
    PhonePoseState& g = state_.phonePose;
    g.pitch = g.roll = g.yaw = g.lift = g.forward = g.side = g.energy = 0.0f;
    const float speed = horizontalLength(state_.player.vel);
    const float speedMix = clampf(speed / RUN_MAX_SPEED, 0.0f, 1.0f);
    const float sprintMix = running ? clampf(speed / (RUN_MAX_SPEED * 0.72f), 0.0f, 1.0f) : 0.0f;
    const float runMix = clampf((speed - WALK_MAX_SPEED * 0.72f) / (RUN_MAX_SPEED - WALK_MAX_SPEED * 0.72f), 0.0f, 1.0f);
    const float targetEnergy = state_.player.grounded && speed > 0.08f ? clampf(speedMix * 0.72f + sprintMix * 0.28f, 0.0f, 1.0f) : 0.0f;
    const float response = targetEnergy > g.rollEnergy ? 10.0f : 5.5f;
    g.rollEnergy += (targetEnergy - g.rollEnergy) * std::min(1.0f, dt * response);
    if (state_.player.grounded && speed > 0.025f) {
        const float tighten = lerpf(1.0f, 0.78f, sprintMix);
        g.phase += speed * dt / (PHONE_GAIT_CYLINDER_RADIUS * tighten);
    }
    const float step = std::sin(g.phase), stepAbs = std::abs(step), planted = step >= 0 ? 1.0f : -1.0f;
    const float quadrature = std::cos(g.phase);
    const float contactEase = std::pow(stepAbs, 0.20f);
    const float transferEase = std::pow(1.0f - std::abs(quadrature), 0.34f);
    const float coneSweep = std::sin(g.phase * 2.0f);
    const float coneCatch = std::pow(std::max(0.0f, coneSweep), 0.42f);
    const float mirroredCone = planted * (contactEase * 0.72f + transferEase * 0.28f);
    const float oloidLoop = std::sin(g.phase + DB_PI * 0.5f);
    const float rotationPower = clampf(runMix * 0.72f + sprintMix * 0.28f, 0.0f, 1.0f);
    const float exaggeration = lerpf(1.15f, 1.55f, rotationPower);
    const float pitchAmp = lerpf(PHONE_GAIT_WALK_PITCH, PHONE_GAIT_RUN_PITCH, rotationPower) * exaggeration;
    const float rollAmp = lerpf(PHONE_GAIT_WALK_ROLL, PHONE_GAIT_RUN_ROLL, rotationPower) * exaggeration;
    const float yawAmp = lerpf(PHONE_GAIT_WALK_YAW, PHONE_GAIT_RUN_YAW, rotationPower) * exaggeration;
    const float suppression = 1.0f - std::max(state_.vacuum.pose, state_.meleePose) * 0.65f;
    const float energy = g.rollEnergy * suppression;
    const float forwardRoll = coneCatch * 0.72f + contactEase * 0.28f;
    const float oblique = planted * (transferEase * 0.62f + stepAbs * 0.38f) * lerpf(0.7f, 1.18f, rotationPower);
    g.energy = energy;
    g.pitch = -forwardRoll * pitchAmp * energy;
    g.roll = (-mirroredCone * rollAmp + quadrature * PHONE_GAIT_CONE_TWIST * transferEase) * energy;
    g.yaw = (mirroredCone * yawAmp + oblique * PHONE_GAIT_OBLIQUE_SWEEP) * energy;
    g.lift = (contactEase * 0.58f + transferEase * 0.42f) * PHONE_GAIT_LIFT * energy;
    g.forward = forwardRoll * PHONE_GAIT_FORWARD_OFFSET * energy;
    g.side = (mirroredCone * PHONE_GAIT_SIDE_OFFSET + oloidLoop * PHONE_GAIT_OLOID_MEANDER * transferEase) * energy;
}

float Game::getSegmentAabbHitT(const Vec3& from, const Vec3& to, const RoomCollider& box, float pad) const {
    float tMin = 0.0f;
    float tMax = 1.0f;
    const float origins[3] = {from.x, from.y, from.z};
    const float deltas[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
    const float mins[3] = {box.minX - pad, box.bottomY - pad, box.minZ - pad};
    const float maxs[3] = {box.maxX + pad, box.topY + pad, box.maxZ + pad};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(deltas[axis]) < 0.00001f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return -1.0f;
            continue;
        }
        const float inv = 1.0f / deltas[axis];
        float a = (mins[axis] - origins[axis]) * inv;
        float b = (maxs[axis] - origins[axis]) * inv;
        if (a > b) std::swap(a, b);
        tMin = std::max(tMin, a);
        tMax = std::min(tMax, b);
        if (tMin > tMax) return -1.0f;
    }
    return clampf(tMin, 0.0f, 1.0f);
}

void Game::constrainThirdPersonCamera(Vec3& desired, const Vec3& lookBase) const {
    Vec3 start = lookBase;
    start.y += 0.58f;
    const Vec3 end = desired;
    const float localStartZ = wrapZ(start.z);
    const float localEndZ = localStartZ + (end.z - start.z);
    const Vec3 localStart{start.x, start.y, localStartZ};
    const Vec3 localEnd{end.x, end.y, localEndZ};
    const Vec3 segment=localEnd-localStart;
    // The phone may legitimately stand closer to a wall than the camera's
    // padded sphere radius.  A cast that begins in that padding must be allowed
    // to leave through the nearest face; treating the initial overlap as a hit
    // collapses the boom to first-person distance exactly while climbing/hanging.
    const auto cameraHit=[&](const RoomCollider& box,float pad){
        const float mins[3]={box.minX-pad,box.bottomY-pad,box.minZ-pad};
        const float maxs[3]={box.maxX+pad,box.topY+pad,box.maxZ+pad};
        const float p[3]={localStart.x,localStart.y,localStart.z};
        const float d[3]={segment.x,segment.y,segment.z};
        const bool inside=p[0]>=mins[0]&&p[0]<=maxs[0]&&p[1]>=mins[1]&&p[1]<=maxs[1]&&p[2]>=mins[2]&&p[2]<=maxs[2];
        if(inside){
            float nearest=p[0]-mins[0];float outward=-d[0];
            const float gaps[5]={maxs[0]-p[0],p[1]-mins[1],maxs[1]-p[1],p[2]-mins[2],maxs[2]-p[2]};
            const float velocities[5]={d[0],-d[1],d[1],-d[2],d[2]};
            for(int i=0;i<5;++i)if(gaps[i]<nearest){nearest=gaps[i];outward=velocities[i];}
            if(outward>0.0f)return -1.0f;
        }
        return getSegmentAabbHitT(localStart,localEnd,box,pad);
    };

    float nearestT = 1.0f;
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const float t = cameraHit(state_.roomColliders[i], CAMERA_COLLISION_RADIUS);
        if (t >= 0.0f && t < nearestT) nearestT = t;
    }

    const float doorX0 = -2.1f;
    const float doorX1 = 2.1f;
    const float wallPad = 0.3f;
    const auto makeBounds = [](float minX, float maxX, float minZ, float maxZ, float bottomY, float topY) {
        RoomCollider c{};
        c.minX = minX; c.maxX = maxX; c.minZ = minZ; c.maxZ = maxZ; c.bottomY = bottomY; c.topY = topY;
        return c;
    };
    const RoomCollider roomBounds[] = {
        makeBounds(-ROOM_WIDTH * 0.5f - 0.3f, -ROOM_WIDTH * 0.5f + 0.5f, -ROOM_DEPTH * 0.5f, ROOM_DEPTH * 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds( ROOM_WIDTH * 0.5f - 0.5f,  ROOM_WIDTH * 0.5f + 0.3f, -ROOM_DEPTH * 0.5f, ROOM_DEPTH * 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(-ROOM_WIDTH * 0.5f, doorX0, -ROOM_DEPTH * 0.5f - wallPad, -ROOM_DEPTH * 0.5f + 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX1, ROOM_WIDTH * 0.5f, -ROOM_DEPTH * 0.5f - wallPad, -ROOM_DEPTH * 0.5f + 0.5f, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX0, doorX1, -ROOM_DEPTH * 0.5f - wallPad, -ROOM_DEPTH * 0.5f + 0.5f, 3.72f, ROOM_WALL_HEIGHT),
        makeBounds(-ROOM_WIDTH * 0.5f, doorX0, ROOM_DEPTH * 0.5f - 0.5f, ROOM_DEPTH * 0.5f + wallPad, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX1, ROOM_WIDTH * 0.5f, ROOM_DEPTH * 0.5f - 0.5f, ROOM_DEPTH * 0.5f + wallPad, GROUND_Y, ROOM_WALL_HEIGHT),
        makeBounds(doorX0, doorX1, ROOM_DEPTH * 0.5f - 0.5f, ROOM_DEPTH * 0.5f + wallPad, 3.72f, ROOM_WALL_HEIGHT)
    };
    for (const RoomCollider& c : roomBounds) {
        const float t = cameraHit(c, CAMERA_COLLISION_RADIUS * 0.8f);
        if (t >= 0.0f && t < nearestT) nearestT = t;
    }

    if (nearestT < 1.0f) {
        const Vec3 worldSegment = end - start;
        const float dist = length(worldSegment);
        const float safeT = dist > 0.001f ? std::max(0.0f, nearestT - CAMERA_COLLISION_BACKOFF / dist) : 0.0f;
        desired = start + worldSegment * safeT;
    }

    desired.x = clampf(desired.x, -ROOM_WIDTH * 0.5f + CAMERA_COLLISION_RADIUS, ROOM_WIDTH * 0.5f - CAMERA_COLLISION_RADIUS);
    const float desiredTileOriginZ = getRoomTileOriginZ(getRoomTileIndex(desired.z));
    const float desiredLocalZ = wrapZ(desired.z);
    const bool nearDoorSeam = std::abs(std::abs(desiredLocalZ) - ROOM_DEPTH * 0.5f) < CAMERA_COLLISION_RADIUS * 2.8f;
    Vec3 desiredInLocal = desired; desiredInLocal.z = desiredLocalZ;
    const bool desiredInDoorAperture = isInsideDoorAperture(desiredInLocal, CAMERA_COLLISION_RADIUS * 0.8f);
    if (!nearDoorSeam || !desiredInDoorAperture) {
        desired.z = desiredTileOriginZ + clampf(desiredLocalZ, -ROOM_DEPTH * 0.5f + CAMERA_COLLISION_RADIUS, ROOM_DEPTH * 0.5f - CAMERA_COLLISION_RADIUS);
    }
    desired.y = clampf(desired.y, GROUND_Y + 0.65f, ROOM_WALL_HEIGHT - 0.45f);
}

void Game::updateCamera(float dt) {
    CameraState& camera = state_.camera;
    const PlayerState& player = state_.player;
    const float cp = std::cos(camera.pitch);
    const Vec3 aimForward = normalized({-std::sin(camera.yaw) * cp, std::sin(camera.pitch), -std::cos(camera.yaw) * cp});
    const float horizontalSpeed=std::sqrt(player.vel.x*player.vel.x+player.vel.z*player.vel.z);
    const float motionFov=clampf((horizontalSpeed-2.5f)*1.05f,0.0f,9.0f)+(!player.grounded?2.2f:0.0f)+(state_.meleeVisual.airLungeLandingPending?4.8f:0.0f);
    const bool mobile = state_.localSettings.mobileFraming;
    const float targetFov=camera.firstPerson?(mobile?60.0f:64.0f):(mobile?56.0f:60.0f)+motionFov*(mobile?0.72f:1.0f);
    if(dt>0.0f)camera.verticalFovDegrees+=(targetFov-camera.verticalFovDegrees)*(1.0f-std::exp(-(targetFov>camera.verticalFovDegrees?5.2f:2.8f)*dt));else camera.verticalFovDegrees=targetFov;
    if (player.inSecretRoom) {
        camera.spectatedPlayerId = -1;
        camera.firstPerson = false;
        const Vec3 desired{37.18f, 1.34f, 0.0f};
        const Vec3 desiredTarget{41.16f, 0.62f, 0.0f};
        const float desiredFov = mobile ? 56.0f : 53.0f;
        if (dt > 0.0f) {
            const float response = 1.0f - std::exp(-8.5f * dt);
            camera.pos += (desired - camera.pos) * response;
            camera.lookTarget += (desiredTarget - camera.lookTarget) * response;
            camera.verticalFovDegrees += (desiredFov - camera.verticalFovDegrees) * response;
        } else {
            camera.pos = desired;
            camera.lookTarget = desiredTarget;
            camera.verticalFovDegrees = desiredFov;
        }
        camera.yaw = -DB_PI * 0.5f;
        camera.pitch = -0.08f;
        camera.forward = normalized(camera.lookTarget - camera.pos);
        return;
    }
    if(state_.attractMode){
        const float action=state_.meleeVisual.visualTimer>0.0f||state_.vacuum.active?1.0f:0.0f;
        const float travelSpeed=horizontalLength(player.vel);
        CinematicState& cinematic=state_.cinematic;
        if(!cinematic.attractCameraYawValid){
            cinematic.attractCameraYaw=camera.yaw;
            cinematic.attractCameraYawValid=true;
        }
        if(travelSpeed>0.65f){
            const float travelYaw=std::atan2(-player.vel.x,-player.vel.z);
            const float directionResponse=dt>0.0f?1.0f-std::exp(-1.35f*dt):1.0f;
            cinematic.attractCameraYaw=approachAngle(cinematic.attractCameraYaw,travelYaw,directionResponse);
        }
        const Vec3 travelForward{-std::sin(cinematic.attractCameraYaw),0.0f,-std::cos(cinematic.attractCameraYaw)};
        const Vec3 travelRight{std::cos(cinematic.attractCameraYaw),0.0f,-std::sin(cinematic.attractCameraYaw)};
        const Vec3 motionLead=travelSpeed>0.05f?normalized(Vec3{player.vel.x,0.0f,player.vel.z})*std::min(0.34f,travelSpeed*0.045f):Vec3{};
        const Vec3 subject=player.pos+motionLead;
        // Follow sustained travel rather than frame-perfect combat aim. This
        // keeps the phone composed through the room while target changes are
        // free to happen without whipping the audience between shoulders.
        Vec3 desired=subject-travelForward*(2.20f-action*0.10f)+travelRight*0.72f+Vec3{0,1.08f,0};
        constrainThirdPersonCamera(desired,subject);
        const Vec3 desiredTarget=subject+travelForward*0.18f+Vec3{0,0.48f,0};
        const float response=dt>0.0f?1.0f-std::exp(-3.10f*dt):1.0f;
        camera.pos+=(desired-camera.pos)*response;
        camera.lookTarget+=(desiredTarget-camera.lookTarget)*response;
        const float desiredFov=47.0f+action*1.0f;
        camera.verticalFovDegrees+=(desiredFov-camera.verticalFovDegrees)*response;
        camera.forward=normalized(camera.lookTarget-camera.pos);
        camera.firstPerson=false;
        camera.spectatedPlayerId=-1;
        return;
    }
    if(simulationPlayerId_==0&&state_.multiplayer.enabled&&!player.alive){
        const NetworkPeerState* subject=nullptr;
        for(const auto& peer:state_.multiplayer.peers){
            if(peer.active&&peer.playerId!=state_.multiplayer.localPlayerId&&peer.player.alive&&!peer.player.downed){subject=&peer;break;}
        }
        if(subject){
            const PlayerState& watched=subject->player;
            const Vec3 forward{-std::sin(watched.yaw),0.0f,-std::cos(watched.yaw)};
            const Vec3 right{std::cos(watched.yaw),0.0f,-std::sin(watched.yaw)};
            const float speed=horizontalLength(watched.vel);
            Vec3 desired=watched.pos-forward*1.68f+right*0.54f+Vec3{0,0.86f,0};
            constrainThirdPersonCamera(desired,watched.pos);
            const Vec3 desiredTarget=watched.pos+forward*(0.34f+clampf(speed*0.045f,0.0f,0.34f))+Vec3{0,0.40f,0};
            const float response=dt>0.0f?1.0f-std::exp(-(camera.spectatedPlayerId==subject->playerId?5.2f:3.2f)*dt):1.0f;
            camera.pos+=(desired-camera.pos)*response;
            camera.lookTarget+=(desiredTarget-camera.lookTarget)*response;
            const float desiredFov=46.0f+clampf((speed-3.0f)*0.52f,0.0f,5.0f);
            camera.verticalFovDegrees+=(desiredFov-camera.verticalFovDegrees)*(dt>0.0f?1.0f-std::exp(-4.5f*dt):1.0f);
            camera.forward=normalized(camera.lookTarget-camera.pos);
            camera.firstPerson=false;
            camera.spectatedPlayerId=subject->playerId;
            return;
        }
    }
    camera.spectatedPlayerId=-1;
    if (localPhoneMenuPresentation(state_)) {
        const PhoneTransformState& phone = state_.phoneTransform;
        const float fovRadians = MENU_CAMERA_VERTICAL_FOV * DB_PI / 180.0f;
        const float menuDistance = PHONE_BODY_HEIGHT / (2.0f * MENU_PHONE_VIEWPORT_HEIGHT * std::tan(fovRadians * 0.5f));
        Vec3 desiredTarget = phone.position;
        Vec3 desired = desiredTarget + phone.screenNormal * menuDistance;
        if (dt > 0.0f) {
            const float response = 1.0f - std::exp(-12.0f * dt);
            camera.pos += (desired - camera.pos) * response;
            camera.lookTarget += (desiredTarget - camera.lookTarget) * response;
            camera.verticalFovDegrees += (MENU_CAMERA_VERTICAL_FOV - camera.verticalFovDegrees) * response;
        } else {
            camera.pos = desired;
            camera.lookTarget = desiredTarget;
            camera.verticalFovDegrees = MENU_CAMERA_VERTICAL_FOV;
        }
        camera.forward = normalized(camera.lookTarget - camera.pos);
        return;
    }
    if (camera.firstPerson) {
        camera.pos = player.pos + Vec3{0, 0.72f, 0} + aimForward * 0.18f;
        camera.lookTarget = camera.pos + aimForward * 10.0f;
        camera.forward = normalized(camera.lookTarget-camera.pos);
        return;
    }
    const float leadAmount=clampf((horizontalSpeed-2.0f)*0.0045f,0.0f,0.038f);
    const Vec3 motionLead{player.vel.x*leadAmount,clampf(player.jumpVel*0.022f,-0.18f,0.14f),player.vel.z*leadAmount};
    const float leadScale = mobile ? 0.20f : 0.25f;
    const float boom = mobile ? 3.22f : 3.0f;
    const float height = mobile ? 1.18f : 1.1f;
    Vec3 desired = player.pos+motionLead*leadScale - aimForward * boom + Vec3{0, height, 0};
    if (desired.y < GROUND_Y + 0.8f) desired.y = GROUND_Y + 0.8f;
    constrainThirdPersonCamera(desired, player.pos);
    Vec3 desiredTarget=player.pos+motionLead+aimForward*10.0f;
    desiredTarget.y+=mobile?0.42f:0.45f;
    const MeleeVisualState& melee=state_.meleeVisual;
    if(melee.airLungeCameraLag>0.0f&&dt>0.0f){
        const float recoveryT=melee.landingRecoveryDuration>0.0f
            ? 1.0f-clampf(melee.landingRecovery/melee.landingRecoveryDuration,0.0f,1.0f)
            : 0.0f;
        const float responseRate=melee.airLungeLandingPending
            ? AIR_MELEE_CAMERA_RESPONSE
            : AIR_MELEE_CAMERA_RESPONSE+(AIR_MELEE_CAMERA_RECOVERY_RESPONSE-AIR_MELEE_CAMERA_RESPONSE)*recoveryT;
        const float response=1.0f-std::exp(-responseRate*dt);
        camera.pos+= (desired-camera.pos)*response;
        camera.lookTarget+=(desiredTarget-camera.lookTarget)*response;
        const Vec3 cameraError=camera.pos-desired;const float cameraErrorLength=length(cameraError);
        if(cameraErrorLength>AIR_MELEE_CAMERA_MAX_ERROR)camera.pos=desired+cameraError*(AIR_MELEE_CAMERA_MAX_ERROR/cameraErrorLength);
        const Vec3 targetError=camera.lookTarget-desiredTarget;const float targetErrorLength=length(targetError);
        if(targetErrorLength>AIR_MELEE_CAMERA_MAX_ERROR*1.35f)camera.lookTarget=desiredTarget+targetError*(AIR_MELEE_CAMERA_MAX_ERROR*1.35f/targetErrorLength);
    }else{
        camera.pos=desired;
        camera.lookTarget=desiredTarget;
    }
    if(state_.cinematic.menuExitActive&&!state_.multiplayer.enabled&&!state_.uiPaused&&!state_.cinematic.introActive&&dt>0.0f){
        state_.cinematic.menuExitElapsed=std::min(MENU_EXIT_CAMERA_DURATION,state_.cinematic.menuExitElapsed+dt);
        const float t=smooth01(state_.cinematic.menuExitElapsed/MENU_EXIT_CAMERA_DURATION);
        camera.pos=state_.cinematic.menuExitCameraPos*(1.0f-t)+desired*t;
        camera.lookTarget=state_.cinematic.menuExitLookTarget*(1.0f-t)+desiredTarget*t;
        if(state_.cinematic.menuExitElapsed>=MENU_EXIT_CAMERA_DURATION)state_.cinematic.menuExitActive=false;
    }
    camera.forward = normalized(camera.lookTarget-camera.pos);
}

void Game::updateAttractInput(float dt){
    InputState& input=state_.input;
    input=InputState{};
    const TargetState* subject=nullptr;
    float best=9999.0f;
    for(const auto& target:state_.targets){
        if(!target.alive)continue;
        const float distance=horizontalLength(target.pos-state_.player.pos);
        if((target.slurpable?0.0f:8.0f)+distance<best){best=(target.slurpable?0.0f:8.0f)+distance;subject=&target;}
    }
    if(!subject)return;
    const Vec3 delta=subject->pos-state_.player.pos;
    const float distance=horizontalLength(delta);
    const float desiredYaw=std::atan2(-delta.x,-delta.z);
    // The title player may exchange targets in a crowded fight.  Follow those
    // decisions through the shortest arc instead of teleporting the shoulder
    // camera from one side of the arena to the other on a single tick.
    const float aimResponse=dt>0.0f?1.0f-std::exp(-5.0f*dt):1.0f;
    state_.camera.yaw=approachAngle(state_.camera.yaw,desiredYaw,aimResponse);
    const float showcasePhase=seededRoomValue(3201.0f)*DB_PI*2.0f;
    input.touchMoveZ=distance>(subject->slurpable?1.45f:2.15f)?0.88f:0.12f;
    if(!subject->slurpable&&distance>2.8f)input.touchMoveX=std::sin(state_.frame*0.045f+showcasePhase)*0.46f;
    input.touchSprint=distance>3.0f;
    if(subject->slurpable){input.touchPrimaryHeld=true;return;}
    const int beat=(state_.frame+static_cast<int>(seededRoomValue(3202.0f)*96.0f))%96;
    if(beat==12&&state_.player.grounded)input.jumpPressed=true;
    if((beat==23||beat==61)&&distance<6.2f)input.meleePressed=true;
    if(beat==82&&state_.player.souls>0)input.shootPressed=true;
}

void Game::updateIntroCamera(float dt) {
    CinematicState& cinematic = state_.cinematic;
    if (!cinematic.introActive) return;
    cinematic.introElapsed = std::min(INTRO_CAMERA_DURATION, cinematic.introElapsed + dt);
    const float linear = clampf(cinematic.introElapsed / INTRO_CAMERA_DURATION, 0.0f, 1.0f);
    const float productPhase=clampf(linear/0.68f,0.0f,1.0f);
    const float productEase=smooth01(productPhase);
    const Vec3 phoneFocus=state_.phoneTransform.screenCenter-state_.phoneTransform.screenUp*0.018f;
    const float productYaw=cinematic.baseYaw-0.58f+productEase*0.76f;
    const Vec3 productForward{-std::sin(productYaw),0.0f,-std::cos(productYaw)};
    const Vec3 screenFacing=normalized(state_.phoneTransform.screenNormal*0.72f-productForward*0.05f);
    const Vec3 productCamera=phoneFocus+screenFacing*0.31f+state_.phoneTransform.screenUp*0.020f;

    const float cp=std::cos(state_.camera.pitch);
    const Vec3 gameplayForward=normalized({-std::sin(cinematic.baseYaw)*cp,std::sin(state_.camera.pitch),-std::cos(cinematic.baseYaw)*cp});
    Vec3 gameplayCamera=state_.player.pos-gameplayForward*3.0f+Vec3{0.0f,1.1f,0.0f};
    constrainThirdPersonCamera(gameplayCamera,state_.player.pos);
    const Vec3 gameplayTarget=state_.player.pos+gameplayForward*10.0f+Vec3{0.0f,0.45f,0.0f};
    const float handoff=smooth01((linear-0.68f)/0.32f);
    state_.camera.pos=productCamera*(1.0f-handoff)+gameplayCamera*handoff;
    state_.camera.lookTarget=phoneFocus*(1.0f-handoff)+gameplayTarget*handoff;
    state_.camera.forward = normalized(state_.camera.lookTarget - state_.camera.pos);
    if (linear >= 1.0f) {
        cinematic.introActive = false;
        updateCamera(0.0f);
    }
}

void Game::updateDeathCamera(float dt) {
    CinematicState& cinematic = state_.cinematic;
    if (!cinematic.deathActive) return;
    cinematic.deathElapsed = std::min(DEATH_CAMERA_DURATION, cinematic.deathElapsed + dt);
    const float linear = clampf(cinematic.deathElapsed / DEATH_CAMERA_DURATION, 0.0f, 1.0f);
    const float t = linear * linear * (3.0f - 2.0f * linear);
    const float orbitYaw = cinematic.baseYaw + t * 0.72f;
    const Vec3 orbitForward{-std::sin(orbitYaw), 0.0f, -std::cos(orbitYaw)};
    Vec3 desired = state_.player.pos - orbitForward * 5.3f + Vec3{0.0f, 2.15f, 0.0f};
    constrainThirdPersonCamera(desired, state_.player.pos);
    state_.camera.pos = cinematic.startCameraPos * (1.0f - t) + desired * t;
    state_.camera.lookTarget = state_.player.pos + Vec3{0.0f, 0.48f, 0.0f};
    state_.camera.forward = normalized(state_.camera.lookTarget - state_.camera.pos);
}

void Game::tryJump() {
    if (state_.player.grounded || state_.player.coyoteTimer > 0) startGroundJump();
    else if (state_.player.airJumpsRemaining > 0) startAirJump();
}
void Game::startGroundJump() {
    PlayerState& p = state_.player;
    if(!spendBattery(BATTERY_JUMP_COST,BatteryReason::Jump)) return;
    p.jumpVel = JUMP_SPEED; p.grounded = false; p.coyoteTimer = 0; p.jumpBufferTimer = 0; p.airJumpsRemaining = 1;
    state_.meleeVisual.landingRecovery=0.0f;
    state_.phonePose.doubleJumpVacuumPause=std::max(state_.phonePose.doubleJumpVacuumPause,0.12f);
}
void Game::startAirJump() {
    PlayerState& p = state_.player;
    if(!spendBattery(BATTERY_DOUBLE_JUMP_COST,BatteryReason::DoubleJump)) return;
    p.jumpVel = AIR_JUMP_SPEED; p.jumpBufferTimer = 0; p.airJumpsRemaining -= 1;
    PhonePoseState& pose = state_.phonePose;
    pose.doubleJumpTimer = std::max(pose.doubleJumpTimer, 0.30f);
    pose.doubleJumpVacuumPause = std::max(pose.doubleJumpVacuumPause, 0.16f);
    const InputState& input = state_.input;
    const float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    const float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    Vec3 direction = cameraForwardFlat()*forwardAxis + cameraRightFlat()*strafeAxis;
    pose.doubleJumpFlipYaw = lengthSq(direction) > 0.000001f
        ? std::atan2(-normalized(direction).x, -normalized(direction).z)
        : state_.camera.yaw;
}

void Game::triggerMelee(bool authoritativeDamage) {
    // A body lunge owns the player until its computed ground contact. The
    // shorter grounded-combo cooldown must never be allowed to renew flight.
    if(state_.meleeVisual.airLungeLandingPending) return;
    if (state_.meleeCooldown > 0) return;
    const int comboIndex = state_.meleeComboWindow > 0.0f ? (state_.meleeVisual.comboIndex + 1) % 4 : 0;
    const MeleeCombo& combo = MELEE_COMBOS[comboIndex];
    const bool airborne=!state_.player.grounded;
    const float upwardAim=airborne?clampf(state_.camera.pitch/0.62f,0.0f,1.0f):0.0f;
    const int lungeLevel=upgradeLevel(UpgradeTrack::Lunge);
    const float altitudeEfficiency=1.0f/(1.0f+0.045f*static_cast<float>(lungeLevel));
    const bool rebound=airborne&&state_.progression.run.lungeReboundTimer>0.0f;
    const float baseCost=combo.cost+upwardAim*5.5f*altitudeEfficiency;
    if (!spendBattery(baseCost*(rebound?LUNGE_HEADSHOT_REBOUND_COST_MULT:1.0f),BatteryReason::Melee)) return;
    if(rebound)state_.progression.run.lungeReboundTimer=0.0f;
    state_.meleeComboWindow = MELEE_COMBO_WINDOW;
    state_.meleeCooldown = combo.cooldown/(1.0f+0.045f*static_cast<float>(lungeLevel)); state_.meleePose = 1.0f;
    MeleeVisualState& visual = state_.meleeVisual;
    ++visual.actionSequence;
    const int attackLevel=upgradeLevel(UpgradeTrack::Attack);
    const float comboEscalation=airborne?1.0f:1.0f+0.035f*static_cast<float>(attackLevel*comboIndex);
    visual.comboIndex=comboIndex; visual.variant=combo.variant; visual.range=combo.range; visual.damage=combo.damage*(1.0f+0.07f*static_cast<float>(airborne?lungeLevel:attackLevel))*comboEscalation;
    visual.hitRadius=combo.hitRadius; visual.visualDuration=combo.visual; visual.visualTimer=combo.visual;
    visual.locomotionLunge=airborne;
    visual.dashTimer=airborne?0.0f:combo.dash; visual.dashSpeed=combo.dashSpeed; visual.travel=0.0f; visual.lunge=combo.lunge;
    visual.airLungePending=airborne;visual.airLungeLandingPending=airborne;
    visual.airLungeSpeed=airborne?AIR_MELEE_LOCOMOTION_DISTANCE/AIR_MELEE_LOCOMOTION_DURATION:0.0f;
    visual.airLungeVerticalKick=3.0f+upwardAim*2.8f;
    visual.airLungeTimer=airborne?AIR_MELEE_LOCOMOTION_DURATION:0.0f;
    visual.airLungeRotation=0.0f;visual.airLungeAngularVelocity=airborne?AIR_MELEE_ANGULAR_VELOCITY:0.0f;visual.airLungeCameraLag=airborne?1.0f:0.0f;
    if(airborne){visual.visualDuration=AIR_MELEE_LOCOMOTION_DURATION;visual.visualTimer=AIR_MELEE_LOCOMOTION_DURATION;}
    visual.recoilDistance=combo.recoilDistance; visual.recoilSpeed=combo.recoilSpeed; visual.visualHit=false; visual.hitMask=0;
    visual.previousContactPosition=state_.phoneTransform.position;visual.contactPositionValid=airborne;
    visual.direction=cameraForwardFlat();
    const Vec3 assistOrigin=state_.player.pos+Vec3{0,airborne?0.70f:0.42f,0};
    visual.direction=assistedActionDirection(assistOrigin,visual.direction,airborne?4.2f:3.1f,airborne?0.76f:0.80f,airborne?0.36f:0.28f,true);
    visual.direction.y=0.0f;
    if(lengthSq(visual.direction)<0.0001f)visual.direction=cameraForwardFlat();else visual.direction=normalized(visual.direction);
    visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
    visual.impact=visual.origin+visual.direction*(combo.range*0.72f);
    if(!airborne&&authoritativeDamage) applyMeleeHits();
}

int Game::applyMeleeHits() {
    if(state_.multiplayer.enabled&&!state_.multiplayer.authoritativeHost)return 0;
    MeleeVisualState& visual=state_.meleeVisual;
    const Vec3 phoneCurrent=state_.phoneTransform.position;
    const Vec3 phonePrevious=visual.contactPositionValid?visual.previousContactPosition:phoneCurrent;
    visual.previousContactPosition=phoneCurrent;visual.contactPositionValid=true;
    int newHits=0; int totalHits=0; int headshots=0; std::array<Vec3,TARGET_COUNT> headshotPositions{}; std::array<bool,TARGET_COUNT> headshotCritical{};std::array<float,TARGET_COUNT> headshotAttackProgress{},headshotKillCharge{};
    for(int i=0;i<TARGET_COUNT;++i) if((visual.hitMask&(1u<<i))!=0) ++totalHits;
    for (int i=0;i<TARGET_COUNT;++i) { TargetState& t=state_.targets[i]; if (!gameplay::isCombatTarget(t) || (visual.hitMask&(1u<<i))!=0) continue;
        const Vec3 delta{t.pos.x-state_.player.pos.x,0,t.pos.z-state_.player.pos.z};
        bool lungeBodyContact=false;
        if(visual.locomotionLunge){
            const float bodyRadius=PASS7_HUMAN_VISUAL_SPEC.torsoWidth*0.5f*t.scale+AIR_MELEE_PHONE_RADIUS+AIR_MELEE_BODY_FORGIVENESS;
            const float bodyY[3]={0.30f*t.scale,0.57f*t.scale,0.82f*t.scale};
            for(float y:bodyY){const Vec3 bodyPoint=t.pos+Vec3{0,y,0};if(pointSegmentDistanceSq(bodyPoint,phonePrevious,phoneCurrent)<=bodyRadius*bodyRadius){lungeBodyContact=true;break;}}
        }else{
            const float forwardDist=dotXZ(delta,visual.direction);
            if(forwardDist < -0.35f || forwardDist > visual.range) continue;
            const Vec3 sideDelta=delta-visual.direction*forwardDist;
            const float hitRadius=visual.hitRadius+(t.brute?0.28f:0.0f);
            if(lengthSq(sideDelta)>hitRadius*hitRadius) continue;
        }
        const Vec3 headCenter=targetHeadCenter(t);
        const Vec3 headBase=headCenter-Vec3{0,PASS7_HUMAN_VISUAL_SPEC.headRadius*t.scale*HEAD_CONTACT_NECK_FRACTION,0};
        const float headRadius=PASS7_HUMAN_VISUAL_SPEC.headRadius*t.scale+LUNGE_HEAD_CONTACT_RADIUS;
        const bool headshot=visual.locomotionLunge&&std::min(pointSegmentDistanceSq(headCenter,phonePrevious,phoneCurrent),pointSegmentDistanceSq(headBase,phonePrevious,phoneCurrent))<=headRadius*headRadius;
        if(visual.locomotionLunge&&!headshot&&!lungeBodyContact)continue;
        const Vec3 away = normalized(Vec3{t.pos.x - state_.player.pos.x, 0.0f, t.pos.z - state_.player.pos.z});
        const Vec3 right{std::cos(t.visualYaw), 0.0f, -std::sin(t.visualYaw)};
        t.hitDirectionLocal = clampf(away.x * right.x + away.z * right.z, -1.0f, 1.0f);
        float synergyDamage=1.0f;
        if(visual.locomotionLunge){
            const int momentum=pairSynergyTier(UpgradeTrack::Lunge,UpgradeTrack::Attack);
            synergyDamage+=static_cast<float>(momentum)*0.07f*clampf(horizontalLength(state_.player.vel)/AIR_MELEE_LOCOMOTION_DISTANCE,0.0f,2.0f);
        }else if(state_.progression.run.relayPrimerStacks>0){
            synergyDamage+=state_.progression.run.relayPrimerStacks*RELAY_PRIMER_DAMAGE_PER_STACK;
        }
        if(headshot&&t.grabbedPlayerId>=0)releaseTargetGrab(i);
        damageSoulShell(i,headshot?headshotDamage(t):visual.damage*synergyDamage*outgoingDamageMultiplier()*(1.0f+std::min(0.75f,totalHits*0.12f)));
        if(!headshot){state_.progression.run.accuracyStacks=0;state_.progression.run.accuracyMultiplier=1.0f;state_.progression.run.accuracyDecayTimer=0.0f;}
        if(headshot){const float armorMax=t.brute?SOUL_ARMOR_BRUTE:SOUL_ARMOR_NORMAL;headshotPositions[headshots]=headCenter;headshotCritical[headshots]=t.slurpable||t.armor<=armorMax*HEADSHOT_CRITICAL_ARMOR_FRACTION;headshotAttackProgress[headshots]=t.attackTimer>0.0f?1.0f-clampf(t.attackTimer/HUMAN_ATTACK_DURATION,0.0f,1.0f):-1.0f;headshotKillCharge[headshots]=t.slurpable?1.0f:1.0f-clampf(t.armor/armorMax,0.0f,1.0f);++headshots;}
        spawnFlameBurst(t.pos+Vec3{0,0.65f,0},newHits>0?0.95f+static_cast<float>(newHits+1)*0.18f:0.55f);
        visual.hitMask|=(1u<<i); visual.visualHit=true; visual.impact=t.pos+Vec3{0,0.62f,0}; ++newHits; ++totalHits;
    }
    if(newHits>0){
        emitAudio(AudioCue::PhoneAttack,0.44f);registerMeleeBatteryHit(newHits);
        for(int hit=0;hit<headshots;++hit)rewardHeadshot(headshotPositions[hit],headshotCritical[hit],visual.locomotionLunge,headshotAttackProgress[hit],headshotKillCharge[hit]);
        if(visual.locomotionLunge&&pairSynergyTier(UpgradeTrack::Lunge,UpgradeTrack::Attack)>0)state_.progression.run.impactGuardTimer=IMPACT_GUARD_SECONDS;
        if(!visual.locomotionLunge&&state_.progression.run.relayPrimerStacks>0){state_.progression.run.relayPrimerStacks=0;state_.progression.run.relayPrimerTimer=0.0f;setEnergyTicker("RELAY CLOSED",2);}
        if(headshots>0&&visual.locomotionLunge)continueLungeFromHeadshot();
        if(!visual.locomotionLunge){const float recoilScale=totalHits>1?0.35f:1.0f;state_.player.pos-=visual.direction*(visual.recoilDistance*recoilScale);state_.player.vel-=visual.direction*(visual.recoilSpeed*recoilScale);visual.dashTimer=totalHits>1?visual.dashTimer*0.35f:0.0f;}
    }
    return newHits;
}

Vec3 Game::targetHeadCenter(const TargetState& target) const {
    // The authoritative FBX is normalized from the floor to HUMAN_MODEL_HEIGHT.
    // Its head sphere therefore sits one visual head radius below that top.
    return {target.pos.x,
        (PASS7_HUMAN_VISUAL_SPEC.totalHeight-PASS7_HUMAN_VISUAL_SPEC.headRadius)*target.scale,
        target.pos.z};
}

float Game::headshotDamage(const TargetState& target) const {
    const float fullArmor=target.brute?SOUL_ARMOR_BRUTE:SOUL_ARMOR_NORMAL;
    return fullArmor/static_cast<float>(std::max(1,state_.requiredSouls));
}

Vec3 Game::assistedActionDirection(const Vec3& origin, const Vec3& direction, float maxDistance, float minDot, float maxBlend, bool preferHead) const {
    Vec3 base=normalized(direction);
    if(!state_.localSettings.mobileFraming || lengthSq(base)<0.0001f) return base;
    int best=-1;
    float bestScore=0.0f;
    Vec3 bestDirection=base;
    for(int i=0;i<TARGET_COUNT;++i){
        const TargetState& target=state_.targets[i];
        if(!gameplay::isCombatTarget(target))continue;
        const float armorMax=target.brute?SOUL_ARMOR_BRUTE:SOUL_ARMOR_NORMAL;
        const float damage=1.0f-clampf(target.armor/std::max(0.001f,armorMax),0.0f,1.0f);
        const Vec3 body{target.pos.x,(0.66f+0.08f*damage)*target.scale,target.pos.z};
        const Vec3 aimPoint=preferHead&&damage>0.18f?targetHeadCenter(target):body;
        Vec3 toTarget=aimPoint-origin;
        const float distance=length(toTarget);
        if(distance<=0.001f||distance>maxDistance)continue;
        toTarget=toTarget*(1.0f/distance);
        const float alignment=dot3(base,toTarget);
        if(alignment<minDot)continue;
        const float distanceScore=1.0f-clampf(distance/maxDistance,0.0f,1.0f);
        const float alignmentScore=clampf((alignment-minDot)/std::max(0.001f,1.0f-minDot),0.0f,1.0f);
        const float score=alignmentScore*0.74f+distanceScore*0.26f+damage*0.08f;
        if(score>bestScore){bestScore=score;best=i;bestDirection=toTarget;}
    }
    if(best<0)return base;
    const float blend=clampf(bestScore*maxBlend,0.0f,maxBlend);
    return normalized(base*(1.0f-blend)+bestDirection*blend);
}

void Game::continueLungeFromHeadshot() {
    MeleeVisualState& lunge=state_.meleeVisual;
    lunge.airLungePending=false;
    lunge.airLungeLandingPending=false;
    lunge.locomotionLunge=false;
    lunge.airLungeTimer=0.0f;
    lunge.visualTimer=0.0f;
    lunge.airLungeAngularVelocity=0.0f;
    lunge.contactPositionValid=false;
    state_.meleeCooldown=0.0f;
    state_.progression.run.lungeReboundTimer=LUNGE_HEADSHOT_REBOUND_WINDOW;
}

void Game::rewardHeadshot(const Vec3& position, bool critical, bool fromLunge, float enemyAttackProgress, float killCharge) {
    auto& run=state_.progression.run;
    run.accuracyStacks=std::min(ACCURACY_STACK_CAP,run.accuracyStacks+1);
    run.accuracyMultiplier=1.0f+static_cast<float>(run.accuracyStacks)*ACCURACY_STACK_BONUS;
    run.accuracyDecayTimer=ACCURACY_CHAIN_TIMEOUT;
    const int precision=pairSynergyTier(UpgradeTrack::Shot,UpgradeTrack::Lunge);
    run.headshotRegenTax=std::min(0.65f,run.headshotRegenTax+0.12f*std::max(0.55f,1.0f-0.08f*precision));
    const float timingForgiveness=0.012f*static_cast<float>(precision);
    const bool perfect=enemyAttackProgress>=HEADSHOT_PARRY_EARLY_PHASE-timingForgiveness&&enemyAttackProgress<=HEADSHOT_PARRY_LATE_PHASE+timingForgiveness;
    gainBattery(HEADSHOT_BATTERY_GAIN*run.accuracyMultiplier*(perfect?1.20f:1.0f),BatteryReason::Headshot);
    run.batteryRegenLock=std::min(run.batteryRegenLock,perfect?0.08f:0.22f);
    run.headshotRechargeBoost=std::max(run.headshotRechargeBoost,HEADSHOT_RECHARGE_BOOST_SECONDS*(perfect?1.35f:1.0f));
    char ticker[48]{};
    std::snprintf(ticker,sizeof(ticker),perfect?"PERFECT HEADSHOT X%.2F":"HEADSHOT CHAIN X%.2F",run.accuracyMultiplier);
    setEnergyTicker(ticker,2);
    state_.hud.headshotPulse=1.0f;
    if(critical)state_.hud.criticalHitPulse=1.0f;
    state_.hud.headshotKillCharge=clampf(std::max(state_.hud.headshotKillCharge,killCharge),0.0f,1.0f);
    if(perfect)state_.hud.perfectPulse=1.0f;
    spawnFlameBurst(position,1.35f);
    spawnParticleBurst(position);
    emitAudio(critical?AudioCue::HeadshotCritical:AudioCue::Headshot,critical?0.86f:0.72f);
    emitAudio(AudioCue::RewardWoah,0.42f);
    if(precision>0){
        state_.meleeCooldown=std::max(0.0f,state_.meleeCooldown-0.035f*static_cast<float>(precision));
        if(!fromLunge&&perfect&&state_.player.airJumpsRemaining<1)state_.player.airJumpsRemaining=1;
    }
}

bool Game::damageSoulShell(int index, float amount) {
    if(index<0 || index>=TARGET_COUNT) return false;
    TargetState& t=state_.targets[index];
    if(!gameplay::isCombatTarget(t)) return false;
    if(!t.slurpable) {
        t.armor-=amount;
        t.armorRegenDelay=ENEMY_ARMOR_REGEN_DELAY;
        t.hitFlash=1.0f;
        if(t.armor<=0.0f) {
            t.armor=0.0f; t.slurpable=true; t.soulState=SoulState::Free; t.soulMorph=0.0f; t.hitFlash=1.35f;
            if(t.grabbedPlayerId>=0) releaseTargetGrab(index);
            spawnShellShatter(t);
            if(t.brute && nextFlowerRandom()<FLOWER_DROP_CHANCE) spawnFlowerPowerup(t.pos.x,GROUND_Y+0.42f,t.pos.z);
        }
    } else {
        t.hitFlash=std::max(t.hitFlash,0.75f);
    }
    Vec3 away=normalized(Vec3{t.pos.x-state_.player.pos.x,0.0f,t.pos.z-state_.player.pos.z});
    t.vel.x+=away.x*2.4f; t.vel.z+=away.z*2.4f; t.vel.y=std::max(t.vel.y,1.2f);
    feedSupplementalBattery(FLOWER_ATTACK_FEED);
    spawnParticleBurst(t.pos+Vec3{0,0.65f,0});
    return true;
}

void Game::updateMeleeDash(float dt) {
    MeleeVisualState& visual=state_.meleeVisual;
    if(visual.airLungeCameraLag>0.0f){
        visual.airLungeCameraLag*=std::exp(-AIR_MELEE_CAMERA_LAG_DECAY*dt);
        if(visual.airLungeCameraLag<0.001f)visual.airLungeCameraLag=0.0f;
    }
    if(visual.airLungeLandingPending){
        visual.airLungeTimer=std::max(0.0f,visual.airLungeTimer-dt);
        visual.airLungeRotation+=visual.airLungeAngularVelocity*dt;
        visual.airLungeAngularVelocity*=std::exp(-AIR_MELEE_ANGULAR_DAMPING*dt);
        visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
        if(!visual.visualHit) visual.impact=visual.origin+visual.direction*(visual.range*0.72f);
        return;
    }
    if(visual.dashTimer<=0.0f) return;
    const float step=std::min(visual.dashSpeed*dt,std::max(0.0f,visual.range-visual.travel));
    visual.dashTimer=std::max(0.0f,visual.dashTimer-dt); visual.travel+=step;
    state_.player.pos+=visual.direction*step; state_.player.vel.x*=0.55f; state_.player.vel.z*=0.55f;
    visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
    if(!visual.visualHit) visual.impact=visual.origin+visual.direction*(visual.range*0.72f);
    applyMeleeHits();
}

void Game::finishAirLungeLanding(float impactSpeed){
    MeleeVisualState& visual=state_.meleeVisual;
    visual.airLungeLandingPending=false;
    visual.locomotionLunge=false;
    visual.airLungeTimer=0.0f;
    visual.visualTimer=0.0f;
    visual.airLungeAngularVelocity=0.0f;
    visual.wallGripTimer=0.0f;
    visual.contactPositionValid=false;
    visual.landingRecoveryDuration=clampf(0.06f+std::max(0.0f,impactSpeed-2.0f)*0.018f,0.06f,0.24f);
    visual.landingRecovery=visual.landingRecoveryDuration;
    visual.landingPosePitch=clampf(visual.airLungeRotation,0.28f,1.15f);
    state_.player.vel*=AIR_MELEE_LANDING_RETENTION;
    if(pairSynergyTier(UpgradeTrack::Lunge,UpgradeTrack::Attack)>0)state_.progression.run.impactGuardTimer=std::max(state_.progression.run.impactGuardTimer,IMPACT_GUARD_SECONDS*0.65f);
}
void Game::shootStoredSoul() {
    if (state_.player.souls <= 0) return;
    for (auto& pending : state_.pendingShots) if (!pending.active) {
        if (!spendBattery(BATTERY_SHOOT_COST,BatteryReason::Shoot)) return;
        const int storedIndex=state_.player.souls-1;
        pending=PendingShotState{}; pending.active=true; pending.brute=state_.player.storedSoulBrute[storedIndex];
        state_.player.storedSoulBrute[storedIndex]=false;
        state_.player.souls--;
        state_.hud.shootJoinTimer=0.18f;
        state_.energy.dischargeTimer=0.34f;
        return;
    }
}

void Game::processPendingShots(float dt) {
    for(auto& pending:state_.pendingShots) {
        if(!pending.active) continue;
        pending.age+=dt;
        if(state_.phonePose.screenForwardTurn<=0.45f && pending.age<0.09f) continue;
        BulletState* slot=nullptr;
        for(auto& bullet:state_.bullets) if(!bullet.alive){slot=&bullet;break;}
        if(!slot) {
            if(pending.age>0.75f && state_.player.souls<MAX_STORED_SOULS) {
                state_.player.storedSoulBrute[state_.player.souls]=pending.brute;
                state_.player.souls++;
                pending=PendingShotState{};
            }
            continue;
        }
        Vec3 direction=state_.camera.forward;
        direction.y=clampf(direction.y,BULLET_MAX_DOWN_AIM,BULLET_MAX_UP_AIM);
        direction=normalized(direction);
        direction=assistedActionDirection(
            state_.phoneTransform.screenCenter+state_.phoneTransform.screenNormal*(SCREEN_FRONT_OFFSET+0.28f),
            direction,
            pending.brute?15.0f:13.0f,
            pending.brute?0.94f:0.955f,
            pending.brute?0.34f:0.26f,
            true
        );
        direction.y=clampf(direction.y,BULLET_MAX_DOWN_AIM,BULLET_MAX_UP_AIM);
        direction=normalized(direction);
        *slot=BulletState{}; slot->alive=true; slot->life=BULLET_LIFE; slot->brute=pending.brute;
        slot->pos=state_.phoneTransform.screenCenter+state_.phoneTransform.screenNormal*(SCREEN_FRONT_OFFSET+0.28f);
        slot->pos.y=std::max(slot->pos.y,0.95f);
        state_.environmentVisual.latestShotOrigin=slot->pos;
        state_.environmentVisual.latestShotAge=0.0f;
        slot->vel=direction*(slot->brute?BULLET_BRUTE_SPEED:BULLET_SPEED);
        slot->vel.y+=BULLET_VERTICAL_LIFT;
        emitAudio(AudioCue::SentMessage,0.62f);
        spawnParticleBurst(slot->pos);
        pending=PendingShotState{};
    }
}
void Game::releaseSoul(int index) {
    if (index < 0 || index >= TARGET_COUNT) return;
    TargetState& t = state_.targets[index];
    const float phase = clampf(t.ingestProgress, 0.0f, 1.0f);
    if (phase >= SOUL_CAPTURE_COMMIT_PHASE) { queueSoulCapture(index); return; }
    Vec3 direction = t.pos - state_.phoneTransform.vacuumPullPoint;
    if (lengthSq(direction) < 0.001f) direction = {std::sin(state_.camera.yaw), 0.18f, std::cos(state_.camera.yaw)};
    direction = normalized(direction);
    const float recoil = 4.5f + phase * 10.5f;
    t.vel = {direction.x * recoil, 1.4f + phase * 2.2f, direction.z * recoil};
    if (phase > 0.015f) emitAudio(AudioCue::EndCallTone, 0.34f);
    t.ingestProgress = 0.0f;
    t.captureCollapseAmount = 0.0f;
    t.latchedToScreen = false;
    t.vacuumPullAmount = 0.0f;
    resetSoulLattice(t);
    t.soulState = SoulState::Recoiling;
    t.recoilTime = SOUL_RECOIL_DURATION;
    t.networkOwnerPlayerId = -1;
}
void Game::queueSoulCapture(int index) {
    if (index < 0 || index >= TARGET_COUNT) return;
    TargetState& t = state_.targets[index];
    if (t.captureQueued || t.captureCommitted || !t.alive || !t.slurpable) return;
    t.captureQueued = true;
    t.ingestProgress = std::max(t.ingestProgress, SOUL_CAPTURE_COMMIT_PHASE);
}
void Game::processQueuedSoulCaptures() {
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i];
        if (!t.captureQueued || t.captureCommitted) continue;
        if (state_.player.souls >= MAX_STORED_SOULS) {
            t.captureQueued = false;
            continue;
        }
        t.captureQueued = false;
        t.captureCommitted = true;
        captureSoul(i);
    }
}
void Game::captureSoul(int index) {
    if (index < 0 || index >= TARGET_COUNT) return;
    TargetState& t = state_.targets[index];
    if (state_.player.souls >= MAX_STORED_SOULS || !t.alive || !t.slurpable) return;
    t.alive = false;
    t.visibility = 0.0f;
    t.soulCubeAmount = 0.0f;
    syncTargetReactionVisual(t);
    const Vec3 capturedAt=t.pos;
    state_.player.storedSoulBrute[state_.player.souls]=t.brute;
    state_.player.souls++;
    ++state_.progression.run.roomCaptures;
    state_.progression.run.roomHeat=clampf(state_.progression.run.roomHeat+0.045f,0.0f,1.0f);
    gainBattery(BATTERY_CAPTURE_GAIN,BatteryReason::Ingest);
    feedSupplementalBattery(FLOWER_SLURP_FEED);
    emitAudio(AudioCue::ReceivedMessage,0.58f);
    emitAudio(AudioCue::RewardNice,0.30f);
    t.captureQueued=false; t.captureCommitted=false; t.soulState=SoulState::Free; t.networkOwnerPlayerId=-1;
    spawnParticleBurst(capturedAt);
    queueHumanRespawn(capturedAt);
}

void Game::queueHumanRespawn(const Vec3& avoid) {
    if(state_.roomClear) return;
    for(auto& request:state_.respawnQueue) if(!request.active) {
        request.active=true; request.avoid=avoid;
        const float roomScale=1.0f+0.16f*std::log2(1.0f+static_cast<float>(std::max(0,state_.roomIndex-1)));
        const float heatScale=1.0f+state_.progression.run.roomHeat*1.35f;
        request.delay=lerpf(HUMAN_RESPAWN_DELAY_MIN,HUMAN_RESPAWN_DELAY_MAX,nextFlowerRandom())/std::min(4.0f,roomScale*heatScale);
        return;
    }
}

void Game::updateRoomPopulation(float dt) {
    if(state_.roomClear){for(auto& request:state_.respawnQueue) request=HumanRespawnRequest{}; return;}
    state_.progression.run.roomElapsed+=dt;
    const float timeHeat=clampf(state_.progression.run.roomElapsed/ROOM_HEAT_SECONDS,0.0f,1.0f);
    const float captureHeat=clampf(static_cast<float>(state_.progression.run.roomCaptures)*0.045f,0.0f,0.55f);
    state_.progression.run.roomHeat=std::max(state_.progression.run.roomHeat,clampf(timeHeat+captureHeat,0.0f,1.0f));
    for(auto& request:state_.respawnQueue) if(request.active) request.delay-=dt;
    int active=0;
    for(const auto& target:state_.targets) if(gameplay::isActiveHuman(target)) ++active;
    for(int q=TARGET_COUNT-1;q>=0 && active<activeHumanTarget();--q) {
        HumanRespawnRequest& request=state_.respawnQueue[q];
        if(!request.active || request.delay>0.0f) continue;
        int slot=-1;
        for(int i=0;i<TARGET_COUNT;++i){const TargetState& target=state_.targets[i]; if(!target.alive && !target.captureQueued && !target.captureCommitted && target.soulState==SoulState::Free){slot=i;break;}}
        if(slot<0) break;
        respawnTarget(slot);
        TargetState& target=state_.targets[slot];
        target.pos=chooseHumanSpawnPoint(slot,&request.avoid); chooseHumanWalkTarget(slot); request=HumanRespawnRequest{}; ++active;
    }
}
void Game::respawnTarget(int index) {
    TargetState& t = state_.targets[index]; t = TargetState{}; t.alive = true;
    t.brute = seededRoomValue(520 + index) < 0.18f;
    t.armor = t.brute ? SOUL_ARMOR_BRUTE : SOUL_ARMOR_NORMAL;
    t.scale = t.brute ? HUMAN_SCALE_BRUTE : 1.0f;
    t.health = 1.0f;
    t.pos = chooseHumanSpawnPoint(index);
    t.visualYaw = seededRoomValue(540 + index) * DB_PI * 2.0f;
    t.visualWalkPhase = seededRoomValue(560 + index) * DB_PI * 2.0f;
    t.floatOffset=seededRoomValue(565+index)*DB_PI*2.0f;
    t.spinSpeed=0.4f+seededRoomValue(570+index)*0.8f;
    t.attackCooldown=seededRoomValue(580+index)*0.5f;
    t.attackVariant=static_cast<int>(seededRoomValue(590+index)*4.0f)%4;
    resetSoulLattice(t);
    chooseHumanWalkTarget(index);
    syncTargetReactionVisual(t);
}

Vec3 Game::chooseHumanSpawnPoint(int index, const Vec3* avoid) const {
    const float tileOrigin=getRoomTileOriginZ(state_.topology.currentTileIndex);
    const float playerLocalZ=wrapZ(state_.player.pos.z);
    Vec3 best{0.0f,GROUND_Y,tileOrigin+ROOM_MIN_SPAWN_Z+4.5f};
    float bestScore=-1.0e9f;
    for(int attempt=0;attempt<32;++attempt){
        // Each browser candidate gets its own obstacle-clearance retries.
        Vec3 candidate=best;
        for(int retry=0;retry<18;++retry){
            const int seed=index+attempt*17+state_.roomIndex*31+retry*101;
            const float x=(seededRoomValue(900+seed)-0.5f)*20.0f;
            const float localZ=ROOM_MIN_SPAWN_Z+seededRoomValue(1200+seed)*(ROOM_MAX_SPAWN_Z-ROOM_MIN_SPAWN_Z);
            candidate={x,GROUND_Y,tileOrigin+localZ};
            if(!isHumanPointBlocked(x,candidate.z,0.7f)) break;
        }
        if(isHumanPointBlocked(candidate.x,candidate.z,0.7f)) continue;
        const float pdx=candidate.x-state_.player.pos.x,pdz=wrapZ(candidate.z)-playerLocalZ;
        const float playerDistSq=pdx*pdx+pdz*pdz;
        float oldDistSq=1.0e9f;
        if(avoid){const float dx=candidate.x-avoid->x,dz=wrapZ(candidate.z)-wrapZ(avoid->z);oldDistSq=dx*dx+dz*dz;}
        float overlapPenalty=0.0f;
        for(int h=0;h<TARGET_COUNT;++h){if(h==index)continue;const TargetState& other=state_.targets[h];if(!other.alive||other.slurpable||other.soulState!=SoulState::Free)continue;const float dx=candidate.x-other.pos.x,dz=wrapZ(candidate.z)-wrapZ(other.pos.z);if(dx*dx+dz*dz<6.25f)overlapPenalty+=100.0f;}
        const float score=std::min(playerDistSq,144.0f)+std::min(oldDistSq,144.0f)-overlapPenalty;
        if(score>bestScore){bestScore=score;best=candidate;}
        if(playerDistSq>=64.0f&&oldDistSq>=36.0f&&overlapPenalty<=0.0f)return candidate;
    }
    return best;
}

bool Game::isHumanPointBlocked(float x,float z,float radius) const {
    const float localZ=wrapZ(z);
    if(x < -ROOM_WIDTH*0.5f+radius || x > ROOM_WIDTH*0.5f-radius) return true;
    for(int i=0;i<state_.debug.colliderCount;++i){const RoomCollider& c=state_.roomColliders[i];
        if(x>c.minX-radius && x<c.maxX+radius && localZ>c.minZ-radius && localZ<c.maxZ+radius) return true;}
    return false;
}

void Game::chooseHumanWalkTarget(int index) {
    TargetState& t=state_.targets[index];
    const float tileOrigin=getRoomTileOriginZ(getRoomTileIndex(t.pos.z));
    for(int attempt=0;attempt<10;++attempt){
        const int seed=700+index*97+t.walkTargetSequence*19+attempt*2;
        const float angle=seededRoomValue(seed)*DB_PI*2.0f;
        const float radius=1.8f+seededRoomValue(seed+1)*HUMAN_WALK_RANGE;
        const float x=clampf(t.pos.x+std::cos(angle)*radius,-ROOM_WIDTH*0.5f+1.1f,ROOM_WIDTH*0.5f-1.1f);
        const float localZ=clampf(wrapZ(t.pos.z)+std::sin(angle)*radius,ROOM_MIN_SPAWN_Z,ROOM_MAX_SPAWN_Z);
        if(!isHumanPointBlocked(x,tileOrigin+localZ,0.5f)){t.walkTarget={x,GROUND_Y,tileOrigin+localZ}; ++t.walkTargetSequence; return;}
    }
    t.walkTarget=t.pos; ++t.walkTargetSequence;
}

void Game::releaseTargetGrab(int targetIndex){if(targetIndex<0||targetIndex>=TARGET_COUNT)return;TargetState& target=state_.targets[targetIndex];const int id=target.grabbedPlayerId;if(id==0){state_.player.grabbedByTarget=-1;state_.player.grabEscape=0;state_.player.grabLastDirection=0;clearInputState();}else if(id>0&&id<NETWORK_PLAYER_COUNT&&state_.multiplayer.peers[id].active){auto& player=state_.multiplayer.peers[id].player;player.grabbedByTarget=-1;player.grabEscape=0;player.grabLastDirection=0;state_.multiplayer.peers[id].input=InputState{};}target.grabbedPlayerId=-1;target.grabCooldown=18.0f;target.attackTimer=0;target.attackCooldown=1.15f;}

void Game::updateTargetGrab(int targetIndex,float dt){TargetState& target=state_.targets[targetIndex];const int id=target.grabbedPlayerId;PlayerState* player=id==0?&state_.player:(id>0&&id<NETWORK_PLAYER_COUNT&&state_.multiplayer.peers[id].active?&state_.multiplayer.peers[id].player:nullptr);InputState* input=id==0?&state_.input:(id>0&&id<NETWORK_PLAYER_COUNT&&state_.multiplayer.peers[id].active?&state_.multiplayer.peers[id].input:nullptr);if(!player||!input||!player->alive||player->downed){releaseTargetGrab(targetIndex);return;}const Vec3 forward{-std::sin(target.visualYaw),0,-std::cos(target.visualYaw)};player->pos=target.pos+forward*0.46f+Vec3{0,0.78f,0};player->vel={};player->jumpVel=0;player->grounded=false;player->battery=std::max(0.0f,player->battery-6.0f*dt);const float axis=std::abs(input->wiggleAxis)>0.001f?input->wiggleAxis:((input->right?1.0f:0.0f)-(input->left?1.0f:0.0f)+input->touchMoveX);input->wiggleAxis=0.0f;const int direction=axis>0.55f?1:(axis<-0.55f?-1:0);if(direction!=0&&direction!=player->grabLastDirection){player->grabLastDirection=direction;player->grabEscape=std::min(1.0f,player->grabEscape+0.20f);}if(player->grabEscape>=1.0f){releaseTargetGrab(targetIndex);player->vel=forward*3.0f;return;}if(player->battery<=0.0f){if(state_.multiplayer.enabled){player->downed=true;player->bleedoutTimer=15.0f;player->reviveCharge=0;}else if(player->souls>0&&!player->soloSoulRebootUsed){--player->souls;player->battery=15.0f;player->soloSoulRebootUsed=true;}else triggerRunDeath();releaseTargetGrab(targetIndex);}}

void Game::updateTargets(float dt) {
    state_.enemyAttackCadence=std::max(0.0f,state_.enemyAttackCadence-dt);
    if(state_.enemyAttackOwner>=0){const TargetState& owner=state_.targets[state_.enemyAttackOwner];if(!owner.alive||owner.slurpable||owner.attackTimer<=0.0f)state_.enemyAttackOwner=-1;}
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i];
        if (!t.alive) continue;
        gameplay::updateLooseSoulMotion(t, dt);
        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);
        t.visibility = 1.0f;
        t.vacuumPullAmount = 0.0f;
        t.captureCollapseAmount = clampf(t.ingestProgress, 0.0f, 1.0f);
        t.grabCooldown=std::max(0.0f,t.grabCooldown-dt);
        if(t.grabbedPlayerId>=0){updateTargetGrab(i,dt);continue;}
        if (t.slurpable) {
            t.soulMorph = std::min(1.0f, t.soulMorph + dt / SOUL_MORPH_DURATION);
            t.locomotionAmount = 0.0f;
        } else {
            t.soulMorph = 0.0f;
            t.armorRegenDelay=std::max(0.0f,t.armorRegenDelay-dt);
            if(t.armorRegenDelay<=0.0f){
                const float fullArmor=t.brute?SOUL_ARMOR_BRUTE:SOUL_ARMOR_NORMAL;
                if(t.armor<fullArmor){
                    const float roomScale=std::log2(1.0f+static_cast<float>(std::max(0,state_.roomIndex)));
                    const float regenPerSecond=std::min(0.50f,0.035f+roomScale*0.018f+state_.progression.run.roomHeat*0.16f);
                    t.armor=std::min(fullArmor,t.armor+regenPerSecond*dt);
                }
            }
            const float currentTileOrigin=getRoomTileOriginZ(state_.topology.currentTileIndex);
            const float targetTileOrigin=getRoomTileOriginZ(getRoomTileIndex(t.pos.z));
            if(std::abs(currentTileOrigin-targetTileOrigin)>0.001f){const float shift=currentTileOrigin-targetTileOrigin; t.pos.z+=shift; t.walkTarget.z+=shift;}
            t.pos.y=GROUND_Y; t.attackCooldown=std::max(0.0f,t.attackCooldown-dt);
            int attackedPlayerId=0;
            Vec3 attackedPlayerPos=state_.player.pos;
            float nearestPlayerDistance=state_.player.downed?9999.0f:horizontalLength(Vec3{attackedPlayerPos.x-t.pos.x,0,attackedPlayerPos.z-t.pos.z});
            if(state_.multiplayer.authoritativeHost){for(int id=1;id<NETWORK_PLAYER_COUNT;++id){const auto& peer=state_.multiplayer.peers[id];if(!peer.active||!peer.player.alive||peer.player.downed)continue;const float distance=horizontalLength(Vec3{peer.player.pos.x-t.pos.x,0,peer.player.pos.z-t.pos.z});if(distance<nearestPlayerDistance){nearestPlayerDistance=distance;attackedPlayerId=id;attackedPlayerPos=peer.player.pos;}}}
            Vec3 toPlayer{attackedPlayerPos.x-t.pos.x,0,attackedPlayerPos.z-t.pos.z};
            float playerDist=horizontalLength(toPlayer);
            if(playerDist>0.001f && playerDist<HUMAN_ATTACK_NOTICE_RANGE) t.visualYaw=std::atan2(-toPlayer.x/playerDist,-toPlayer.z/playerDist);
            if(t.attackTimer>0.0f){
                t.attackTimer=std::max(0.0f,t.attackTimer-dt); t.locomotionAmount=0.0f;
                const float progress=1.0f-clampf(t.attackTimer/HUMAN_ATTACK_DURATION,0.0f,1.0f);
                if(progress<HUMAN_SWING_COMMIT_PHASE&&playerDist>0.001f){t.attackDirection=toPlayer*(1.0f/playerDist);t.attackTargetPlayerId=attackedPlayerId;t.visualYaw=std::atan2(-t.attackDirection.x,-t.attackDirection.z);}
                else {
                    t.visualYaw=std::atan2(-t.attackDirection.x,-t.attackDirection.z);
                    attackedPlayerId=t.attackTargetPlayerId;
                    attackedPlayerPos=state_.player.pos;
                    if(attackedPlayerId>0&&attackedPlayerId<NETWORK_PLAYER_COUNT&&state_.multiplayer.peers[attackedPlayerId].active)attackedPlayerPos=state_.multiplayer.peers[attackedPlayerId].player.pos;
                    else attackedPlayerId=0;
                }
                const float sweepT=clampf((progress-HUMAN_SWING_COMMIT_PHASE)/(HUMAN_SWING_END_PHASE-HUMAN_SWING_COMMIT_PHASE),0.0f,1.0f);
                const float side=t.attackVariant%2==0?1.0f:-1.0f;
                const float arcAngle=side*lerpf(1.12f,-1.12f,smooth01(sweepT));
                const float cs=std::cos(arcAngle),sn=std::sin(arcAngle);
                const Vec3 swingDir{t.attackDirection.x*cs-t.attackDirection.z*sn,0,t.attackDirection.x*sn+t.attackDirection.z*cs};
                const Vec3 liveToPlayer{attackedPlayerPos.x-t.pos.x,0,attackedPlayerPos.z-t.pos.z};const float liveDist=horizontalLength(liveToPlayer);
                const float sweepFacing=liveDist>0.001f?dot3(liveToPlayer*(1.0f/liveDist),swingDir):-1.0f;
                if(!t.attackHit && progress>=HUMAN_SWING_COMMIT_PHASE && progress<=HUMAN_SWING_END_PHASE && liveDist<=HUMAN_ATTACK_HIT_RANGE+0.18f && sweepFacing>=0.90f){
                    const Vec3 away=liveDist>0.001f?liveToPlayer*(-1.0f/liveDist):Vec3{0,0,1};
                    PlayerState* victim=attackedPlayerId==0?&state_.player:&state_.multiplayer.peers[attackedPlayerId].player;
                    if(victim->battery<22.0f&&!victim->downed&&victim->grabbedByTarget<0&&t.grabCooldown<=0.0f){victim->grabbedByTarget=i;victim->grabEscape=0;victim->grabLastDirection=0;t.grabbedPlayerId=attackedPlayerId;t.attackTimer=0.0f;state_.enemyAttackOwner=-1;}
                    else if(attackedPlayerId==0){state_.player.vel+=away*HUMAN_ATTACK_KNOCKBACK;spendBattery(HUMAN_ATTACK_BATTERY_COST,BatteryReason::Hit);}
                    else {NetworkPeerState local;savePlayerContext(local);loadPlayerContext(state_.multiplayer.peers[attackedPlayerId]);simulationPlayerId_=attackedPlayerId;state_.player.vel+=away*HUMAN_ATTACK_KNOCKBACK;spendBattery(HUMAN_ATTACK_BATTERY_COST,BatteryReason::Hit);savePlayerContext(state_.multiplayer.peers[attackedPlayerId]);simulationPlayerId_=0;loadPlayerContext(local);}
                    if(state_.time-state_.audio.lastDamageAckTime>=0.18f){state_.audio.lastDamageAckTime=state_.time;emitAudio(AudioCue::NegativeAck,0.26f);}
                    t.attackHit=true;
                }
                if(t.attackTimer<=0.0f&&state_.enemyAttackOwner==i){state_.enemyAttackOwner=-1;state_.enemyAttackCadence=0.34f;}
            } else if(playerDist<HUMAN_ATTACK_START_RANGE && t.attackCooldown<=0.0f && state_.enemyAttackOwner<0 && state_.enemyAttackCadence<=0.0f){
                t.attackTimer=HUMAN_ATTACK_DURATION; t.attackCooldown=HUMAN_ATTACK_COOLDOWN;
                t.attackVariant=(t.attackVariant+1)%4; t.attackHit=false; t.locomotionAmount=0.0f;
                t.attackDirection=playerDist>0.001f?toPlayer*(1.0f/playerDist):Vec3{0,0,-1};t.attackTargetPlayerId=attackedPlayerId;state_.enemyAttackOwner=i;
            } else {
                Vec3 destination=(playerDist<HUMAN_ATTACK_NOTICE_RANGE && playerDist>HUMAN_ATTACK_START_RANGE*0.88f)?attackedPlayerPos:t.walkTarget;
                Vec3 delta{destination.x-t.pos.x,0,destination.z-t.pos.z}; float dist=horizontalLength(delta);
                if(dist<HUMAN_WALK_TARGET_RADIUS && playerDist>=HUMAN_ATTACK_NOTICE_RANGE){chooseHumanWalkTarget(i); delta=t.walkTarget-t.pos; delta.y=0; dist=horizontalLength(delta);}
                if(dist>0.001f){
                    const Vec3 dir=delta*(1.0f/dist); const float aggro=playerDist<HUMAN_ATTACK_NOTICE_RANGE?1.28f:1.0f;
                    const float variation=0.82f+0.18f*std::sin(static_cast<float>(i)*12.9898f);
                    const float speed=HUMAN_WALK_SPEED*aggro*(t.brute?0.56f:1.0f)*variation;
                    const float step=std::min(dist,speed*dt); const Vec3 next=t.pos+dir*step;
                    if(isHumanPointBlocked(next.x,next.z,0.42f)) chooseHumanWalkTarget(i);
                    else {t.pos=next; t.visualYaw=std::atan2(-dir.x,-dir.z); t.visualWalkPhase+=step*HUMAN_WALK_PHASE_PER_METER;}
                    t.locomotionAmount=step>0.00001f?1.0f:0.0f;
                } else t.locomotionAmount=0.0f;
            }
        }
        t.soulCubeAmount = t.slurpable ? smooth01(t.soulMorph) : 0.0f;
        if ((!t.slurpable || t.soulMorph < 0.995f) && t.ingestProgress < 0.01f) t.humanAnimationTime += dt;
        syncTargetReactionVisual(t);
    }
}

void Game::updateVacuum(float dt) {
    VacuumState& v = state_.vacuum;
    if(state_.player.ledgeHanging||state_.player.ledgeMantleTimer>0.0f)v.active=false;
    v.power = clampf(v.power + (v.active ? VACUUM_CHARGE_SPEED : -VACUUM_DECAY_SPEED) * dt, 0, 1);
    v.pose += ((v.active ? 1.0f : 0.0f) - v.pose) * std::min(1.0f, dt * 10.0f);
    v.fieldStrength += ((v.active ? 1.0f : 0.0f) - v.fieldStrength) * std::min(1.0f, dt * 5.0f);
    v.target = -1;
    const bool attractionActive = v.active && v.power > 0.32f;
    const Vec3 pullPoint = state_.phoneTransform.vacuumPullPoint;
    auto nearestWorldPos = [&](const TargetState& target) {
        Vec3 p = target.pos;
        const int centerTile = getRoomTileIndex(pullPoint.z);
        float bestZ = wrapZ(target.pos.z) + getRoomTileOriginZ(centerTile);
        float bestDist = std::abs(bestZ - pullPoint.z);
        for (int offset : {-1, 1}) {
            const float candidate = wrapZ(target.pos.z) + getRoomTileOriginZ(centerTile + offset);
            const float candidateDist = std::abs(candidate - pullPoint.z);
            if (candidateDist < bestDist) { bestZ = candidate; bestDist = candidateDist; }
        }
        p.z = bestZ;
        return p;
    };
    auto writeCanonical = [&](TargetState& target, const Vec3& world) {
        target.pos = world;
        target.pos.z = wrapZ(world.z) + getRoomTileOriginZ(state_.topology.currentTileIndex);
    };
    auto keepOutsidePhoneSolid = [&](Vec3 world, bool preferScreenFront) {
        if(state_.camera.firstPerson)return world;
        Vec3 local=inverseRotate(state_.phoneTransform.orientation,world-state_.phoneTransform.position);
        const float hx=PHONE_SOLID_HALF_X+SOUL_CORE_SOLID_RADIUS;
        const float hy=PHONE_SOLID_HALF_Y+SOUL_CORE_SOLID_RADIUS;
        const float hz=PHONE_SOLID_HALF_Z+SOUL_CORE_SOLID_RADIUS;
        if(std::abs(local.x)>hx||std::abs(local.y)>hy||std::abs(local.z)>hz)return world;
        if(preferScreenFront)local.z=hz;
        else {
            const float px=hx-std::abs(local.x),py=hy-std::abs(local.y),pz=hz-std::abs(local.z);
            if(px<=py&&px<=pz)local.x=(local.x<0.0f?-1.0f:1.0f)*hx;
            else if(py<=px&&py<=pz)local.y=(local.y<0.0f?-1.0f:1.0f)*hy;
            else local.z=(local.z<0.0f?-1.0f:1.0f)*hz;
        }
        return state_.phoneTransform.position+rotate(state_.phoneTransform.orientation,local);
    };
    auto insideCylinder = [&](const Vec3& p) {
        const Vec3 d = p - state_.phoneTransform.position;
        return d.x*d.x + d.z*d.z <= SOUL_CAPTURE_CYLINDER_RADIUS*SOUL_CAPTURE_CYLINDER_RADIUS &&
            std::abs(d.y) <= SOUL_CAPTURE_CYLINDER_HEIGHT * 0.5f;
    };
    auto inOffer = [&](const Vec3& p) {
        const Vec3 toSoul = p - state_.camera.pos;
        const float forwardDistance = dot3(toSoul, state_.camera.forward);
        if (forwardDistance <= 0.0f || forwardDistance > SOUL_ATTRACTION_RANGE) return false;
        const Vec3 radial = toSoul - state_.camera.forward * forwardDistance;
        const float coneRadius = SOUL_ATTRACTION_CONE_RADIUS * (0.24f + forwardDistance / SOUL_ATTRACTION_RANGE);
        return lengthSq(radial) <= coneRadius * coneRadius;
    };

    int offeredFreeSoul = -1;
    float offeredScore = 1e9f;
    if (attractionActive) {
        for (int i = 0; i < TARGET_COUNT; ++i) {
            TargetState& t = state_.targets[i];
            if (!gameplay::isFreeVacuumOffer(t)) continue;
            const Vec3 p = nearestWorldPos(t);
            if (!inOffer(p)) continue;
            const float score = length(pullPoint - p) + (insideCylinder(p) ? -3.5f : 0.0f);
            if (score < offeredScore) { offeredScore = score; offeredFreeSoul = i; }
        }
    }

    float bestScore = 1e9f;
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i];
        if (!gameplay::isLooseSoul(t)) continue;
        if(t.networkOwnerPlayerId>=0&&t.networkOwnerPlayerId!=simulationPlayerId_) continue;
        if (t.captureQueued || t.captureCommitted) continue;
        const Vec3 soulWorld = nearestWorldPos(t);
        const bool offered = attractionActive &&
            (t.soulState == SoulState::Latched || t.soulState == SoulState::Ingesting || i == offeredFreeSoul || insideCylinder(soulWorld));
        if (!attractionActive && (t.soulState == SoulState::Latched || t.soulState == SoulState::Ingesting)) {
            releaseSoul(i); continue;
        }
        if (!offered) {
            if (t.soulState == SoulState::Attracted) { t.soulState = SoulState::Free; t.networkOwnerPlayerId=-1; }
            if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; }
            continue;
        }
        if (t.soulState == SoulState::Free) { t.soulState = SoulState::Attracted; t.networkOwnerPlayerId=simulationPlayerId_; }
        const float score = length(pullPoint - soulWorld) + (insideCylinder(soulWorld) ? -2.5f : 0.0f);
        if (score < bestScore) { bestScore = score; v.target = i; }
        const float soulMass = t.brute ? 1.45f : 1.0f;
        if (t.soulState == SoulState::Attracted) {
            Vec3 delta = pullPoint - soulWorld; const float d = std::max(length(delta), 0.001f);
            const float proximity = 1.0f - clampf(d / SOUL_ATTRACTION_RANGE, 0.0f, 1.0f);
            const float speed = v.power * (3.2f + smooth01(proximity)*5.8f + (insideCylinder(soulWorld)?8.5f:0.0f)) / soulMass;
            Vec3 next = soulWorld + normalized(delta) * std::min(d, speed*dt);
            next=keepOutsidePhoneSolid(next,insideCylinder(soulWorld));
            writeCanonical(t, next); t.vel = {}; t.vacuumPullAmount = v.power;
            if (insideCylinder(next) || d <= SOUL_LATCH_DISTANCE) { t.soulState = SoulState::Latched; t.latchedToScreen = true; }
            syncTargetReactionVisual(t); continue;
        }
        if (t.soulState == SoulState::Latched || t.soulState == SoulState::Ingesting) {
            t.latchedToScreen = true;
            Vec3 sealPoint = pullPoint;
            if (!state_.camera.firstPerson) {
                Vec3 local = inverseRotate(state_.phoneTransform.orientation, soulWorld - state_.phoneTransform.screenCenter);
                local.x = clampf(local.x, -SCREEN_HALF_WIDTH*0.92f, SCREEN_HALF_WIDTH*0.92f);
                local.y = clampf(local.y, -SCREEN_HALF_HEIGHT*0.92f, SCREEN_HALF_HEIGHT*0.92f);
                local.z = SCREEN_FRONT_OFFSET + SOUL_SEAL_BODY_OFFSET;
                sealPoint = state_.phoneTransform.screenCenter + rotate(state_.phoneTransform.orientation, local);
            }
            t.latchPoint = sealPoint;
            const float phase = clampf(t.ingestProgress, 0.0f, 1.0f);
            const float sealEase = smooth01(clampf((phase-0.08f)/(0.32f-0.08f),0.0f,1.0f));
            const float pressureEase = smooth01(clampf((phase-0.32f)/(0.78f-0.32f),0.0f,1.0f));
            const float popEase = smooth01(clampf((phase-0.78f)/(1.0f-0.78f),0.0f,1.0f));
            Vec3 toSeal = sealPoint - soulWorld; const float d = length(toSeal);
            const float move = std::min(d, ((7.0f+sealEase*5.0f+popEase*14.0f)/soulMass)*dt);
            Vec3 next = d > 0.001f ? soulWorld + normalized(toSeal)*move : soulWorld;
            next=keepOutsidePhoneSolid(next,true);
            writeCanonical(t,next); t.vel={}; t.vacuumPullAmount=std::max(v.power,t.ingestProgress);
            const bool sealed = std::max(0.0f,d-move) <= SOUL_SEAL_DISTANCE;
            t.soulState = sealed ? SoulState::Ingesting : SoulState::Latched;
            if (sealed) {
                const float slurpRate=std::pow(1.18f,static_cast<float>(state_.runRules.fasterSlurpStacks));
                t.ingestProgress=clampf(t.ingestProgress+dt*v.power*(0.38f+sealEase*0.55f+pressureEase*0.85f+popEase*2.25f)*slurpRate/soulMass,0.0f,1.0f);
                t.health-=VACUUM_DAMAGE*v.power*dt*0.10f;
            }
            t.captureCollapseAmount=t.ingestProgress; syncTargetReactionVisual(t);
            if (t.ingestProgress>=SOUL_CAPTURE_COMMIT_PHASE || t.health<=0.0f) queueSoulCapture(i);
        }
    }
    const float lockTarget = (v.target != -1 && attractionActive) ? 1.0f : 0.0f;
    v.lockStrength += (lockTarget - v.lockStrength) * std::min(1.0f, dt * 8.0f);
    v.coneTightness += (lockTarget - v.coneTightness) * std::min(1.0f, dt * 4.0f);
}

void Game::resetSoulLattice(TargetState& target) {
    target.latticeVisualPull=0;target.latticeVisualPullVelocity=0;target.tetherVisible=false;target.tetherWidth=0;
    for(int n=0;n<SOUL_LATTICE_NODE_COUNT;++n){target.latticePos[n]=latticeRest(n);target.latticeSurfacePos[n]=target.latticePos[n];target.latticeVel[n]={};}
}

void Game::updateSoulLattices() {
    const Vec3 up{0,1,0};const float step=0.016f;
    for(int i=0;i<TARGET_COUNT;++i){TargetState& target=state_.targets[i];
        if(!target.alive){target.tetherVisible=false;continue;}
        const int owner=target.networkOwnerPlayerId;
        const bool remoteOwner=owner>0&&owner<NETWORK_PLAYER_COUNT&&state_.multiplayer.peers[owner].active;
        const PhoneTransformState& ownerPhone=remoteOwner?state_.multiplayer.peers[owner].phoneTransform:state_.phoneTransform;
        const VacuumState& ownerVacuum=remoteOwner?state_.multiplayer.peers[owner].vacuum:state_.vacuum;
        const Vec3 center=target.pos+Vec3{0,0.57f+target.soulVisual.verticalOffset,0};
        Vec3 pullDir=ownerPhone.vacuumPullPoint-center;if(lengthSq(pullDir)>0.001f)pullDir=normalized(pullDir);else pullDir={std::sin(state_.time*target.spinSpeed),0,std::cos(state_.time*target.spinSpeed)};
        Vec3 side{pullDir.z,0,-pullDir.x};if(lengthSq(side)<0.001f)side={1,0,0};else side=normalized(side);
        const bool slurpable=target.slurpable,latched=target.latchedToScreen;
        const bool owned=target.soulState==SoulState::Attracted||target.soulState==SoulState::Latched||target.soulState==SoulState::Ingesting;
        const bool livePin=ownerVacuum.active&&ownerVacuum.power>0.01f&&(ownerVacuum.target==i||owned);
        const float ingest=clampf(target.ingestProgress,0,1);const bool activelyPulled=livePin||ingest>0.01f;
        const float sealPhase=smoothRange(ingest,0.32f,0.56f),pressurePhase=smoothRange(ingest,0.56f,0.86f),popPhase=smoothRange(ingest,0.86f,1.0f);
        const float breakStress=slurpable?1.0f:clampf(1-target.armor/(target.brute?SOUL_ARMOR_BRUTE:SOUL_ARMOR_NORMAL),0,1),suction=clampf(1-target.health,0,1),hit=clampf(target.hitFlash,0,1);
        const float visualTarget=std::max({livePin?ownerVacuum.power*ownerVacuum.lockStrength:0.0f,ingest,suction*0.25f,breakStress*0.16f,hit*0.18f});
        springScalar(target.latticeVisualPull,target.latticeVisualPullVelocity,activelyPulled?visualTarget:0,activelyPulled?4.8f:3.2f,activelyPulled?0.78f:0.58f,step,target.latticeVisualPull,target.latticeVisualPullVelocity);
        const float visualPull=clampf(target.latticeVisualPull,0,1),slurpStrength=clampf(std::max(visualPull,livePin?ownerVacuum.power:0),0,1);
        const float latchLimpness=latched?clampf(0.18f+slurpStrength*0.62f+ingest*0.34f,0,1):0;
        const float ingestEase=smooth01(ingest),collapse=std::max({smoothRange(suction,0.76f,1.0f),pressurePhase*0.55f,popPhase});
        const bool tetherAllowed=livePin&&owned;const float tetherIn=tetherAllowed?std::max(smoothRange(visualPull,0.08f,0.38f),smoothRange(ingest,0.02f,0.22f)):0;
        const float readyPulse=slurpable?1+std::sin(state_.time*18.0f+i)*0.055f:1,hitPulse=1+hit*0.16f,idleBreath=(1+std::sin(state_.time*3.0f+i*1.7f)*0.035f)*readyPulse*hitPulse;
        Vec3 anchorSum{};float anchorWeight=0,bestScore=-1e9f;Vec3 bestTip=center;
        for(int n=0;n<SOUL_LATTICE_NODE_COUNT;++n){const Vec3 rest=latticeRest(n);const float restLen=std::max(length(rest),0.001f);const Vec3 outward=rest*(1/restLen);const float facing=clampf(dot3(outward,pullDir)*0.5f+0.5f,0,1),surface=latticeSurface(n),corner=latticeCorner(n);
            const float breathe=std::sin(state_.time*4.0f+i*1.31f+n*0.77f)*0.018f,stressWave=std::sin(state_.time*12.0f+i*2.17f+n*1.9f)*visualPull,direct=surface*visualPull*std::pow(facing,1.35f),faceCluster=std::max(surface*std::pow(facing,2.4f),direct);
            const float armPattern=surface*std::pow(facing,3.0f)*(0.5f+0.5f*std::sin(state_.time*7.0f+i*4.9f+n*2.2f)),cheek=surface*std::pow(facing,1.7f)*(1-corner*0.55f)*(0.65f+0.35f*std::sin(state_.time*11.0f+i+n));
            Vec3 desired=rest*(idleBreath+breathe);const float neck=surface*std::pow(facing,4.2f),shoulder=surface*std::pow(facing,1.8f)*(1-neck),rearLag=surface*std::pow(1-facing,1.4f),axisOffset=dot3(rest,pullDir);const Vec3 radial=desired-pullDir*axisOffset;const float taper=visualPull*(neck*0.72f+shoulder*0.28f+collapse*facing*0.55f);
            desired+=pullDir*(visualPull*neck*(0.18f+armPattern*0.08f)+direct*0.12f+visualPull*shoulder*0.10f+collapse*facing*0.12f-visualPull*rearLag*0.13f);desired+=radial*(-taper);desired+=side*(stressWave*surface*(0.04f+cheek*0.04f));desired+=up*(std::sin(state_.time*10.0f+i*3.1f+n)*visualPull*surface*0.045f);
            if(ingest>0.001f&&livePin){const float localDepth=facing,nodeDelay=(1-localDepth)*0.36f+corner*0.05f,nodeIngest=clampf((ingestEase-nodeDelay)/std::max(1-nodeDelay,0.001f),0,1),nodeEase=smooth01(nodeIngest);const float apertureX=clampf((rest.x/0.23f)*0.035f,-0.035f,0.035f),apertureY=clampf((rest.y/0.23f)*0.0625f,-0.0625f,0.0625f);const Vec3 aperture=ownerPhone.screenCenter+ownerPhone.screenRight*apertureX+ownerPhone.screenUp*apertureY+ownerPhone.screenNormal*0.018f;const Vec3 toScreen=aperture-(center+desired);const float sealWeight=sealPhase*std::pow(localDepth,2.6f),pressureWeight=pressurePhase*(0.25f+localDepth*0.55f),rearBulge=pressurePhase*(1-popPhase)*std::pow(1-localDepth,1.7f),popWeight=popPhase*(0.45f+nodeEase*0.85f),swallow=clampf(sealWeight+pressureWeight+popWeight,0,1),squeeze=(sealWeight*0.35f+pressureWeight*0.68f+popWeight)*surface;desired+=toScreen*swallow+radial*(-squeeze)+pullDir*(-rearBulge*0.18f+popWeight*0.16f);const float jitter=std::sin(state_.time*35.0f+i*3.7f+n*1.9f)*pressurePhase*(1-popPhase)*0.018f;desired+=side*(jitter*surface)+up*(jitter*0.7f*surface);}
            Vec3& current=target.latticePos[n];Vec3& velocity=target.latticeVel[n];const float stiffness=latched?7.0f+(1-latchLimpness)*6.5f:(activelyPulled?14.5f:20.0f),damping=latched?0.84f+latchLimpness*0.11f:(activelyPulled?0.76f:0.66f);velocity+=(desired-current)*(stiffness*step);if(!activelyPulled)velocity+=(rest-current)*(1.15f*step*60.0f);velocity*=std::pow(damping,step*60.0f);current+=velocity*step;if(!activelyPulled&&lengthSq(current-rest)<0.00008f&&lengthSq(velocity)<0.00008f){current=rest;velocity={};}
            const Vec3 world=center+current;if(faceCluster>0.35f){anchorSum+=world*faceCluster;anchorWeight+=faceCluster;}const float score=faceCluster*4+visualPull*facing-length(world-ownerPhone.vacuumPullPoint)*0.18f;if(surface>0.5f&&score>bestScore){bestScore=score;bestTip=world;}
            Vec3 surfacePos=current;if(latched){const float rear=1-facing,limp=clampf(latchLimpness+sealPhase*0.16f+pressurePhase*0.18f,0,1),sag=limp*rear*(1-popPhase)*(0.20f+slurpStrength*0.24f),smear=rear*limp*(1-popPhase)*(0.12f+slurpStrength*0.20f);surfacePos.y-=sag;surfacePos+=pullDir*(-smear);surfacePos*=1-limp*rear*0.10f;const Vec3 surfaceWorld=center+surfacePos;const float minFront=0.006f+rear*limp*(0.018f+slurpStrength*0.022f),signedDistance=dot3(surfaceWorld-ownerPhone.screenCenter,ownerPhone.screenNormal);if(popPhase<0.72f&&signedDistance<minFront)surfacePos+=ownerPhone.screenNormal*(minFront-signedDistance);}target.latticeSurfacePos[n]=surfacePos;
        }
        target.tetherAnchor=anchorWeight>0.001f?(anchorSum*(1/anchorWeight))*0.38f+bestTip*0.62f:center+pullDir*0.38f;target.tetherDestination=ownerPhone.vacuumPullPoint;target.tetherVisible=tetherAllowed&&tetherIn>0.01f;target.tetherWidth=target.tetherVisible?std::max(0.12f,(0.92f-visualPull*0.62f-collapse*0.18f-ingestEase*0.26f)*tetherIn):0;
    }
}

void Game::updateCrosshair(float dt) {
    HudState& hud=state_.hud;
    const bool fullyCommitted=state_.dead||!state_.started||state_.cinematic.introActive||state_.uiPaused
        ||state_.meleeVisual.airLungeLandingPending;
    const bool shooting=state_.energy.dischargeTimer>0.0f||state_.energy.dischargePositionAmount>0.01f
        ||state_.hud.shootJoinTimer>0.0f;
    const bool crosshairAction=state_.vacuum.active||shooting;
    bool soulInFlight=false;for(const auto& bullet:state_.bullets)if(bullet.alive){soulInFlight=true;break;}
    const bool critAction=state_.meleeVisual.locomotionLunge||state_.meleeVisual.airLungeLandingPending||shooting||soulInFlight;
    const float critTarget=!state_.dead&&state_.started&&!state_.cinematic.introActive&&!state_.uiPaused&&critAction?1.0f:0.0f;
    hud.critMarkerOpacity+=(critTarget-hud.critMarkerOpacity)*std::min(1.0f,dt*(critTarget>hud.critMarkerOpacity?8.0f:6.0f));
    const float opacityTarget=!fullyCommitted&&crosshairAction?1.0f:0.0f;
    const float opacityResponse=opacityTarget>hud.crosshairOpacity?12.0f:14.0f;
    hud.crosshairOpacity+=(opacityTarget-hud.crosshairOpacity)*std::min(1.0f,dt*opacityResponse);
    const float spinDt=std::min(dt,0.05f);
    const float hover=std::sin(state_.time*3.0f)*2.0f;
    const float slurpStrength=state_.vacuum.active
        ? clampf(state_.vacuum.power*(0.72f+state_.vacuum.lockStrength*0.28f),0.0f,1.0f)
        : 0.0f;
    float spread=state_.vacuum.active?15.0f+slurpStrength*8.0f+hover:8.0f+hover;
    if(slurpStrength>0.01f) {
        const float spinSpeed=80.0f+slurpStrength*620.0f;
        hud.crosshairRotationDegrees=std::fmod(hud.crosshairRotationDegrees+spinSpeed*spinDt,360.0f);
    } else {
        hud.crosshairRotationDegrees+=(0.0f-hud.crosshairRotationDegrees)*std::min(1.0f,spinDt*8.0f);
    }
    if(hud.shootJoinTimer>0.0f) {
        const float joinEase=clampf(hud.shootJoinTimer/0.18f,0.0f,1.0f);
        spread=lerpf(spread,0.0f,joinEase);
        hud.crosshairRotationDegrees=lerpf(hud.crosshairRotationDegrees,45.0f,joinEase);
        hud.shootJoinTimer=std::max(0.0f,hud.shootJoinTimer-dt);
    }
    hud.crosshairSpreadPixels=spread;
}

void Game::updateBullets(float dt) {
    for (auto& b : state_.bullets) if (b.alive) {
        const Vec3 previous=b.pos;
        b.life-=dt;
        b.vel.y-=BULLET_GRAVITY*dt;
        const float drag=std::pow(BULLET_AIR_DRAG_PER_SECOND,dt);
        b.vel*=drag;
        b.pos+=b.vel*dt;
        b.spin+=dt*10.0f;

        bool deposited=false;
        if(!state_.roomClear){
            const float hitRadius=ROOM_DEPOSIT_HIT_RADIUS*1.35f;
            const float missRadius=hitRadius*1.85f;
            const float tileOrigin=getRoomTileOriginZ(state_.topology.currentTileIndex);
            for(int i=0;i<state_.requiredSouls;++i){
                if(state_.captures[i].filled) continue;
                Vec3 goal=state_.captures[i].pos; goal.z+=tileOrigin;
                const bool sphereHit=lengthSq(b.pos-goal)<hitRadius*hitRadius || pointSegmentDistanceSq(goal,previous,b.pos)<hitRadius*hitRadius;
                bool wallHit=false;
                const float wallZ=goal.z;
                if((previous.z-wallZ)*(b.pos.z-wallZ)<=0.0f){
                    const float span=b.pos.z-previous.z;
                    const float t=std::abs(span)>0.0001f?clampf((wallZ-previous.z)/span,0.0f,1.0f):0.0f;
                    const float crossX=previous.x+(b.pos.x-previous.x)*t;
                    const float crossY=previous.y+(b.pos.y-previous.y)*t;
                    wallHit=std::abs(crossX-goal.x)<hitRadius*0.95f && std::abs(crossY-goal.y)<hitRadius*0.95f;
                }
                if(!sphereHit && !wallHit) continue;
                int filledSlot=0;
                for(int fill=0;fill<state_.requiredSouls;++fill) if(!state_.captures[fill].filled){state_.captures[fill].filled=true; awardGoalToken(state_.captures[fill]); ++state_.depositedSouls; filledSlot=fill; break;}
                emitAudio(static_cast<AudioCue>(static_cast<int>(AudioCue::Capture1)+state_.captureSoundSlots[filledSlot%5]),0.72f);emitAudio(AudioCue::RewardNice,0.28f);
                spawnParticleBurst(b.pos);
                spawnParticleBurst(goal);
                b.alive=false; deposited=true; break;
            }
            if(!deposited&&!b.depositNearMissPlayed){
                const float wallZ=ROOM_GRID_Z+tileOrigin;
                if((previous.z-wallZ)*(b.pos.z-wallZ)<=0.0f){
                    const float span=b.pos.z-previous.z;
                    const float crossing=std::abs(span)>0.0001f?clampf((wallZ-previous.z)/span,0.0f,1.0f):0.0f;
                    const float crossX=previous.x+(b.pos.x-previous.x)*crossing;
                    const float crossY=previous.y+(b.pos.y-previous.y)*crossing;
                    float nearestMissSq=1e9f;
                    for(int i=0;i<state_.requiredSouls;++i){if(state_.captures[i].filled)continue;const Vec3& goal=state_.captures[i].pos;const float dx=crossX-goal.x,dy=crossY-goal.y;nearestMissSq=std::min(nearestMissSq,dx*dx+dy*dy);}
                    if(nearestMissSq<missRadius*missRadius){b.depositNearMissPlayed=true;emitAudio(AudioCue::PaymentFailure,0.46f);}
                }
            }
        }
        if(deposited) continue;
        for(int i=0;i<TARGET_COUNT && b.alive;++i) {
            TargetState& target=state_.targets[i];
            if(!target.alive || target.slurpable || target.captureQueued || target.captureCommitted) continue;
            const Vec3 shellCenter{target.pos.x,0.65f,target.pos.z};
            const float hitRadius=b.brute?0.95f:0.72f;
            const Vec3 headCenter=targetHeadCenter(target);
            const float headRadius=PASS7_HUMAN_VISUAL_SPEC.headRadius*target.scale+BULLET_HEAD_CONTACT_RADIUS;
            // Fired cubes use the visible face center for precision. The lunge
            // keeps the neck capsule assist, but projectiles at body height
            // must remain body shots so relay-primer and shell damage stay
            // deterministic across platforms.
            const bool headshot=pointSegmentDistanceSq(headCenter,previous,b.pos)<=headRadius*headRadius;
            const bool bodyHit=pointSegmentDistanceSq(shellCenter,previous,b.pos)<=hitRadius*hitRadius;
            if(!bodyHit&&!headshot)continue;
            const int shotLevel=upgradeLevel(UpgradeTrack::Shot);
            const float shotDamage=(b.brute?1.65f:0.9f)*(1.0f+0.07f*static_cast<float>(shotLevel))*outgoingDamageMultiplier();
            if(headshot&&target.grabbedPlayerId>=0)releaseTargetGrab(i);
            if(!damageSoulShell(i,headshot?headshotDamage(target):shotDamage)) continue;
            if(!headshot){state_.progression.run.accuracyStacks=0;state_.progression.run.accuracyMultiplier=1.0f;state_.progression.run.accuracyDecayTimer=0.0f;}
            if(headshot){const float attackProgress=target.attackTimer>0.0f?1.0f-clampf(target.attackTimer/HUMAN_ATTACK_DURATION,0.0f,1.0f):-1.0f;const float armorMax=target.brute?SOUL_ARMOR_BRUTE:SOUL_ARMOR_NORMAL;const float killCharge=target.slurpable?1.0f:1.0f-clampf(target.armor/armorMax,0.0f,1.0f);rewardHeadshot(headCenter,target.slurpable||target.armor<=armorMax*HEADSHOT_CRITICAL_ARMOR_FRACTION,false,attackProgress,killCharge);}
            else {
                const int relay=pairSynergyTier(UpgradeTrack::Shot,UpgradeTrack::Attack);
                if(relay>0){state_.progression.run.relayPrimerStacks=std::min(RELAY_PRIMER_STACK_CAP,state_.progression.run.relayPrimerStacks+1);state_.progression.run.relayPrimerTimer=RELAY_PRIMER_SECONDS+0.2f*relay;}
            }
            target.vel+=b.vel*(0.08f+0.004f*static_cast<float>(shotLevel));
            target.vel.y=std::max(target.vel.y,1.0f);
            b.alive=false;
        }
        if(b.life<=0.0f || b.pos.y<-3.5f || std::abs(b.pos.x)>ROOM_WIDTH*1.25f || std::abs(wrapZ(b.pos.z))>ROOM_DEPTH*1.25f) b.alive=false;
    }
}
void Game::updateCaptures(float dt) {
    (void)dt;
    int filled = 0; for(int i=0;i<state_.requiredSouls;++i) if(state_.captures[i].filled) ++filled;
    state_.depositedSouls=filled;
    const bool wasClear=state_.roomClear;
    state_.roomClear = filled >= state_.requiredSouls;
    if(state_.roomClear && !wasClear){emitAudio(AudioCue::PaymentSuccess,0.68f);emitAudio(AudioCue::RewardWoah,0.44f);if(state_.roomIndex==10){state_.secretTv.knockCueTimer=5.4f;setEnergyTicker("KNOCK KNOCK",2);}}
}
void Game::clampRoom(Vec3& pos) {
    pos.x = clampf(pos.x, -ROOM_WIDTH * 0.5f + PLAYER_WALL_MARGIN, ROOM_WIDTH * 0.5f - PLAYER_WALL_MARGIN);
}
