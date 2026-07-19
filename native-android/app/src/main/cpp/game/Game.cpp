#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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
constexpr float GRAVITY = 14.0f;
constexpr float JUMP_SPEED = 4.5f;
constexpr float AIR_JUMP_SPEED = 4.25f;
constexpr float COYOTE_TIME = 0.12f;
constexpr float JUMP_BUFFER = 0.12f;
constexpr float LANDING_MOMENTUM_BOOST = 1.04f;
constexpr float WALL_CLIMB_SPEED = 3.15f;
constexpr float WALL_CLIMB_GRIP = 0.64f;
constexpr float WALL_CLIMB_MAX_HEIGHT = 1.25f;
constexpr float WALL_CLIMB_PUSH_DOT = -0.18f;
constexpr float CEILING_CLEARANCE = 0.42f;
constexpr float PLAYER_CEILING_BODY_CLEARANCE = 0.42f;
constexpr float PLAYER_COLLISION_RADIUS = 0.34f;
constexpr float PLAYER_SUPPORT_RADIUS = 0.34f;
constexpr float WALL_CLIMB_RADIUS = 0.34f;
constexpr float CAMERA_COLLISION_RADIUS = 0.42f;
constexpr float CAMERA_COLLISION_BACKOFF = 0.16f;
constexpr float INTRO_CAMERA_DURATION = 1.15f;
constexpr float DEATH_CAMERA_DURATION = 1.35f;
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
constexpr float BATTERY_ACTIVE_REGEN = 3.0f;
constexpr float BATTERY_WALK_DRAIN = 0.45f;
constexpr float BATTERY_SPRINT_DRAIN = 3.0f;
constexpr float BATTERY_AIR_DRAIN = 0.9f;
constexpr float BATTERY_WALL_CLIMB_DRAIN = 4.2f;
constexpr float BATTERY_VACUUM_DRAIN = 1.35f;
constexpr float BATTERY_JUMP_COST = 3.0f;
constexpr float BATTERY_DOUBLE_JUMP_COST = 6.0f;
constexpr float BATTERY_SHOOT_COST = 7.0f;
constexpr float BATTERY_CAPTURE_GAIN = 18.0f;
constexpr float BATTERY_SOUL_EFFICIENCY = 0.16f;
constexpr float BATTERY_MELEE_HIT_GAIN = 12.0f;
constexpr float BATTERY_COMBO_GROWTH = 1.22f;
constexpr float BATTERY_COMBO_TIMEOUT = 1.8f;
constexpr float MULTI_HIT_BONUS_GROWTH = 1.12f;
constexpr float MULTI_HIT_BATTERY_BONUS = 4.5f;
constexpr float FLOWER_ATTACK_FEED = 2.8f;
constexpr float FLOWER_SLURP_FEED = 9.0f;
constexpr float FLOWER_PICKUP_VALUE = 46.0f;
constexpr float FLOWER_DROP_CHANCE = 0.26f;
constexpr float FLOWER_PICKUP_RADIUS = 1.05f;
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
constexpr float AIR_MELEE_LOCOMOTION_DISTANCE = 5.40f;
constexpr float AIR_MELEE_BODY_RADIUS = 0.72f;
constexpr float AIR_MELEE_ANGULAR_VELOCITY = 4.4f;
constexpr float AIR_MELEE_ANGULAR_DAMPING = 2.2f;
constexpr float AIR_MELEE_CAMERA_RESPONSE = 5.2f;
constexpr float AIR_MELEE_CAMERA_LAG_DECAY = 2.4f;
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
constexpr int KEY_F = 34;
constexpr int KEY_SHIFT_LEFT = 59;
constexpr int KEY_SHIFT_RIGHT = 60;
constexpr int KEY_SPACE = 62;
constexpr int TOUCH_DOWN = 0;
constexpr int TOUCH_UP = 1;
constexpr int TOUCH_MOVE = 2;
constexpr int TOUCH_CANCEL = 3;

float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float dotXZ(const Vec3& a, const Vec3& b) { return a.x * b.x + a.z * b.z; }
float dot3(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
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
}

void Game::reset() {
    state_ = GameState{};
    resetRoom();
    state_.started=true;
    state_.dead=false;
    state_.uiPaused=false;
    emitAudio(AudioCue::VcInvitation,0.58f);
}

void Game::restart() {
    reset();
    state_.cinematic.introActive = true;
    state_.cinematic.introElapsed = 0.0f;
    state_.cinematic.baseYaw = state_.camera.yaw;
}

void Game::prepareStartScreen(){
    reset();
    state_.started=false;
    state_.dead=false;
    state_.uiPaused=false;
    state_.hud.gameOver=false;
    clearInputState();
    state_.audio=AudioState{};
}

void Game::setUiPaused(bool paused){
    if(!state_.started||state_.dead)return;
    state_.uiPaused=paused;
    clearInputState();
}

