#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace db::debug {

enum class FeatureId : std::uint16_t {
    RuntimeFrame,
    RuntimeSimulation,
    RuntimeRender,
    RuntimePresent,
    PlayerBattery,
    PlayerSouls,
    PlayerAlive,
    PlayerDowned,
    PlayerPosition,
    PlayerVelocity,
    PlayerGrabState,
    PlayerInvincible,
    PlayerInfiniteBattery,
    PlayerInfiniteSouls,
    EnergySupplemental,
    EnergyFlowerStacks,
    VacuumPower,
    VacuumFieldStrength,
    VacuumLockStrength,
    VacuumTarget,
    SoulFreeCount,
    SoulAttractedCount,
    SoulLatchedCount,
    SoulIngestingCount,
    SoulRecoilingCount,
    SoulRevolvingCount,
    SoulLatticeEnabled,
    SoulLatticeNodeCount,
    RoomIndex,
    RoomSeed,
    RoomClear,
    RoomHeat,
    RoomElapsed,
    RoomCaptures,
    DoorTransitionActive,
    DoorTransitionProgress,
    CaptureFilledCount,
    EnemyAliveCount,
    EnemyBruteCount,
    EnemyHealthMultiplier,
    EnemyArmorMultiplier,
    EnemyDamageMultiplier,
    EnemySpeedMultiplier,
    EnemyAttackRateMultiplier,
    EnemyAiEnabled,
    EnemyMovementEnabled,
    EnemyAttacksEnabled,
    EnemyRespawnEnabled,
    BulletAliveCount,
    ParticleAliveCount,
    TvAvailable,
    TvSignal,
    TvDamage,
    TvTolerance,
    TvBroken,
    TvDonationCooldown,
    TvRoomActive,
    ProgressPermanentTokens,
    ProgressShotLevel,
    ProgressLungeLevel,
    ProgressAttackLevel,
    RenderShadows,
    RenderParticles,
    RenderPortalWindow,
    RenderHumans,
    RenderHumanAnimation,
    RenderHumanSkinning,
    RenderSoulLattice,
    RenderTv,
    RenderHud,
    TimeScale,
    ArtificialFrameStall,
    SimulationPaused,
    BreaktestMode,
    Count
};

enum class FeatureKind : std::uint8_t {
    ReadOnly,
    Boolean,
    Integer,
    Float,
    Action,
    Scenario
};

enum class FeatureCategory : std::uint8_t {
    Runtime,
    Player,
    Energy,
    Vacuum,
    Souls,
    Room,
    Enemies,
    Combat,
    Tv,
    Progression,
    Rendering,
    Time,
    Stress
};

struct FeatureDescriptor {
    FeatureId id;
    std::string_view name;
    FeatureCategory category;
    FeatureKind kind;
    double safeMinimum;
    double safeMaximum;
    double breakMinimum;
    double breakMaximum;
    double fineStep;
    double coarseStep;
    bool logChanges;
    bool includeInSnapshot;
};

// Descriptors are registered by the platform-neutral DebugRegistry implementation.
// New gameplay features should add one stable FeatureId and one descriptor, then bind
// getter/setter/action callbacks in a single place. UI, logging, presets, snapshots,
// and automation consume the same registry rather than duplicating switch statements.

} // namespace db::debug