float Game::batteryDrainMultiplier() const {
    return 1.0f / (1.0f + static_cast<float>(state_.player.souls) * BATTERY_SOUL_EFFICIENCY);
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

void Game::updateParticles(float dt) {
    for(auto& particle:state_.particles) {
        if(particle.life<=0.0f) continue;
        particle.vel.y-=8.0f*dt;
        particle.pos+=particle.vel*dt;
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
    const float remaining = consumeSupplementalBattery(std::max(0.0f, amount) * batteryDrainMultiplier());
    player.battery = clampf(player.battery - remaining, 0.0f, 100.0f);
    if(reason!=BatteryReason::Continuous){
        const char* label=reason==BatteryReason::Melee?"MELEE":reason==BatteryReason::Shoot?"SHOOT":reason==BatteryReason::Hit?"HIT":reason==BatteryReason::Climb?"CLIMB":"JUMP";char message[48]{};
        const float spent=before-player.battery;if(spent>0.001f)std::snprintf(message,sizeof(message),"-%.1F %s",spent,label);else std::snprintf(message,sizeof(message),"FLOWER %s",label);setEnergyTicker(message,spent>0.001f?1:0);
    }
    updateBatteryAudio(before);
    if (player.battery <= 0.0f) {
        player.battery = 0.0f;
        triggerRunDeath();
        return false;
    }
    return true;
}

void Game::gainBattery(float amount,BatteryReason reason) {
    if (!state_.player.alive) return;
    const float before=state_.player.battery;
    state_.player.battery = clampf(state_.player.battery + std::max(0.0f, amount), 0.0f, 100.0f);
    if(reason!=BatteryReason::Continuous){const char* label=reason==BatteryReason::Ingest?"INGEST":reason==BatteryReason::NextRoom?"ROOM":reason==BatteryReason::Chain?"CHAIN":"COMBO";char message[48]{};std::snprintf(message,sizeof(message),"+%.1F %s",state_.player.battery-before,label);setEnergyTicker(message,reason==BatteryReason::Combo||reason==BatteryReason::Chain?2:0);}
    updateBatteryAudio(before);
}

void Game::emitAudio(AudioCue cue,float volume) {
    AudioState& audio=state_.audio;
    const unsigned int serial=audio.nextSerial++;
    AudioEventState& event=audio.events[(serial-1u)%AUDIO_EVENT_COUNT];
    event.cue=cue; event.serial=serial; event.volume=volume;
}

void Game::updateBatteryAudio(float beforeValue) {
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
    if (drain > 0.0f) spendBattery(drain * dt);
    else gainBattery((active ? BATTERY_ACTIVE_REGEN : BATTERY_IDLE_REGEN) * dt);
}

void Game::triggerRunDeath() {
    if(simulationPlayerId_!=0){state_.player.alive=false;state_.player.battery=0;state_.vacuum=VacuumState{};clearInputState();return;}
    if(state_.dead) return;
    state_.dead=true; state_.started=false; state_.uiPaused=false;
    state_.cinematic.introActive=false;
    state_.cinematic.deathActive=true;
    state_.cinematic.deathElapsed=0.0f;
    state_.cinematic.baseYaw=state_.camera.yaw;
    state_.cinematic.startCameraPos=state_.camera.pos;
    state_.camera.firstPerson=false;
    state_.player.alive=false; state_.player.battery=0.0f; state_.player.vel={}; state_.player.jumpVel=0.0f;
    state_.vacuum=VacuumState{};
    clearActivePowerups();
    for(auto& flower:state_.flowers) flower=FlowerPowerupState{};
    state_.energy.comboHits=0; state_.energy.comboMultiplier=1.0f; state_.energy.lastComboHitTime=-9999.0f;
    if(state_.audio.slurpPlaying){state_.audio.slurpPlaying=false;emitAudio(AudioCue::SlurpRingtoneStop,0.13f);}
    emitAudio(AudioCue::VcEnded,0.64f);
    clearInputState();
    state_.hud.batteryFill=0.0f; state_.hud.lowBattery=true; state_.hud.gameOver=true;
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
    state_.debug.colliderCount = std::min(ROOM_COLLIDER_COUNT, 8 + std::min(state_.roomIndex, 7));
    for (auto& c : state_.roomColliders) c = RoomCollider{};
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        float px = (seededRoomValue(20 + i) - 0.5f) * (ROOM_WIDTH - 8.0f);
        float pz = ROOM_MIN_SPAWN_Z + seededRoomValue(60 + i) * (ROOM_MAX_SPAWN_Z - ROOM_MIN_SPAWN_Z);
        const bool keepStartClear = std::abs(px) < 4.5f && pz > ROOM_START_Z - 4.5f;
        const bool keepGoalClear = std::abs(px) < 5.5f && std::abs(pz - ROOM_GRID_Z) < 4.5f;
        if (keepStartClear) px += px < 0.0f ? -5.0f : 5.0f;
        if (keepGoalClear) pz += 5.0f;
        const float w = 1.0f + seededRoomValue(120 + i) * 1.7f;
        const float d = 1.0f + seededRoomValue(150 + i) * 1.7f;
        const float h = 0.55f + seededRoomValue(90 + i) * 1.45f;
        RoomCollider& c = state_.roomColliders[i];
        c.minX = px - w * 0.5f; c.maxX = px + w * 0.5f;
        c.minZ = pz - d * 0.5f; c.maxZ = pz + d * 0.5f;
        c.bottomY = 0.0f; c.topY = h;
        c.width = w; c.depth = d; c.height = h; c.center = {px, h * 0.5f, pz};
    }
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
    if (keyCode == KEY_C && down) input.cameraTogglePressed = true;
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
    input.jumpPressed = false;
    input.meleePressed = false;
    input.shootPressed = false;
    input.cameraTogglePressed = false;
    input.touching = false;
}

void Game::setTouch(int action, float x, float y, int pointerCount) {
    (void)pointerCount;
    if(state_.dead && action==TOUCH_DOWN){restart(); return;}
    // Android's role-based controller owns gameplay input.  This legacy raw
    // touch channel is retained only for restart/touch diagnostics; allowing it
    // to own primaryHeld made every action-button press vacuum simultaneously.
    InputState& input = state_.input;
    if (action == TOUCH_DOWN) {
        input.touching = true;
        input.touchX = input.lastTouchX = x; input.touchY = input.lastTouchY = y;
    } else if (action == TOUCH_MOVE) {
        input.touchX = input.lastTouchX = x; input.touchY = input.lastTouchY = y;
    } else if (action == TOUCH_UP || action == TOUCH_CANCEL) {
        input.touching = false;
    }
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
    if(state_.dead) {
        updateDeathCamera(dt);
        updateParticles(dt*DEATH_PRESENTATION_SCALE);
        state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
        state_.hud.lowBattery=state_.player.battery<24.0f;
        state_.hud.gameOver=true;
        return;
    }
    if(!state_.started) {
        state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
        state_.hud.lowBattery=state_.player.battery<24.0f;
        state_.hud.gameOver=false;
        return;
    }
    if(state_.cinematic.introActive) {
        clearInputState();
        state_.phoneVisual=makePhoneVisualState(0.0f,0.0f,0.0f,state_.time,false);
        updatePhoneTransform();
        updateIntroCamera(dt);
        state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0.0f,1.0f);
        state_.hud.lowBattery=state_.player.battery<24.0f;
        state_.hud.gameOver=false;
        return;
    }
    if(state_.uiPaused){
        updateCamera(0.0f);
        updateSoulLattices();
        updateCrosshair(0.0f);
        return;
    }
    if(state_.multiplayer.enabled&&!state_.multiplayer.authoritativeHost){updateNetworkGuest(dt);return;}
    updateInputActions(dt);
    updatePlayer(dt);
    updateNetworkPeers(dt);
    state_.phoneVisual = makePhoneVisualState(state_.vacuum.pose, state_.vacuum.power, 0.0f, state_.time, state_.camera.firstPerson);
    const float phoneActionAmount=std::max(state_.vacuum.pose,state_.energy.dischargePositionAmount);
    state_.phoneVisual.actionLift=phoneActionAmount*0.65f;
    state_.phoneVisual.actionForward=phoneActionAmount*0.25f;
    updatePhoneTransform();
    processPendingShots(dt);
    updateTargets(dt);
    updateVacuum(dt);
    updateSlurpAudio();
    updateBattery(dt);
    if(state_.dead) {
        state_.hud.batteryFill=0.0f; state_.hud.lowBattery=true; state_.hud.gameOver=true;
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
}

void Game::configureNetworkHost(){state_.multiplayer=MultiplayerRuntimeState{};state_.multiplayer.enabled=true;state_.multiplayer.authoritativeHost=true;state_.multiplayer.connected=true;state_.multiplayer.localPlayerId=0;std::snprintf(state_.multiplayer.status.data(),state_.multiplayer.status.size(),"HOSTING");}
void Game::configureNetworkGuest(int localPlayerId){state_.multiplayer=MultiplayerRuntimeState{};state_.multiplayer.enabled=true;state_.multiplayer.authoritativeHost=false;state_.multiplayer.connected=true;state_.multiplayer.localPlayerId=std::max(1,std::min(NETWORK_PLAYER_COUNT-1,localPlayerId));std::snprintf(state_.multiplayer.status.data(),state_.multiplayer.status.size(),"CONNECTED");}
void Game::disableNetwork(){state_.multiplayer=MultiplayerRuntimeState{};}
void Game::setNetworkRoom(const char* code,const char* status,bool connected){if(code)std::snprintf(state_.multiplayer.roomCode.data(),state_.multiplayer.roomCode.size(),"%.6s",code);if(status)std::snprintf(state_.multiplayer.status.data(),state_.multiplayer.status.size(),"%.63s",status);state_.multiplayer.connected=connected;}
void Game::setNetworkPeerActive(int playerId,bool active){if(playerId<0||playerId>=NETWORK_PLAYER_COUNT||playerId==state_.multiplayer.localPlayerId)return;auto& peer=state_.multiplayer.peers[playerId];if(active&&!peer.active){peer=NetworkPeerState{};peer.active=true;peer.playerId=playerId;peer.player.pos=state_.player.pos+Vec3{static_cast<float>(playerId)*0.75f,0,0};peer.camera=state_.camera;peer.energy=state_.energy;}else if(!active)peer=NetworkPeerState{};}
void Game::setNetworkPeerInput(int playerId,unsigned int sequence,float moveX,float moveZ,float yaw,float pitch,unsigned short buttons){if(playerId<=0||playerId>=NETWORK_PLAYER_COUNT||!state_.multiplayer.authoritativeHost)return;setNetworkPeerActive(playerId,true);auto& peer=state_.multiplayer.peers[playerId];if(sequence<=peer.lastInputSequence)return;const unsigned short previous=peer.inputButtons;peer.lastInputSequence=sequence;peer.inputButtons=buttons;peer.input.touchMoveX=clampf(moveX,-1,1);peer.input.touchMoveZ=clampf(moveZ,-1,1);peer.input.touchSprint=(buttons&(1u<<4))!=0;peer.input.touchPrimaryHeld=(buttons&(1u<<6))!=0;peer.input.jumpPressed=(buttons&(1u<<5))!=0&&(previous&(1u<<5))==0;peer.input.meleePressed=(buttons&(1u<<7))!=0&&(previous&(1u<<7))==0;peer.input.shootPressed=(buttons&(1u<<8))!=0&&(previous&(1u<<8))==0;peer.input.cameraTogglePressed=(buttons&(1u<<9))!=0&&(previous&(1u<<9))==0;peer.camera.yaw=yaw;peer.camera.pitch=clampf(pitch,-DB_PI*0.48f,DB_PI*0.48f);}
void Game::applyNetworkPeerSnapshot(int playerId,const PlayerState& player,float pitch,float vacuumPower,float vacuumPose,int vacuumTarget,float meleeTimer,float dischargeAmount){if(playerId<0||playerId>=NETWORK_PLAYER_COUNT)return;if(playerId==state_.multiplayer.localPlayerId){state_.player=player;state_.camera.pitch=pitch;state_.vacuum.power=vacuumPower;state_.vacuum.pose=vacuumPose;state_.vacuum.target=vacuumTarget;state_.meleeVisual.visualTimer=meleeTimer;state_.energy.dischargePositionAmount=dischargeAmount;updatePhoneTransform();return;}setNetworkPeerActive(playerId,true);auto& peer=state_.multiplayer.peers[playerId];peer.player=player;peer.camera.pitch=pitch;peer.vacuum.power=vacuumPower;peer.vacuum.pose=vacuumPose;peer.vacuum.target=vacuumTarget;peer.meleeVisual.visualTimer=meleeTimer;peer.energy.dischargePositionAmount=dischargeAmount;peer.phoneVisual=makePhoneVisualState(vacuumPose,vacuumPower,0,state_.time,false);peer.phoneTransform.position=player.pos+Vec3{0,0.54f,0};peer.phoneTransform.orientation=quatAxisAngle({0,1,0},player.yaw);peer.phoneTransform.screenRight=rotate(peer.phoneTransform.orientation,{1,0,0});peer.phoneTransform.screenUp=rotate(peer.phoneTransform.orientation,{0,1,0});peer.phoneTransform.screenNormal=rotate(peer.phoneTransform.orientation,{0,0,1});peer.phoneTransform.screenCenter=peer.phoneTransform.position+peer.phoneTransform.screenNormal*PHONE_SCREEN_Z_OFFSET;peer.phoneTransform.vacuumPullPoint=peer.phoneTransform.screenCenter+peer.phoneTransform.screenNormal*0.24f;}

void Game::savePlayerContext(NetworkPeerState& c) const{c.input=state_.input;c.player=state_.player;c.energy=state_.energy;c.camera=state_.camera;c.vacuum=state_.vacuum;c.pendingShots=state_.pendingShots;c.phonePose=state_.phonePose;c.phoneTransform=state_.phoneTransform;c.phoneVisual=state_.phoneVisual;c.hud=state_.hud;c.meleeVisual=state_.meleeVisual;c.meleeCooldown=state_.meleeCooldown;c.meleePose=state_.meleePose;c.meleeComboWindow=state_.meleeComboWindow;}
void Game::loadPlayerContext(const NetworkPeerState& c){state_.input=c.input;state_.player=c.player;state_.energy=c.energy;state_.camera=c.camera;state_.vacuum=c.vacuum;state_.pendingShots=c.pendingShots;state_.phonePose=c.phonePose;state_.phoneTransform=c.phoneTransform;state_.phoneVisual=c.phoneVisual;state_.hud=c.hud;state_.meleeVisual=c.meleeVisual;state_.meleeCooldown=c.meleeCooldown;state_.meleePose=c.meleePose;state_.meleeComboWindow=c.meleeComboWindow;}
void Game::updateNetworkPeers(float dt){if(!state_.multiplayer.enabled||!state_.multiplayer.authoritativeHost)return;NetworkPeerState local;savePlayerContext(local);for(int id=1;id<NETWORK_PLAYER_COUNT;++id){auto& peer=state_.multiplayer.peers[id];if(!peer.active)continue;loadPlayerContext(peer);simulationPlayerId_=id;updateInputActions(dt);updatePlayer(dt);state_.phoneVisual=makePhoneVisualState(state_.vacuum.pose,state_.vacuum.power,0,state_.time,state_.camera.firstPerson);updatePhoneTransform();processPendingShots(dt);updateVacuum(dt);processQueuedSoulCaptures();updateBattery(dt);updateCamera(dt);savePlayerContext(peer);}simulationPlayerId_=0;loadPlayerContext(local);}
void Game::updateNetworkGuest(float dt){simulationPlayerId_=state_.multiplayer.localPlayerId;const bool melee=state_.input.meleePressed,shoot=state_.input.shootPressed;state_.input.meleePressed=false;state_.input.shootPressed=false;updateInputActions(dt);state_.input.meleePressed=melee;state_.input.shootPressed=shoot;updatePlayer(dt);state_.vacuum.active=(state_.input.primaryHeld||state_.input.touchPrimaryHeld)&&state_.player.alive&&state_.player.battery>1;state_.vacuum.power=clampf(state_.vacuum.power+(state_.vacuum.active?2.4f:-3.2f)*dt,0,1);state_.vacuum.pose+=((state_.vacuum.active?1.0f:0.0f)-state_.vacuum.pose)*std::min(1.0f,dt*10.0f);state_.phoneVisual=makePhoneVisualState(state_.vacuum.pose,state_.vacuum.power,0,state_.time,state_.camera.firstPerson);updatePhoneTransform();updateCamera(dt);updateSoulLattices();updateCrosshair(dt);state_.hud.batteryFill=clampf(state_.player.battery/100.0f,0,1);state_.hud.storedSouls=state_.player.souls;state_.input.meleePressed=false;state_.input.shootPressed=false;simulationPlayerId_=0;}

void Game::updateInputActions(float dt) {
    InputState& input = state_.input;
    if (input.cameraTogglePressed) state_.camera.firstPerson = !state_.camera.firstPerson;
    state_.camera.yaw -= input.lookDeltaX * 0.003f;
    state_.camera.pitch = clampf(state_.camera.pitch - input.lookDeltaY * 0.003f, -DB_PI * 0.48f, DB_PI * 0.48f);
    input.lookDeltaX = input.lookDeltaY = 0.0f;
    if (input.jumpPressed) { state_.player.jumpBufferTimer = JUMP_BUFFER; tryJump(); }
    if (input.meleePressed) triggerMelee();
    if (input.shootPressed) shootStoredSoul();
    input.cameraTogglePressed = input.jumpPressed = input.meleePressed = input.shootPressed = false;
    state_.meleeCooldown = std::max(0.0f, state_.meleeCooldown - dt);
    state_.meleeComboWindow = std::max(0.0f, state_.meleeComboWindow - dt);
    state_.meleeVisual.visualTimer = std::max(0.0f, state_.meleeVisual.visualTimer - dt);
    state_.meleePose = std::max(0.0f, state_.meleePose - dt * 5.5f);
    state_.vacuum.active = (input.primaryHeld || input.touchPrimaryHeld) && state_.player.alive && state_.player.battery > 1.0f;
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
            const float pushes[4] = {
                std::abs(player.pos.x - (c.minX - radius)), std::abs((c.maxX + radius) - player.pos.x),
                std::abs(localPlayerZ - (c.minZ - radius)), std::abs((c.maxZ + radius) - localPlayerZ)
            };
            int axis = 0; for (int p = 1; p < 4; ++p) if (pushes[p] < pushes[axis]) axis = p;
            if (axis == 0) { player.pos.x = c.minX - radius; if (player.vel.x > 0) player.vel.x = 0; }
            else if (axis == 1) { player.pos.x = c.maxX + radius; if (player.vel.x < 0) player.vel.x = 0; }
            else if (axis == 2) { localPlayerZ = c.minZ - radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z > 0) player.vel.z = 0; }
            else { localPlayerZ = c.maxZ + radius; player.pos.z = tileOriginZ + localPlayerZ; if (player.vel.z < 0) player.vel.z = 0; }
        }
    }
}

void Game::applyWallClimb(float dt) {
    PlayerState& p = state_.player;
    InputState& input = state_.input;
    if (p.grounded || !input.jumpHeld) return;
    const float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    const float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
    if (std::abs(forwardAxis) + std::abs(strafeAxis) <= 0.05f) return;
    Vec3 intent = cameraForwardFlat() * forwardAxis + cameraRightFlat() * strafeAxis;
    intent = normalized(intent);
    Vec3 normal{}; float topY = 0.0f; bool contact = false;
    const float radius = WALL_CLIMB_RADIUS, climbGap = 0.08f, localZ = wrapZ(p.pos.z);
    constexpr float doorwayClimbHalfWidth = 2.1f;
    const float minX = -ROOM_WIDTH * 0.5f + 1.1f, maxX = ROOM_WIDTH * 0.5f - 1.1f;
    const float minZ = -ROOM_DEPTH * 0.5f + 0.8f, maxZ = ROOM_DEPTH * 0.5f - 0.72f;
    if (p.pos.x <= minX + climbGap) { normal = {1,0,0}; topY = getPlayerCeilingLimit(); contact = true; }
    else if (p.pos.x >= maxX - climbGap) { normal = {-1,0,0}; topY = getPlayerCeilingLimit(); contact = true; }
    // The front/rear wall is segmented around the portal.  Treating its open
    // centre as a climbable boundary creates an invisible step while crossing.
    else if (localZ <= minZ + climbGap && std::abs(p.pos.x) > doorwayClimbHalfWidth) { normal = {0,0,1}; topY = getPlayerCeilingLimit(); contact = true; }
    else if (localZ >= maxZ - climbGap && std::abs(p.pos.x) > doorwayClimbHalfWidth) { normal = {0,0,-1}; topY = getPlayerCeilingLimit(); contact = true; }
    for (int i = 0; !contact && i < state_.debug.colliderCount; ++i) {
        const RoomCollider& c = state_.roomColliders[i];
        const bool inZ = localZ > c.minZ - radius && localZ < c.maxZ + radius;
        const bool inX = p.pos.x > c.minX - radius && p.pos.x < c.maxX + radius;
        if (inZ && std::abs(p.pos.x - (c.minX - radius)) < climbGap + 0.05f) { normal={-1,0,0}; topY=c.topY; contact=true; }
        else if (inZ && std::abs(p.pos.x - (c.maxX + radius)) < climbGap + 0.05f) { normal={1,0,0}; topY=c.topY; contact=true; }
        else if (inX && std::abs(localZ - (c.minZ - radius)) < climbGap + 0.05f) { normal={0,0,-1}; topY=c.topY; contact=true; }
        else if (inX && std::abs(localZ - (c.maxZ + radius)) < climbGap + 0.05f) { normal={0,0,1}; topY=c.topY; contact=true; }
    }
    if (!contact || dotXZ(intent, normal) >= WALL_CLIMB_PUSH_DOT) return;
    const float climbLimit = std::min(getPlayerCeilingLimit(), topY + GROUND_Y + WALL_CLIMB_MAX_HEIGHT);
    if (p.pos.y >= climbLimit) { p.pos.y = climbLimit; if (p.jumpVel > 0) p.jumpVel = 0; return; }
    p.jumpVel = std::max(p.jumpVel, WALL_CLIMB_SPEED * batteryPower(p));
    p.vel.x *= WALL_CLIMB_GRIP; p.vel.z *= WALL_CLIMB_GRIP;
    spendBattery(BATTERY_WALL_CLIMB_DRAIN * dt,BatteryReason::Climb);
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
        state_.roomIndex += 1; state_.roomSeed += 9973 + static_cast<int>(seededRoomValue(991.0f)*1000000.0f); state_.topology.advancing = true;
        advanceRunRulesForRoom();
        state_.roomClear=false;
        gainBattery(18.0f,BatteryReason::NextRoom);
        state_.player.souls=0;
        state_.player.storedSoulBrute.fill(false);
        state_.requiredSouls=std::min(9,5+state_.runRules.requiredSlotStacks);
        state_.depositedSouls=0;
        const float startX=-((static_cast<float>(state_.requiredSouls)-1.0f)*0.82f)*0.5f;
        for(int i=0;i<CAPTURE_COUNT;++i){state_.captures[i]=CapturePointState{}; state_.captures[i].pos={startX+static_cast<float>(i)*0.82f,3.05f,ROOM_GRID_Z};}
        for(auto& bullet:state_.bullets) bullet=BulletState{};
        for(auto& pending:state_.pendingShots) pending=PendingShotState{};
        buildRoomColliders();
        for(auto& request:state_.respawnQueue) request=HumanRespawnRequest{};
        for(int i=0;i<TARGET_COUNT;++i){if(i<activeHumanTarget()) respawnTarget(i); else state_.targets[i]=TargetState{};}
    }
}

int Game::activeHumanTarget() const {
    return std::min(TARGET_COUNT,std::min(ACTIVE_HUMAN_TARGET_CAP,
        ACTIVE_HUMAN_TARGET+std::max(0,state_.roomIndex-1)+state_.runRules.crowdedRoomStacks));
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

void Game::updatePlayer(float dt) {
    PlayerState& p = state_.player;
    InputState& input = state_.input;
    const float previousZ = p.pos.z;
    if (p.jumpBufferTimer > 0) p.jumpBufferTimer = std::max(0.0f, p.jumpBufferTimer - dt);
    if (p.grounded) p.coyoteTimer = COYOTE_TIME; else p.coyoteTimer = std::max(0.0f, p.coyoteTimer - dt);
    const bool running = input.sprint || input.touchSprint;
    const float power = batteryPower(p);
    const bool committedLunge=state_.meleeVisual.locomotionLunge&&state_.meleeVisual.airLungeTimer>0.0f;
    const float airControl = p.grounded ? 1.0f : AIR_ACCEL_MULT*(committedLunge?0.18f:1.0f);
    const float airSpeed = p.grounded ? 1.0f : AIR_MAX_SPEED_MULT;
    const float accel = (running ? RUN_ACCEL : WALK_ACCEL) * power * airControl;
    const float maxSpeed = (running ? RUN_MAX_SPEED : WALK_MAX_SPEED) * power * airSpeed;
    updateMeleeDash(dt);
    const float vacuumSlow = 1.0f - state_.vacuum.pose * (1.0f - VACUUM_MOVE_MULT);
    float forwardAxis = (input.forward ? 1.0f : 0.0f) - (input.back ? 1.0f : 0.0f) + input.touchMoveZ;
    float strafeAxis = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f) + input.touchMoveX;
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
        const float flightTime=std::max(0.05f,melee.airLungeTimer);
        const Vec3 predicted=p.pos+direction*(melee.airLungeSpeed*flightTime);
        const float landingY=getPlayerSupportY(predicted.x,predicted.z);
        p.jumpVel=(landingY-p.pos.y+0.5f*GRAVITY*flightTime*flightTime)/flightTime;
        melee.airLungePending = false;
    }
    if (!p.grounded) {
        p.jumpVel -= GRAVITY * dt;
        applyWallClimb(dt);
        p.pos.y += p.jumpVel * dt;
        if (p.pos.y > getPlayerCeilingLimit()) { p.pos.y = getPlayerCeilingLimit(); if (p.jumpVel > 0) p.jumpVel = 0; }
        const float support = getPlayerSupportY(p.pos.x, p.pos.z);
        if (p.pos.y <= support) {
            p.pos.y = support; p.jumpVel = 0; p.grounded = true; p.coyoteTimer = COYOTE_TIME; p.airJumpsRemaining = 1;
            state_.phonePose.doubleJumpTimer = 0.0f;
            if (horizontalLength(p.vel) > 1.2f) p.vel *= LANDING_MOMENTUM_BOOST;
        }
    }
    if (p.grounded && p.jumpBufferTimer > 0) startGroundJump();
    p.pos += p.vel * dt;
    resolvePlayerObstacleCollisions();
    if(simulationPlayerId_==0) updateRoomTopology(previousZ, p.pos.z);
    if (p.pos.y > getPlayerCeilingLimit()) { p.pos.y = getPlayerCeilingLimit(); if (p.jumpVel > 0) p.jumpVel = 0; }
    const float supportAfter = getPlayerSupportY(p.pos.x, p.pos.z);
    if (p.grounded) {
        if (p.pos.y > supportAfter + 0.12f) p.grounded = false; else p.pos.y = supportAfter;
    } else if (p.jumpVel <= 0 && p.pos.y <= supportAfter) {
        p.pos.y = supportAfter; p.jumpVel = 0; p.grounded = true; p.coyoteTimer = COYOTE_TIME; p.airJumpsRemaining = 1;
        state_.phonePose.doubleJumpTimer = 0.0f;
    }
    if(p.grounded&&state_.meleeVisual.airLungeLandingPending){
        state_.meleeVisual.airLungeLandingPending=false;
        state_.meleeVisual.airLungeTimer=0.0f;
        state_.meleeVisual.visualTimer=0.0f;
        state_.meleeVisual.airLungeAngularVelocity=0.0f;
    }
    const float minX = -ROOM_WIDTH * 0.5f + 1.1f, maxX = ROOM_WIDTH * 0.5f - 1.1f;
    if (p.pos.x < minX) { p.pos.x = minX; if (p.vel.x < 0) p.vel.x = 0; p.vel.z *= WALL_SLIDE_RETENTION; }
    else if (p.pos.x > maxX) { p.pos.x = maxX; if (p.vel.x > 0) p.vel.x = 0; p.vel.z *= WALL_SLIDE_RETENTION; }
    const float friction = p.grounded ? GROUND_FRICTION : AIR_FRICTION;
    p.vel *= std::pow(friction, dt * 60.0f);
    p.targetYaw = state_.camera.yaw;
    p.yaw = state_.camera.yaw;
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
    const Vec3 forward{-std::sin(state_.player.yaw), 0.0f, -std::cos(state_.player.yaw)};
    const Vec3 right{std::cos(state_.player.yaw), 0.0f, -std::sin(state_.player.yaw)};
    transform.orientation = quatNormalized(
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

void Game::updatePhoneActionPose(float dt, bool running, float forwardAxis, float strafeAxis) {
    PhonePoseState& pose = state_.phonePose;
    pose.doubleJumpVacuumPause = std::max(0.0f, pose.doubleJumpVacuumPause - dt);
    pose.doubleJumpTimer = std::max(0.0f, pose.doubleJumpTimer - dt);
    const bool locomotionLunge = state_.meleeVisual.locomotionLunge && state_.meleeVisual.visualTimer > 0.0f;
    const bool jumpFlip = pose.doubleJumpTimer > 0.0f && !locomotionLunge;
    const bool dischargeFacing = state_.energy.dischargeTimer > 0.0f;
    state_.energy.dischargePositionAmount += ((dischargeFacing ? 1.0f : 0.0f) - state_.energy.dischargePositionAmount) * std::min(1.0f, dt * 12.0f);
    const bool vacuumFacing = (state_.vacuum.active && pose.doubleJumpVacuumPause <= 0.0f) || dischargeFacing;
    const float targetTurn = vacuumFacing ? 1.0f : 0.0f;
    pose.screenForwardTurn += (targetTurn - pose.screenForwardTurn) * std::min(1.0f, dt * 4.5f);
    const float easedTurn = pose.screenForwardTurn*pose.screenForwardTurn*(3.0f-2.0f*pose.screenForwardTurn);

    const float inputMag = std::max(1.0f, std::sqrt(forwardAxis*forwardAxis + strafeAxis*strafeAxis));
    const float lean = running ? 0.5f : 0.35f;
    Quat base = quatAxisAngle({0,1,0}, state_.camera.yaw);
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
        if (melee.visualTimer > 0.0f) {
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
                q = quatAxisAngle({0,1,0}, state_.camera.yaw)
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

    float nearestT = 1.0f;
    for (int i = 0; i < state_.debug.colliderCount; ++i) {
        const float t = getSegmentAabbHitT(localStart, localEnd, state_.roomColliders[i], CAMERA_COLLISION_RADIUS);
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
        const float t = getSegmentAabbHitT(localStart, localEnd, c, CAMERA_COLLISION_RADIUS * 0.8f);
        if (t >= 0.0f && t < nearestT) nearestT = t;
    }

    if (nearestT < 1.0f) {
        const Vec3 segment = end - start;
        const float dist = length(segment);
        const float safeT = dist > 0.001f ? std::max(0.0f, nearestT - CAMERA_COLLISION_BACKOFF / dist) : 0.0f;
        desired = start + segment * safeT;
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
    if (camera.firstPerson) {
        camera.pos = player.pos + Vec3{0, 0.72f, 0} + aimForward * 0.18f;
        camera.lookTarget = camera.pos + aimForward * 10.0f;
        camera.forward = normalized(camera.lookTarget-camera.pos);
        return;
    }
    Vec3 desired = player.pos - aimForward * 3.0f + Vec3{0, 1.1f, 0};
    if (desired.y < GROUND_Y + 0.8f) desired.y = GROUND_Y + 0.8f;
    constrainThirdPersonCamera(desired, player.pos);
    Vec3 desiredTarget=player.pos+aimForward*10.0f;
    desiredTarget.y+=0.45f;
    if(state_.meleeVisual.airLungeCameraLag>0.0f&&dt>0.0f){
        const float response=1.0f-std::exp(-AIR_MELEE_CAMERA_RESPONSE*dt);
        camera.pos+= (desired-camera.pos)*response;
        camera.lookTarget+=(desiredTarget-camera.lookTarget)*response;
    }else{
        camera.pos=desired;
        camera.lookTarget=desiredTarget;
    }
    camera.forward = normalized(camera.lookTarget-camera.pos);
}

void Game::updateIntroCamera(float dt) {
    CinematicState& cinematic = state_.cinematic;
    if (!cinematic.introActive) return;
    cinematic.introElapsed = std::min(INTRO_CAMERA_DURATION, cinematic.introElapsed + dt);
    const float linear = clampf(cinematic.introElapsed / INTRO_CAMERA_DURATION, 0.0f, 1.0f);
    const float productPhase=clampf(linear/0.68f,0.0f,1.0f);
    const float productEase=smooth01(productPhase);
    const Vec3 phoneFocus=state_.phoneTransform.position+Vec3{0.0f,0.02f,0.0f};
    const float productYaw=cinematic.baseYaw-0.58f+productEase*0.76f;
    const Vec3 productForward{-std::sin(productYaw),0.0f,-std::cos(productYaw)};
    const Vec3 productCamera=phoneFocus-productForward*0.54f+Vec3{0.0f,0.11f,0.0f};

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
    spendBattery(BATTERY_JUMP_COST,BatteryReason::Jump);
    p.jumpVel = JUMP_SPEED; p.grounded = false; p.coyoteTimer = 0; p.jumpBufferTimer = 0; p.airJumpsRemaining = 1;
}
void Game::startAirJump() {
    PlayerState& p = state_.player;
    spendBattery(BATTERY_DOUBLE_JUMP_COST,BatteryReason::DoubleJump);
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

void Game::triggerMelee() {
    if (state_.meleeCooldown > 0) return;
    const int comboIndex = state_.meleeComboWindow > 0.0f ? (state_.meleeVisual.comboIndex + 1) % 4 : 0;
    const MeleeCombo& combo = MELEE_COMBOS[comboIndex];
    if (!spendBattery(combo.cost,BatteryReason::Melee)) return;
    state_.meleeComboWindow = MELEE_COMBO_WINDOW;
    state_.meleeCooldown = combo.cooldown; state_.meleePose = 1.0f;
    MeleeVisualState& visual = state_.meleeVisual;
    visual.comboIndex=comboIndex; visual.variant=combo.variant; visual.range=combo.range; visual.damage=combo.damage;
    visual.hitRadius=combo.hitRadius; visual.visualDuration=combo.visual; visual.visualTimer=combo.visual;
    const bool airborne=!state_.player.grounded;
    visual.locomotionLunge=airborne;
    visual.dashTimer=airborne?0.0f:combo.dash; visual.dashSpeed=combo.dashSpeed; visual.travel=0.0f; visual.lunge=combo.lunge;
    visual.airLungePending=airborne;visual.airLungeLandingPending=airborne;
    visual.airLungeSpeed=airborne?AIR_MELEE_LOCOMOTION_DISTANCE/AIR_MELEE_LOCOMOTION_DURATION:0.0f;
    visual.airLungeTimer=airborne?AIR_MELEE_LOCOMOTION_DURATION:0.0f;
    visual.airLungeRotation=0.0f;visual.airLungeAngularVelocity=airborne?AIR_MELEE_ANGULAR_VELOCITY:0.0f;visual.airLungeCameraLag=airborne?1.0f:0.0f;
    if(airborne){visual.visualDuration=AIR_MELEE_LOCOMOTION_DURATION;visual.visualTimer=AIR_MELEE_LOCOMOTION_DURATION;}
    visual.recoilDistance=combo.recoilDistance; visual.recoilSpeed=combo.recoilSpeed; visual.visualHit=false; visual.hitMask=0;
    visual.direction=cameraForwardFlat(); visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
    visual.impact=visual.origin+visual.direction*(combo.range*0.72f);
    if(!airborne) applyMeleeHits();
}

int Game::applyMeleeHits() {
    MeleeVisualState& visual=state_.meleeVisual;
    int newHits=0; int totalHits=0;
    for(int i=0;i<TARGET_COUNT;++i) if((visual.hitMask&(1u<<i))!=0) ++totalHits;
    for (int i=0;i<TARGET_COUNT;++i) { TargetState& t=state_.targets[i]; if (!t.alive || (visual.hitMask&(1u<<i))!=0) continue;
        const Vec3 delta{t.pos.x-state_.player.pos.x,0,t.pos.z-state_.player.pos.z};
        if(visual.locomotionLunge){
            const float hitRadius=AIR_MELEE_BODY_RADIUS+(t.brute?0.28f:0.0f);
            if(lengthSq(delta)>hitRadius*hitRadius || std::abs(t.pos.y-state_.player.pos.y)>1.15f) continue;
        }else{
            const float forwardDist=dotXZ(delta,visual.direction);
            if(forwardDist < -0.35f || forwardDist > visual.range) continue;
            const Vec3 sideDelta=delta-visual.direction*forwardDist;
            const float hitRadius=visual.hitRadius+(t.brute?0.28f:0.0f);
            if(lengthSq(sideDelta)>hitRadius*hitRadius) continue;
        }
        const Vec3 away = normalized(Vec3{t.pos.x - state_.player.pos.x, 0.0f, t.pos.z - state_.player.pos.z});
        const Vec3 right{std::cos(t.visualYaw), 0.0f, -std::sin(t.visualYaw)};
        t.hitDirectionLocal = clampf(away.x * right.x + away.z * right.z, -1.0f, 1.0f);
        damageSoulShell(i, visual.damage*(1.0f+std::min(0.75f,totalHits*0.12f)));
        spawnFlameBurst(t.pos+Vec3{0,0.65f,0},newHits>0?0.95f+static_cast<float>(newHits+1)*0.18f:0.55f);
        visual.hitMask|=(1u<<i); visual.visualHit=true; visual.impact=t.pos+Vec3{0,0.62f,0}; ++newHits; ++totalHits;
    }
    if(newHits>0){emitAudio(AudioCue::PhoneAttack,0.44f);registerMeleeBatteryHit(newHits);if(!visual.locomotionLunge){const float recoilScale=totalHits>1?0.35f:1.0f;state_.player.pos-=visual.direction*(visual.recoilDistance*recoilScale);state_.player.vel-=visual.direction*(visual.recoilSpeed*recoilScale);visual.dashTimer=totalHits>1?visual.dashTimer*0.35f:0.0f;}}
    return newHits;
}

bool Game::damageSoulShell(int index, float amount) {
    if(index<0 || index>=TARGET_COUNT) return false;
    TargetState& t=state_.targets[index];
    if(!t.alive || t.captureQueued || t.captureCommitted) return false;
    if(!t.slurpable) {
        t.armor-=amount;
        t.hitFlash=1.0f;
        if(t.armor<=0.0f) {
            t.armor=0.0f; t.slurpable=true; t.soulState=SoulState::Free; t.soulMorph=0.0f; t.hitFlash=1.35f;
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
    visual.airLungeCameraLag=std::max(0.0f,visual.airLungeCameraLag-dt*AIR_MELEE_CAMERA_LAG_DECAY);
    if(visual.airLungeTimer>0.0f){
        const float previousTimer=visual.airLungeTimer;
        visual.airLungeTimer=std::max(0.0f,visual.airLungeTimer-dt);
        visual.airLungeRotation+=visual.airLungeAngularVelocity*dt;
        visual.airLungeAngularVelocity*=std::exp(-AIR_MELEE_ANGULAR_DAMPING*dt);
        visual.origin=state_.player.pos+visual.direction*0.22f+Vec3{0,0.42f,0};
        if(!visual.visualHit) visual.impact=visual.origin+visual.direction*(visual.range*0.72f);
        applyMeleeHits();
        if(previousTimer>0.0f&&visual.airLungeTimer<=0.0f&&visual.airLungeLandingPending){
            PlayerState& player=state_.player;
            player.pos.y=getPlayerSupportY(player.pos.x,player.pos.z);
            player.jumpVel=0.0f;
            player.grounded=true;
            player.coyoteTimer=COYOTE_TIME;
            player.airJumpsRemaining=1;
            visual.airLungeLandingPending=false;
            visual.visualTimer=0.0f;
            visual.airLungeAngularVelocity=0.0f;
        }
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
        *slot=BulletState{}; slot->alive=true; slot->life=BULLET_LIFE; slot->brute=pending.brute;
        slot->pos=state_.phoneTransform.screenCenter+state_.phoneTransform.screenNormal*(SCREEN_FRONT_OFFSET+0.28f);
        slot->pos.y=std::max(slot->pos.y,0.95f);
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
    gainBattery(BATTERY_CAPTURE_GAIN,BatteryReason::Ingest);
    feedSupplementalBattery(FLOWER_SLURP_FEED);
    emitAudio(AudioCue::ReceivedMessage,0.58f);
    t.captureQueued=false; t.captureCommitted=false; t.soulState=SoulState::Free; t.networkOwnerPlayerId=-1;
    spawnParticleBurst(capturedAt);
    queueHumanRespawn(capturedAt);
}

void Game::queueHumanRespawn(const Vec3& avoid) {
    if(state_.roomClear) return;
    for(auto& request:state_.respawnQueue) if(!request.active) {
        request.active=true; request.avoid=avoid;
        request.delay=lerpf(HUMAN_RESPAWN_DELAY_MIN,HUMAN_RESPAWN_DELAY_MAX,nextFlowerRandom());
        return;
    }
}

void Game::updateRoomPopulation(float dt) {
    if(state_.roomClear){for(auto& request:state_.respawnQueue) request=HumanRespawnRequest{}; return;}
    for(auto& request:state_.respawnQueue) if(request.active) request.delay-=dt;
    int active=0;
    for(const auto& target:state_.targets) if(target.alive && !target.slurpable && target.soulState==SoulState::Free) ++active;
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

void Game::updateTargets(float dt) {
    state_.enemyAttackCadence=std::max(0.0f,state_.enemyAttackCadence-dt);
    if(state_.enemyAttackOwner>=0){const TargetState& owner=state_.targets[state_.enemyAttackOwner];if(!owner.alive||owner.slurpable||owner.attackTimer<=0.0f)state_.enemyAttackOwner=-1;}
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i];
        if (!t.alive) continue;
        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);
        t.visibility = 1.0f;
        t.vacuumPullAmount = 0.0f;
        t.captureCollapseAmount = clampf(t.ingestProgress, 0.0f, 1.0f);
        if (t.slurpable) {
            t.soulMorph = std::min(1.0f, t.soulMorph + dt / SOUL_MORPH_DURATION);
            t.locomotionAmount = 0.0f;
        } else {
            t.soulMorph = 0.0f;
            const float currentTileOrigin=getRoomTileOriginZ(state_.topology.currentTileIndex);
            const float targetTileOrigin=getRoomTileOriginZ(getRoomTileIndex(t.pos.z));
            if(std::abs(currentTileOrigin-targetTileOrigin)>0.001f){const float shift=currentTileOrigin-targetTileOrigin; t.pos.z+=shift; t.walkTarget.z+=shift;}
            t.pos.y=GROUND_Y; t.attackCooldown=std::max(0.0f,t.attackCooldown-dt);
            int attackedPlayerId=0;
            Vec3 attackedPlayerPos=state_.player.pos;
            float nearestPlayerDistance=horizontalLength(Vec3{attackedPlayerPos.x-t.pos.x,0,attackedPlayerPos.z-t.pos.z});
            if(state_.multiplayer.authoritativeHost){for(int id=1;id<NETWORK_PLAYER_COUNT;++id){const auto& peer=state_.multiplayer.peers[id];if(!peer.active||!peer.player.alive)continue;const float distance=horizontalLength(Vec3{peer.player.pos.x-t.pos.x,0,peer.player.pos.z-t.pos.z});if(distance<nearestPlayerDistance){nearestPlayerDistance=distance;attackedPlayerId=id;attackedPlayerPos=peer.player.pos;}}}
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
                    if(attackedPlayerId==0){state_.player.vel+=away*HUMAN_ATTACK_KNOCKBACK;spendBattery(HUMAN_ATTACK_BATTERY_COST,BatteryReason::Hit);}
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
            if (!t.alive || !t.slurpable || t.captureQueued || t.captureCommitted ||
                (t.soulState != SoulState::Free && t.soulState != SoulState::Attracted)) continue;
            const Vec3 p = nearestWorldPos(t);
            if (!inOffer(p)) continue;
            const float score = length(pullPoint - p) + (insideCylinder(p) ? -3.5f : 0.0f);
            if (score < offeredScore) { offeredScore = score; offeredFreeSoul = i; }
        }
    }

    float bestScore = 1e9f;
    for (int i = 0; i < TARGET_COUNT; ++i) {
        TargetState& t = state_.targets[i];
        if (!t.alive || !t.slurpable) continue;
        if(t.networkOwnerPlayerId>=0&&t.networkOwnerPlayerId!=simulationPlayerId_) continue;
        if (t.soulState == SoulState::Recoiling) {
            t.recoilTime -= dt;
            t.vel.y -= 5.5f * dt;
            t.pos += t.vel * dt;
            const float damping = std::max(0.0f, 1.0f - 3.5f * dt);
            t.vel.x *= damping; t.vel.z *= damping;
            if (t.pos.y < GROUND_Y) { t.pos.y = GROUND_Y; t.vel.y = 0.0f; }
            if (t.recoilTime <= 0.0f) { t.soulState = SoulState::Free; t.networkOwnerPlayerId=-1; }
            continue;
        }
        if (t.captureQueued || t.captureCommitted) continue;
        const Vec3 soulWorld = nearestWorldPos(t);
        const bool offered = attractionActive &&
            (t.soulState == SoulState::Latched || t.soulState == SoulState::Ingesting || i == offeredFreeSoul || insideCylinder(soulWorld));
        if (!attractionActive && (t.soulState == SoulState::Latched || t.soulState == SoulState::Ingesting)) {
            releaseSoul(i); continue;
        }
        if (!offered) {
            if (t.soulState == SoulState::Attracted) { t.soulState = SoulState::Free; t.networkOwnerPlayerId=-1; }
            if (t.soulState == SoulState::Free) { t.ingestProgress = std::max(0.0f, t.ingestProgress - dt * SOUL_CAPTURE_DECAY); t.latchedToScreen = false; t.vel = {}; }
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
            if (!state_.camera.firstPerson) {
                Vec3 phoneLocal = inverseRotate(state_.phoneTransform.orientation, next-state_.phoneTransform.position);
                const float hx=PHONE_SOLID_HALF_X+SOUL_CORE_SOLID_RADIUS, hy=PHONE_SOLID_HALF_Y+SOUL_CORE_SOLID_RADIUS, hz=PHONE_SOLID_HALF_Z+SOUL_CORE_SOLID_RADIUS;
                if (std::abs(phoneLocal.x)<=hx && std::abs(phoneLocal.y)<=hy && std::abs(phoneLocal.z)<=hz) phoneLocal.z=hz;
                next = state_.phoneTransform.position + rotate(state_.phoneTransform.orientation, phoneLocal);
            }
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
    bool aimTarget=state_.vacuum.target!=-1;
    const Vec3 characterForward=cameraForwardFlat();
    for(const auto& target:state_.targets) {
        if(!target.alive) continue;
        Vec3 world=target.pos;
        const int centerTile=getRoomTileIndex(state_.camera.pos.z);
        float bestZ=wrapZ(target.pos.z)+getRoomTileOriginZ(centerTile);
        for(int offset:{-1,1}) {
            const float candidate=wrapZ(target.pos.z)+getRoomTileOriginZ(centerTile+offset);
            if(std::abs(candidate-state_.camera.pos.z)<std::abs(bestZ-state_.camera.pos.z)) bestZ=candidate;
        }
        world.z=bestZ;
        const Vec3 toTarget=world-state_.camera.pos;
        const float forwardDistance=dot3(toTarget,state_.camera.forward);
        if(forwardDistance<=0.0f) continue;
        Vec3 flat{target.pos.x-state_.player.pos.x,0.0f,wrapZ(target.pos.z)-wrapZ(state_.player.pos.z)};
        if(lengthSq(flat)>0.00001f && dot3(normalized(flat),characterForward)<0.72f) continue;
        const Vec3 radial=toTarget-state_.camera.forward*forwardDistance;
        if(lengthSq(radial)<=0.55f*0.55f) {aimTarget=true; break;}
    }
    hud.hasAimTarget=aimTarget;

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
                for(int fill=0;fill<state_.requiredSouls;++fill) if(!state_.captures[fill].filled){state_.captures[fill].filled=true; ++state_.depositedSouls; filledSlot=fill; break;}
                emitAudio(static_cast<AudioCue>(static_cast<int>(AudioCue::Capture1)+state_.captureSoundSlots[filledSlot%5]),0.72f);
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
            if(pointSegmentDistanceSq(shellCenter,previous,b.pos)>hitRadius*hitRadius) continue;
            if(!damageSoulShell(i,b.brute?1.65f:0.9f)) continue;
            target.vel+=b.vel*0.08f;
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
    if(state_.roomClear && !wasClear) emitAudio(AudioCue::PaymentSuccess,0.68f);
}
void Game::clampRoom(Vec3& pos) {
    pos.x = clampf(pos.x, -ROOM_WIDTH * 0.5f + 1.1f, ROOM_WIDTH * 0.5f - 1.1f);
}
